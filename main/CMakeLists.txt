idf_component_register(
    SRCS "led_strip_encoder.c" "led_task.c" "latch.c" "http.c" "pn532.c" "main.c" "auth.c"
    INCLUDE_DIRS "include/"
    EMBED_TXTFILES "tg_api_root.pem")
