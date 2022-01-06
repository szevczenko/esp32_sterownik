idf_component_register(SRCS "atmega_communication.c" "battery.c" "but.c" "buzzer.c" "error_siewnik.c" "fast_add.c" 
                            "keepalive.c" "measure.c" "motor.c" "pcf8574.c" "ringBuff.c" "servo.c" "sleep.c" "vibro.c"
                            "server_conroller.c"
                    INCLUDE_DIRS "." 
                    REQUIRES config backend menu main)
