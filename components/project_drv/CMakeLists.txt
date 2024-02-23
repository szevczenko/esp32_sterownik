idf_component_register(SRCS "error_siewnik.c" "error_solarka.c" 
                            "measure.c" "motor.c" "servo.c" "vibro.c"
                            "server_conroller.c"
                    INCLUDE_DIRS "." 
                    REQUIRES backend menu main drv)
