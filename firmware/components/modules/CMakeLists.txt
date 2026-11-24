set(embed_files
    "web/www/loading.jpg.gz"
    "web/www/favicon.ico.gz"
    "web/www/index.js.gz"
    "web/www/index.css.gz"
    "web/www/index_ov3660.html.gz"
    "web/www/index_ov5640.html.gz"
)

set(src_dirs
    ai
    trace
)

set(include_dirs
    ai
    web
    trace
)

set(requires
    esp-dl
    esp_http_server
    nvs_flash
    esp_adc
    esp_timer
    esp_wifi
    esp_driver_gpio
    esp_driver_ledc
    fb_gfx
    mdns
    esp-code-scanner
    esp32-camera
)

idf_component_register(
    SRC_DIRS ${src_dirs}
    INCLUDE_DIRS ${include_dirs}
    REQUIRES ${requires}
    EMBED_FILES ${embed_files}
)

component_compile_options(-ffast-math -O3 -Wno-error=format -Wno-format)
