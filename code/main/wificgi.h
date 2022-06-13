#ifndef _WIFICGI_H_
#define _WIFICGI_H_

#include <esp_http_server.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "wificgi.h"

esp_err_t wifiscan_cgi_get_handler(httpd_req_t *req);
esp_err_t connstatus_cgi_get_handler(httpd_req_t *req);
esp_err_t setmode_cgi_get_handler(httpd_req_t *req);
esp_err_t connect_cgi_post_handler(httpd_req_t *req);
esp_err_t wifiinfo_get_handler(httpd_req_t *req);
void scan_end_event(void* handler_arg, esp_event_base_t base, int32_t id, void* event_data);

#endif //_WIFICGI_H_
