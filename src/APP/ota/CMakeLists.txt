idf_component_register(SRCS "lot_ota.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_https_ota app_update mbedtls)
