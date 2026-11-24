#include "who_ai_utils.hpp"

#include "esp_log.h"
#include "esp_camera.h"

#include "dl_image.hpp"
#include "my_usart.h"
#include "my_user_iic.h"
#include "my_usart1_user.h"

static const char *TAG = "ai_utils";

// extern uint16_t AI_Mode ;

// +-------+--------------------+----------+
// |       |       RGB565       |  RGB888  |
// +=======+====================+==========+
// |  Red  | 0b0000000011111000 | 0x0000FF |
// +-------+--------------------+----------+
// | Green | 0b1110000000000111 | 0x00FF00 |
// +-------+--------------------+----------+
// |  Blue | 0b0001111100000000 | 0xFF0000 |
// +-------+--------------------+----------+

void draw_detection_result(uint16_t *image_ptr, int image_height, int image_width, std::list<dl::detect::result_t> &results)
{
    int i = 0;
    for (std::list<dl::detect::result_t>::iterator prediction = results.begin(); prediction != results.end(); prediction++, i++)
    {
        dl::image::draw_hollow_rectangle(image_ptr, image_height, image_width,
                                         DL_MAX(prediction->box[0], 0),
                                         DL_MAX(prediction->box[1], 0),
                                         DL_MAX(prediction->box[2], 0),
                                         DL_MAX(prediction->box[3], 0),
                                         0b1110000000000111);

        if (prediction->keypoint.size() == 10)
        {
            dl::image::draw_point(image_ptr, image_height, image_width, DL_MAX(prediction->keypoint[0], 0), DL_MAX(prediction->keypoint[1], 0), 4, 0b0000000011111000); // Left eye / 左眼
            dl::image::draw_point(image_ptr, image_height, image_width, DL_MAX(prediction->keypoint[2], 0), DL_MAX(prediction->keypoint[3], 0), 4, 0b0000000011111000); // Left corner of the mouth / 嘴角左侧
            dl::image::draw_point(image_ptr, image_height, image_width, DL_MAX(prediction->keypoint[4], 0), DL_MAX(prediction->keypoint[5], 0), 4, 0b1110000000000111); // Nose / 鼻子
            dl::image::draw_point(image_ptr, image_height, image_width, DL_MAX(prediction->keypoint[6], 0), DL_MAX(prediction->keypoint[7], 0), 4, 0b0001111100000000); // Right eye / 右眼
            dl::image::draw_point(image_ptr, image_height, image_width, DL_MAX(prediction->keypoint[8], 0), DL_MAX(prediction->keypoint[9], 0), 4, 0b0001111100000000); // Right corner of the mouth / 嘴角右侧
        }
    }
}

void draw_detection_result(uint8_t *image_ptr, int image_height, int image_width, std::list<dl::detect::result_t> &results)
{
    int i = 0;
    for (std::list<dl::detect::result_t>::iterator prediction = results.begin(); prediction != results.end(); prediction++, i++)
    {
        dl::image::draw_hollow_rectangle(image_ptr, image_height, image_width,
                                         DL_MAX(prediction->box[0], 0),
                                         DL_MAX(prediction->box[1], 0),
                                         DL_MAX(prediction->box[2], 0),
                                         DL_MAX(prediction->box[3], 0),
                                         0x00FF00);

        if (prediction->keypoint.size() == 10)
        {
            dl::image::draw_point(image_ptr, image_height, image_width, DL_MAX(prediction->keypoint[0], 0), DL_MAX(prediction->keypoint[1], 0), 4, 0x0000FF); // Left eye / 左眼
            dl::image::draw_point(image_ptr, image_height, image_width, DL_MAX(prediction->keypoint[2], 0), DL_MAX(prediction->keypoint[3], 0), 4, 0x0000FF); // Left corner of the mouth / 嘴角左侧
            dl::image::draw_point(image_ptr, image_height, image_width, DL_MAX(prediction->keypoint[4], 0), DL_MAX(prediction->keypoint[5], 0), 4, 0x00FF00); // Nose / 鼻子
            dl::image::draw_point(image_ptr, image_height, image_width, DL_MAX(prediction->keypoint[6], 0), DL_MAX(prediction->keypoint[7], 0), 4, 0xFF0000); // Right eye / 右眼
            dl::image::draw_point(image_ptr, image_height, image_width, DL_MAX(prediction->keypoint[8], 0), DL_MAX(prediction->keypoint[9], 0), 4, 0xFF0000); // Right corner of the mouth / 嘴角右侧
        }
    }
}


uint8_t result_data[64] = {0};

char llx[8] = {0};
char lly[8] = {0};
char rrx[8] = {0};
char rry[8] = {0};

// Format a nonnegative coordinate as three digits (for example, 074 or 150) to prevent buffer overflow / 将非负坐标格式化为 3 位数字（如 074、150），防止缓冲区溢出
static void ccinttostr(int num, char *buf)
{
    if (num < 0) num = 0;
    if (num > 999) num = 999;
    sprintf(buf, "%03d", num);
}


void print_detection_result(std::list<dl::detect::result_t> &results)
{
    int i = 0;
    for (std::list<dl::detect::result_t>::iterator prediction = results.begin(); prediction != results.end(); prediction++, i++)
    {
        ESP_LOGI("detection_result", "Target[%d] (Human Face, Score: %.2f): Box=(%3d, %3d, %3d, %3d)",
                 i, prediction->score, prediction->box[0], prediction->box[1], prediction->box[2], prediction->box[3]);
        
        ccinttostr(prediction->box[0],llx);
        ccinttostr(prediction->box[1],lly);
        ccinttostr(prediction->box[2],rrx);
        ccinttostr(prediction->box[3],rry);

        sprintf((char*)result_data,"$%s,%s,%s,%s,#",llx, lly, rrx, rry); // Top-left and bottom-right coordinates / 左上角和右下角坐标
        Uart_Send_Data(result_data,strlen((char*)result_data)); 

        // Synchronize the result over UART1 / 通过 UART1 同步结果
        Uart1_Send_Data(result_data,strlen((char*)result_data)); 

        // Also expose the recognition result over I2C / 同时通过 I2C 提供识别结果
        set_IIC_data(prediction->box[0], prediction->box[1], prediction->box[2], prediction->box[3]);

        if (prediction->keypoint.size() == 10)
        {
            ESP_LOGI("detection_result", "      [Keypoints] LeftEye:(%3d, %3d), RightEye:(%3d, %3d), Nose:(%3d, %3d), MouthL:(%3d, %3d), MouthR:(%3d, %3d)",
                     prediction->keypoint[0], prediction->keypoint[1],  // Left eye / 左眼
                     prediction->keypoint[6], prediction->keypoint[7],  // Right eye / 右眼
                     prediction->keypoint[4], prediction->keypoint[5],  // Nose / 鼻子
                     prediction->keypoint[2], prediction->keypoint[3],  // Left corner of the mouth / 嘴角左侧
                     prediction->keypoint[8], prediction->keypoint[9]); // Right corner of the mouth / 嘴角右侧
        }
    }
}

void *app_camera_decode(camera_fb_t *fb)
{
    if (fb->format == PIXFORMAT_RGB565)
    {
        return (void *)fb->buf;
    }
    else
    {
        uint8_t *image_ptr = (uint8_t *)malloc(fb->height * fb->width * 3 * sizeof(uint8_t));
        if (image_ptr)
        {
            if (fmt2rgb888(fb->buf, fb->len, fb->format, image_ptr))
            {
                return (void *)image_ptr;
            }
            else
            {
                ESP_LOGE(TAG, "fmt2rgb888 failed");
                dl::tool::free_aligned(image_ptr);
            }
        }
        else
        {
            ESP_LOGE(TAG, "malloc memory for image rgb888 failed");
        }
    }
    return NULL;
}