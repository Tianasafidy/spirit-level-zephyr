// Enter the driver name so that when we initialize the (maybe numerous)
// DM163 peripherals we can designated them by index.
#define DT_DRV_COMPAT siti_dm163

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/led.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "dm163.h"

#define NUM_LEDS 8
#define NUM_CHANNELS (NUM_LEDS * 3)

LOG_MODULE_REGISTER(dm163, LOG_LEVEL_DBG);

struct dm163_data {
  uint8_t brightness[NUM_CHANNELS];
  uint8_t channels[NUM_CHANNELS];
  const struct gpio_dt_spec *row_to_turn_off;
  struct k_mutex dm163_data_mutex;
};


struct dm163_config {
  const struct gpio_dt_spec en;
  const struct gpio_dt_spec dck;
  const struct gpio_dt_spec lat;
  const struct gpio_dt_spec rst;
  const struct gpio_dt_spec selbk;
  const struct gpio_dt_spec sin;
};

#define CONFIGURE_PIN(dt, flags)                                               \
  do {                                                                         \
    if (!device_is_ready((dt)->port)) {                                        \
      LOG_ERR("device %s is not ready", (dt)->port->name);                     \
      return -ENODEV;                                                          \
    }                                                                          \
    gpio_pin_configure_dt(dt, flags);                                          \
  } while (0)

static void pulse_data(const struct dm163_config *config, uint8_t data, int bits){

  for(int i = bits - 1 ; i >= 0 ; i --){
    gpio_pin_set_dt(&config->sin,(( data >> i) && 0x01)); 
    gpio_pin_set_dt(&config->dck,1); 
    gpio_pin_set_dt(&config->dck,0); 
  }

}

void dm163_turn_off_row(const struct device *dev, const struct gpio_dt_spec *row)
{
    struct dm163_data *data = dev->data;
    data->row_to_turn_off = row;  // on garde l’adresse du GPIO
}

static void flush_channels(const struct device *dev) {

  const struct dm163_config *config = dev->config;
  struct dm163_data *data = dev->data;

  k_mutex_lock(&data->dm163_data_mutex, K_FOREVER);

  for (int i = NUM_CHANNELS - 1; i >= 0; i--) {

      pulse_data(config, data->channels[i], 8);

      if (i == NUM_CHANNELS - (6 * 3) && data->row_to_turn_off) {
          gpio_pin_set_dt(data->row_to_turn_off, 0); 
          data->row_to_turn_off = NULL;              
      }
  }

  gpio_pin_set_dt(&config->lat, 1);
  gpio_pin_set_dt(&config->lat, 0);
  k_mutex_unlock(&data->dm163_data_mutex);
}


static void flush_brightness(const struct device *dev){
  const struct dm163_config *config = dev->config;
  struct dm163_data *data = dev->data;
  
  k_mutex_lock(&data->dm163_data_mutex, K_FOREVER);
  gpio_pin_set_dt(&config->selbk, 0);
  for (int i = NUM_CHANNELS - 1; i >= 0; i--)
    pulse_data(config, data->brightness[i], 6);

  gpio_pin_set_dt(&config->lat, 1);
  gpio_pin_set_dt(&config->lat, 0);

  gpio_pin_set_dt(&config->selbk, 1);
  k_mutex_unlock(&data->dm163_data_mutex);
}

static int dm163_set_brightness(const struct device *dev, uint32_t led, uint8_t value){
  struct dm163_data * data = dev-> data; 

  if(value > 100 || led > 7 )
    return -EINVAL;  

  uint8_t converted_value = (value*63)/100; // bri 

  uint32_t first_channel = ((led + 1)*3) - 1; 
  data->brightness[first_channel] = converted_value; // brightness between 0 and 63 
  data->brightness[first_channel - 1] = converted_value; // brightness between 0 and 63 
  data->brightness[first_channel - 2] = converted_value; // brightness between 0 and 63 

  flush_brightness(dev); 
  
  return 0; 
}

static int dm163_on(const struct device *dev, uint32_t led){
  struct dm163_data * data = dev-> data; 
  if(led > 7)
    return -EINVAL; 

  uint32_t first_channel = ((led + 1)*3) - 1; 
  data->channels[first_channel] = 0xff; 
  data->channels[first_channel - 1] = 0xff; 
  data->channels[first_channel - 2] = 0xff; 

  flush_channels(dev); 
  return 0; 
}


static int dm163_off(const struct device *dev, uint32_t led){
  struct dm163_data * data = dev-> data; 
  if(led > 7)
    return -EINVAL; 

  uint32_t first_channel = ((led + 1)*3) - 1; 
  data->channels[first_channel] = 0x00; 
  data->channels[first_channel - 1] = 0x00; 
  data->channels[first_channel - 2] = 0x00; 
  flush_channels(dev); 
  return 0; 
}

int dm163_set_color(const struct device *dev, uint32_t led, uint8_t num_colors,
                    const uint8_t *color){
    struct dm163_data * data = dev-> data;
    if(led > 7 || num_colors > 3) {
      return -EINVAL; 
    }
    
    uint32_t first_channel = ((led + 1)*3) - 1; 

    for(uint8_t i = 0; i < num_colors; i++){
      data->channels[first_channel - i] = color[num_colors - i - 1];  
    }

    flush_channels(dev); 
    return 0; 
}

int dm163_write_channels(const struct device *dev, uint32_t start_channel,
                         uint32_t num_channels, const uint8_t *buf){
  // assuming that channels are feed from 23 to 0
  struct dm163_data * data = dev->data; 

//  printf("\n"); 
//  printf("start_channel : %d \n", start_channel); 
//  printf("num_channel : %d \n", num_channels); 


  if((start_channel - num_channels + 1) < 0 || num_channels > 24)
    return -EINVAL; 
  
  for(uint32_t i = 0; i < num_channels; i ++){
    data->channels[start_channel - i] = buf[num_channels - i - 1];  
 //   printf("buf %d : %d\n", start_channel - i, buf[num_channels - 1 - i]); 
  }
  
  flush_channels(dev); 
  return 0; 
  
}

static int dm163_init(const struct device *dev) {
  const struct dm163_config *config = dev->config;


  LOG_DBG("starting initialization of device %s", dev->name);

  // Disable DM163 outputs while configuring if this pin
  // is connected.
  if (config->en.port) {
    CONFIGURE_PIN(&config->en, GPIO_OUTPUT_INACTIVE);
  }
  // Configure all pins. Make reset active so that the DM163
  // initiates a reset. We want the clock (dck) and latch (lat)
  // to be inactive at start. selbk will select bank 1 by default.
  CONFIGURE_PIN(&config->rst, GPIO_OUTPUT_ACTIVE);
  CONFIGURE_PIN(&config->dck, GPIO_OUTPUT_INACTIVE);
  CONFIGURE_PIN(&config->lat, GPIO_OUTPUT_INACTIVE);
  CONFIGURE_PIN(&config->selbk, GPIO_OUTPUT_ACTIVE);
  CONFIGURE_PIN(&config->sin, GPIO_OUTPUT);

  struct dm163_data *data = dev->data;

  k_usleep(1); // 100ns min
  // Cancel reset by making it inactive.
  gpio_pin_set_dt(&config->rst, 0);

  memset(&data->brightness, 0x3f, sizeof(data->brightness));
  memset(&data->channels, 0x00, sizeof(data->channels));
  
  k_mutex_init(&data->dm163_data_mutex);

  flush_brightness(dev);
  flush_channels(dev);

  // Enable the outputs if this pin is connected.
  if (config->en.port) {
    gpio_pin_set_dt(&config->en, 1);
  }
  LOG_INF("device %s initialized", dev->name);
  return 0;
}

static const struct led_driver_api dm163_api = {
    .on = dm163_on,
    .off = dm163_off,
    .set_brightness = dm163_set_brightness,
    .set_color = dm163_set_color, 
    .write_channels = dm163_write_channels,  
};

// Macro to initialize the DM163 peripheral with index i
#define DM163_DEVICE(i)                                                        \
                                                                               \
  /* Build a dm163_config for DM163 peripheral with index i, named          */ \
  /* dm163_config_/i/ (for example dm163_config_0 for the first peripheral) */ \
  static const struct dm163_config dm163_config_##i = {                        \
      .en = GPIO_DT_SPEC_GET_OR(DT_DRV_INST(i), en_gpios, {0}),                \
      .dck = GPIO_DT_SPEC_GET(DT_DRV_INST(i), dck_gpios),                      \
      .lat = GPIO_DT_SPEC_GET(DT_DRV_INST(i), lat_gpios),                      \
      .rst = GPIO_DT_SPEC_GET(DT_DRV_INST(i), rst_gpios),                      \
      .selbk = GPIO_DT_SPEC_GET(DT_DRV_INST(i), selbk_gpios),                  \
      .sin = GPIO_DT_SPEC_GET(DT_DRV_INST(i), sin_gpios),                      \
  };                                                                           \
                                                                               \
  /* Build a new dm163_data_/i/ structure for dynamic data                  */ \
  static struct dm163_data dm163_data_##i = {};                                \
                                                                               \
  DEVICE_DT_INST_DEFINE(i, &dm163_init, NULL, &dm163_data_##i,                 \
                        &dm163_config_##i, POST_KERNEL,                        \
                        CONFIG_LED_INIT_PRIORITY, &dm163_api);


// Apply the DM163_DEVICE to all DM163 peripherals not marked "disabled"
// in the device tree and pass it the corresponding index.
DT_INST_FOREACH_STATUS_OKAY(DM163_DEVICE)
