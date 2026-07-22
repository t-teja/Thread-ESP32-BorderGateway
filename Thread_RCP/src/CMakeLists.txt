idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "." "../include"
    REQUIRES nvs_flash openthread esp_event driver esp_driver_uart
)
