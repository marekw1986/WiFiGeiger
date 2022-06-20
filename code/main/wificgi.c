#include <esp_http_server.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "http_server.h"
#include "cJSON.h"
#include "wificgi.h"

extern const char *TAG;

uint16_t ap_count = 0;
wifi_ap_record_t *ap_info;

static int scanInProgress = 0;
os_timer_t scan_timer;

static char* constructAPsJSON(void);
static void scan_timer_func(void* param);

esp_err_t wifiscan_cgi_get_handler(httpd_req_t *req)
{
	char *out;
	
	if (!check_authentication(req)) {return ESP_OK;}
     
    out = constructAPsJSON();
    if (out) {
		httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, out, strlen(out));
        free(out);
        if (!scanInProgress) {
			os_timer_disarm(&scan_timer);
			os_timer_setfn(&scan_timer, scan_timer_func, NULL);
			os_timer_arm(&scan_timer, 500, 0);	
        }
        return ESP_OK;        
    }
     
    return ESP_FAIL;
}


esp_err_t connstatus_cgi_get_handler(httpd_req_t *req)
{
    char*  buf;
    size_t buf_len;
    char resp[32] = "";
    
    if (!check_authentication(req)) {return ESP_OK;}

    /* Read URL query string length and allocate memory for length + 1,
     * extra byte for null termination */
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (buf) {
            if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query => %s", buf);
            }
            free(buf);
        }
    }
    /* Send response with custom headers and body set as the
     * string passed in user context*/
    httpd_resp_send(req, resp, strlen(resp));

    return ESP_OK;
}

esp_err_t setmode_cgi_get_handler(httpd_req_t *req)
{
    char*  buf;
    size_t buf_len;
    
    if (!check_authentication(req)) {return ESP_OK;}

    /* Read URL query string length and allocate memory for length + 1,
     * extra byte for null termination */
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (buf) {
            if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query => %s", buf);
                char param[32];
                /* Get value of expected key from query string */
                if (httpd_query_key_value(buf, "mode", param, sizeof(param)) == ESP_OK) {
                    ESP_LOGI(TAG, "Found URL query parameter => mode=%s", param);
                    esp_wifi_set_mode(atoi(param));
                }
            }
            free(buf);
        }
    }
    /* Send response with custom headers and body set as the
     * string passed in user context*/
    const char* resp_str = "<html><head>\
    <script language=\"JavaScript\" type=\"text/javascript\">\
    function redirect() {location.href=\"/ui/wifi\";}\
	</script>\
    </head><body onload=\"redirect()\"></body></html>";
    httpd_resp_send(req, resp_str, strlen(resp_str));

    return ESP_OK;
}

esp_err_t connect_cgi_post_handler(httpd_req_t *req)
{
    char*  buf;
    size_t buf_len;
    
    if (!check_authentication(req)) {return ESP_OK;}

    /* Read URL query string length and allocate memory for length + 1,
     * extra byte for null termination */
    buf_len = (req->content_len) + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (buf) {
			int ret = httpd_req_recv(req, buf, buf_len);
			if (ret <= 0) {
				if (ret == HTTPD_SOCK_ERR_TIMEOUT) {httpd_resp_send_408(req);}
				return ESP_FAIL;
			}
			buf[ret] = '\0';
			ESP_LOGI(TAG, "Found POST data => %s", buf);
			char param[32];
			if (httpd_query_key_value(buf, "essid", param, sizeof(param)) == ESP_OK) {
				ESP_LOGI(TAG, "ESSID: %s", param);
			}
			if (httpd_query_key_value(buf, "passwd", param, sizeof(param)) == ESP_OK) {
				ESP_LOGI(TAG, "Password: %s", param);
			}			
			free(buf);
		}
    }
    /* Send response with custom headers and body set as the
     * string passed in user context*/
    httpd_resp_set_hdr(req, "Location", "/ui/wifi/connecting.html");
    httpd_resp_set_status(req, "301 Moved Permanently");
    httpd_resp_send(req, "", strlen(""));

    return ESP_OK;
}

esp_err_t wifiinfo_get_handler(httpd_req_t *req) {
	char *data;
    cJSON *root;
    
    if (!check_authentication(req)) {return ESP_OK;}
	
	root = cJSON_CreateObject();
	{
		char buff[32];
		wifi_mode_t mode;
		if (esp_wifi_get_mode(&mode) == ESP_OK) {
			switch(mode) {
				case WIFI_MODE_STA:
				snprintf(buff, sizeof(buff)-1, "STA");
				break;
				case WIFI_MODE_AP:
				snprintf(buff, sizeof(buff)-1, "AP");
				break;
				case WIFI_MODE_APSTA:
				snprintf(buff, sizeof(buff)-1, "AP+STA");
				break;
				default:
				snprintf(buff, sizeof(buff)-1, "undetermined (error)");
				break;	
			}
			cJSON_AddStringToObject(root, "mode", buff);
		}
		wifi_config_t wifi_cfg;
		if (esp_wifi_get_config(ESP_IF_WIFI_STA, &wifi_cfg) == ESP_OK) {
			cJSON_AddStringToObject(root, "ssid", (const char*)wifi_cfg.sta.ssid);
		}
	}
	data = cJSON_Print(root);
	cJSON_Delete(root);
	httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, data, strlen(data));
    free(data);	
	
	return ESP_OK;
}

void scan_end_event(void* handler_arg, esp_event_base_t base, int32_t id, void* event_data)
{   
    ESP_LOGI(TAG, "Scan ended");
    scanInProgress = 0;
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_LOGI(TAG, "Total APs scanned = %u", ap_count);
    if (ap_count > 0) {
		ap_info = (wifi_ap_record_t*)malloc(ap_count * sizeof(wifi_ap_record_t));
		if (ap_info) {
			ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_count, ap_info));
			for (int i = 0; i < ap_count; i++) {
				ESP_LOGI(TAG, "SSID \t\t%s", ap_info[i].ssid);
				ESP_LOGI(TAG, "RSSI \t\t%d", ap_info[i].rssi);
				ESP_LOGI(TAG, "Channel \t\t%d\n", ap_info[i].primary);
			}
		}
	}
}

static char* constructAPsJSON(void) {
	cJSON *root;
	cJSON *result;
    cJSON *APs;
    cJSON *ap_object;
    
    char buf[32];
	char *out;
	
	root = cJSON_CreateObject();
    if (root == NULL) return NULL;
	cJSON_AddItemToObject(root, "result", result = cJSON_CreateObject());
	cJSON_AddNumberToObject(result, "inProgress", scanInProgress);
    if ( scanInProgress == 0 ) {
        APs = cJSON_AddArrayToObject(result, "APs");
        for (int i = 0; i < ap_count; i++) {
			cJSON_AddItemToArray(APs, ap_object = cJSON_CreateObject());
			cJSON_AddStringToObject(ap_object, "essid", (const char*)ap_info[i].ssid);
			snprintf(buf, sizeof(buf)-1, "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx", ap_info[i].bssid[0],  ap_info[i].bssid[1], ap_info[i].bssid[2], ap_info[i].bssid[3], ap_info[i].bssid[4], ap_info[i].bssid[5]);
			cJSON_AddStringToObject(ap_object, "bssid", buf);
			cJSON_AddNumberToObject(ap_object, "rssi", ap_info[i].rssi);
			cJSON_AddNumberToObject(ap_object, "enc", ap_info[i].authmode);
			cJSON_AddNumberToObject(ap_object, "channel", ap_info[i].primary);
		}
    }
	out = cJSON_Print(root);
	cJSON_Delete(root);
    if (out == NULL) return NULL;
	
	return out;
}

static void scan_timer_func(void* param) {
	wifi_scan_config_t scan_config = {
		.ssid = 0,
		.bssid = 0,
		.channel = 0,	/* 0--all channel scan */
		.show_hidden = 0,
		.scan_type = WIFI_SCAN_TYPE_ACTIVE,
		.scan_time.active.min = 120,
		.scan_time.active.max = 150,
	};	
	
	ESP_LOGI(TAG, "Scanning WiFi"); 
	if (ap_info) {
		free(ap_info);
		ap_info = NULL;
	}
	esp_wifi_scan_start(&scan_config, false);
	scanInProgress = 1;	
}
