#ifndef _COMMON_H_
#define _COMMON_H_

char* constructDataJSON(void);
char* constructSettingsJSON(void);
uint32_t get_uptime(void);
void increment_uptime(void);
void spi_filesystem_init(void);
void set_reset_timer (void);

#endif //_COMMON_H_
