/**
 * Karl Palsson <karlp@remake.is> November 2014
 * handle the mosquitto event loop with libevent2
 * and _nothing_ else.  Subscribing and message handler are someone else's job!
 * See LICENSE.txt
 */

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <event2/event.h>


#include "app.h"
#include "mosq-manage.h"
#include "uglylogging.h"

#define LOG_TAG __FILE__
#define DLOG(format, args...)         ugly_log(UDEBUG, LOG_TAG, format, ## args)
#define ILOG(format, args...)         ugly_log(UINFO, LOG_TAG, format, ## args)
#define WLOG(format, args...)         ugly_log(UWARN, LOG_TAG, format, ## args)
#define fatal(format, args...)        ugly_log(UFATAL, LOG_TAG, format, ## args)


static void
mosq_reconnect_handler(evutil_socket_t fd, short what, void *arg) {
	(void)fd;
	(void)what;
	struct _squash *st = arg;
	if (mosq_setup(st)) {
		/* this will handle restarting... */
		/* now schedule a reconnection for 5 seconds away. */
		struct timeval later = {5, 0};
		event_base_once(st->base, -1, EV_TIMEOUT, mosq_reconnect_handler, st, &later);
	}
}


static void
mosq_reconnect(struct _squash *st)
{
	mosq_cleanup(st);
	/* now schedule a reconnection for 5 seconds away. */
	struct timeval later = {5, 0};
	event_base_once(st->base, -1, EV_TIMEOUT, mosq_reconnect_handler, st, &later);
}


static void
mosq_ev_io(evutil_socket_t fd, short what, void *arg) {
	(void)fd;
	int rc;
	struct _squash *st = arg;
	if (what & EV_READ) {
		rc = mosquitto_loop_read(st->mosq, 1);
		if (MOSQ_ERR_SUCCESS != rc) {
			mosq_reconnect(st);
			return;
		}
	}
	if (what & EV_TIMEOUT) {
		rc = mosquitto_loop_misc(st->mosq);
		if (MOSQ_ERR_SUCCESS != rc) {
			mosq_reconnect(st);
			return;
		}
	}
	if (what & EV_WRITE) {
		rc = mosquitto_loop_write(st->mosq, 1);
		if (MOSQ_ERR_SUCCESS != rc) {
			mosq_reconnect(st);
			return;
		}
	}
	/* Only get to here if the other routines passed ok */
	if (mosquitto_want_write(st->mosq)) {
		event_add(st->mosq_write, NULL);
	}
}

void mosq_logger(struct mosquitto *mosq, void *obj, int level, const char *msg)
{
	(void) mosq;
	(void) obj;
	DLOG("mosquitto -> level: %d, msg: %s\n", level, msg);
}


bool mosq_setup(struct _squash *st)
{
	mosquitto_lib_init();

	DLOG("mosquitto -> (re)connecting\n");
	st->mosq = mosquitto_new(NULL, true, st);
	mosquitto_log_callback_set(st->mosq, mosq_logger);
	mosquitto_message_callback_set(st->mosq, st->msg_handler);
	int rc = mosquitto_connect(st->mosq, st->mq_host, 1883, 60);
	if (MOSQ_ERR_SUCCESS != rc) {
		WLOG("Failed to connect: %s\n", strerror(errno));
		rc = -1;
		goto unwind;
	}

	int mosq_fd = mosquitto_socket(st->mosq);
	if (evutil_make_socket_nonblocking(mosq_fd)) {
		WLOG("Failed to make non-blocking: fd = %d, possibly ok\n", mosq_fd);
	}
	st->mosq_readidle = event_new(st->base, mosq_fd, EV_READ|EV_PERSIST, mosq_ev_io, st);
	if (st->mosq_readidle == NULL) {
		WLOG("Failed to create mosquitto read/idle watcher\n");
		rc = -1;
		goto unwind_readidle;
	}
	st->mosq_write = event_new(st->base, mosq_fd, EV_WRITE, mosq_ev_io, st);
	if (st->mosq_write == NULL) {
		WLOG("Failed to create mosquitto write watcher\n");
		rc = -1;
		goto unwind_write;
	}
	if (mosquitto_want_write(st->mosq)) {
		event_add(st->mosq_write, NULL);
	}

	struct timeval mosq_idle_loop_time = { 0, 100 * 1000 };
	if (event_add(st->mosq_readidle, &mosq_idle_loop_time) < 0) {
		WLOG("Failed to activate mosquitto watcher\n");
		rc = -1;
		goto unwind_write;
	}
	goto out;

unwind_write:
	event_free(st->mosq_write);
unwind_readidle:
	event_free(st->mosq_readidle);
unwind:
	mosquitto_destroy(st->mosq);
	mosquitto_lib_cleanup();

out:
	return rc == 0;
}

/* You can only call this if you _setup() succeeded all the way!. */
void mosq_cleanup(struct _squash *st)
{
	event_free(st->mosq_write);
	event_free(st->mosq_readidle);
	mosquitto_disconnect(st->mosq);
	mosquitto_destroy(st->mosq);
	mosquitto_lib_cleanup();
}
