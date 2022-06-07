#include <esp_http_server.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "http_server.h"
#include "common.h"
#include "cJSON.h"
#include "geiger.h"

extern const char *TAG;

char configToken[10];

/* An HTTP GET handler */
esp_err_t hello_get_handler(httpd_req_t *req)
{
    char*  buf;
    size_t buf_len;

    /* Get header value string length and allocate memory for length + 1,
     * extra byte for null termination */
    buf_len = httpd_req_get_hdr_value_len(req, "Host") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        /* Copy null terminated value string into buffer */
        if (httpd_req_get_hdr_value_str(req, "Host", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Host: %s", buf);
        }
        free(buf);
    }

    buf_len = httpd_req_get_hdr_value_len(req, "Test-Header-2") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_hdr_value_str(req, "Test-Header-2", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Test-Header-2: %s", buf);
        }
        free(buf);
    }

    buf_len = httpd_req_get_hdr_value_len(req, "Test-Header-1") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_hdr_value_str(req, "Test-Header-1", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Test-Header-1: %s", buf);
        }
        free(buf);
    }

    /* Read URL query string length and allocate memory for length + 1,
     * extra byte for null termination */
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
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

    /* Set some custom headers */
    httpd_resp_set_hdr(req, "Custom-Header-1", "Custom-Value-1");
    httpd_resp_set_hdr(req, "Custom-Header-2", "Custom-Value-2");

    /* Send response with custom headers and body set as the
     * string passed in user context*/
    const char* resp_str = (const char*) req->user_ctx;
    httpd_resp_send(req, resp_str, strlen(resp_str));

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
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found URL query => %s", buf);
            char param[32];
            /* Get value of expected key from query string */
            if (httpd_query_key_value(buf, "mode", param, sizeof(param)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => mode=%s", param);
                esp_wifi_set_mode(atoi(param));
                //esp_restart();
            }
        }
        free(buf);
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
	
	ESP_LOGI(TAG, "Reading file");
	char *ext = strchr(req->user_ctx, '.')+1;
	if (ext) {
		if (strcmp(ext, "js") == 0) {
			httpd_resp_set_type(req, "application/javascript");
		}
		else if (strcmp(ext, "json") == 0) {
			httpd_resp_set_type(req, "application/json");
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

esp_err_t token_get_handler(httpd_req_t *req) {
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
    .user_ctx  = "/spiffs/ui/setmode.cgi"
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
    config.max_uri_handlers = 20;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &hello);
        httpd_register_uri_handler(server, &data_json);
        httpd_register_uri_handler(server, &settings_json);
        httpd_register_uri_handler(server, &setmode_cgi_get);
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
