idf_component_register(SRCS "cmd_client.c" "cmd_server.c" "libtelnet.c" "parse_cmd.c" "telnet.c" "wifidrv.c" "cmd_client_req.c"
                    INCLUDE_DIRS "." 
                    REQUIRES config backend menu main)
