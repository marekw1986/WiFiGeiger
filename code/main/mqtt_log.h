#ifndef _MQTT_LOG_H_
#define _MQTT_LOG_H_

#include <time.h>

void mqtt_client_init(void);
void mqtt_client_stop(void);
time_t mqtt_get_last_log_timestamp(void);
uint8_t mqtt_get_connection_status(void);

#endif //_MQTT_LOG_H_
