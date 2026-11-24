cmake_minimum_required(VERSION 3.16)
project(esp32_s3_cam_stm32 LANGUAGES C)

# ==============================================================================
# ESP32-S3 CAM STM32 HAL Driver Library
# ==============================================================================

add_library(esp32_s3_cam STATIC
    driver/ESP32_S3_CAM.c
    driver/ESP32_S3_CAM_port.c
)

target_include_directories(esp32_s3_cam PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/driver>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(esp32_s3_cam PUBLIC c_std_99)

# Modern namespaced alias
add_library(ESP32_S3_CAM::ESP32_S3_CAM ALIAS esp32_s3_cam)

# Optional: Header-only interface library for projects providing custom ports
add_library(esp32_s3_cam_headers INTERFACE)
target_include_directories(esp32_s3_cam_headers INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/driver>
)
