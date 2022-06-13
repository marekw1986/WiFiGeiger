#include <esp_http_server.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "cJSON.h"
#include "wificgi.h"

extern const char *TAG;

static uint8_t scanInProgress = 0;

static char* constructAPsJSON(void);

esp_err_t wifiscan_cgi_get_handler(httpd_req_t *req)
{
    char*  buf;
    size_t buf_len;
	char *out;

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
    
    if (!scanInProgress) {
        ESP_LOGI(TAG, "Scanning WiFi"); 
        esp_wifi_scan_start(NULL, false);
        scanInProgress = 1;
    }
    
    if (scanInProgress) {
        out = constructAPsJSON();
        if (out) {
            httpd_resp_send(req, out, strlen(out));
            free(out);
            return ESP_OK;
        }
    }
     
    httpd_resp_send(req, "", strlen(""));

    return ESP_OK;
}


esp_err_t connstatus_cgi_get_handler(httpd_req_t *req)
{
    char*  buf;
    size_t buf_len;
    char resp[32] = "";

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
    char resp[32] = "";

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
			free(buf);
		}
    }
    /* Send response with custom headers and body set as the
     * string passed in user context*/
    httpd_resp_send(req, resp, strlen(resp));

    return ESP_OK;
}

esp_err_t wifiinfo_get_handler(httpd_req_t *req) {
	char *data;
    cJSON *root;
	
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
    uint16_t ap_count = 0;
    wifi_ap_record_t *ap_info;
    
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
		free(ap_info);
	}
}

static char* constructAPsJSON(void) {
	cJSON *root;
	cJSON *result;
	char *out;
	
	root = cJSON_CreateObject();
    if (root == NULL) return NULL;
	cJSON_AddItemToObject(root, "geiger", result = cJSON_CreateObject());
	cJSON_AddNumberToObject(result, "inProgress", scanInProgress);
	out = cJSON_Print(root);
	cJSON_Delete(root);
    if (out == NULL) return NULL;
	
	return out;
}
