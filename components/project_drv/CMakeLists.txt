idf_component_register(SRCS "error_siewnik.c" "error_solarka.c" 
                            "measure.c" "motor.c" "servo.c" "vibro.c"
                            "server_controller.c" "tank_sensor.c" "e108_position_driver.c"
                    INCLUDE_DIRS "." 
                    REQUIRES backend menu main drv)
