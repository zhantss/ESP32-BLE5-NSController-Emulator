#include "led_indicator.h"

#include "esp_cpu.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static gpio_num_t s_led_gpio = GPIO_NUM_NC;

// WS2812 timing parameters (cycles @ 160 MHz, ~6.25 ns/cycle)
// Target ~1.25 us per bit total
#define WS2812_T1H_CYCLES 120  // ~750 ns high for bit 1
#define WS2812_T1L_CYCLES  60  // ~375 ns low  for bit 1
#define WS2812_T0H_CYCLES  50  // ~310 ns high for bit 0
#define WS2812_T0L_CYCLES 120  // ~750 ns low  for bit 0
#define WS2812_RESET_US    50  // >50 us low for reset

static inline void IRAM_ATTR delay_cycles(uint32_t cycles) {
  uint32_t start = esp_cpu_get_cycle_count();
  while (esp_cpu_get_cycle_count() - start < cycles) {
    ;
  }
}

static void IRAM_ATTR ws2812_send_byte(gpio_num_t gpio, uint8_t byte) {
  for (int i = 7; i >= 0; i--) {
    if (byte & (1 << i)) {
      gpio_set_level(gpio, 1);
      delay_cycles(WS2812_T1H_CYCLES);
      gpio_set_level(gpio, 0);
      delay_cycles(WS2812_T1L_CYCLES);
    } else {
      gpio_set_level(gpio, 1);
      delay_cycles(WS2812_T0H_CYCLES);
      gpio_set_level(gpio, 0);
      delay_cycles(WS2812_T0L_CYCLES);
    }
  }
}

static void ws2812_send_color(gpio_num_t gpio, uint8_t g, uint8_t r, uint8_t b) {
  portENTER_CRITICAL_SAFE(NULL);
  ws2812_send_byte(gpio, g);
  ws2812_send_byte(gpio, r);
  ws2812_send_byte(gpio, b);
  portEXIT_CRITICAL_SAFE(NULL);
  // Reset delay
  esp_rom_delay_us(WS2812_RESET_US);
}

void led_indicator_init(gpio_num_t gpio) {
  s_led_gpio = gpio;
  gpio_config_t io_conf = {
      .pin_bit_mask = (1ULL << gpio),
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  gpio_config(&io_conf);
  gpio_set_level(gpio, 0);
}

void led_indicator_set_status(device_status_t status) {
  if (s_led_gpio == GPIO_NUM_NC) {
    return;
  }

  uint8_t g = 0, r = 0, b = 0;
  switch (status) {
    case DEV_BOOT:
      g = 0xFF;
      r = 0xFF;
      b = 0x00; // Yellow
      break;
    case DEV_ADV_IND:
      g = 0x00;
      r = 0x00;
      b = 0xFF; // Blue
      break;
    case DEV_READY:
      g = 0x00;
      r = 0xFF;
      b = 0x00; // Green
      break;
    default:
      break;
  }

  ws2812_send_color(s_led_gpio, g, r, b);
}
