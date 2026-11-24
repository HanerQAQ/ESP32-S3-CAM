#include "s3_cam_human_face_detection.hpp"

#include "esp_log.h"
#include "esp_camera.h"
#include "my_usart.h"
#include "dl_image.hpp"
#include "fb_gfx.h"
#include "human_face_detect_msr01.hpp"
#include "human_face_detect_mnp01.hpp"

#include "s3_cam_ai_utils.hpp"
#include "my_user_iic.h"
#include "my_usart1_user.h"
#include <stdlib.h>
#include "s3_cam_camera.h"
#include "img_converters.h"

#define TWO_STAGE_ON 1 // Print the eyes, nose, and other facial landmarks / 打印眼睛、鼻子等关键点

static const char *TAG = "human_face_detection";

static QueueHandle_t xQueueFrameI = NULL;
static QueueHandle_t xQueueEvent = NULL;
static QueueHandle_t xQueueFrameO = NULL;
static QueueHandle_t xQueueResult = NULL;

static bool gEvent = true;
static bool gReturnFB = true;

char nodatabuff[20]={'\0'};

static void task_process_handler(void *arg)
{
    camera_fb_t *frame = NULL;
    HumanFaceDetectMSR01 detector(0.3F, 0.3F, 10, 0.3F);
#if TWO_STAGE_ON
    HumanFaceDetectMNP01 detector2(0.4F, 0.3F, 10);
#endif

    while (true)
    {
        if (gEvent)
        {
            bool is_detected = false;
            if (xQueueReceive(xQueueFrameI, &frame, portMAX_DELAY))
            {
#if TWO_STAGE_ON
                std::list<dl::detect::result_t> &detect_candidates = detector.infer((uint16_t *)frame->buf, {(int)frame->height, (int)frame->width, 3});
                std::list<dl::detect::result_t> &detect_results = detector2.infer((uint16_t *)frame->buf, {(int)frame->height, (int)frame->width, 3}, detect_candidates);
#else
                std::list<dl::detect::result_t> &detect_results = detector.infer((uint16_t *)frame->buf, {(int)frame->height, (int)frame->width, 3});
#endif

                if (detect_results.size() > 0)
                {
                    draw_detection_result((uint16_t *)frame->buf, frame->height, frame->width, detect_results);
                    for (auto &pred : detect_results)
                    {
                        int lx = DL_MAX(pred.box[0], 2);
                        if (lx + 4 * 14 > (int)frame->width) lx = DL_MAX(2, (int)frame->width - 4 * 14 - 2);
                        int ly = (pred.box[1] >= 22) ? (pred.box[1] - 4) : DL_MIN((int)frame->height - 6, pred.box[3] + 20);
                        fb_gfx_print(frame, lx, ly, 0x07E0, "Face");
                    }
                    print_detection_result(detect_results);

                    i2c_target_box_t faces[10];
                    uint8_t count = 0;
                    for (auto &pred : detect_results)
                    {
                        if (count < 10) {
                            faces[count].target_type = count + 1; // Face index from 1 to N / 人脸序号，范围为 1..N
                            faces[count].x = (uint16_t)DL_MAX(pred.box[0], 0);
                            faces[count].y = (uint16_t)DL_MAX(pred.box[1], 0);
                            faces[count].w = (uint16_t)DL_MAX(pred.box[2] - pred.box[0], 0);
                            faces[count].h = (uint16_t)DL_MAX(pred.box[3] - pred.box[1], 0);
                            count++;
                        }
                    }
                    i2c_proto_set_face_detect_result(faces, count);

                    is_detected = true;
                }
                else
                {
                    // No face was detected / 未检测到人脸
                    char nodatabuff[24];
                    snprintf(nodatabuff, sizeof(nodatabuff), "$000,000,%03d,%03d,#", (int)frame->width, (int)frame->height);
                    Uart_Send_Data((uint8_t*)nodatabuff, strlen(nodatabuff)); 
                    // Synchronize the result over UART1 / 通过 UART1 同步结果
                    Uart1_Send_Data((uint8_t*)nodatabuff, strlen(nodatabuff)); 

                    // Synchronize the result over I2C / 通过 I2C 同步结果
                    set_IIC_data(0, 0, (int)frame->width, (int)frame->height);
                    i2c_proto_set_face_detect_result(NULL, 0);

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
                xQueueSend(xQueueResult, &is_detected, 0);
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

void register_human_face_detection(const QueueHandle_t frame_i,
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

    xTaskCreatePinnedToCore(task_process_handler, TAG, 4 * 1024, NULL, 5, NULL, 1);
    if (xQueueEvent)
        xTaskCreatePinnedToCore(task_event_handler, TAG, 4 * 1024, NULL, 5, NULL, 0);
}
