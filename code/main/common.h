#ifndef _COMMON_H_
#define _COMMON_H_

char* constructJSON(void);
uint32_t get_uptime(void);
void increment_uptime(void);
void spi_filesystem_init(void);

#endif //_COMMON_H_
