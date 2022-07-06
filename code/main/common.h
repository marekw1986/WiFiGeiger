#ifndef _COMMON_H_
#define _COMMON_H_

#include "esp_timer.h"

#define millis() (uint32_t)(esp_timer_get_time()/1000)

char* constructDataJSON(void);
char* constructSettingsJSON(void);
uint32_t get_uptime(void);
void increment_uptime(void);
void spi_filesystem_init(void);
void set_reset_timer (void);
uint8_t is_password_valid(const char* pass);
uint8_t is_valid_ip_address(const char *ip);
void sntp_sync_time_func(struct timeval *tv);

#endif //_COMMON_H_
