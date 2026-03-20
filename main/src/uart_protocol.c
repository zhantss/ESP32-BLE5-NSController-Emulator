#include "uart_protocol.h"
#include "uart_simple.h"
#include "uart_easycon.h"

#include "esp_log.h"

#include <string.h>
#include <stdio.h>

// Log tag
#define LOG_PROTOCOL "protocol"

// Protocol statistics
static uart_protocol_stats_t protocol_stats = {
    .frames_received = 0,
    .frames_parsed = 0,
    .parse_errors = 0,
    .checksum_errors = 0,
    .protocol_detections = 0,
    .protocol_switches = 0
};

// Debug logging flag
static bool protocol_debug_enabled = false;

// Protocol registry
static const uart_protocol_impl_t* protocol_registry[] = {
    &simple_protocol_impl,
    &easycon_protocol_impl,
    NULL // Sentinel
};

// Get protocol implementation by type
const uart_protocol_impl_t* uart_protocol_get_impl(uart_protocol_t protocol) {
    for (int i = 0; protocol_registry[i] != NULL; i++) {
        if (protocol_registry[i]->protocol == protocol) {
            return protocol_registry[i];
        }
    }

    ESP_LOGW(LOG_PROTOCOL, "Protocol implementation not found: %d", protocol);
    return NULL;
}

// Get current protocol from configuration
uart_protocol_t uart_protocol_get_configured(void) {
    // Get protocol from compile-time configuration
    #ifdef CONFIG_UART_PROTOCOL
        uart_protocol_t protocol = (uart_protocol_t)CONFIG_UART_PROTOCOL;

        // Validate protocol value
        if (protocol == UART_PROTOCOL_SIMPLE ||
            protocol == UART_PROTOCOL_EASYCON ||
            protocol == UART_PROTOCOL_AUTO_DETECT) {
            return protocol;
        }

        ESP_LOGW(LOG_PROTOCOL, "Invalid configured protocol: %d, using SIMPLE", protocol);
    #endif

    // Default to simple protocol
    return UART_PROTOCOL_SIMPLE;
}

// Auto-detect protocol from data
uart_protocol_t uart_protocol_auto_detect(const uint8_t* data, size_t len) {
    if (len < 2) {
        return UART_PROTOCOL_SIMPLE; // Default if not enough data
    }

    protocol_stats.protocol_detections++;

    // Try each protocol's detection function
    for (int i = 0; protocol_registry[i] != NULL; i++) {
        const uart_protocol_impl_t* impl = protocol_registry[i];

        // Skip auto-detect protocol itself
        if (impl->protocol == UART_PROTOCOL_AUTO_DETECT) {
            continue;
        }

        if (impl->detect_protocol != NULL) {
            if (impl->detect_protocol(data, len)) {
                ESP_LOGD(LOG_PROTOCOL, "Auto-detected protocol: %s", impl->name);
                return impl->protocol;
            }
        }
    }

    // Default to simple protocol if none detected
    ESP_LOGD(LOG_PROTOCOL, "No protocol detected, using SIMPLE");
    return UART_PROTOCOL_SIMPLE;
}

// Get protocol statistics
void uart_protocol_get_stats(uart_protocol_stats_t* stats) {
    if (stats != NULL) {
        memcpy(stats, &protocol_stats, sizeof(uart_protocol_stats_t));
    }
}

// Reset protocol statistics
void uart_protocol_reset_stats(void) {
    memset(&protocol_stats, 0, sizeof(uart_protocol_stats_t));
    ESP_LOGI(LOG_PROTOCOL, "Protocol statistics reset");
}

// Hex dump utility for debugging
void uart_protocol_hex_dump(const char* tag, const uint8_t* data, size_t len) {
    if (!protocol_debug_enabled || tag == NULL || data == NULL || len == 0) {
        return;
    }

    ESP_LOGD(tag, "Hex dump (%d bytes):", len);

    for (size_t i = 0; i < len; i += 16) {
        char line[80];
        char* ptr = line;

        // Address
        ptr += sprintf(ptr, "  %04X: ", (unsigned int)i);

        // Hex bytes
        for (size_t j = 0; j < 16; j++) {
            if (i + j < len) {
                ptr += sprintf(ptr, "%02X ", data[i + j]);
            } else {
                ptr += sprintf(ptr, "   ");
            }

            if (j == 7) {
                ptr += sprintf(ptr, " ");
            }
        }

        // ASCII representation
        ptr += sprintf(ptr, " |");
        for (size_t j = 0; j < 16 && i + j < len; j++) {
            uint8_t c = data[i + j];
            if (c >= 32 && c <= 126) {
                ptr += sprintf(ptr, "%c", c);
            } else {
                ptr += sprintf(ptr, ".");
            }
        }
        ptr += sprintf(ptr, "|");

        ESP_LOGD(tag, "%s", line);
    }
}

// Get protocol name string
const char* uart_protocol_get_name(uart_protocol_t protocol) {
    const uart_protocol_impl_t* impl = uart_protocol_get_impl(protocol);
    if (impl != NULL) {
        return impl->name;
    }

    return "Unknown Protocol";
}

// Update statistics
void uart_protocol_update_stats_frame_received(void) {
    protocol_stats.frames_received++;
}

void uart_protocol_update_stats_frame_parsed(void) {
    protocol_stats.frames_parsed++;
}

void uart_protocol_update_stats_parse_error(void) {
    protocol_stats.parse_errors++;
}

void uart_protocol_update_stats_checksum_error(void) {
    protocol_stats.checksum_errors++;
}

void uart_protocol_update_stats_protocol_switch(void) {
    protocol_stats.protocol_switches++;
}

// Set debug logging
void uart_protocol_set_debug(bool enable) {
    protocol_debug_enabled = enable;
    const uart_protocol_impl_t* current_impl = NULL;

    current_impl = g_uart_manager.protocol_impl;
    if (current_impl != NULL) {
        current_impl->set_debug(enable);
    } else {
        ESP_LOGW(LOG_PROTOCOL, "Current protocol not set, unable to set debug");
    }
    ESP_LOGI(LOG_PROTOCOL, "Protocol debug logging %s", enable ? "enabled" : "disabled");
}

// Parse frame using current protocol
dev_uart_event_type_t uart_protocol_parse_frame(const uart_protocol_impl_t* impl,
                                                const uint8_t* data, size_t len,
                                                dev_uart_event_t* event) {
    if (impl == NULL || impl->parse_frame == NULL || data == NULL || event == NULL) {
        return UART_EVENT_UNKNOWN;
    }

    protocol_stats.frames_received++;

    // Debug logging
    if (protocol_debug_enabled) {
        ESP_LOGD(LOG_PROTOCOL, "Parsing frame with %s, len=%d", impl->name, len);
        uart_protocol_hex_dump(LOG_PROTOCOL, data, len);
    }

    // Parse the frame
    dev_uart_event_type_t result = impl->parse_frame(data, len, event);

    // Update statistics
    if (result != UART_EVENT_UNKNOWN) {
        protocol_stats.frames_parsed++;
    } else {
        protocol_stats.parse_errors++;
    }

    return result;
}

void uart_protocol_init_all(void) {
    ESP_LOGI(LOG_PROTOCOL, "Initializing all protocols");

    for (int i = 0; protocol_registry[i] != NULL; i++) {
        const uart_protocol_impl_t* impl = protocol_registry[i];

        if (impl->init_protocol != NULL) {
            impl->init_protocol();
            ESP_LOGD(LOG_PROTOCOL, "Initialized %s", impl->name);
        }
    }
}


void uart_protocol_deinit_all(void) {
    ESP_LOGI(LOG_PROTOCOL, "Deinitializing all protocols");

    for (int i = 0; protocol_registry[i] != NULL; i++) {
        const uart_protocol_impl_t* impl = protocol_registry[i];

        if (impl->deinit_protocol != NULL) {
            impl->deinit_protocol();
            ESP_LOGD(LOG_PROTOCOL, "Deinitialized %s", impl->name);
        }
    }
}