#ifndef _I2C_MASTER_H_
#define _I2C_MASTER_H_

#include "driver/i2c.h"

#define I2C_MASTER_SCL_IO           2                /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO           14               /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM              I2C_NUM_0        /*!< I2C port number for master dev */

esp_err_t i2c_master_init(void);
void i2c_task_example(void *arg);

#endif //_I2C_MASTER_H_
