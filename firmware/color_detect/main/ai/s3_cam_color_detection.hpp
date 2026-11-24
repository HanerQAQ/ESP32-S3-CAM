#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    COLOR_DETECTION_IDLE = 0,
    OPEN_REGISTER_COLOR_BOX,
    CLOSE_REGISTER_COLOR_BOX,
    REGISTER_COLOR,
    DELETE_COLOR,
    INCREASE_COLOR_AREA,
    DECREASE_COLOR_AREA,
    SWITCH_RESULT,
} color_detection_state_t;

void register_color_detection(QueueHandle_t frame_i,
                              QueueHandle_t event,
                              QueueHandle_t result,
                              QueueHandle_t frame_o,
                              const bool camera_fb_return);

/**
 * @brief Capture a live pristine JPEG frame from the camera pipeline for ColorTool.
 * @param out_buf Output pointer to JPEG buffer (caller must free with free())
 * @param out_len Output pointer to JPEG buffer length
 * @param timeout_ticks Maximum FreeRTOS ticks to wait for next frame
 * @return true on success
 */
bool color_detection_get_frame_jpeg(uint8_t **out_buf, size_t *out_len, TickType_t timeout_ticks);

/**
 * @brief Capture a live pristine raw RGB565 binary frame for /api/capture_binary (RAW button).
 * @param out_buf Output pointer to RGB565 buffer (caller must free with free())
 * @param out_len Output pointer to buffer length
 * @param width Output width
 * @param height Output height
 * @param timeout_ticks Maximum FreeRTOS ticks to wait
 * @return true on success
 */
bool color_detection_get_frame_raw(uint8_t **out_buf, size_t *out_len, uint16_t *width, uint16_t *height, TickType_t timeout_ticks);

/**
 * @brief Get the latest detection results formatted as JSON for /api/detections.
 * @param buf Output buffer for JSON string
 * @param max_len Size of buffer
 */
void s3_cam_color_get_detections_json(char *buf, size_t max_len);

#ifdef __cplusplus
}

struct VisionConfig;
void color_detection_apply_vision_config(const VisionConfig &cfg);
#endif
