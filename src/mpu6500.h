#ifndef SRC_MPU6500_H_
#define SRC_MPU6500_H_

#include <c_types.h>
#include "typedefs.h"

int mpu6500_init(void);
sint16 accelReadX(void);


#endif /* SRC_MPU6500_H_ */
