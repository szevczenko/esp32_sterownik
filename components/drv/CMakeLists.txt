idf_component_register(SRCS "battery.c" "but.c" "buzzer.c" "error_siewnik.c" "error_solarka.c" "fast_add.c" 
                            "keepalive.c" "measure.c" "motor.c" "pcf8574.c" "ringBuff.c" "servo.c" "sleep.c" "vibro.c"
                            "server_conroller.c" "ultrasonar.c" "power_on.c"
                    INCLUDE_DIRS "." 
                    REQUIRES config backend menu main)
