#ifndef _HTTP_SERVER_H_
#define _HTTP_SERVER_H_

#include <esp_http_server.h>

void http_server_init (void);
void http_server_deinit(void);
uint8_t check_authentication (httpd_req_t *req);

#endif //_HTTP_SERVER_H_
