idf_component_register(SRCS "menu_param.c" "menu.c" "ssd1306_fonts.c" "ssd1306_tests.c" "ssd1306.c" "ssdFigure.c" "menu_drv.c" "menu_main.c"
                            "wifi_menu.c" "menu_default.c" "start_menu.c" "menu_bootup.c" "menu_state.c" "menu_backend.c" "menu_settings.c"
                    INCLUDE_DIRS "." 
                    REQUIRES config backend menu main nvs_flash)
