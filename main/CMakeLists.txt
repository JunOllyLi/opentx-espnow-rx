idf_component_register(SRCS
        "${OUTPUT_PULSE}.cpp"
        "rx.cpp"
        "espnow_rc_rx.cpp"
        "led_qtpy_esp32s2.cpp"
                    INCLUDE_DIRS ""
                    REQUIRES arduino extra_arduino_libs)

