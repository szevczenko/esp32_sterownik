idf_component_register(SRCS "ssdFigure.c" "menu_main.c" "menu_bootup.c" "menu_state.c"
                            "wifi_menu.c" "menu_default.c" "start_menu.c" "menu_backend.c"
                            "menu_low_battery.c" "dictionary.c" "menu_settings.c"
                    INCLUDE_DIRS "." 
                    REQUIRES backend menu main nvs_flash oled oled_ui)
