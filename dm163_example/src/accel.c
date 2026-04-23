#include "accel.h"
#include <math.h>

#define XL_INT1	DT_ALIAS(xlint1)

extern struct k_msgq x_tilt_msgq;
extern struct k_msgq y_tilt_msgq;

K_THREAD_STACK_DEFINE(comp_filter_stack_area, COMP_FILTER_STACK_SIZE);
struct k_thread comp_filter_thread;

struct k_mutex raw_tilt_mutex;

int16_t raw_x_tlit_xl, raw_y_tlit_xl; 
int16_t raw_x_tlit_gyro, raw_y_tlit_gyro; 
int16_t x_gyro_dps_bias = 0 ; 
int16_t y_gyro_dps_bias = 0 ; 

static struct k_work read_xl_data_work; 

const struct i2c_dt_spec lsm6dsl_i2c = I2C_DT_SPEC_GET(ACCEL_PATH); 

static const struct gpio_dt_spec lsm6dsl_int1 = GPIO_DT_SPEC_GET_OR(ACCEL_PATH, irq_gpios,
							      {0});
static struct gpio_callback lsm6dsl_int1_cb_data;

void read_xl_data(struct k_work *item){
    int16_t x_xl, y_xl, z_xl; 
    if(read_accel_3_axes(&x_xl, &y_xl, &z_xl) == 0){
        printf("Acceleration X : %d mg ; Y : %d mg ; Z : %d mg \n", x_xl, y_xl, z_xl); 
    }
}

static inline float mg_to_g(int16_t mg) { return (float)mg / 1000.0f; }

void read_tilt_from_accel(int16_t *x_axis_tilt, int16_t *y_axis_tilt)
{

    int16_t ax_mg,ay_mg, az_mg; 
    read_accel_3_axes(&ax_mg, &ay_mg, &az_mg); 
    float ax = mg_to_g(ax_mg);
    float ay = mg_to_g(ay_mg);
    float az = mg_to_g(az_mg);

    float x_axis_tilt_rad  = atan2f(ay, az); 
    float y_axis_tilt_rad = atan2f(-ax, sqrtf(ay*ay + az*az));
    
    const float RAD2DEG = 180.0f/3.141592653589793f; 
    *x_axis_tilt  = (int16_t)(x_axis_tilt_rad  * RAD2DEG);
    *y_axis_tilt =(int16_t)(y_axis_tilt_rad * RAD2DEG);
}

int read_tilt_from_gyro(int16_t *x_axis_tilt, int16_t *y_axis_tilt){
    int16_t x_dps, y_dps, z_dps; 
    static int64_t last_time = 0; 
    static int64_t now = 0;  
    static int16_t x_tilt_local, y_tilt_local = 0; 

    int ret = read_gyro_3_axes(&x_dps, &y_dps, &z_dps); 
    if( ret != 0){
        return ret;  
    }

    x_dps = x_dps - x_gyro_dps_bias; 
    y_dps = y_dps - y_gyro_dps_bias; 

    now = k_uptime_get(); 
    float delta_t = (float)((now - last_time)/1000.0f);  

    x_tilt_local +=(int16_t)(x_dps*delta_t); 
    y_tilt_local += (int16_t)(y_dps*delta_t); 

    *x_axis_tilt = x_tilt_local;
    *y_axis_tilt = y_tilt_local;  

    last_time = now; 
    return 0; 
}

void xl_read_x_y_tilt_angle(struct k_work *item)
{
	int16_t x_axis_tilt, y_axis_tilt;

    read_tilt_from_accel(&x_axis_tilt, &y_axis_tilt);
    printf("X AXIS TILT : %d deg || Y AXIS TILT : %d deg\n",
            x_axis_tilt, y_axis_tilt);
}

void accel_data_ready(const struct device *dev, struct gpio_callback *cb, 
        uint32_t pins)
{
    k_work_submit(&read_xl_data_work);    
}

int read_single_byte(uint8_t reg, uint8_t * value){
        return i2c_reg_read_byte_dt(&lsm6dsl_i2c, reg, value);  
}

int write_single_byte(uint8_t reg, uint8_t value){
    return i2c_reg_write_byte_dt(&lsm6dsl_i2c, reg, value); 
}

void read_xl_gyro_data(struct k_work *item){
    uint8_t status_reg_value; 

    read_single_byte(STATUS_REG_REG,&status_reg_value); 
    
    k_mutex_lock(&raw_tilt_mutex, K_FOREVER);
    if(status_reg_value & 0x01){
        read_tilt_from_accel(&raw_x_tlit_xl, &raw_y_tlit_xl);

    //    printf("[ACCEL] X AXIS TILT : %d deg || Y AXIS TILT : %d deg\n",
    //        raw_x_tlit_xl, raw_y_tlit_xl);
    }

    if(status_reg_value & 0x02){
        if( read_tilt_from_gyro(&raw_x_tlit_gyro, &raw_y_tlit_gyro) == 0){
        //    printf("[GYRO] X TILT : %d deg ; Y TILT : %d deg \n",raw_x_tlit_gyro, raw_y_tlit_gyro ); 
        }
    }
    k_mutex_unlock(&raw_tilt_mutex);
}

void complemetary_filter_func(void *, void *, void *)
{
    static float filtered_x = 0.0f;
    static float filtered_y = 0.0f;
    bool first_run = true;

    while (1)
    {
        int16_t accel_x, accel_y;
        int16_t gyro_x, gyro_y;
        int16_t x_tilt = 0;  
        int16_t y_tilt = 0;  

        k_mutex_lock(&raw_tilt_mutex, K_FOREVER);
        accel_x = raw_x_tlit_xl;
        accel_y = raw_y_tlit_xl;
        gyro_x  = raw_x_tlit_gyro;
        gyro_y  = raw_y_tlit_gyro;
        k_mutex_unlock(&raw_tilt_mutex);

        if (first_run) {
            filtered_x = (float)accel_x;
            filtered_y = (float)accel_y;
            first_run = false;
        }

        const float accel_weight = 0.8f;
        const float gyro_weight  = 0.2f;

        filtered_x = accel_weight * (float)accel_x + gyro_weight * (filtered_x - (float)gyro_x);
        filtered_y = accel_weight * (float)accel_y + gyro_weight * (filtered_y - (float)gyro_y);

      //  printf("[COMPL_FILTER] X TILT : %d deg ; Y TILT : %d deg \n",
      //         (int16_t)lrintf(filtered_x), (int16_t)lrintf(filtered_y));
         
        x_tilt = (int16_t)filtered_x; 
        y_tilt = (int16_t)filtered_y; 

        while (k_msgq_put(&x_tilt_msgq, &x_tilt, K_NO_WAIT) != 0)
        {
            k_msgq_purge(&x_tilt_msgq);
        }

        while (k_msgq_put(&y_tilt_msgq, &y_tilt, K_NO_WAIT) != 0)
        {
            k_msgq_purge(&y_tilt_msgq);
        }

        k_sleep(K_MSEC(50));
    }
}


void calibrate_gyro(void){
    int16_t x_axis_tilt, y_axis_tilt;
    int16_t x_mdps, y_mdps, z_mdps;

    read_tilt_from_accel(&x_axis_tilt, &y_axis_tilt);
    printf("[ACCEL] X AXIS TILT : %d deg || Y AXIS TILT : %d deg\n",
        x_axis_tilt, y_axis_tilt);

    const int discard_first = 5;
    const int sample_number = 100;
    const int delay_ms = 5;

    x_gyro_dps_bias = 0;
    y_gyro_dps_bias = 0;

    int32_t sum_x = 0;
    int32_t sum_y = 0;
    int valid = 0;

    for (int i = 0; i < sample_number + discard_first; i++) {
        if (read_gyro_3_axes(&x_mdps, &y_mdps, &z_mdps) == 0) {
            if (i >= discard_first) {
                sum_x += (int32_t)x_mdps;
                sum_y += (int32_t)y_mdps;
                valid++;
            }
        }
        k_sleep(K_MSEC(delay_ms));
    }

    if (valid > 0) {
        int32_t avg_x = sum_x / valid;
        int32_t avg_y = sum_y / valid;
        x_gyro_dps_bias = (int16_t)avg_x;
        y_gyro_dps_bias = (int16_t)avg_y;
    } else {
        x_gyro_dps_bias = 0;
        y_gyro_dps_bias = 0;
    }

    printf("[GYRO BIAS] X : %d mdps ; Y : %d mdps (averaged %d samples)\n",
           x_gyro_dps_bias, y_gyro_dps_bias, valid);
}


int read_accel_3_axes(int16_t * x_accel, int16_t * y_accel, int16_t * z_accel){
   uint8_t out_xl[6];  

    int ret = i2c_burst_read_dt(&lsm6dsl_i2c, OUTX_L_XL_REG, out_xl, 6); 
    if(ret != 0){
        printf("I2C burst read failed (err %d) \n", ret); 
        return ret; 
    }
    
    int16_t x_raw = (int16_t)(out_xl[1] << 8) | out_xl[0]; 
    int16_t y_raw = (int16_t)(out_xl[3] << 8) | out_xl[2]; 
    int16_t z_raw = (int16_t)(out_xl[5] << 8) | out_xl[4]; 
    
    *x_accel = (x_raw * 61)/1000; 
    *y_accel = (y_raw * 61)/1000; 
    *z_accel = (z_raw * 61)/1000; 

    return 0; 
}

int read_gyro_3_axes(int16_t * x_dps, int16_t * y_dps, int16_t * z_dps){
   uint8_t out_gyro[6];  

    int ret = i2c_burst_read_dt(&lsm6dsl_i2c, OUTX_L_G_REG, out_gyro, 6); 
    if(ret != 0){
        printf("I2C burst read failed (err %d) \n", ret); 
        return ret; 
    }
    
    int16_t x_raw = (int16_t)(out_gyro[1] << 8) | out_gyro[0]; 
    int16_t y_raw = (int16_t)(out_gyro[3] << 8) | out_gyro[2]; 
    int16_t z_raw = (int16_t)(out_gyro[5] << 8) | out_gyro[4]; 
    
    const float LSB_GYRO = 0.00875; // +- 250 full scale 
	*x_dps = (int16_t)(x_raw * LSB_GYRO) ;
	*y_dps = (int16_t)(y_raw * LSB_GYRO);
	*z_dps = (int16_t)(z_raw * LSB_GYRO);

    return 0;
}

int config_gyro(void){

    if (!device_is_ready(lsm6dsl_i2c.bus))
    {
        printf("I2C2 device not ready\n");
        return -ENODEV;
    }

    int value = 0x80; // G_HM_MODE = 0b1
    int ret = write_single_byte(CTRL7_G_REG, value); 
    if(ret !=0){
        printf("I2C write failed (err %d)\n", ret); 
    }

    value = 0x10; // ODR_G = 0b0001 
    ret = write_single_byte(CTRL2_G_REG, value); 
    if(ret !=0){
        printf("I2C write failed (err %d)\n", ret); 
    }

    return 0; 

}

int init_accelerometer(bool use_gyro){

    k_mutex_init(&raw_tilt_mutex);
    
    k_work_init(&read_xl_data_work, read_xl_gyro_data);

    if(!gpio_is_ready_dt(&lsm6dsl_int1)){
        printf("Error: button device %s is not ready\n",
		       lsm6dsl_int1.port->name);
		return 0;
    }

	int ret = gpio_pin_configure_dt(&lsm6dsl_int1, GPIO_INPUT);
	if (ret != 0) {
		printf("Error %d: failed to configure %s pin %d\n",
		       ret, lsm6dsl_int1.port->name, lsm6dsl_int1.pin);
		return 0;
	}
    
    ret = gpio_pin_interrupt_configure_dt(&lsm6dsl_int1,
					      GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		printf("Error %d: failed to configure interrupt on %s pin %d\n",
			ret, lsm6dsl_int1.port->name, lsm6dsl_int1.pin);
		return 0;
	}

	gpio_init_callback(&lsm6dsl_int1_cb_data, accel_data_ready, BIT(lsm6dsl_int1.pin));
	gpio_add_callback(lsm6dsl_int1.port, &lsm6dsl_int1_cb_data);
	printf("Set up lsm6dsl_int1 at %s pin %d\n", lsm6dsl_int1.port->name, lsm6dsl_int1.pin);

    if (!device_is_ready(lsm6dsl_i2c.bus))
    {
        printf("I2C2 device not ready\n");
        return -ENODEV;
    }

    uint8_t who_am_i = 0;

    ret = read_single_byte(WHO_AM_I_REG,&who_am_i);  
    if(ret != 0  || who_am_i != LSM6DSL_ADDR)
    {
        printf("I2C read failed (err %d) or wrong addr found\n", ret);
        return -1; 
    }

    uint8_t value = 0x01; // SW_RESET = 0b1 
    ret = write_single_byte(CTRL3_C_REG, value); 
    if(ret != 0){
        printf("I2C write failed (err %d)\n", ret); 
    }

    uint8_t reg_value; 
    do {
        k_sleep(K_MSEC(10));
        read_single_byte(CTRL3_C_REG, &reg_value);
    } while (reg_value & 0x01); // wait until S_RESET bit is cleared

    printf("Sensor reset successfully !!!! \n"); 

    value = 0x10; // XL_HM_MODE = 0b1
    ret = write_single_byte(CTRL6_C_REG, value); 
    if(ret !=0){
        printf("I2C write failed (err %d)\n", ret); 
    }

    // get accel value at same freauency as gyro
    value = 0xb0; // ODR_XL[3:0] = 0b0001 and FS_XL[1:0]= 0b00
    ret = write_single_byte(CTRL1_XL_REG, value); 
    if(ret != 0){
        printf("I2C write failed (err %d)\n", ret); 
    }

    if(use_gyro){
        ret = config_gyro(); 
        if(ret != 0){
        printf("Gyro config failed (err %d)\n", ret); 
        }
    }

    value = (use_gyro) ? 0x03 : 0x01; // INT_DRDY_XL = 0b1 
    ret = write_single_byte(INT1_CTRL_REG, value); 
    if(ret != 0){
        printf("I2C write failed (err %d)\n", ret); 
    }

    value = 0x04; // IF_INC = 0b1 
    ret = write_single_byte(CTRL3_C_REG, value); 
    if(ret != 0){
        printf("I2C write failed (err %d)\n", ret); 
    }
    
    calibrate_gyro(); // TODO: check why we need a first read to make the interrupt working 


    k_thread_create(&comp_filter_thread, comp_filter_stack_area,
                        K_THREAD_STACK_SIZEOF(comp_filter_stack_area),
                        complemetary_filter_func,
                        NULL, NULL, NULL,
                        COMP_FILTER_PRIORITY, 0, K_NO_WAIT);

    return 0; 
}

