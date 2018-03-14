#ifndef SRC_DRIVERS_I2C_BITBANG_H_
#define SRC_DRIVERS_I2C_BITBANG_H_

#include <stdbool.h>

#define SDA_IO		5
#define SDA_IO_MUX	PERIPHS_IO_MUX_GPIO5_U
#define SDA_IO_FUNC	FUNC_GPIO5

#define SCL_IO		4
#define SCL_IO_MUX	PERIPHS_IO_MUX_GPIO4_U
#define SCL_IO_FUNC	FUNC_GPIO4


void i2c_gpio_init(void);

bool i2c_write_byte(bool send_start,
                    bool send_stop,
                    unsigned char byte);

void i2c_stop_cond(void);


#endif /* SRC_DRIVERS_I2C_BITBANG_H_ */
