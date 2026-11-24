#pragma once

#include <list>
#include "dl_detect_define.hpp"
#include "esp_camera.h"

/**
 * @brief Draw detection results on an RGB565 image / 在 RGB565 图像上绘制检测结果
 * 
 * @param image_ptr     Image pointer / 图像指针
 * @param image_height  Image height / 图像高度
 * @param image_width   Image width / 图像宽度
 * @param results       Detection results / 检测结果
 */
void draw_detection_result(uint16_t *image_ptr, int image_height, int image_width, std::list<dl::detect::result_t> &results);

/**
 * @brief Draw detection results on an RGB888 image / 在 RGB888 图像上绘制检测结果
 * 
 * @param image_ptr     Image pointer / 图像指针
 * @param image_height  Image height / 图像高度
 * @param image_width   Image width / 图像宽度
 * @param results       Detection results / 检测结果
 */

void draw_detection_result(uint8_t *image_ptr, int image_height, int image_width, std::list<dl::detect::result_t> &results);

/**
 * @brief Print detection results in the terminal / 在终端打印检测结果
 * 
 * @param results Detection results / 检测结果
 */
void print_detection_result(std::list<dl::detect::result_t> &results);

/**
 * @brief Decode a frame buffer / 解码帧缓冲区 
 *        - Return fb->buf when fb->format is PIXFORMAT_RGB565 / 当 fb->format 为 PIXFORMAT_RGB565 时返回 fb->buf
 *        - Otherwise return a new RGB888 buffer; remember to free it / 否则返回新分配的 RGB888 缓冲区，使用后请释放
 * 
 * @param fb Frame buffer / 帧缓冲区 
 */
void *app_camera_decode(camera_fb_t *fb);
