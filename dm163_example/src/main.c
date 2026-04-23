#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/led.h>
#include <zephyr/kernel.h>
#include "dm163.h"
#include "accel.h"
#include "spirit_level.h"

#define DISPLAY_STACK 500
#define DISPLAY_PRIORITY 4

// defined in spirit_level.c
extern struct k_mutex image_mutex;
extern image_t image; 

#define DM163_NODE DT_NODELABEL(dm163)
static const struct device *dm163_dev = DEVICE_DT_GET(DM163_NODE);

#define RGB_MATRIX_NODE DT_NODELABEL(rgb_matrix)

BUILD_ASSERT(DT_PROP_LEN(RGB_MATRIX_NODE, rows_gpios) == 8);

static const struct gpio_dt_spec rows[] = {
    GPIO_DT_SPEC_GET_BY_IDX(RGB_MATRIX_NODE, rows_gpios, 0),
    GPIO_DT_SPEC_GET_BY_IDX(RGB_MATRIX_NODE, rows_gpios, 1),
    GPIO_DT_SPEC_GET_BY_IDX(RGB_MATRIX_NODE, rows_gpios, 2),
    GPIO_DT_SPEC_GET_BY_IDX(RGB_MATRIX_NODE, rows_gpios, 3),
    GPIO_DT_SPEC_GET_BY_IDX(RGB_MATRIX_NODE, rows_gpios, 4),
    GPIO_DT_SPEC_GET_BY_IDX(RGB_MATRIX_NODE, rows_gpios, 5),
    GPIO_DT_SPEC_GET_BY_IDX(RGB_MATRIX_NODE, rows_gpios, 6),
    GPIO_DT_SPEC_GET_BY_IDX(RGB_MATRIX_NODE, rows_gpios, 7),
};

struct k_sem image_sem;
struct k_timer fps_timer;

void signal_line_display(struct k_timer *timer_id){
    k_sem_give(&image_sem);
}

K_THREAD_STACK_DEFINE(display_stack, DISPLAY_STACK);

struct k_thread display_thread;

void display_func(void *, void *, void *) {
    uint32_t next_line = 0;
    uint32_t last_line = 7;

    static image_t local_image =  {
        .channels = {
            // Ligne 0
            {0,0,0,   0,0,0,  0,0,0,   0,0,0,   0,0,0,   0,0,0,   0,0,0,  0,0,0},

            // Ligne 1
            {0,0,0,   0,0,0,  0,0,0,   0,0,0,   0,0,0,   0,0,0,   0,0,0,  0,0,0},

            // Ligne 2
            {0,0,0,   0,0,0,  0,0,0,   0,0,0,   0,0,0,   0,0,0,   0,0,0,  0,0,0},

            // Ligne 3
            {0,0,0,   0,0,0,  0,0,0,   0,0,0,   0,0,0,   0,0,0,   0,0,0,  0,0,0},

            // Ligne 4
            {0,0,0,   0,0,0,  0,0,0,   0,0,0,   0,0,0,   0,0,0,   0,0,0,  0,0,0},

            // Ligne 5
            {0,0,0,   0,0,0,  0,0,0,   0,0,0,   0,0,0,   0,0,0,   0,0,0,  0,0,0},

            // Ligne 6
            {0,0,0,   0,0,0,  0,0,0,   0,0,0,   0,0,0,   0,0,0,   0,0,0,  0,0,0},

            // Ligne 7
            {0,0,0,   0,0,0,  0,0,0,   0,0,0,   0,0,0,   0,0,0,   0,0,0,  0,0,0},
        }
    };  

    while (1) {

        if(next_line == 0) {
            if(k_mutex_lock(&image_mutex, K_NO_WAIT) == 0) {
                local_image = image; 
                k_mutex_unlock(&image_mutex); 
            }
        }

        k_sem_take(&image_sem, K_FOREVER);

        dm163_turn_off_row(dm163_dev, &rows[last_line]); 

        led_write_channels(dm163_dev, 23, 24, local_image.channels[next_line]);

        gpio_pin_set_dt(&rows[next_line], 1);

        last_line = next_line;
        next_line = (next_line + 1) % 8;
    }
}

int main() {
    k_sem_init(&image_sem, 0, 1);

    k_timer_init(&fps_timer, signal_line_display, NULL); 

    if (!device_is_ready(dm163_dev)) {
        return -ENODEV;
    }

    for (int row = 0; row < 8; row++)
        gpio_pin_configure_dt(&rows[row], GPIO_OUTPUT_INACTIVE);

    for (int i = 0; i < 8; i++)
        led_set_brightness(dm163_dev, i, 10);

    k_timer_start(&fps_timer, K_MSEC(2), K_MSEC(2));

    k_thread_create(&display_thread, display_stack,
                K_THREAD_STACK_SIZEOF(display_stack),
                display_func,
                NULL, NULL, NULL,
                DISPLAY_PRIORITY, 0, K_NO_WAIT); 

    init_spirit_level(); 

    if(init_accelerometer(true) != 0)
        return 1;  

    return 0; 
}
