idf_component_register(SRCS "lot_power.c" "frequency.c" "power_mode.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_pm esp_hw_support)
