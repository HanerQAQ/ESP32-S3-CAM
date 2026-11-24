#include "s3_cam_human_face_recognition.hpp"
#include "my_usart.h"
#include "my_usart1_user.h"
#include "esp_log.h"
#include "esp_camera.h"

#include "dl_image.hpp"
#include "fb_gfx.h"

#include "human_face_detect_msr01.hpp"
#include "human_face_detect_mnp01.hpp"
#include "face_recognition_tool.hpp"
#include "my_user_iic.h"


#if CONFIG_MFN_V1
#if CONFIG_S8
#include "face_recognition_112_v1_s8.hpp"
#elif CONFIG_S16
#include "face_recognition_112_v1_s16.hpp"
#endif
#endif

#include "s3_cam_ai_utils.hpp"
#include <stdlib.h>
#include "s3_cam_camera.h"
#include "img_converters.h"

using namespace std;
using namespace dl;

static const char *TAG = "human_face_recognition";

static QueueHandle_t xQueueFrameI = NULL;
static QueueHandle_t xQueueEvent = NULL;
static QueueHandle_t xQueueFrameO = NULL;
static QueueHandle_t xQueueResult = NULL;

static recognizer_state_t gEvent = DETECT;
static bool gReturnFB = true;
static face_info_t recognize_result;

SemaphoreHandle_t xMutex;

typedef enum
{
    SHOW_STATE_IDLE,
    SHOW_STATE_DELETE,
    SHOW_STATE_RECOGNIZE,
    SHOW_STATE_ENROLL,
    SHOW_STATE_NO_ID,
} show_state_t;

#define RGB565_MASK_RED 0xF800
#define RGB565_MASK_GREEN 0x07E0
#define RGB565_MASK_BLUE 0x001F
#define FRAME_DELAY_NUM 16

static void rgb_print(camera_fb_t *fb, uint32_t color, const char *str)
{
    int x = ((int)fb->width - (int)(strlen(str) * 14)) / 2;
    if (x < 2) x = 2;
    fb_gfx_print(fb, x, 10, color, str);
}

static int rgb_printf(camera_fb_t *fb, uint32_t color, const char *format, ...)
{
    char loc_buf[64];
    char *temp = loc_buf;
    int len;
    va_list arg;
    va_list copy;
    va_start(arg, format);
    va_copy(copy, arg);
    len = vsnprintf(loc_buf, sizeof(loc_buf), format, arg);
    va_end(copy);
    if (len >= sizeof(loc_buf))
    {
        temp = (char *)malloc(len + 1);
        if (temp == NULL)
        {
            return 0;
        }
    }
    vsnprintf(temp, len + 1, format, arg);
    va_end(arg);
    rgb_print(fb, color, temp);
    if (len > 64)
    {
        free(temp);
    }
    return len;
}

static void task_process_handler(void *arg)
{
    camera_fb_t *frame = NULL;
    HumanFaceDetectMSR01 detector(0.3F, 0.3F, 10, 0.3F);
    HumanFaceDetectMNP01 detector2(0.4F, 0.3F, 10);

#if CONFIG_MFN_V1
#if CONFIG_S8
    FaceRecognition112V1S8 *recognizer = new FaceRecognition112V1S8();
#elif CONFIG_S16
    FaceRecognition112V1S16 *recognizer = new FaceRecognition112V1S16();
#endif
#endif
    show_state_t frame_show_state = SHOW_STATE_IDLE;
    recognizer_state_t _gEvent;
    recognizer->set_partition((esp_partition_type_t)32, (esp_partition_subtype_t)32, "fr");
    int partition_result = recognizer->set_ids_from_flash();
    (void)partition_result;
    i2c_proto_set_face_enrolled_count((uint8_t)recognizer->get_enrolled_id_num());

    auto handle_face_delete = [&]() {
        int32_t target_id = i2c_proto_get_face_delete_target();
        bool ok = false;
        vTaskDelay(10);
        if (target_id <= 0)
        {
            recognizer->clear_id(true);
            ok = true;
            ESP_LOGW("DELETE", "Cleared all face IDs, %d IDs left", recognizer->get_enrolled_id_num());
        }
        else
        {
            int rem = recognizer->delete_id((int)target_id, true);
            ok = (rem >= 0);
            ESP_LOGW("DELETE", "Delete face ID %d -> %s (%d IDs left)",
                     (int)target_id, ok ? "ok" : "not_found", recognizer->get_enrolled_id_num());
        }
        i2c_proto_set_face_enrolled_count((uint8_t)recognizer->get_enrolled_id_num());
        i2c_proto_set_face_delete_result(ok);
        frame_show_state = SHOW_STATE_DELETE;
    };

    while (true)
    {
        uint8_t face_data[15]="\0";
        xSemaphoreTake(xMutex, portMAX_DELAY);
        _gEvent = gEvent;
        gEvent = DETECT;
        xSemaphoreGive(xMutex);

        

        if (_gEvent)
        {
            bool is_detected = false;

            if (xQueueReceive(xQueueFrameI, &frame, portMAX_DELAY))
            {
                std::list<dl::detect::result_t> &detect_candidates = detector.infer((uint16_t *)frame->buf, {(int)frame->height, (int)frame->width, 3});
                std::list<dl::detect::result_t> &detect_results = detector2.infer((uint16_t *)frame->buf, {(int)frame->height, (int)frame->width, 3}, detect_candidates);

                if (detect_results.size() == 1)
                    is_detected = true;

                if (is_detected)
                {
                    auto &box = detect_results.front().box;
                    int cx = (box[2] - box[0]) / 2 + box[0];
                    int cy = (box[3] - box[1]) / 2 + box[1];
                    int w = box[2] - box[0];
                    int h = box[3] - box[1];
                    set_IIC_face_data(cx, cy, w, h);
                    set_IIC_data(box[0], box[1], box[2], box[3]);

                    int enrolled_count = recognizer->get_enrolled_id_num();
                    if (_gEvent == ENROLL)
                    {
                        recognizer->enroll_id((uint16_t *)frame->buf, {(int)frame->height, (int)frame->width, 3}, detect_results.front().keypoint, "", true);
                        uint16_t new_id = recognizer->get_enrolled_ids().empty() ? 1 : recognizer->get_enrolled_ids().back().id;
                        ESP_LOGW("ENROLL", "ID %d is enrolled, total: %d", new_id, recognizer->get_enrolled_id_num());
                        i2c_proto_set_register_result(true, new_id);
                        i2c_proto_set_face_enrolled_count((uint8_t)recognizer->get_enrolled_id_num());
                        frame_show_state = SHOW_STATE_ENROLL;
                    }
                    else if (_gEvent == DELETE)
                    {
                        handle_face_delete();
                    }
                    else // This branch handles DETECT and RECOGNIZE events / 此分支处理 DETECT 和 RECOGNIZE 事件
                    {
                        if (enrolled_count > 0)
                        {
                            recognize_result = recognizer->recognize((uint16_t *)frame->buf, {(int)frame->height, (int)frame->width, 3}, detect_results.front().keypoint);
                            if (recognize_result.id > 0)
                            {
                                ESP_LOGI("RECOGNIZE", "Match ID: %d, Similarity: %f", recognize_result.id, recognize_result.similarity);
                                sprintf((char*)face_data,"@ID:%d!\r\n",recognize_result.id);
                                Uart_Send_Data(face_data,strlen((char*)face_data)); 
                                Uart1_Send_Data(face_data,strlen((char*)face_data)); 
                                set_IIC_data_id(recognize_result.id);
                            }
                            else
                            {
                                ESP_LOGW("RECOGNIZE", "Unknown Face (who?), Similarity: %f", recognize_result.similarity);
                                sprintf((char*)face_data,"@ID:-1!\r\n");
                                Uart_Send_Data(face_data,strlen((char*)face_data)); 
                                Uart1_Send_Data(face_data,strlen((char*)face_data)); 
                                set_IIC_data_id(-1);
                            }
                            frame_show_state = SHOW_STATE_RECOGNIZE;
                        }
                        else
                        {
                            set_IIC_data_id(0);
                            frame_show_state = SHOW_STATE_NO_ID;
                        }

                        i2c_face_recog_target_t rtarget;
                        rtarget.face_id = (recognize_result.id > 0) ? (uint16_t)recognize_result.id : 0;
                        rtarget.x = (uint16_t)DL_MAX(box[0], 0);
                        rtarget.y = (uint16_t)DL_MAX(box[1], 0);
                        rtarget.w = (uint16_t)DL_MAX(box[2] - box[0], 0);
                        rtarget.h = (uint16_t)DL_MAX(box[3] - box[1], 0);
                        i2c_proto_set_face_recog_result(&rtarget, 1);
                        i2c_proto_set_face_enrolled_count((uint8_t)enrolled_count);
                    }
                }
                else
                {
                    set_IIC_data(0, 0, (int)frame->width, (int)frame->height);
                    set_IIC_data_id(0);
                    i2c_proto_set_face_recog_result(NULL, 0);
                    i2c_proto_set_face_enrolled_count((uint8_t)recognizer->get_enrolled_id_num());
                    if (_gEvent == ENROLL)
                    {
                        i2c_proto_set_register_result(false, 0);
                    }
                    else if (_gEvent == DELETE)
                    {
                        handle_face_delete();
                    }
                    else if (frame_show_state == SHOW_STATE_RECOGNIZE || frame_show_state == SHOW_STATE_NO_ID)
                    {
                        frame_show_state = SHOW_STATE_IDLE;
                    }
                }

                if (frame_show_state != SHOW_STATE_IDLE)
                {
                    static int frame_count = 0;
                    switch (frame_show_state)
                    {
                    case SHOW_STATE_DELETE:
                        rgb_printf(frame, RGB565_MASK_RED, "%d IDs left", recognizer->get_enrolled_id_num());
                        break;

                    case SHOW_STATE_RECOGNIZE:
                        if (is_detected && detect_results.size())
                        {
                            auto &box = detect_results.front().box;
                            if (recognize_result.id > 0)
                            {
                                dl::image::draw_hollow_rectangle((uint16_t *)frame->buf, frame->height, frame->width,
                                                                 DL_MAX(box[0], 0), DL_MAX(box[1], 0),
                                                                 DL_MAX(box[2], 0), DL_MAX(box[3], 0),
                                                                 0b1110000000000111);
                                rgb_printf(frame, RGB565_MASK_GREEN, "ID %d", recognize_result.id);
                            }
                            else
                            {
                                dl::image::draw_hollow_rectangle((uint16_t *)frame->buf, frame->height, frame->width,
                                                                 DL_MAX(box[0], 0), DL_MAX(box[1], 0),
                                                                 DL_MAX(box[2], 0), DL_MAX(box[3], 0),
                                                                 0b0000000011111000);
                                rgb_print(frame, RGB565_MASK_RED, "who ?");
                            }
                        }
                        break;

                    case SHOW_STATE_ENROLL:
                        if (is_detected && detect_results.size())
                        {
                            auto &box = detect_results.front().box;
                            dl::image::draw_hollow_rectangle((uint16_t *)frame->buf, frame->height, frame->width,
                                                             DL_MAX(box[0], 0), DL_MAX(box[1], 0),
                                                             DL_MAX(box[2], 0), DL_MAX(box[3], 0),
                                                             0b0001111100000000);
                            rgb_printf(frame, RGB565_MASK_BLUE, "Enroll: ID %d", recognizer->get_enrolled_ids().back().id);
                        }
                        break;

                    case SHOW_STATE_NO_ID:
                        rgb_print(frame, RGB565_MASK_RED, "No ID (Press BOOT)");
                        break;

                    default:
                        break;
                    }

                    if (frame_show_state == SHOW_STATE_ENROLL || frame_show_state == SHOW_STATE_DELETE)
                    {
                        if (++frame_count > FRAME_DELAY_NUM)
                        {
                            frame_count = 0;
                            frame_show_state = SHOW_STATE_IDLE;
                        }
                    }
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


            if (xQueueResult && is_detected)
            {
                xQueueSend(xQueueResult, &recognize_result, 0);
            }
        }
    }
}

static void task_event_handler(void *arg)
{
    recognizer_state_t _gEvent;
    while (true)
    {
        xQueueReceive(xQueueEvent, &(_gEvent), portMAX_DELAY);
        xSemaphoreTake(xMutex, portMAX_DELAY);
        gEvent = _gEvent;
        xSemaphoreGive(xMutex);
    }
}

void register_human_face_recognition(const QueueHandle_t frame_i,
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
    xMutex = xSemaphoreCreateMutex();

    xTaskCreatePinnedToCore(task_process_handler, TAG, 4 * 1024, NULL, 5, NULL, 1);// Run the task on Core 1 / 任务运行在 Core 1
    if (xQueueEvent)
        xTaskCreatePinnedToCore(task_event_handler, TAG, 4 * 1024, NULL, 5, NULL, 1);//5
}
