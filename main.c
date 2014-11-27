/*
 * mosq_squasher
 * Karl Palsson <karlp@remake.is> November 2014
 *
 * Take topic pairs and receive on one, publish compressed on the other
 */
#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <event2/event.h>
#include <mosquitto.h>
#include <zlib.h>


#include "app.h"
#include "mosq-manage.h"
#include "uglylogging.h"
#include "version.h"

#define APP_NAME "mosq-squasher"

#define LOG_TAG __FILE__
#define DLOG(format, args...)         ugly_log(UDEBUG, LOG_TAG, format, ## args)
#define ILOG(format, args...)         ugly_log(UINFO, LOG_TAG, format, ## args)
#define WLOG(format, args...)         ugly_log(UWARN, LOG_TAG, format, ## args)
#define fatal(format, args...)        ugly_log(UFATAL, LOG_TAG, format, ## args)

static void task_statistics(evutil_socket_t fd, short what, void *ctx) {
	(void) fd;
	(void) what;
	struct _squash *st = ctx;

	int total_interval = 0;
	int interval = st->stats_dump_interval_secs;
	int total_overall = 0;
	char topic[1024];
	char msg[1024];
	for (int i = 0; i < st->entry_count; i++) {
		struct _squash_entry *e = &st->entries[i];
		WLOG("Topic in: %s, topic out: %s, messages this interval: %d, total: %d\n",
			e->topic_in, e->topic_out, e->msgs_processed_interval, e->msgs_processed_total);
		total_interval += e->msgs_processed_interval;
		total_overall += e->msgs_processed_total;
		e->msgs_processed_interval = 0;
		if (st->stats_topic) {
			sprintf(topic, "%s/topic_in/%s/total", st->stats_topic, e->topic_in);
			sprintf(msg, "%d", e->msgs_processed_total);
			mosquitto_publish(st->mosq, NULL, topic, strlen(msg), msg, 0, true);
			sprintf(topic, "%s/topic_in/%s/successful", st->stats_topic, e->topic_in);
			sprintf(msg, "%d", e->msgs_processed_success);
			mosquitto_publish(st->mosq, NULL, topic, strlen(msg), msg, 0, true);
		}
	}
	if (st->stats_topic) {
		sprintf(topic, "%s/overall/total", st->stats_topic);
		sprintf(msg, "%d", total_overall);
		mosquitto_publish(st->mosq, NULL, topic, strlen(msg), msg, 0, true);
	}
	ILOG("%d total messages processed this %d sec interval\n", total_interval, interval);
}

/**
 * For we just use signals to exit cleanly
 * @param fd unused
 * @param what the signal that we caught
 * @param arg will be a cbms_state pointer
 */
static void app_handle_signals(evutil_socket_t fd, short what, void *arg) {
	(void) what;
	struct _squash *st = arg;
	WLOG("Stopping server due to signal: %d\n", fd);
	event_base_loopbreak(st->base);
}

int parse_options(int argc, char** argv, struct _squash *st) {
	static struct option long_options[] = {
		{"topic", required_argument, NULL, 't'},
		{"stats", optional_argument, NULL, 's'},
		{"verbose", optional_argument, NULL, 'v'},
		{"mqhost", required_argument, NULL, 'm'},
		{"help", no_argument, NULL, 'h'},
		{0, 0, 0, 0},
	};

	int option_index = 0;
	int c;
	char *token_ptr;
	while ((c = getopt_long(argc, argv, "hm:t:v::", long_options, &option_index)) != -1) {
		switch (c) {
			case 0:
				printf("XXXXX Shouldn't really normally come here, only if there's no corresponding option\n");
				printf("option %s", long_options[option_index].name);
				if (optarg) {
					printf(" with arg %s", optarg);
				}
				printf("\n");
				break;
			case 't':
				/* Need to split the in:out string pair */
				token_ptr = strtok(optarg, ":");
				if (!token_ptr) {
					WLOG("topic map not recognised (no ':'): %s\n", optarg);
					break;
				}
				st->entries[st->entry_count].topic_in = strdup(token_ptr);
				token_ptr = strtok(NULL, ":");
				st->entries[st->entry_count].topic_out = strdup(token_ptr);
				DLOG("adding topic map: in: %s, out: %s\n",
					st->entries[st->entry_count].topic_in,
					st->entries[st->entry_count].topic_out);
				st->entry_count++;
				break;
			case 'h':
				printf("%s - usage:\n\n", argv[0]);
				printf("  -m mqhost.example.org, --mqhost=mq.example.org\n");
				printf("\t\t\tHost mqtt broker to connect to (port 1883 fixed at present)\n");
				printf("  -t topic_in:topic_out, --topic=in:out\n");
				printf("\t\t\tspecify topic in/out pairs, only concrete topics supported at this time\n");
				printf("  --stats[=topic]\n");
				printf("\t\t\ttopic prefix for publishing statistics, defaults to 'status/" APP_NAME"'\n");
				printf("  -vXX, --verbose=XX\tspecify a specific verbosity level (0..99)\n");
				printf("  -v, --verbose\tspecify generally verbose logging\n");
				printf("  -h, --help\t\tPrint this help\n");
				exit(EXIT_SUCCESS);
				break;
			case 'm':
				DLOG("MQ host set to %s\n", optarg);
				st->mq_host = strdup(optarg);
				break;
			case 's':
				if (optarg) {
					st->stats_topic = strdup(optarg);
				} else {
					char buf[1024];
					sprintf(buf, "status/" APP_NAME);
					st->stats_topic = strdup(buf);
				}
				DLOG("Statistics topic prefix: %s\n", st->stats_topic);
				break;
			case 'v':
				if (optarg) {
					st->logging_level = atoi(optarg);
				} else {
					st->logging_level = UDEBUG;
				}
				DLOG("Logging level set to %d\n", st->logging_level);
				ugly_init_named(st->logging_level, APP_NAME);
				break;
		}
	}

	if (optind < argc) {
		printf("non-option ARGV-elements: ");
		while (optind < argc)
			printf("%s ", argv[optind++]);
		printf("\n");
	}
	return 0;
}

void msg_zipper(struct mosquitto *mosq, void *obj, const struct mosquitto_message * msg) {
	(void) mosq;
	struct _squash *st = obj;

	for (int i = 0; i < st->entry_count; i++) {
		if (strcmp(msg->topic, st->entries[i].topic_in) == 0) {
			st->entries[i].msgs_processed_total++;
			size_t msz = compressBound(msg->payloadlen);
			uint8_t *buf = malloc(msz);
			if (!buf) {
				WLOG("malloc compression buffer failed, that's bad mkay?\n");
				break;
			}
			int rc = compress2(buf, &msz, msg->payload, msg->payloadlen, Z_BEST_COMPRESSION);
			if (rc != Z_OK) {
				WLOG("compression failed unexpectedly, rc=%d\n", rc);
				goto out;
			}
			// TODO - copying the qos is ok, but still only matches our subscription?
			rc = mosquitto_publish(st->mosq, NULL, st->entries[i].topic_out, msz, buf, msg->qos, msg->retain);
			if (rc == MOSQ_ERR_SUCCESS) {
				st->entries[i].msgs_processed_success++;
			}
out:
			free(buf);
			/* matched, no point looking at remaining topics */
			break;
		}
	}
}

void cfg_set_defaults(struct _squash *st) {
	st->mq_host = strdup("localhost");
	st->logging_level = UDEBUG;
	st->stats_dump_interval_secs = 15;
}

void cfg_dump(struct _squash *st) {
	printf(APP_NAME " operating with MQTT host: %s\n", st->mq_host);
	printf("Compressing the following topic pairs with zlib:\n");
	for (int i = 0; i < st->entry_count; i++) {
		printf("\t%s\t:%s\n", st->entries[i].topic_in, st->entries[i].topic_out);
	}
	if (st->stats_topic) {
		printf("Statistics heirarchy (%d sec interval): %s\n",
			st->stats_dump_interval_secs, st->stats_topic);
	}
}

void app_cleanup(struct _squash *st) {
	event_base_free(st->base);
	for (int i = 0; i < st->entry_count; i++) {
		struct _squash_entry *e = &st->entries[i];
		if (e->topic_in) {
			free(e->topic_in);
		}
		if (e->topic_out) {
			free(e->topic_out);
		}
	}
	if (st->stats_topic) {
		free(st->stats_topic);
	}
}

int main(int argc, char** argv) {
	struct _squash state = {0};
	cfg_set_defaults(&state);
	ugly_init_named(state.logging_level, APP_NAME); // enable default logging for startup code.

	parse_options(argc, argv, &state);
	cfg_dump(&state);

	if (state.entry_count == 0) {
		fatal("No topic maps configured, aborting. Please see /etc/config/%s\n", APP_NAME);
	}

	state.base = event_base_new();
	if (!state.base) {
		fatal("Couldn't initialize event loop\n");
	}
	DLOG("libevent2 -> libevent %s initialized with %s method\n",
		event_get_version(),
		event_base_get_method(state.base));

	ILOG("Running application version %s\n", VERSION_VCS_REVISION);
	state.msg_handler = msg_zipper;
	if (!mosq_setup(&state)) {
		app_cleanup(&state);
		fatal("Setting up mosquitto client failed. Can't continue");
	}

	for (int i = 0; i < state.entry_count; i++) {
		mosquitto_subscribe(state.mosq, NULL, state.entries[i].topic_in, 2);
	}

	/*
	 * Create event loop tasks.  Note, we're being a bit shitty here, and just using periodic timers.
	 * This is really just polling, with a few tidier lumps around it.
	 * We eventually want to move to scheduling device reads a bit better,
	 * this is still just reading a device at most every 100ms, no matter how many are ready to be read
	 */
	struct event *sigterm = event_new(state.base, SIGTERM, EV_SIGNAL | EV_PERSIST, app_handle_signals, &state);
	event_add(sigterm, NULL);
	struct event *sigint = event_new(state.base, SIGINT, EV_SIGNAL | EV_PERSIST, app_handle_signals, &state);
	event_add(sigint, NULL);

	/* stats task */
	struct event *ev_stats = event_new(state.base, -1, EV_PERSIST, task_statistics, &state);
	if (!ev_stats) {
		fatal("Couldn't create the statistics handler: %s", strerror(errno));
	}
	struct timeval stats_interval = {state.stats_dump_interval_secs, 0};
	if (event_add(ev_stats, &stats_interval) == -1) {
		fatal("Couldn't enable the statistics handler: %s", strerror(errno));
	}


	/* libevent loop */
	do {
		if (event_base_got_break(state.base) ||
			event_base_got_exit(state.base))
			break;
		if (state.request_quit) {
			break;
		}
	} while (event_base_loop(state.base, EVLOOP_ONCE) == 0);

	/*
	 * at shutdown time
	 */
	event_free(sigterm);
	event_free(sigint);
	event_free(ev_stats);
	mosq_cleanup(&state);
	app_cleanup(&state);
	return (EXIT_SUCCESS);
}
