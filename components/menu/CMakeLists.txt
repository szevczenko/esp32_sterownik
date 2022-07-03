idf_component_register(SRCS "menu_param.c" "menu.c" "ssdFigure.c" "menu_drv.c" "menu_main.c"
                            "wifi_menu.c" "menu_default.c" "start_menu.c" "menu_bootup.c" "menu_state.c" "menu_backend.c" "menu_settings.c"
                            "menu_low_battery.c"
                    INCLUDE_DIRS "." 
                    REQUIRES config backend menu main nvs_flash oled)
