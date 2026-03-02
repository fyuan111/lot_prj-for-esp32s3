idf_component_register(SRCS "lot_ota.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_https_ota esp_http_client app_update mbedtls version wifi)
