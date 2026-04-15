#ifndef _LED_INDICATOR_H_
#define _LED_INDICATOR_H_

#include "driver/gpio.h"
#include "device.h"

/**
 * @brief Initialize the RGB LED indicator.
 *
 * @param gpio GPIO number connected to the addressable RGB LED.
 */
void led_indicator_init(gpio_num_t gpio);

/**
 * @brief Set the LED color based on device status.
 *
 * @param status Current device status.
 */
void led_indicator_set_status(device_status_t status);

#endif // _LED_INDICATOR_H_
