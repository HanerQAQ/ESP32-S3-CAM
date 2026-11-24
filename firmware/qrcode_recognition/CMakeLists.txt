cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS
    ../components/esp-dl
    ../components/esp-code-scanner
    ../components/fb_gfx
    ../components/esp32-camera
    ../components/mdns
    ../components/modules
)

add_compile_options(-fdiagnostics-color=always)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(qrcode_recognition)
