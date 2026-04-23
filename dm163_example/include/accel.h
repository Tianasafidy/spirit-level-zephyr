#ifndef ACCEL_H
#define ACCEL_H

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util.h>

#define COMP_FILTER_STACK_SIZE 500
#define COMP_FILTER_PRIORITY 7

#define ACCEL_PATH DT_ALIAS(accel0)
#define LSM6DSL_ADDR 0x6a

#define WHO_AM_I_REG  0x0F
#define CTRL6_C_REG 0x15 // XL_HM_MODE = 0b1
#define CTRL1_XL_REG 0x10 // ODR_XL = 0b0001 12.5 Hz FS_XL [1:0] = 0b00
#define STATUS_REG_REG 0x1e // GDA: gyro date available XLDA : accel data avalaible  
#define INT1_CTRL_REG 0x0d // INT1_DRDY_XL and INT1_DRDY_G
#define OUTX_L_XL_REG 0x28 
#define OUTX_H_XL_REG 0x29
#define OUTY_L_XL_REG 0x2a
#define OUTY_H_XL_REG 0x2b
#define OUTZ_L_XL_REG 0x2c 
#define OUTZ_H_XL_REG 0x2d

#define OUTX_L_G_REG 0x22 
#define OUTX_H_G_REG 0x23
#define OUTY_L_G_REG 0x24
#define OUTY_H_G_REG 0x25
#define OUTZ_L_G_REG 0x26 
#define OUTZ_H_G_REG 0x27

#define CTRL3_C_REG 0x12 // IF_INC = 1 to enable multile byte read SW_RESET = 1 at start up
#define CTRL7_G_REG 0x16 // GH_HM_MODE = 1 to disable high perf mode
#define CTRL2_G_REG 0x11 // ODR_G = 0b0001 12.5 Hz


int init_accelerometer(bool use_gyro); 
int read_accel_3_axes(int16_t * x_accel, int16_t * y_accel, int16_t * z_accel); 
int read_gyro_3_axes(int16_t * x_dps, int16_t * y_dps, int16_t * z_dps); 

#endif // ACCEL_H