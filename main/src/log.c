#include "device.h"

#include "log.h"

void log_print_ltk_hex(const char* data_name, const uint8_t *data) {
  ESP_LOGI(LOG_APP, "%s: %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x", 
    data_name, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7], 
    data[8], data[9], data[10], data[11], data[12], data[13], data[14], data[15]);
}

void log_print_addr(const void *addr) {
  const uint8_t *mac;
  mac = addr;
  ESP_LOGI(LOG_APP, "%02x:%02x:%02x:%02x:%02x:%02x", 
    mac[5], mac[4], mac[3], mac[2], mac[1], mac[0]);
}