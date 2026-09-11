idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES
        retriever_link
        retriever_protocol
        retriever_imu
        driver
        esp_timer
)
