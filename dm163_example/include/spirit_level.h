#ifndef SPIRIT_LEVEL_H 
#define SPIRIT_LEVEL_H 
#include <zephyr/kernel.h>
#include <stdio.h>
#include <stdint.h>
#include <zephyr/device.h>
#include <zephyr/drivers/led.h>

#define NUM_LEDS 8
#define NUM_CHANNELS (NUM_LEDS * 3)

#define SPIRIT_LEVEL_STACK 500
#define SPIRIT_LEVEL_PRIORITY 6

void init_spirit_level(void); 
void init_tilt_kmsg(void); 

typedef struct {
    uint8_t channels[8][NUM_CHANNELS]; 
} image_t;

typedef struct 
{
    uint8_t r; 
    uint8_t g; 
    uint8_t b; 
} color_t;



#endif


