#include "spirit_level.h"

image_t image = {
    .channels = {
        // Ligne 0
        {0,0,0,   0,0,0,  0,0,0,   0,0,0,   0,0,0,   0,0,0,   0,0,0,  0,0,0},

        // Ligne 1
        {0,0,0,   0,0,0,  0,0,0,   0,0,0,   0,0,0,   0,0,0,   0,0,0,  0,0,0},

        // Ligne 2
        {0,0,0,   0,0,0,  0,0,0,   0,0,0,   0,0,0,   0,0,0,   0,0,0,  0,0,0},

        // Ligne 3
        {0,0,0,   0,0,0,  0,0,0,   255,0,0,   255,0,0,   0,0,0,   0,0,0,  0,0,0},

        // Ligne 4
        {0,0,0,   0,0,0,  0,0,0,   255,0,0,   255,0,0,   0,0,0,   0,0,0,  0,0,0},

        // Ligne 5
        {0,0,0,   0,0,0,  0,0,0,   0,0,0,   0,0,0,   0,0,0,   0,0,0,  0,0,0},

        // Ligne 6
        {0,0,0,   0,0,0,  0,0,0,   0,0,0,   0,0,0,   0,0,0,   0,0,0,  0,0,0},

        // Ligne 7
        {0,0,0,   0,0,0,  0,0,0,   0,0,0,   0,0,0,   0,0,0,   0,0,0,  0,0,0},
    }
};

char x_tilt_msg_buff[5 * sizeof(int16_t)];
char y_tilt_msg_buff[5 * sizeof(int16_t)];

const color_t green = {0, 255 , 0 }; 
const color_t clear = {0, 0 , 0 }; 
const color_t red = {255, 0 , 0 }; 

struct k_msgq x_tilt_msgq;
struct k_msgq y_tilt_msgq;

struct k_mutex image_mutex;

K_THREAD_STACK_DEFINE(spirit_level_stack, SPIRIT_LEVEL_STACK);

struct k_thread spirit_level_thread;

void set_pixel_color(image_t * image, color_t color, uint8_t x, uint8_t y){
    image->channels[7 - y][x*3] = color.r;     
    image->channels[7 - y][x*3 + 1] = color.g;     
    image->channels[7 - y][x*3 + 2] = color.b;     
}

void set_2_by_2_square(image_t * image, color_t color, uint8_t x, uint8_t y){
    for(uint8_t i = 0; i < 2; i++){
        set_pixel_color(image, color, x + i, y );     
    } 

    for(uint8_t i = 0; i < 2; i++){
        set_pixel_color(image, color, x + i, y - 1 );     
    } 
}

uint8_t map_y_tilt_to_pos(int16_t x) {
    // Clamp pour rester dans [-40, 40]
    if (x < -40) x = -40;
    if (x >  40) x =  40;

    float ratio = (float)(x + 40) / 80.0f;  // [0,1]
    uint8_t y = (uint8_t)(ratio * 7.0f + 0.5f); 

    return y; // [1,7]
}

uint8_t map_x_tilt_to_pos(int16_t x) {
    // Clamp pour rester dans [-40, 40]
    if (x < -40) x = -40;
    if (x >  40) x =  40;

    float ratio = (float)(x + 40) / 80.0f;  // [0,1]
    uint8_t y = (uint8_t)(ratio * 7.0f); // arrondi par defaut  

    return y; // [1,7]
}


void spirit_level_func(void *, void *, void *)
{
    int16_t x_tilt;
    int16_t y_tilt;

    // pixel position of the up-left corner of the square
    static uint8_t x_pos = 3; 
    static uint8_t y_pos = 4; 

    while (1)
    {
        if (k_msgq_get(&x_tilt_msgq, &x_tilt, K_NO_WAIT) == 0 &&
            k_msgq_get(&y_tilt_msgq, &y_tilt, K_NO_WAIT) == 0)
        {

            uint8_t y_next = map_y_tilt_to_pos(y_tilt); 
            if (y_next < 1) y_next = 1; 
            uint8_t x_next = map_x_tilt_to_pos(x_tilt); 
            if (x_next >= 7) x_next = 0;

            printf("X next: %d ; Y next %d ; X tilt %d  ; Y tilt %d\n", x_next , y_next, x_tilt, y_tilt); 
            if (k_mutex_lock(&image_mutex, K_NO_WAIT) == 0)
            {
                
                set_2_by_2_square(&image,clear , x_pos, y_pos); 
                if(x_next == 3 && y_next == 4)
                    set_2_by_2_square(&image, green, x_next, y_next); 
                else 
                    set_2_by_2_square(&image, red, x_next, y_next); 

                k_mutex_unlock(&image_mutex);

                y_pos = y_next; 
                x_pos = x_next; 
            }
        }

        k_sleep(K_MSEC(50));
    }
}





void init_spirit_level(void){

    init_tilt_kmsg(); 

    k_mutex_init(&image_mutex);

    k_thread_create(&spirit_level_thread, spirit_level_stack,
                    K_THREAD_STACK_SIZEOF(spirit_level_stack),
                    spirit_level_func,
                    NULL, NULL, NULL,
                    SPIRIT_LEVEL_PRIORITY, 0, K_MSEC(150));
}

void init_tilt_kmsg(void){
    k_msgq_init(&x_tilt_msgq, x_tilt_msg_buff, sizeof(int16_t), 5);
    k_msgq_init(&y_tilt_msgq, y_tilt_msg_buff, sizeof(int16_t), 5);
}