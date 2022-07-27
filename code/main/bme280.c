#include <math.h>
#include <string.h>
#include <time.h>
#include "i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "bme280.h"

static const char *TAG = "BME280";
static bme_data_t current_data;

uint16_t bme_T1;
int16_t bme_T2;
int16_t bme_T3;
uint16_t bme_P1;
int16_t bme_P2;
int16_t bme_P3;
int16_t bme_P4;
int16_t bme_P5;
int16_t bme_P6;
int16_t bme_P7;
int16_t bme_P8;
int16_t bme_P9;
uint8_t bme_H1;
int16_t bme_H2;
uint8_t bme_H3;
int16_t bme_H4;
int16_t bme_H5;
int8_t bme_H6;
int32_t bme_adc_T;
int32_t bme_adc_P;
int32_t bme_adc_H;
int32_t bme_t_fine;

void bme_init(void) {
	bme_T1 = 0;
	bme_T2 = 0;
	bme_T3 = 0;
	bme_P1 = 0;
	bme_P2 = 0;
	bme_P3 = 0;
	bme_P4 = 0;
	bme_P5 = 0;
	bme_P6 = 0;
	bme_P7 = 0;
	bme_P8 = 0;
	bme_P9 = 0;
	bme_H1 = 0;
	bme_H2 = 0;
	bme_H3 = 0;
	bme_H4 = 0;
	bme_H5 = 0;
	bme_H6 = 0;
	memset(&current_data, 0x00, sizeof(current_data));
}

uint8_t bme_read_data(uint8_t reg)
{
	uint8_t tmp = 0;
    if (i2c_master_read_buf(I2C_MASTER_NUM, BME280_ADDR, reg, &tmp, 1) == ESP_OK) {
		ESP_LOGI(TAG, "Prawidłowy odczyt z BME280");
	}
	else {
		ESP_LOGI(TAG, "Błąd odczytu z BME280");
	}
	return tmp;
}

void bme_write_data(uint8_t reg, uint8_t value) {
    if (i2c_master_write_buf(I2C_MASTER_NUM, BME280_ADDR, reg, &value, 1) == ESP_OK) {
		ESP_LOGI(TAG, "Prawidłowy zapis do BME280");
	}
	else {
		ESP_LOGI(TAG, "Błąd zapisu do BME280");
	}
}

uint8_t bme_setup(void) {
	if(bme_read_data(0xD0) != 0x60) return 0;

	bme_write_data(0xE0, 0xB6);

	vTaskDelay(300 / portTICK_RATE_MS);
	while((bme_read_data(0xF3) & 0x01) != 0) vTaskDelay(100 / portTICK_RATE_MS);

	bme_read_compensation_data();
	bme_set_sampling();

	return 1;
}

void bme_read_compensation_data(void)
{
	bme_T1 = (bme_read_data(0x89) << 8) + bme_read_data(0x88);
	bme_T2 = (bme_read_data(0x8B) << 8) + bme_read_data(0x8A);
	bme_T3 = (bme_read_data(0x8D) << 8) + bme_read_data(0x8C);

	bme_P1 = (bme_read_data(0x8F) << 8) + bme_read_data(0x8E);
    bme_P2 = (bme_read_data(0x91) << 8) + bme_read_data(0x90);
    bme_P3 = (bme_read_data(0x93) << 8) + bme_read_data(0x92);
    bme_P4 = (bme_read_data(0x95) << 8) + bme_read_data(0x94);
    bme_P5 = (bme_read_data(0x97) << 8) + bme_read_data(0x96);
    bme_P6 = (bme_read_data(0x99) << 8) + bme_read_data(0x98);
    bme_P7 = (bme_read_data(0x9B) << 8) + bme_read_data(0x9A);
    bme_P8 = (bme_read_data(0x9D) << 8) + bme_read_data(0x9C);
    bme_P9 = (bme_read_data(0x9F) << 8) + bme_read_data(0x9E);

    bme_H1 = bme_read_data(0xA1);
    bme_H2 = (bme_read_data(0xE2) << 8) + bme_read_data(0xE1);
    bme_H3 = bme_read_data(0xE3);
    bme_H4 = (bme_read_data(0xE4) << 4) | (bme_read_data(0xE5) & 0x0F);
    bme_H5 = (bme_read_data(0xE6) << 4) | (bme_read_data(0xE5) >> 4);
    bme_H6 = bme_read_data(0xE7);
}

void bme_set_sampling(void) {
	bme_write_data(0xF2, 0x05);
    bme_write_data(0xF5, 0x00);
    bme_write_data(0xF4, 0xB7);
}

void bme_read_temp_press_and_hum(void) {
	bme_adc_T = (bme_read_data(0xFA) << 16) + (bme_read_data(0xFB) << 8) + bme_read_data(0xFC);
	bme_adc_P = (bme_read_data(0xF7) << 16) + (bme_read_data(0xF8) << 8) + bme_read_data(0xF9);
	bme_adc_H = (bme_read_data(0xFD) << 8) + bme_read_data(0xFE);
}

double bme_get_temperature(void) {
	int32_t var1, var2;

	if (bme_adc_T == 0x800000) return 0.0;
	bme_adc_T >>= 4;

	var1 = ((((bme_adc_T >> 3) - ((int32_t)bme_T1 << 1))) * ((int32_t)bme_T2)) >> 11;
	var2 = (((((bme_adc_T >> 4) - ((int32_t)bme_T1)) * ((bme_adc_T>>4) - ((int32_t)bme_T1))) >> 12) * ((int32_t)bme_T3)) >> 14;
	bme_t_fine = var1 + var2;

	double T = (bme_t_fine * 5 + 128) >> 8;
	return T/100;
}

double bme_get_pressure(void) {
	int64_t var1, var2, p;

	if(bme_adc_P == 0x800000) return 0.0;
	bme_adc_P >>= 4;

	var1 = ((int64_t)bme_t_fine) - 128000;
	var2 = var1 * var1 * (int64_t)bme_P6;
	var2 = var2 + ((var1*(int64_t)bme_P5) << 17);
	var2 = var2 + (((int64_t)bme_P4)<<35);
	var1 = ((var1 * var1 * (int64_t)bme_P3)>>8) + ((var1 * (int64_t)bme_P2)<<12);
	var1 = (((((int64_t)1)<<47)+var1))*((int64_t)bme_P1)>>33;

	if(var1 == 0) return 0;
	p = 1048576 - bme_adc_P;
	p = (((p<<31) - var2)*3125) / var1;
	var1 = (((int64_t)bme_P9) * (p>>13) * (p>>13)) >> 25;
	var2 = (((int64_t)bme_P8) * p) >> 19;

	p = ((p + var1 + var2) >> 8) + (((int64_t)bme_P7)<<4);
	return ((double)p / 256.0) / 100.0;
}

double bme_get_humidity(void) {
	if(bme_adc_H == 0x8000) return 0.0;

	int32_t v_x1_u32r;

	v_x1_u32r = (bme_t_fine - ((int32_t)76800));

	v_x1_u32r = (((((bme_adc_H << 14) - (((int32_t) bme_H4) << 20)
			- (((int32_t) bme_H5) * v_x1_u32r)) + ((int32_t) 16384)) >> 15)
			* (((((((v_x1_u32r * ((int32_t) bme_H6)) >> 10)
					* (((v_x1_u32r * ((int32_t) bme_H3)) >> 11)
							+ ((int32_t) 32768))) >> 10) + ((int32_t) 2097152))
					* ((int32_t) bme_H2) + 8192) >> 14));

	v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) * ((int32_t)bme_H1)) >> 4));

	v_x1_u32r = (v_x1_u32r < 0) ? 0 : v_x1_u32r;
	v_x1_u32r = (v_x1_u32r > 419430400) ? 419430400 : v_x1_u32r;

	float h = (v_x1_u32r >> 12);
	return (h / 1024.0) / 100.0;
}

double bme_get_altitude(double p, double p0) {
	return 44330 * (1 - pow((p/p0), (1/5.255)));
}

void bme_update_data(void) {	
	bme_read_temp_press_and_hum();
	current_data.temperature = bme_get_temperature();
	current_data.pressure = bme_get_pressure(); //value in hPa
	current_data.humidity = bme_get_humidity() * 100; //value in percents
	current_data.timestamp = time(NULL);
}

bme_data_t bme_get_data(void) {
	return current_data;
}
