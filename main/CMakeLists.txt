file(GLOB_RECURSE srcs "main.c" "src/*.c")

idf_component_register(SRCS "${srcs}"
                    PRIV_REQUIRES bt nvs_flash mbedtls esp_driver_uart esp_driver_usb_serial_jtag
                    INCLUDE_DIRS "." "./include")
