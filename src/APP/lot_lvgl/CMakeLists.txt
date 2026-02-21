idf_component_register(SRCS "lot_lvgl.c"
                            "UI/src/Homepage.c"
                       INCLUDE_DIRS "include"
                                    "UI/include"
                                    "../../SYSTEM"
                       REQUIRES lcd esp_lvgl_port time)
