#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <strings.h>
#include <mqtt_protocol.h>
#include <mosquitto.h>
#include "client_shared.h"
#include "pub_shared.h"

#define UNUSED(A) (void)(A)

/* Global variables for use in callbacks. See sub_client.c for an example of
 * using a struct to hold variables for use in callbacks. */
static bool first_publish = true;
static int last_mid_sent = -1;
static char *line_buf = NULL;
static bool connected = true;
static bool disconnect_sent = false;
static int publish_count = 0;
static bool ready_for_repeat = false;
int mid_sent = 0;
int status = STATUS_CONNECTING;
struct mosq_config cfg;
static struct timeval next_publish_tv;

void init_config(struct mosq_config *cfg, int pub_or_sub)
{
	memset(cfg, 0, sizeof(*cfg));
	cfg->port = -1;
	cfg->max_inflight = 20;
	cfg->keepalive = 60;
	cfg->clean_session = true;
	cfg->eol = true;
	cfg->repeat_count = 1;
	cfg->repeat_delay.tv_sec = 0;
	cfg->repeat_delay.tv_usec = 0;
	cfg->protocol_version = MQTT_PROTOCOL_V311;
}

void client_config_cleanup(struct mosq_config *cfg)
{
	int i;
	free(cfg->id);
	free(cfg->id_prefix);
	free(cfg->host);
	free(cfg->file_input);
	free(cfg->message);
	free(cfg->topic);
	free(cfg->bind_address);
	free(cfg->username);
	free(cfg->password);
	free(cfg->will_topic);
	free(cfg->will_payload);
	free(cfg->format);
	free(cfg->response_topic);
	if(cfg->topics){
		for(i=0; i<cfg->topic_count; i++){
			free(cfg->topics[i]);
		}
		free(cfg->topics);
	}
	if(cfg->filter_outs){
		for(i=0; i<cfg->filter_out_count; i++){
			free(cfg->filter_outs[i]);
		}
		free(cfg->filter_outs);
	}
	if(cfg->unsub_topics){
		for(i=0; i<cfg->unsub_topic_count; i++){
			free(cfg->unsub_topics[i]);
		}
		free(cfg->unsub_topics);
	}
	mosquitto_property_free_all(&cfg->connect_props);
	mosquitto_property_free_all(&cfg->publish_props);
	mosquitto_property_free_all(&cfg->subscribe_props);
	mosquitto_property_free_all(&cfg->unsubscribe_props);
	mosquitto_property_free_all(&cfg->disconnect_props);
	mosquitto_property_free_all(&cfg->will_props);
}

int cfg_add_topic(struct mosq_config *cfg, int type, char *topic, const char *arg)
{
	if(mosquitto_validate_utf8(topic, strlen(topic))){
		fprintf(stderr, "Error: Malformed UTF-8 in %s argument.\n\n", arg);
		return 1;
	}
	if(type == CLIENT_PUB){
		if(mosquitto_pub_topic_check(topic) == MOSQ_ERR_INVAL){
			fprintf(stderr, "Error: Invalid publish topic '%s', does it contain '+' or '#'?\n", topic);
			return 1;
		}
		cfg->topic = strdup(topic);
	}
	return 0;
}

int client_config_load(struct mosq_config *cfg, int pub_or_sub, int argc, char *argv[])
{
	int rc;
	int i;

	init_config(cfg, pub_or_sub);

	/* Deal with real argc/argv */
    /* Process a tokenised single line from a file or set of real argc/argv */
	for(i=1; i<argc; i++){
		if(!strcmp(argv[i], "-m") || !strcmp(argv[i], "--message")){
			if(cfg->pub_mode != MSGMODE_NONE){
				fprintf(stderr, "Error: Only one type of message can be sent at once.\n\n");
				return 1;
			}else if(i==argc-1){
				fprintf(stderr, "Error: -m argument given but no message specified.\n\n");
				return 1;
			}else{
				cfg->message = strdup(argv[i+1]);
				cfg->msglen = strlen(cfg->message);
				cfg->pub_mode = MSGMODE_CMD;
			}
			i++;
		}else if(!strcmp(argv[i], "-t") || !strcmp(argv[i], "--topic")){
			if(i==argc-1){
				fprintf(stderr, "Error: -t argument given but no topic specified.\n\n");
				return 1;
			}else{
				if(cfg_add_topic(cfg, pub_or_sub, argv[i + 1], "-t"))
					return 1;
				i++;
			}
		}
	}

	if(!cfg->host){
		cfg->host = strdup("localhost");
		if(!cfg->host){
			if(!cfg->quiet) fprintf(stderr, "Error: Out of memory.\n");
			return 1;
		}
	}

	rc = mosquitto_property_check_all(CMD_PUBLISH, cfg->publish_props);
	if(rc){
		if(!cfg->quiet) fprintf(stderr, "Error in PUBLISH properties: %s\n", mosquitto_strerror(rc));
		return 1;
	}
	return MOSQ_ERR_SUCCESS;
}

int client_connect(struct mosquitto *mosq, struct mosq_config *cfg)
{
	char *err;
	int rc;
	int port;

	if(cfg->port < 0){
		port = 1883;
	}else{
		port = cfg->port;
	}

	rc = mosquitto_connect_bind_v5(mosq, cfg->host, port, cfg->keepalive, cfg->bind_address, cfg->connect_props);
	if(rc>0){
		if(!cfg->quiet){
			if(rc == MOSQ_ERR_ERRNO){
				err = strerror(errno);
				fprintf(stderr, "Error: %s\n", err);
			}else{
				fprintf(stderr, "Unable to connect (%s).\n", mosquitto_strerror(rc));
			}
		}
		mosquitto_lib_cleanup();
		return rc;
	}
	return MOSQ_ERR_SUCCESS;
}

static void set_repeat_time(void)
{
	gettimeofday(&next_publish_tv, NULL);
	next_publish_tv.tv_sec += cfg.repeat_delay.tv_sec;
	next_publish_tv.tv_usec += cfg.repeat_delay.tv_usec;

	next_publish_tv.tv_sec += next_publish_tv.tv_usec/1e6;
	next_publish_tv.tv_usec = next_publish_tv.tv_usec%1000000;
}

void my_disconnect_callback(struct mosquitto *mosq, void *obj, int rc, const mosquitto_property *properties)
{
	UNUSED(mosq);
	UNUSED(obj);
	UNUSED(rc);
	UNUSED(properties);
	connected = false;
}

int my_publish(struct mosquitto *mosq, int *mid, const char *topic, int payloadlen, void *payload, int qos, bool retain)
{
	ready_for_repeat = false;
	if(cfg.protocol_version == MQTT_PROTOCOL_V5 && cfg.have_topic_alias && first_publish == false){
		return mosquitto_publish_v5(mosq, mid, NULL, payloadlen, payload, qos, retain, cfg.publish_props);
	}else{
		first_publish = false;
		return mosquitto_publish_v5(mosq, mid, topic, payloadlen, payload, qos, retain, cfg.publish_props);
	}
}

void my_connect_callback(struct mosquitto *mosq, void *obj, int result, int flags, const mosquitto_property *properties)
{
	int rc = MOSQ_ERR_SUCCESS;

	UNUSED(obj);
	UNUSED(flags);
	UNUSED(properties);

	if(!result){
		if (cfg.pub_mode == MSGMODE_CMD)
			rc = my_publish(mosq, &mid_sent, cfg.topic, cfg.msglen, cfg.message, cfg.qos, cfg.retain);

		if(rc){
			if(!cfg.quiet){
				switch(rc){
					case MOSQ_ERR_INVAL:
						fprintf(stderr, "Error: Invalid input. Does your topic contain '+' or '#'?\n");
						break;
					case MOSQ_ERR_NOMEM:
						fprintf(stderr, "Error: Out of memory when trying to publish message.\n");
						break;
					case MOSQ_ERR_NO_CONN:
						fprintf(stderr, "Error: Client not connected when trying to publish.\n");
						break;
					case MOSQ_ERR_PROTOCOL:
						fprintf(stderr, "Error: Protocol error when communicating with broker.\n");
						break;
					case MOSQ_ERR_PAYLOAD_SIZE:
						fprintf(stderr, "Error: Message payload is too large.\n");
						break;
					case MOSQ_ERR_QOS_NOT_SUPPORTED:
						fprintf(stderr, "Error: Message QoS not supported on broker, try a lower QoS.\n");
						break;
				}
			}
			mosquitto_disconnect_v5(mosq, 0, cfg.disconnect_props);
		}
	}else{
		if(result && !cfg.quiet){
			if(cfg.protocol_version == MQTT_PROTOCOL_V5){
				fprintf(stderr, "%s\n", mosquitto_reason_string(result));
			}else{
				fprintf(stderr, "%s\n", mosquitto_connack_string(result));
			}
		}
	}
}

void my_publish_callback(struct mosquitto *mosq, void *obj, int mid, int reason_code, const mosquitto_property *properties)
{
	UNUSED(obj);
	UNUSED(properties);

	last_mid_sent = mid;
	if(reason_code > 127){
		if(!cfg.quiet) fprintf(stderr, "Warning: Publish %d failed: %s.\n", mid, mosquitto_reason_string(reason_code));
	}
	publish_count++;

    if(publish_count < cfg.repeat_count){
		ready_for_repeat = true;
		set_repeat_time();
	}else if(disconnect_sent == false){
		mosquitto_disconnect_v5(mosq, 0, cfg.disconnect_props);
		disconnect_sent = true;
	}
}

int pub_shared_loop(struct mosquitto *mosq)
{
	int rc;
	do{
		rc = mosquitto_loop(mosq, 1000, 1);
	}while(rc == MOSQ_ERR_SUCCESS && connected);
	return 0;
}

int main(int argc, char *argv[])
{
	struct mosquitto *mosq = NULL;
	int rc;

	mosquitto_lib_init();

	memset(&cfg, 0, sizeof(struct mosq_config));
	client_config_load(&cfg, CLIENT_PUB, argc, argv);

	mosq = mosquitto_new(cfg.id, true, NULL);
	if(!mosq){
		switch(errno){
			case ENOMEM:
				if(!cfg.quiet) fprintf(stderr, "Error: Out of memory.\n");
				break;
			case EINVAL:
				if(!cfg.quiet) fprintf(stderr, "Error: Invalid id.\n");
				break;
		}
		goto cleanup;
	}

	mosquitto_connect_v5_callback_set(mosq, my_connect_callback);
	mosquitto_disconnect_v5_callback_set(mosq, my_disconnect_callback);
	mosquitto_publish_v5_callback_set(mosq, my_publish_callback);
	mosquitto_int_option(mosq, MOSQ_OPT_PROTOCOL_VERSION, cfg.protocol_version);
	mosquitto_max_inflight_messages_set(mosq, cfg.max_inflight);

	rc = client_connect(mosq, &cfg);
	if(rc)
		goto cleanup;

	rc = pub_shared_loop(mosq);

	mosquitto_destroy(mosq);
	mosquitto_lib_cleanup();
	client_config_cleanup(&cfg);
	free(line_buf);
	return rc;

cleanup:
	mosquitto_lib_cleanup();
	client_config_cleanup(&cfg);
	free(line_buf);
	return 1;
}
