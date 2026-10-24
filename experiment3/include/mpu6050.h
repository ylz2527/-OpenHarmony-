


#ifndef __CAR_MPU6050_H__
#define __CAR_MPU6050_H__

#include "config.h"

hi_void Mpu6050_Init(hi_void);
hi_void Mpu6050_Measure_By(hi_u8 *datas, hi_u8 len);
hi_void Mpu6050_Measure_Sh(hi_s16 *datas);

#endif