idf_component_register(SRCS "lot_lcd.c"
                       INCLUDE_DIRS "include"
                       REQUIRES driver esp_lcd esp_lcd_ili9341 esp_lcd_touch_xpt2046)
