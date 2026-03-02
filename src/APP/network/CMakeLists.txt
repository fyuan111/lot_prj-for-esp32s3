idf_component_register(SRCS "lot_network.c"
                            "mqtt/lot_mqtt.c"
                            "websocket/lot_websocket.c"
                       INCLUDE_DIRS "include"
                       REQUIRES mqtt esp_event esp_websocket_client wifi json esp_timer version proto modbus)
