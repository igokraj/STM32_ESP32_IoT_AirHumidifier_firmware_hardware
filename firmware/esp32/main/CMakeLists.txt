idf_component_register(SRCS "main.c" "UART/uart.c" "WIFI/wifi.c" "MQTT/mqtt.c"
                    INCLUDE_DIRS "."
                    EMBED_TXTFILES "Cert/AmazonRootCA1.pem"
                                   "Cert/device.pem.key.crt.crt"
                                   "Cert/private.pem.key")