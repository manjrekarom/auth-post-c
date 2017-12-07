#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "mgos.h"
#include "mgos_dht.h"
#include "mgos_mqtt.h"
#include "mgos_config.h"
#include "mgos_net.h"
#include "mgos_http_server.h"
#include "mgos_mongoose.h"

#define BUFFER_SIZE 50
#define LED_PIN mgos_sys_config_get_app_led_pin()
char BUFFER[BUFFER_SIZE];

static void publish_data(void *dht) {
	float temp = mgos_dht_get_temp(dht);
	float humid = mgos_dht_get_humidity(dht);
	snprintf(BUFFER, sizeof BUFFER, "%0.3f", temp);
	bool tPub = mgos_mqtt_pub("ESP8266_MO/temp", BUFFER, strlen(BUFFER), 1, 0);
	snprintf(BUFFER, sizeof BUFFER, "%0.3f", humid);
	bool hPub = mgos_mqtt_pub("ESP8266_MO/humidity", BUFFER, strlen(BUFFER), 1, 0);
	if(tPub && hPub) {
		LOG(LL_INFO, ("Published temp: %f & humidity: %f", temp, humid));
	}
	else {
		LOG(LL_INFO, ("Could not publish! Probably not connected"));
	}
}

static void net_cb(enum mgos_net_event ev, const struct mgos_net_event_data *ev_data, void *arg) {
	switch (ev) {
		case MGOS_NET_EV_DISCONNECTED:
		LOG(LL_INFO, ("%s", "Net disconnected"));
		break;
		case MGOS_NET_EV_CONNECTING:
		LOG(LL_INFO, ("%s", "Net connecting..."));
		break;
		case MGOS_NET_EV_CONNECTED:
		LOG(LL_INFO, ("%s", "Net connected"));
		break;
		case MGOS_NET_EV_IP_ACQUIRED:
		LOG(LL_INFO, ("%s", "Net got IP address"));
		break;
	}
	(void) ev_data;
	(void) arg;
}

//SENSOR END-POINTS: Always "GET" 
static void temp_handler(struct mg_connection *nc, int ev, void *p, void *user_data){
	struct mgos_dht *dht = mgos_dht_create(mgos_sys_config_get_app_dht_pin(), DHT22);
	if (ev == MG_EV_HTTP_REQUEST) {
		struct http_message *hm = (struct http_message *) p;
		// We have received an HTTP request. Parsed request is contained in `hm`.
		// Send HTTP reply to the client which shows full original request.
		float temp = mgos_dht_get_temp(dht);
		//float humid = mgos_dht_get_humidity(dht);
		snprintf(BUFFER, sizeof BUFFER, "{\"temp\":%0.3f}\r\n", temp);
		mg_send_head(nc, 200, (int)strlen(BUFFER), "Content-Type: application/json");
		mg_printf(nc, "%.*s", (int)strlen(BUFFER), BUFFER);
		(void) hm;
	}
	(void) user_data;
}

static void echo_handler(struct mg_connection *nc, int ev, void *p, void *user_data){
	if (ev == MG_EV_HTTP_REQUEST) {
		struct http_message *hm = (struct http_message *) p;
		// We have received an HTTP request. Parsed request is contained in `hm`.
		// Send HTTP reply to the client which shows full original request.
		mg_send_head(nc, 200, hm->body.len, "Content-Type: text/plain");
		LOG(LL_INFO, ("%s", hm->body.p));
		mg_printf(nc, "%.*s", (int)hm->body.len, hm->body.p);
	}
	(void) user_data;
}

//ACTUATOR END_POINTS:- 
//	Accepting x-www-form-urlencoded on POST
//	Sends actuator state on GET
static void led_handler(struct mg_connection *nc, int ev, void *p, void *user_data){

	if (ev == MG_EV_HTTP_REQUEST) {
		struct http_message *hm = (struct http_message *) p;
		
		LOG(LL_INFO, ("%s", hm->body.p));
		LOG(LL_INFO, ("%s", hm->method.p));
		if(!strncmp(hm->method.p, "GET", 3)) {
			snprintf(BUFFER, sizeof BUFFER, "{\"state\": %d}\r\n", mgos_gpio_read(LED_PIN));
		}
		else {
			bool if_state = mg_get_http_var(&(hm->body), "state", BUFFER, 50);
			LOG(LL_INFO, ("%s", BUFFER));
			LOG(LL_INFO, ("%d", if_state));
			if(if_state) {
				//  Write pin levels
				if(!strncmp(BUFFER,"1", 1)) {
					mgos_gpio_write(LED_PIN,1);
					snprintf(BUFFER, sizeof BUFFER, "{\"state\": %d}\r\n", mgos_gpio_read(LED_PIN));
				}
				else {
					mgos_gpio_write(LED_PIN,0);
					snprintf(BUFFER, sizeof BUFFER, "{\"state\": %d}\r\n", mgos_gpio_read(LED_PIN));
				}

			}
		}
		
		LOG(LL_INFO, ("%s", BUFFER));
		mg_send_head(nc, 200, (int)strlen(BUFFER), "Content-Type: application/json");
		mg_printf(nc, "%.*s", (int)strlen(BUFFER), BUFFER);
	}

	(void) user_data;
}

enum mgos_app_init_result mgos_app_init(void) {
	mgos_gpio_set_mode(LED_PIN, 1);
	//struct mgos_dht *dht = mgos_dht_create(mgos_sys_config_get_app_pin(), DHT22);
	//mgos_set_timer(mgos_sys_config_get_app_interval(), true, publish_data, dht);
	mgos_net_add_event_handler(net_cb, NULL);
	mgos_register_http_endpoint("/temp", temp_handler, NULL);
	mgos_register_http_endpoint("/echo", echo_handler, NULL);
	mgos_register_http_endpoint("/led", led_handler, NULL);
	(void) publish_data;
	return MGOS_APP_INIT_SUCCESS;
}