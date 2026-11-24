#include "s3_cam_cat_face_detection.hpp"
#include "esp_log.h"
#include "esp_camera.h"
#include "dl_image.hpp"
#include "fb_gfx.h"
#include "cat_face_detect_mn03.hpp"
#include "s3_cam_ai_utils.hpp"
#include "my_user_iic.h"
#include "my_usart.h"
#include "my_usart1_user.h"
#include <cstring>
#include <cmath>
#include <stdlib.h>
#include "s3_cam_camera.h"
#include "img_converters.h"

static const char *TAG = "cat_face_detection";

static QueueHandle_t xQueueFrameI = NULL;
static QueueHandle_t xQueueEvent = NULL;
static QueueHandle_t xQueueFrameO = NULL;
static QueueHandle_t xQueueResult = NULL;

static bool gEvent = true;
static bool gReturnFB = true;

static void task_process_handler(void *arg)
{
    camera_fb_t *frame = NULL;
    CatFaceDetectMN03 detector(0.4F, 0.3F, 10, 0.3F);

    while (true)
    {
        if (gEvent)
        {
            bool is_detected = false;
            if (xQueueReceive(xQueueFrameI, &frame, portMAX_DELAY))
            {
                std::list<dl::detect::result_t> &detect_results = detector.infer((uint16_t *)frame->buf, {(int)frame->height, (int)frame->width, 3});
                if (detect_results.size() > 0)
                {
                    draw_detection_result((uint16_t *)frame->buf, frame->height, frame->width, detect_results);
                    for (auto &pred : detect_results)
                    {
                        const char *label = "Cat/Dog";
                        int text_w = strlen(label) * 14;
                        int lx = DL_MAX(pred.box[0], 2);
                        if (lx + text_w > (int)frame->width) lx = DL_MAX(2, (int)frame->width - text_w - 2);
                        int ly = (pred.box[1] >= 22) ? (pred.box[1] - 4) : DL_MIN((int)frame->height - 6, pred.box[3] + 20);
                        fb_gfx_print(frame, lx, ly, 0x07E0, label);
                    }
                    print_detection_result(detect_results);

                    i2c_target_box_t targets[10];
                    uint8_t count = 0;
                    for (auto &pred : detect_results)
                    {
                        if (count < 10) {
                            targets[count].target_type = (uint8_t)(count + 1); // Target index from 1 to N; this is a position, not a class / 目标位置索引，范围为 1..N，不表示类别
                            targets[count].x = (uint16_t)DL_MAX(pred.box[0], 0);
                            targets[count].y = (uint16_t)DL_MAX(pred.box[1], 0);
                            targets[count].w = (uint16_t)DL_MAX(pred.box[2] - pred.box[0], 0);
                            targets[count].h = (uint16_t)DL_MAX(pred.box[3] - pred.box[1], 0);
                            count++;
                        }
                    }
                    i2c_proto_set_cat_dog_result(targets, count);

                    is_detected = true;
                }
                else
                {
                    char nodatabuff[24];
                    snprintf(nodatabuff, sizeof(nodatabuff), "$000,000,%03d,%03d,#", (int)frame->width, (int)frame->height);
                    Uart_Send_Data((uint8_t*)nodatabuff, strlen(nodatabuff));
                    Uart1_Send_Data((uint8_t*)nodatabuff, strlen(nodatabuff));
                    set_IIC_data(0, 0, (int)frame->width, (int)frame->height);
                    i2c_proto_set_cat_dog_result(NULL, 0);
                }
            }

            if (frame)  // frame may have been returned early by snapshot path
            {
                if (xQueueFrameO)
                {
                    int fw = (int)frame->width;
                    int fh = (int)frame->height;
                    uint8_t *jpg_buf = NULL;
                    size_t jpg_len = 0;
                    uint8_t jpg_q = app_camera_get_quality();
                    if (fw >= 640 && jpg_q > 90) jpg_q = 90;
                    else if (jpg_q > 95) jpg_q = 95;

                    // Compress in AI task (Core 1); stream handler gets PIXFORMAT_JPEG and skips frame2jpg.
                    bool ok = frame2jpg(frame, jpg_q, &jpg_buf, &jpg_len);

                    // Return raw framebuffer immediately so camera DMA can refill it.
                    esp_camera_fb_return(frame);
                    frame = NULL;

                    if (ok && jpg_buf)
                    {
                        camera_fb_t *jfb = (camera_fb_t *)malloc(sizeof(camera_fb_t));
                        if (jfb)
                        {
                            memset(jfb, 0, sizeof(camera_fb_t));
                            jfb->buf = jpg_buf;
                            jfb->len = jpg_len;
                            jfb->format = PIXFORMAT_JPEG;
                            jfb->width = fw;
                            jfb->height = fh;

                            camera_fb_t *old_jfb = NULL;
                            if (xQueueSend(xQueueFrameO, &jfb, 0) != pdTRUE)
                            {
                                if (xQueueReceive(xQueueFrameO, &old_jfb, 0) == pdTRUE && old_jfb)
                                {
                                    free(old_jfb->buf);
                                    free(old_jfb);
                                }
                                if (xQueueSend(xQueueFrameO, &jfb, 0) != pdTRUE)
                                {
                                    free(jfb->buf);
                                    free(jfb);
                                }
                            }
                        }
                        else
                        {
                            free(jpg_buf);
                        }
                    }
                }
                else
                {
                    esp_camera_fb_return(frame);
                    frame = NULL;
                }
            }


            if (xQueueResult)
            {
                xQueueSend(xQueueResult, &is_detected, portMAX_DELAY);
            }
        }
    }
}

static void task_event_handler(void *arg)
{
    while (true)
    {
        xQueueReceive(xQueueEvent, &(gEvent), portMAX_DELAY);
    }
}

void register_cat_face_detection(const QueueHandle_t frame_i,
                                 const QueueHandle_t event,
                                 const QueueHandle_t result,
                                 const QueueHandle_t frame_o,
                                 const bool camera_fb_return)
{
    xQueueFrameI = frame_i;
    xQueueFrameO = frame_o;
    xQueueEvent = event;
    xQueueResult = result;
    gReturnFB = camera_fb_return;

    xTaskCreatePinnedToCore(task_process_handler, "cat_face_process", 3 * 1024, NULL, 5, NULL, 1);
    if (xQueueEvent)
        xTaskCreatePinnedToCore(task_event_handler, "cat_face_event", 1 * 1024, NULL, 5, NULL, 1);
}
