#ifndef _WIFI_H_
#define _WIFI_H_

#include "pswd.h"

#define EXAMPLE_ESP_WIFI_SSID      SSID
#define EXAMPLE_ESP_WIFI_PASS      PASSWORD
#define EXAMPLE_ESP_MAXIMUM_RETRY  100

#define EXAMPLE_ESP_SOFTAP_WIFI_SSID	"wifiGeiger"
#define EXAMPLE_ESP_SOFTAP_WIFI_PASS	AP_PASSWORD
#define EXAMPLE_MAX_SOFTAP_STA_CONN	4

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

typedef enum {CONNTRY_IDLE,  CONNTRY_WORKING, CONNTRY_SUCCESS, CONNTRY_FAIL} conTryStatus_t;
conTryStatus_t conTryStatus;

void wifi_init(void);
void wifi_init_sta(void);
void wifi_init_softap(void);
void wifi_init_apsta(void);
esp_err_t wifi_connect_sta(char * ssid, char * password);
void wifi_disconnect_sta(void);

#endif //_WIFI_H_
