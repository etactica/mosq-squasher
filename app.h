/* 
 * File:   app.h
 * Author: karlp
 *
 * Created on November 26, 2014, 2:36 PM
 */

#ifndef APP_H
#define	APP_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <event2/event.h>
#include <mosquitto.h>

	typedef void (*mosq_cb_msg_t)(struct mosquitto *mosq, void *obj, const struct mosquitto_message *msg);

	struct _squash_entry {
		char *topic_in;
		char *topic_out;
		int msgs_processed_total;
		int msgs_processed_success;
		int msgs_processed_interval;
	};

	struct _squash {
		struct event_base *base;
		struct mosquitto *mosq;
		struct event *mosq_readidle;
		struct event *mosq_write;
		mosq_cb_msg_t msg_handler;
		struct _squash_entry entries[20];
		int entry_count;
		bool request_quit;

		/* Config */
		int stats_dump_interval_secs;
		int logging_level;
		char *mq_host;
		char *stats_topic;
	};


#ifdef	__cplusplus
}
#endif

#endif	/* APP_H */

