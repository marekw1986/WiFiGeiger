#ifndef _WIFI_H_
#define _WIFI_H_

#include "pswd.h"

#define EXAMPLE_ESP_WIFI_SSID      SSID
#define EXAMPLE_ESP_WIFI_PASS      PASSWORD
#define EXAMPLE_ESP_MAXIMUM_RETRY  100

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

void wifi_init_sta(void);

#endif //_WIFI_H_
