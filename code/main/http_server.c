#include <esp_http_server.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "mbedtls/base64.h"
#include "http_server.h"
#include "common.h"
#include "cJSON.h"
#include "geiger.h"
#include "config.h"
#include "wificgi.h"

#define EMPTY_STR               ""
#define OK_STR                  "ok"
#define INVALID_TOKEN_STR       "invalid_token"
#define INVALID_IP_STR          "invalid_ip"
#define INVALID_NETMASK_STR     "invalid_netmask"
#define INVALID_GW_STR          "invalid_gw"
#define INVALID_DNS1_STR        "invalid_dns1"
#define INVALID_DNS2_STR        "invalid_dns2"
#define INVALID_NTP1_STR        "invalid_ntp1"
#define INVALID_NTP2_STR        "invalid_ntp2"
#define INVALID_NTP3_STR        "invalid_ntp3"
#define INVALID_DHCP_STR        "invalid_dhcp"
#define INVALID_TIMEZONE_STR    "invalid_timezone"
#define INVALID_DLS_STR         "invalid_dls"
#define INVALID_MQTT_STR        "invalid_mqtt"
#define INVALID_TOPIC_STR       "invalid_topic"

#define HTTPD_401      "401 UNAUTHORIZED"           /*!< HTTP Response 401 */

extern const char *TAG;

char configToken[10];

static uint8_t is_valid_token(char* str);

char *http_auth_basic(const char *username, const char *password)
{
    int out;
    char *user_info = NULL;
    char *digest = NULL;
    size_t n = 0;
    asprintf(&user_info, "%s:%s", username, password);
    if (user_info) {
		mbedtls_base64_encode(NULL, 0, &n, (const unsigned char *)user_info, strlen(user_info));
		digest = calloc(1, 6 + n + 1);
		strcpy(digest, "Basic ");
		mbedtls_base64_encode((unsigned char *)digest + 6, n, (size_t *)&out, (const unsigned char *)user_info, strlen(user_info));
		free(user_info);
	}
    return digest;
}


/* An HTTP GET handler */
uint8_t check_authentication (httpd_req_t *req)
{
    char*  buf;
    size_t buf_len;

    /* Get header value string length and allocate memory for length + 1,
     * extra byte for null termination */
    buf_len = httpd_req_get_hdr_value_len(req, "Authorization") + 1;
    if (buf_len > 1) {
        buf = calloc(1, buf_len);
        if (httpd_req_get_hdr_value_str(req, "Authorization", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Authorization: %s", buf);
        } else {
            ESP_LOGE(TAG, "No auth value received");
        }
        
        char *auth_credentials = http_auth_basic("admin", config.password);
        if (strncmp(auth_credentials, buf, buf_len)) {
            ESP_LOGE(TAG, "Not authenticated");
            httpd_resp_set_status(req, HTTPD_401);
            httpd_resp_set_hdr(req, "Connection", "keep-alive");
            httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"wifiGeiger\"");
            httpd_resp_send(req, NULL, 0);
            free(auth_credentials);
			free(buf);
			return 0;
        } else {
            ESP_LOGI(TAG, "Authenticated!");
            free(auth_credentials);
			free(buf);
			return 1;			
        }      
    }
	else {
        ESP_LOGE(TAG, "No auth header received");
        httpd_resp_set_status(req, HTTPD_401);
        httpd_resp_set_hdr(req, "Connection", "keep-alive");
        httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"wifiGeiger\"");
        httpd_resp_send(req, NULL, 0);
        return 0;
    }

    return 0;
}


/* An HTTP GET handler */
esp_err_t hello_get_handler(httpd_req_t *req)
{
    char*  buf;
    size_t buf_len;
    
    if (!check_authentication(req)) {return ESP_OK;}

    /* Get header value string length and allocate memory for length + 1,
     * extra byte for null termination */
    buf_len = httpd_req_get_hdr_value_len(req, "Host") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (buf) {
            /* Copy null terminated value string into buffer */
            if (httpd_req_get_hdr_value_str(req, "Host", buf, buf_len) == ESP_OK) {
                ESP_LOGI(TAG, "Found header => Host: %s", buf);
            }
            free(buf);
        }
    }

    buf_len = httpd_req_get_hdr_value_len(req, "Test-Header-2") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (buf) {
            if (httpd_req_get_hdr_value_str(req, "Test-Header-2", buf, buf_len) == ESP_OK) {
                ESP_LOGI(TAG, "Found header => Test-Header-2: %s", buf);
            }
            free(buf);
        }
    }

    buf_len = httpd_req_get_hdr_value_len(req, "Test-Header-1") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (buf) {
            if (httpd_req_get_hdr_value_str(req, "Test-Header-1", buf, buf_len) == ESP_OK) {
                ESP_LOGI(TAG, "Found header => Test-Header-1: %s", buf);
            }
            free(buf);
        }
    }

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
                if (httpd_query_key_value(buf, "query1", param, sizeof(param)) == ESP_OK) {
                    ESP_LOGI(TAG, "Found URL query parameter => query1=%s", param);
                }
                if (httpd_query_key_value(buf, "query3", param, sizeof(param)) == ESP_OK) {
                    ESP_LOGI(TAG, "Found URL query parameter => query3=%s", param);
                }
                if (httpd_query_key_value(buf, "query2", param, sizeof(param)) == ESP_OK) {
                    ESP_LOGI(TAG, "Found URL query parameter => query2=%s", param);
                }
            }
            free(buf);
        }
    }

    /* Set some custom headers */
    httpd_resp_set_hdr(req, "Custom-Header-1", "Custom-Value-1");
    httpd_resp_set_hdr(req, "Custom-Header-2", "Custom-Value-2");

    /* Send response with custom headers and body set as the
     * string passed in user context*/
    const char* resp_str = (const char*) req->user_ctx;
    httpd_resp_send(req, resp_str, strlen(resp_str));

    return ESP_OK;
}


esp_err_t reset_cgi_get_handler(httpd_req_t *req)
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
                if (httpd_query_key_value(buf, "token", param, sizeof(param)) == ESP_OK) {
                    ESP_LOGI(TAG, "Found URL query parameter => token=%s", param);
                    if (!is_valid_token(param)) {
                        httpd_resp_send(req, INVALID_TOKEN_STR, strlen(INVALID_TOKEN_STR));
                        free(buf);
                        return ESP_OK;
					}
					else {
						if (httpd_query_key_value(buf, "hardreset", param, sizeof(param)) == ESP_OK) {
							if (strcmp(param, "yes") == 0) {
								ESP_LOGI(TAG, "Performing hard reset");
								config_load_defaults();
								esp_err_t err = config_save_settings_to_flash();
								if (err != ESP_OK) printf("Error (%s) saving settings to NVS!\n", esp_err_to_name(err));
							}
						}
						httpd_resp_send(req, OK_STR, strlen(OK_STR));
						set_reset_timer();
                        free(buf);
                        return ESP_OK;
					}
                }
            }
            free(buf);
        }
    }
    /* Send response with custom headers and body set as the
     * string passed in user context*/
    httpd_resp_send(req, EMPTY_STR, strlen(EMPTY_STR));
    return ESP_OK;
}


esp_err_t config_cgi_post_handler(httpd_req_t *req)
{
    char*  buf;
    size_t buf_len;
    config_t newConfig = config;
    
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
            if (httpd_query_key_value(buf, "token", param, sizeof(param)) == ESP_OK) {
                if (!is_valid_token(param)) {
                    httpd_resp_send(req, INVALID_TOKEN_STR, strlen(INVALID_TOKEN_STR));
                    free(buf);
                    return ESP_OK;                }
                else {
                    //parse ip here
                    if (httpd_query_key_value(buf, "ip", param, sizeof(param)) == ESP_OK) {
                        if (is_valid_ip_address(param)) {
                            newConfig.ip.addr = ipaddr_addr(param);
                        }
                        else {
                            httpd_resp_send(req, INVALID_IP_STR, strlen(INVALID_IP_STR));
                            free(buf);
                            return ESP_OK;
                        }
                    }
                    //parse netmask here
                    if (httpd_query_key_value(buf, "netmask", param, sizeof(param)) == ESP_OK) {
                        if (is_valid_ip_address(param)) {
                            newConfig.netmask.addr = ipaddr_addr(param);
                        }
                        else {
                            httpd_resp_send(req, INVALID_NETMASK_STR, strlen(INVALID_NETMASK_STR));
                            free(buf);
                            return ESP_OK;
                        }
                    }
                    //parse gw here
                    if (httpd_query_key_value(buf, "gw", param, sizeof(param)) == ESP_OK) {
                        if (is_valid_ip_address(param)) {
                            newConfig.gw.addr = ipaddr_addr(param);
                        }
                        else {
                            httpd_resp_send(req, INVALID_GW_STR, strlen(INVALID_GW_STR));
                            free(buf);
                            return ESP_OK;
                        }
                    }                    
                    //parse dns1 here
                    if (httpd_query_key_value(buf, "dns1", param, sizeof(param)) == ESP_OK) {
                        if (is_valid_ip_address(param)) {
                            newConfig.dns1.addr = ipaddr_addr(param);
                        }
                        else {
                            httpd_resp_send(req, INVALID_DNS1_STR, strlen(INVALID_DNS1_STR));
                            free(buf);
                            return ESP_OK;
                        }
                    }                    
                    //parse dns2 here
                    if (httpd_query_key_value(buf, "dns2", param, sizeof(param)) == ESP_OK) {
                        if (is_valid_ip_address(param)) {
                            newConfig.dns2.addr = ipaddr_addr(param);
                        }
                        else {
                            httpd_resp_send(req, INVALID_DNS2_STR, strlen(INVALID_DNS2_STR));
                            free(buf);
                            return ESP_OK;
                        }
                    }                    
                    //parse ntp1 here
                    if (httpd_query_key_value(buf, "ntp1", param, sizeof(param)) == ESP_OK) {
                        if ( param[0] && (strlen(param) <= (sizeof(newConfig.ntp1)-1)) ) {
                            strncpy(newConfig.ntp1, param, sizeof(newConfig.ntp1)-1);
                        }
                        else {
                            httpd_resp_send(req, INVALID_NTP1_STR, strlen(INVALID_NTP1_STR));
                            free(buf);
                            return ESP_OK;
                        }
                    }                    
                    //parse ntp2 here
                    if (httpd_query_key_value(buf, "ntp2", param, sizeof(param)) == ESP_OK) {
                        if ( param[0] && (strlen(param) <= (sizeof(newConfig.ntp2)-1)) ) {
                            strncpy(newConfig.ntp2, param, sizeof(newConfig.ntp2)-1);
                        }
                        else {
                            httpd_resp_send(req, INVALID_NTP2_STR, strlen(INVALID_NTP2_STR));
                            free(buf);
                            return ESP_OK;
                        }
                    }                    
                    //parse ntp3 here
                    if (httpd_query_key_value(buf, "ntp3", param, sizeof(param)) == ESP_OK) {
                        if ( param[0] && (strlen(param) <= (sizeof(newConfig.ntp3)-1)) ) {
                            strncpy(newConfig.ntp3, param, sizeof(newConfig.ntp3)-1);
                        }
                        else {
                            httpd_resp_send(req, INVALID_NTP3_STR, strlen(INVALID_NTP3_STR));
                            free(buf);
                            return ESP_OK;
                        }
                    }                    
                    //parse dhcp here
                    if (httpd_query_key_value(buf, "dhcp", param, sizeof(param)) == ESP_OK) {
                        if ( strcmp(param, "yes") == 0 ) { newConfig.use_dhcp = 1; }
                        else if ( strcmp(param, "no") == 0) {newConfig.use_dhcp = 0; }
                        else {
                            httpd_resp_send(req, INVALID_DHCP_STR, strlen(INVALID_DHCP_STR));
                            free(buf);
                            return ESP_OK;
                        }
                    }
                    //parse timezone here
                    if (httpd_query_key_value(buf, "timezone", param, sizeof(param)) == ESP_OK) {
                        int tz = atoi(param);
                        if ( (tz < -11) || (tz > 13) ) {
                            httpd_resp_send(req, INVALID_TIMEZONE_STR, strlen(INVALID_TIMEZONE_STR));
                            free(buf);
                            return ESP_OK;
                        }
                        else {newConfig.timezone = tz;}
                    }
                    //parse dls here
                    if (httpd_query_key_value(buf, "dls", param, sizeof(param)) == ESP_OK) {
                        if (strcmp(param, "yes") == 0) {newConfig.daylight = 1;}
                        else if(strcmp(param, "no") == 0) {newConfig.daylight = 0;}
                        else {
                            httpd_resp_send(req, INVALID_DLS_STR, strlen(INVALID_DLS_STR));
                            free(buf);
                            return ESP_OK;
                        }
                    }
                    //parse mqtt here
                    if (httpd_query_key_value(buf, "mqtt", param, sizeof(param)) == ESP_OK) {
                        if (strlen(param) < sizeof(newConfig.mqtt_server)) {strncpy(newConfig.mqtt_server, param, sizeof(newConfig.mqtt_server)-1);}
                        else {
                            httpd_resp_send(req, INVALID_MQTT_STR, strlen(INVALID_MQTT_STR));
                            free(buf);
                            return ESP_OK;                            
                        }
                    }
                    //parse topic here
                    if (httpd_query_key_value(buf, "topic", param, sizeof(param)) == ESP_OK) {
                        if (strlen(param) < sizeof(newConfig.mqtt_topic)) {strncpy(newConfig.mqtt_topic, param, sizeof(newConfig.mqtt_topic)-1);}
                        else {
                            httpd_resp_send(req, INVALID_TOPIC_STR, strlen(INVALID_TOPIC_STR));
                            free(buf);
                            return ESP_OK;                            
                        }                        
                    }
                    
                    //we parsed all received commands
                    //now save to flash
                    config=newConfig;
                    config_save_settings_to_flash();
                    httpd_resp_send(req, OK_STR, strlen(OK_STR));
                    free(buf);
                    return ESP_OK;                    
                }
            }
            else {
                httpd_resp_send(req, INVALID_TOKEN_STR, strlen(INVALID_TOKEN_STR));
                free(buf);
                return ESP_OK;
            }
			free(buf);
		}
    }
    /* Send response with custom headers and body set as the
     * string passed in user context*/
    httpd_resp_send(req, EMPTY_STR, strlen(EMPTY_STR));
    return ESP_OK;
}


esp_err_t pass_cgi_post_handler(httpd_req_t *req)
{
    char*  buf;
    size_t buf_len;
    char param[32];
    char resp[32] = "";
    
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
			if (httpd_query_key_value(buf, "token", param, sizeof(param)) == ESP_OK) {
				ESP_LOGI(TAG, "Token: %s", param);
				if (strcmp(param, configToken) == 0) {
					if (httpd_query_key_value(buf, "oldpass", param, sizeof(param)) == ESP_OK) {
						if (param[0] == '\0') {
							strncpy(resp, "empty_old", sizeof(resp)-1);
						}
						else if (strcmp(param, config.password) != 0) {
							strncpy(resp, "wrong_old", sizeof(resp)-1);
						}
						else {
							if (httpd_query_key_value(buf, "newpass", param, sizeof(param)) == ESP_OK) {
								if (!is_password_valid(param)) {
									strncpy(resp, "invalid_new", sizeof(resp)-1);
								}
								else {
									snprintf(config.password, sizeof(config.password), "%s", param);
									config_save_settings_to_flash();
									strncpy(resp, "ok", sizeof(resp)-1);
								}
							}
							else {strncpy(resp, "invalid_new", sizeof(resp)-1);}
						}
					}
					else {strncpy(resp, "empty_old", sizeof(resp)-1);}
				}
				else {strncpy(resp, "invalid_token", sizeof(resp)-1);}
			}
			else {strncpy(resp, "invalid_token", sizeof(resp)-1);}
			free(buf);
		}
    }
    /* Send response with custom headers and body set as the
     * string passed in user context*/
    httpd_resp_send(req, resp, strlen(resp));

    return ESP_OK;
}


esp_err_t data_json_get_handler(httpd_req_t *req)
{
    char *data;
    data = constructDataJSON();
    if (data) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, data, strlen(data));
        free(data);
    }
    
    return ESP_OK;
}


esp_err_t settings_json_get_handler(httpd_req_t *req)
{
    char *data;
    
    if (!check_authentication(req)) {return ESP_OK;}
    
    data = constructSettingsJSON();
    if (data) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, data, strlen(data));
        free(data);
    }
    
    return ESP_OK;
}


esp_err_t file_get_handler(httpd_req_t *req)
{
	char data[512];
	
	if (!check_authentication(req)) {return ESP_OK;}
	
	ESP_LOGI(TAG, "Reading file");
	char *ext = strchr(req->user_ctx, '.')+1;
	if (ext) {
		if (strcmp(ext, "js") == 0) {
			httpd_resp_set_type(req, "application/javascript");
		}
		else if (strcmp(ext, "json") == 0) {
			httpd_resp_set_type(req, "application/json");
		}
		else if (strcmp(ext, "png") == 0) {
			httpd_resp_set_type(req, "image/png");
		}
	}
	FILE* f = fopen((const char*) req->user_ctx, "r");
	if (f) {
		uint32_t bytes_read;
		do {
			bytes_read = fread(data, 1, sizeof(data), f);
			httpd_resp_send_chunk(req, data, bytes_read);
		} while (bytes_read == sizeof(data));
		fclose(f);	
		httpd_resp_send_chunk(req, NULL, 0);
	}
	else {
		ESP_LOGE(TAG, "Failed to open file for reading");
		return ESP_FAIL;
	}
    
    return ESP_OK;
}


esp_err_t sysinfo_get_handler(httpd_req_t *req)
{
    char *data;
    cJSON *root;
    
    if (!check_authentication(req)) {return ESP_OK;}
	
	root = cJSON_CreateObject();
	cJSON_AddNumberToObject(root, "uptime", get_uptime());
	time_t now = time(NULL);
	cJSON_AddStringToObject(root, "time", asctime(localtime(&now)));
	cJSON_AddStringToObject(root, "sdk", esp_get_idf_version());
	esp_chip_info_t chip_info;
	esp_chip_info(&chip_info);
	cJSON_AddStringToObject(root, "chip", chip_info.model ? "ESP32" : "ESP8266");
	cJSON_AddNumberToObject(root, "cores", chip_info.cores);
	cJSON_AddNumberToObject(root, "revision", chip_info.revision);
	{
		uint8_t mac[6];
		char buff[20];
		esp_read_mac(mac, 0);	//Read WiFi STA MAC
		snprintf(buff, sizeof(buff)-1, "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx", mac[0],  mac[1], mac[2], mac[3], mac[4], mac[5]);
		cJSON_AddStringToObject(root, "sta_mac", buff);
		esp_read_mac(mac, 1);	//Read WiFi AP MAC
		snprintf(buff, sizeof(buff)-1, "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx", mac[0],  mac[1], mac[2], mac[3], mac[4], mac[5]);
		cJSON_AddStringToObject(root, "ap_mac", buff);
	}
	cJSON_AddNumberToObject(root, "free_heap", esp_get_free_heap_size());
	cJSON_AddNumberToObject(root, "min_free_heap", esp_get_minimum_free_heap_size());
	data = cJSON_Print(root);
	cJSON_Delete(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, data, strlen(data));
    free(data);
    
    return ESP_OK;
}


esp_err_t token_get_handler(httpd_req_t *req) {
	if (!check_authentication(req)) {return ESP_OK;}
	
	for (uint8_t i=0; i<(sizeof(configToken)-1); i++) configToken[i] = 'a'+(esp_random() % 26);
	configToken[sizeof(configToken)-1] = '\0';
    httpd_resp_send(req, configToken, strlen(configToken));
	
	return ESP_OK;
}

httpd_uri_t hello = {
    .uri       = "/hello",
    .method    = HTTP_GET,
    .handler   = hello_get_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx  = "Hello World!"
};

httpd_uri_t setmode_cgi_get = {
    .uri       = "/ui/setmode.cgi",
    .method    = HTTP_GET,
    .handler   = setmode_cgi_get_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx  = NULL
};

httpd_uri_t reset_cgi_get = {
    .uri       = "/ui/reset.cgi",
    .method    = HTTP_GET,
    .handler   = reset_cgi_get_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx  = NULL
};

httpd_uri_t wifiscan_cgi_get = {
    .uri       = "/ui/wifiscan.cgi",
    .method    = HTTP_GET,
    .handler   = wifiscan_cgi_get_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx  = NULL
};

httpd_uri_t connstatus_cgi_get = {
    .uri       = "/ui/wifi/connstatus.cgi",
    .method    = HTTP_GET,
    .handler   = connstatus_cgi_get_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx  = NULL
};

httpd_uri_t config_cgi_post = {
    .uri       = "/ui/config.cgi",
    .method    = HTTP_POST,
    .handler   = config_cgi_post_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx  = NULL
};

httpd_uri_t pass_cgi_post = {
    .uri       = "/ui/pass.cgi",
    .method    = HTTP_POST,
    .handler   = pass_cgi_post_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx  = NULL
};

httpd_uri_t connect_cgi_post = {
    .uri       = "/ui/connect.cgi",
    .method    = HTTP_POST,
    .handler   = connect_cgi_post_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx  = NULL
};

httpd_uri_t data_json = {
    .uri       = "/data.json",
    .method    = HTTP_GET,
    .handler   = data_json_get_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
     .user_ctx = NULL
};

httpd_uri_t settings_json = {
    .uri       = "/ui/settings.json",
    .method    = HTTP_GET,
    .handler   = settings_json_get_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
     .user_ctx = NULL
};

httpd_uri_t sysinfo_json = {
    .uri        = "/sysinfo.json",
    .method     = HTTP_GET,
    .handler    = sysinfo_get_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
     .user_ctx  = NULL
};

httpd_uri_t wifiinfo_json = {
    .uri        = "/wifiinfo.json",
    .method     = HTTP_GET,
    .handler    = wifiinfo_get_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
     .user_ctx  = NULL
};

httpd_uri_t token_get = {
    .uri        = "/ui/token",
    .method     = HTTP_GET,
    .handler    = token_get_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
     .user_ctx  = NULL
};

httpd_uri_t config_html_get = {
    .uri        = "/ui/config.html",
    .method     = HTTP_GET,
    .handler    = file_get_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
     .user_ctx  = "/spiffs/ui/config.html"
};

httpd_uri_t form_css_get = {
    .uri        = "/ui/form.css",
    .method     = HTTP_GET,
    .handler    = file_get_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
     .user_ctx  = "/spiffs/ui/form.css"
};

httpd_uri_t index_html_get = {
    .uri        = "/ui/index.html",
    .method     = HTTP_GET,
    .handler    = file_get_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
     .user_ctx  = "/spiffs/ui/index.html"
};

httpd_uri_t common_js_get = {
    .uri        = "/ui/common.js",
    .method     = HTTP_GET,
    .handler    = file_get_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
     .user_ctx  = "/spiffs/ui/common.js"
};

httpd_uri_t menu_css_get = {
    .uri        = "/ui/menu.css",
    .method     = HTTP_GET,
    .handler    = file_get_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
     .user_ctx  = "/spiffs/ui/menu.css"
};

httpd_uri_t menu_js_get = {
    .uri        = "/ui/menu.js",
    .method     = HTTP_GET,
    .handler    = file_get_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
     .user_ctx  = "/spiffs/ui/menu.js"
};

httpd_uri_t pass_html_get = {
    .uri        = "/ui/pass.html",
    .method     = HTTP_GET,
    .handler    = file_get_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
     .user_ctx  = "/spiffs/ui/pass.html"
};

httpd_uri_t redirect_html_get = {
    .uri        = "/ui/redirect.html",
    .method     = HTTP_GET,
    .handler    = file_get_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
     .user_ctx  = "/spiffs/ui/redirect.html"
};

httpd_uri_t status_html_get = {
    .uri        = "/ui/status.html",
    .method     = HTTP_GET,
    .handler    = file_get_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
     .user_ctx  = "/spiffs/ui/status.html"
};

httpd_uri_t style_css_get = {
    .uri        = "/ui/style.css",
    .method     = HTTP_GET,
    .handler    = file_get_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
     .user_ctx  = "/spiffs/ui/style.css"
};

httpd_uri_t wifi_get = {
    .uri        = "/ui/wifi",
    .method     = HTTP_GET,
    .handler    = file_get_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
     .user_ctx  = "/spiffs/ui/wifi/wifi.html"
};

httpd_uri_t connecting_html_get = {
    .uri        = "/ui/wifi/connecting.html",
    .method     = HTTP_GET,
    .handler    = file_get_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
     .user_ctx  = "/spiffs/ui/wifi/connecting.html"
};

httpd_uri_t icons_png_get = {
    .uri        = "/ui/wifi/icons.png",
    .method     = HTTP_GET,
    .handler    = file_get_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
     .user_ctx  = "/spiffs/ui/wifi/icons.png"
};

httpd_uri_t wifi_css_get = {
    .uri        = "/ui/wifi/wifi.css",
    .method     = HTTP_GET,
    .handler    = file_get_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
     .user_ctx  = "/spiffs/ui/wifi/wifi.css"
};

httpd_uri_t wifi_140medley_get = {
    .uri        = "/ui/wifi/140medley.js",
    .method     = HTTP_GET,
    .handler    = file_get_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
     .user_ctx  = "/spiffs/ui/wifi/140medley.js"
};

httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;
    config.max_uri_handlers = 30;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &hello);
        httpd_register_uri_handler(server, &data_json);
        httpd_register_uri_handler(server, &settings_json);
        httpd_register_uri_handler(server, &setmode_cgi_get);
        httpd_register_uri_handler(server, &reset_cgi_get);
        httpd_register_uri_handler(server, &wifiscan_cgi_get);
        httpd_register_uri_handler(server, &connstatus_cgi_get);
        httpd_register_uri_handler(server, &config_cgi_post);
        httpd_register_uri_handler(server, &connect_cgi_post);
        httpd_register_uri_handler(server, &pass_cgi_post);
        httpd_register_uri_handler(server, &sysinfo_json);
        httpd_register_uri_handler(server, &wifiinfo_json);
        httpd_register_uri_handler(server, &token_get);
        httpd_register_uri_handler(server, &config_html_get);
        httpd_register_uri_handler(server, &form_css_get);
        httpd_register_uri_handler(server, &index_html_get);
        httpd_register_uri_handler(server, &common_js_get);
        httpd_register_uri_handler(server, &menu_css_get);
        httpd_register_uri_handler(server, &menu_js_get);
        httpd_register_uri_handler(server, &pass_html_get);
        httpd_register_uri_handler(server, &redirect_html_get);
        httpd_register_uri_handler(server, &status_html_get);
        httpd_register_uri_handler(server, &style_css_get);
        httpd_register_uri_handler(server, &wifi_get);
        httpd_register_uri_handler(server, &connecting_html_get);
        httpd_register_uri_handler(server, &icons_png_get);
        httpd_register_uri_handler(server, &wifi_css_get);
        httpd_register_uri_handler(server, &wifi_140medley_get);

        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

void stop_webserver(httpd_handle_t server)
{
    // Stop the httpd server
    httpd_stop(server);
}

static httpd_handle_t server = NULL;

static void disconnect_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server) {
        ESP_LOGI(TAG, "Stopping webserver");
        stop_webserver(*server);
        *server = NULL;
    }
}

static void connect_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server == NULL) {
        ESP_LOGI(TAG, "Starting webserver");
        *server = start_webserver();
    }
}


void http_server_init (void) {	
	ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, &server));
    server = start_webserver();
}


static uint8_t is_valid_token(char* str) {
    if (strcmp(str, configToken) != 0) return 0;
    return 1;
}
