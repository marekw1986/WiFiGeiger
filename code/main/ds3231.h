#ifndef DS3231_H_INCLUDED
#define DS3231_H_INCLUDED

#include <time.h>
#include <stdint.h>

#define DS3231_ADDR 0x68

#define DS3231_STAT_OSCILLATOR 0x80
#define DS3231_STAT_32KHZ      0x08
#define DS3231_STAT_BUSY       0x04
#define DS3231_STAT_ALARM_2    0x02
#define DS3231_STAT_ALARM_1    0x01

#define DS3231_CTRL_OSCILLATOR    0x80
#define DS3231_CTRL_SQUAREWAVE_BB 0x40
#define DS3231_CTRL_TEMPCONV      0x20
#define DS3231_CTRL_SQWAVE_4096HZ 0x10
#define DS3231_CTRL_SQWAVE_1024HZ 0x08
#define DS3231_CTRL_SQWAVE_8192HZ 0x18
#define DS3231_CTRL_SQWAVE_1HZ    0x00
#define DS3231_CTRL_ALARM_INTS    0x04
#define DS3231_CTRL_ALARM2_INT    0x02
#define DS3231_CTRL_ALARM1_INT    0x01

#define DS3231_ALARM_NONE 0
#define DS3231_ALARM_1    1
#define DS3231_ALARM_2    2
#define DS3231_ALARM_BOTH 3

#define DS3231_ALARM1_EVERY_SECOND         0
#define DS3231_ALARM1_MATCH_SEC            1
#define DS3231_ALARM1_MATCH_SECMIN         2
#define DS3231_ALARM1_MATCH_SECMINHOUR     3
#define DS3231_ALARM1_MATCH_SECMINHOURDAY  4
#define DS3231_ALARM1_MATCH_SECMINHOURDATE 5

#define DS3231_ALARM2_EVERY_MIN         0
#define DS3231_ALARM2_MATCH_MIN         1
#define DS3231_ALARM2_MATCH_MINHOUR     2
#define DS3231_ALARM2_MATCH_MINHOURDAY  3
#define DS3231_ALARM2_MATCH_MINHOURDATE 4

#define DS3231_ALARM_WDAY   0x40
#define DS3231_ALARM_NOTSET 0x80

#define DS3231_ADDR_TIME    0x00
#define DS3231_ADDR_ALARM1  0x07
#define DS3231_ADDR_ALARM2  0x0b
#define DS3231_ADDR_CONTROL 0x0e
#define DS3231_ADDR_STATUS  0x0f
#define DS3231_ADDR_AGING   0x10
#define DS3231_ADDR_TEMP    0x11

#define DS3231_SET     0
#define DS3231_CLEAR   1
#define DS3231_REPLACE 2

#define DS3231_12HOUR_FLAG 0x40
#define DS3231_12HOUR_MASK 0x1f
#define DS3231_PM_FLAG     0x20
#define DS3231_MONTH_MASK  0x1f

bool ICACHE_FLASH_ATTR ds3231_setTime(struct tm *time);
bool ICACHE_FLASH_ATTR ds3231_setAlarm(uint8_t alarms, struct tm *time1, uint8_t option1, struct tm *time2, uint8_t option2);
bool ICACHE_FLASH_ATTR ds3231_getOscillatorStopFlag(bool *flag);
bool ICACHE_FLASH_ATTR ds3231_clearOscillatorStopFlag();
bool ICACHE_FLASH_ATTR ds3231_getAlarmFlags(uint8_t *alarms);
bool ICACHE_FLASH_ATTR ds3231_clearAlarmFlags(uint8_t alarm);
bool ICACHE_FLASH_ATTR ds3231_enableAlarmInts(uint8_t alarms);
bool ICACHE_FLASH_ATTR ds3231_disableAlarmInts(uint8_t alarms);
bool ICACHE_FLASH_ATTR ds3231_enable32khz();
bool ICACHE_FLASH_ATTR ds3231_disable32khz();
bool ICACHE_FLASH_ATTR ds3231_enableSquarewave();
bool ICACHE_FLASH_ATTR ds3231_disableSquarewave();
bool ICACHE_FLASH_ATTR ds3231_setSquarewaveFreq(uint8_t freq);
bool ICACHE_FLASH_ATTR ds3231_getTempInteger(int8_t *temp);
bool ICACHE_FLASH_ATTR ds3231_getTempFloat(float *temp);
bool ICACHE_FLASH_ATTR ds3231_getTime(struct tm *time);

#endif // DS3231_H_INCLUDED
