#ifndef DM163_H_
#define DM163_H_

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

void dm163_turn_off_row(const struct device *dev, const struct gpio_dt_spec *row);

#endif // DM163_H_
