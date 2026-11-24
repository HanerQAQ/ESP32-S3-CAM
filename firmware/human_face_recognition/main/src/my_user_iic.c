#include <stdio.h>
#include <string.h>
#include "cJSON.h"
#include "my_user_iic.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "driver/gpio.h"
#include "hal/i2c_ll.h"
#include "my_usart.h"
#include "s3_cam_camera.h"

static const char *TAG = "MY_IIC";

static QueueHandle_t xQueuevirtualKeystate = NULL;
static SemaphoreHandle_t s_i2c_mutex = NULL;

// Current project type (cat/dog detection by default) / 当前项目类型（默认猫狗检测）
static i2c_project_type_t s_project_type = I2C_PROJ_CAT_DOG;

// 1. Store cat and dog detection results / 1. 存储猫狗检测结果
#define MAX_TARGETS 10
static i2c_target_box_t s_cat_dog_targets[MAX_TARGETS];
static uint8_t s_cat_dog_count = 0;

// 2. Store color-detection results / 2. 存储颜色识别结果
static uint8_t s_color_mode = 0; // 0: color detection, 1: line following / 0：颜色识别，1：巡线
static uint8_t s_color_targets[5] = {1, 2, 3, 4, 5};
static uint8_t s_color_target_count = 5;
static i2c_target_box_t s_color_blocks[MAX_TARGETS];
static uint8_t s_color_block_count = 0;
static bool s_line_detected = false;
static uint16_t s_line_sx = 0, s_line_sy = 0, s_line_ex = 0, s_line_ey = 0;
static float s_line_angle = 0.0f, s_line_offset = 0.0f;

// 3. Store face-detection results / 3. 存储人脸检测结果
static i2c_target_box_t s_face_detect_faces[MAX_TARGETS];
static uint8_t s_face_detect_count = 0;

// 4. Store face-recognition results / 4. 存储人脸识别结果
static i2c_face_recog_target_t s_face_recog_targets[MAX_TARGETS];
static uint8_t s_face_recog_count = 0;
static uint8_t s_face_enrolled_count = 0;
static bool s_last_register_done = false;
static bool s_last_register_success = false;
static uint16_t s_last_registered_id = 0;
static int32_t s_face_delete_target = 0;
static bool s_face_delete_done = false;
static bool s_face_delete_success = false;

// 5. Store QR-code detection results / 5. 存储二维码识别结果
static bool s_qr_has_result = false;
static uint16_t s_qr_x = 0, s_qr_y = 0, s_qr_w = 0, s_qr_h = 0;
static char s_qr_str[256];
static uint16_t s_qr_str_len = 0;

// Build the transmit frame (32 bytes per frame, up to 32 fragments or 1024 bytes) / 组装发送帧（每帧 32 字节，最多 32 个分片，共 1024 字节）
static uint8_t s_tx_frames[1024];
static int s_tx_frames_len = 0;

void i2c_proto_set_project_type(i2c_project_type_t proj_type)
{
    if (s_i2c_mutex) xSemaphoreTake(s_i2c_mutex, portMAX_DELAY);
    s_project_type = proj_type;
    if (s_i2c_mutex) xSemaphoreGive(s_i2c_mutex);
}

// -----------------------------------------------------------------------------
// Assemble protocol fragments according to chapter 1 of Protocol.md / 按照《协议.md》第一章组装协议分片
// -----------------------------------------------------------------------------
static void i2c_build_response_frames(const uint8_t *payload, uint16_t payload_len, uint8_t *out_buf, int *out_len)
{
    uint16_t actual_len = (payload_len == 0) ? 1 : payload_len;
    uint8_t dummy = 0;
    const uint8_t *p_data = (payload_len == 0) ? &dummy : payload;

    // Limit DATA to 22 bytes per frame and reserve a zero padding byte to prevent boundary conflicts and packet concatenation / 单帧 DATA 最多 22 字节，并预留一个零填充字节，避免边界冲突和粘包
    uint16_t total_fragments = (actual_len + 21) / 22;
    if (total_fragments == 0) total_fragments = 1;

    *out_len = total_fragments * 32;

    for (uint16_t f = 1; f <= total_fragments; f++)
    {
        uint8_t *frame = out_buf + (f - 1) * 32;
        uint16_t offset = (f - 1) * 22;
        uint16_t chunk_len = (actual_len - offset > 22) ? 22 : (actual_len - offset);

        // Bytes 0–1: frame header 0xAA 0x55 / 偏移 0–1：帧头 0xAA 0x55
        frame[0] = 0xAA;
        frame[1] = 0x55;
        // Bytes 2–3: data_len (little-endian) / 偏移 2–3：data_len（小端序）
        frame[2] = (uint8_t)(chunk_len & 0xFF);
        frame[3] = (uint8_t)((chunk_len >> 8) & 0xFF);
        // Bytes 4–5: fragment_id (little-endian, starting at 1) / 偏移 4–5：fragment_id（小端序，从 1 开始）
        frame[4] = (uint8_t)(f & 0xFF);
        frame[5] = (uint8_t)((f >> 8) & 0xFF);
        // Bytes 6–7: total_fragments (little-endian) / 偏移 6–7：total_fragments（小端序）
        frame[6] = (uint8_t)(total_fragments & 0xFF);
        frame[7] = (uint8_t)((total_fragments >> 8) & 0xFF);

        // Bytes 8 through 8+chunk_len-1: DATA / 偏移 8 至 8+chunk_len-1：DATA
        memcpy(&frame[8], p_data + offset, chunk_len);

        // Byte 8+chunk_len: CHECKSUM (byte-wise XOR) / 偏移 8+chunk_len：CHECKSUM（逐字节异或）
        uint8_t checksum = 0;
        for (size_t i = 0; i < chunk_len; i++)
        {
            checksum ^= frame[8 + i];
        }
        frame[8 + chunk_len] = checksum;

        // Pad remaining bytes with zeros to a fixed 32-byte frame / 剩余字节补零，将帧长度固定为 32 字节
        if (8 + chunk_len + 1 < 32)
        {
            memset(&frame[8 + chunk_len + 1], 0, 32 - (8 + chunk_len + 1));
        }
    }
}

// -----------------------------------------------------------------------------
// Data update APIs / 数据更新接口
// -----------------------------------------------------------------------------
void i2c_proto_set_cat_dog_result(const i2c_target_box_t *targets, uint8_t count)
{
    if (!s_i2c_mutex) return;
    xSemaphoreTake(s_i2c_mutex, portMAX_DELAY);
    if (count > MAX_TARGETS) count = MAX_TARGETS;
    s_cat_dog_count = count;
    if (targets && count > 0) {
        memcpy(s_cat_dog_targets, targets, count * sizeof(i2c_target_box_t));
    }
    xSemaphoreGive(s_i2c_mutex);
}

void i2c_proto_set_color_result(const i2c_target_box_t *blocks, uint8_t count)
{
    if (!s_i2c_mutex) return;
    xSemaphoreTake(s_i2c_mutex, portMAX_DELAY);
    if (count > MAX_TARGETS) count = MAX_TARGETS;
    s_color_block_count = count;
    if (blocks && count > 0) {
        memcpy(s_color_blocks, blocks, count * sizeof(i2c_target_box_t));
    }
    xSemaphoreGive(s_i2c_mutex);
}

void i2c_proto_set_line_result(bool detected, uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, float angle, float offset)
{
    if (!s_i2c_mutex) return;
    xSemaphoreTake(s_i2c_mutex, portMAX_DELAY);
    s_line_detected = detected;
    s_line_sx = sx; s_line_sy = sy;
    s_line_ex = ex; s_line_ey = ey;
    s_line_angle = angle;
    s_line_offset = offset;
    xSemaphoreGive(s_i2c_mutex);
}

void i2c_proto_get_color_config(uint8_t *out_mode, uint8_t *out_targets, uint8_t *out_count)
{
    if (!s_i2c_mutex) return;
    xSemaphoreTake(s_i2c_mutex, portMAX_DELAY);
    if (out_mode) *out_mode = s_color_mode;
    if (out_count) *out_count = s_color_target_count;
    if (out_targets) memcpy(out_targets, s_color_targets, s_color_target_count);
    xSemaphoreGive(s_i2c_mutex);
}

void i2c_proto_set_face_detect_result(const i2c_target_box_t *faces, uint8_t count)
{
    if (!s_i2c_mutex) return;
    xSemaphoreTake(s_i2c_mutex, portMAX_DELAY);
    if (count > MAX_TARGETS) count = MAX_TARGETS;
    s_face_detect_count = count;
    if (faces && count > 0) {
        memcpy(s_face_detect_faces, faces, count * sizeof(i2c_target_box_t));
    }
    xSemaphoreGive(s_i2c_mutex);
}

void i2c_proto_set_face_recog_result(const i2c_face_recog_target_t *faces, uint8_t count)
{
    if (!s_i2c_mutex) return;
    xSemaphoreTake(s_i2c_mutex, portMAX_DELAY);
    if (count > MAX_TARGETS) count = MAX_TARGETS;
    s_face_recog_count = count;
    if (faces && count > 0) {
        memcpy(s_face_recog_targets, faces, count * sizeof(i2c_face_recog_target_t));
    }
    xSemaphoreGive(s_i2c_mutex);
}

void i2c_proto_set_face_enrolled_count(uint8_t count)
{
    if (!s_i2c_mutex) return;
    xSemaphoreTake(s_i2c_mutex, portMAX_DELAY);
    s_face_enrolled_count = count;
    xSemaphoreGive(s_i2c_mutex);
}

void i2c_proto_set_register_result(bool success, uint16_t new_id)
{
    if (!s_i2c_mutex) return;
    xSemaphoreTake(s_i2c_mutex, portMAX_DELAY);
    s_last_register_success = success;
    s_last_registered_id = new_id;
    s_last_register_done = true;
    xSemaphoreGive(s_i2c_mutex);
}

void i2c_proto_set_face_delete_target(int32_t target_id)
{
    if (!s_i2c_mutex) return;
    xSemaphoreTake(s_i2c_mutex, portMAX_DELAY);
    s_face_delete_target = target_id;
    xSemaphoreGive(s_i2c_mutex);
}

int32_t i2c_proto_get_face_delete_target(void)
{
    if (!s_i2c_mutex) return 0;
    xSemaphoreTake(s_i2c_mutex, portMAX_DELAY);
    int32_t target = s_face_delete_target;
    xSemaphoreGive(s_i2c_mutex);
    return target;
}

void i2c_proto_set_face_delete_result(bool success)
{
    if (!s_i2c_mutex) return;
    xSemaphoreTake(s_i2c_mutex, portMAX_DELAY);
    s_face_delete_success = success;
    s_face_delete_done = true;
    xSemaphoreGive(s_i2c_mutex);
}

void i2c_proto_set_qr_result(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const char *text, uint16_t len)
{
    if (!s_i2c_mutex) return;
    xSemaphoreTake(s_i2c_mutex, portMAX_DELAY);
    s_qr_has_result = true;
    s_qr_x = x; s_qr_y = y; s_qr_w = w; s_qr_h = h;
    if (len > sizeof(s_qr_str) - 1) len = sizeof(s_qr_str) - 1;
    s_qr_str_len = len;
    if (text && len > 0) {
        memcpy(s_qr_str, text, len);
        s_qr_str[len] = '\0';
    }
    xSemaphoreGive(s_i2c_mutex);
}

void i2c_proto_clear_qr_result(void)
{
    if (!s_i2c_mutex) return;
    xSemaphoreTake(s_i2c_mutex, portMAX_DELAY);
    s_qr_has_result = false;
    s_qr_str_len = 0;
    xSemaphoreGive(s_i2c_mutex);
}

// -----------------------------------------------------------------------------
// Implement legacy APIs for backward compatibility / 实现向下兼容的旧接口
// -----------------------------------------------------------------------------
void set_IIC_face_data(uint8_t cx, uint8_t cy, uint8_t w, uint8_t h)
{
    if (!s_i2c_mutex) return;
    xSemaphoreTake(s_i2c_mutex, portMAX_DELAY);
    uint16_t x = (cx >= w / 2) ? (cx - w / 2) : 0;
    uint16_t y = (cy >= h / 2) ? (cy - h / 2) : 0;
    if (w > 0 && h > 0) {
        s_face_detect_faces[0].target_type = 1;
        s_face_detect_faces[0].x = x; s_face_detect_faces[0].y = y;
        s_face_detect_faces[0].w = w; s_face_detect_faces[0].h = h;
        s_face_detect_count = 1;

        s_face_recog_targets[0].x = x; s_face_recog_targets[0].y = y;
        s_face_recog_targets[0].w = w; s_face_recog_targets[0].h = h;
        s_face_recog_count = 1;
    } else {
        s_face_detect_count = 0;
        s_face_recog_count = 0;
    }
    xSemaphoreGive(s_i2c_mutex);
}

void set_IIC_color_data(int color_idx, uint8_t cx, uint8_t cy, uint8_t w, uint8_t h)
{
    if (color_idx < 0 || color_idx >= 5 || !s_i2c_mutex) return;
    xSemaphoreTake(s_i2c_mutex, portMAX_DELAY);
    uint16_t x = (cx >= w / 2) ? (cx - w / 2) : 0;
    uint16_t y = (cy >= h / 2) ? (cy - h / 2) : 0;
    if (w > 0 && h > 0) {
        s_color_blocks[0].target_type = (uint8_t)(color_idx + 1);
        s_color_blocks[0].x = x; s_color_blocks[0].y = y;
        s_color_blocks[0].w = w; s_color_blocks[0].h = h;
        s_color_block_count = 1;
    } else {
        s_color_block_count = 0;
    }
    xSemaphoreGive(s_i2c_mutex);
}

void set_IIC_data(int16_t lx, int16_t ly, int16_t rx, int16_t ry)
{
    if (lx < 0 || ly < 0 || rx < 0 || ry < 0 || !s_i2c_mutex) return;
    xSemaphoreTake(s_i2c_mutex, portMAX_DELAY);
    uint16_t w = (rx >= lx) ? (rx - lx) : 0;
    uint16_t h = (ry >= ly) ? (ry - ly) : 0;

    if (w > 0 && h > 0) {
        s_cat_dog_targets[0].target_type = 1; // Default to the cat target / 默认为猫目标
        s_cat_dog_targets[0].x = lx; s_cat_dog_targets[0].y = ly;
        s_cat_dog_targets[0].w = w; s_cat_dog_targets[0].h = h;
        s_cat_dog_count = 1;

        s_face_detect_faces[0].target_type = 1;
        s_face_detect_faces[0].x = lx; s_face_detect_faces[0].y = ly;
        s_face_detect_faces[0].w = w; s_face_detect_faces[0].h = h;
        s_face_detect_count = 1;

        s_face_recog_targets[0].x = lx; s_face_recog_targets[0].y = ly;
        s_face_recog_targets[0].w = w; s_face_recog_targets[0].h = h;
        s_face_recog_count = 1;
    } else {
        s_cat_dog_count = 0;
        s_face_detect_count = 0;
        s_face_recog_count = 0;
    }
    xSemaphoreGive(s_i2c_mutex);
}

void set_IIC_data_id(int16_t temp_id)
{
    if (!s_i2c_mutex) return;
    xSemaphoreTake(s_i2c_mutex, portMAX_DELAY);
    if (temp_id > 0) {
        s_face_recog_targets[0].face_id = (uint16_t)temp_id;
    } else {
        s_face_recog_targets[0].face_id = 0;
    }
    xSemaphoreGive(s_i2c_mutex);
}

void Get_IIC_Key_massage(uint8_t keydata)
{
    if (!xQueuevirtualKeystate) return;
    int k = keydata;
    xQueueSend(xQueuevirtualKeystate, &k, 0);
}

void switch_i2c_control(uint8_t REG_flag, uint8_t data)
{
    sensor_t *s = esp_camera_sensor_get();
    if (REG_flag == 0x01 && s) { // Horizontal image flip / 水平镜像
        s->set_hmirror(s, data ? 1 : 0);
    } else if (REG_flag == 0x02 && s) { // Vertical image flip / 垂直翻转
        s->set_vflip(s, data ? 1 : 0);
    } else if (REG_flag == 0x08) { // Virtual key event / 虚拟按键事件
        Get_IIC_Key_massage(data);
    } else if (REG_flag == 0x04) { // Reset the model / 重置模型
        if (data == 0x01) {
            vTaskDelay(pdMS_TO_TICKS(100));
            esp_restart();
        }
    }
}

void recv_i2c_data(uint8_t REG_flag)
{
    // Preserve backward compatibility / 保留向下兼容
}

int I2C_Send_Data(uint8_t* data, uint16_t len)
{
    return i2c_slave_write_buffer(MYI2C_SLAVE_NUM, data, len, pdMS_TO_TICKS(10));
}

int I2C_Send_Byte(uint8_t data)
{
    return i2c_slave_write_buffer(MYI2C_SLAVE_NUM, &data, 1, pdMS_TO_TICKS(10));
}

// -----------------------------------------------------------------------------
// Assemble the get_info protocol response / 组装 get_info 协议响应
// -----------------------------------------------------------------------------
static uint16_t build_get_info_data(const char *param, uint8_t *out_buf, uint16_t max_len)
{
    uint16_t len = 0;

    // 4. Cat/dog detection: report bounding-box coordinates and size without distinguishing species / 4. 猫狗检测：返回目标框坐标和尺寸，不区分猫狗类别
    if (strcmp(param, "cat_dog") == 0 || strcmp(param, "cat") == 0 || strcmp(param, "dog") == 0 ||
        strcmp(param, "cat_face") == 0 || (strlen(param) == 0 && s_project_type == I2C_PROJ_CAT_DOG))
    {
        if (s_cat_dog_count == 0)
        {
            out_buf[0] = 0;
            len = 1;
        }
        else
        {
            out_buf[0] = s_cat_dog_count;
            len = 1;
            for (uint8_t i = 0; i < s_cat_dog_count && len + 9 <= max_len; i++)
            {
                out_buf[len++] = (uint8_t)(i + 1); // Target index from 1 to N / 目标索引编号，范围为 1..N
                out_buf[len++] = (uint8_t)(s_cat_dog_targets[i].x & 0xFF);
                out_buf[len++] = (uint8_t)((s_cat_dog_targets[i].x >> 8) & 0xFF);
                out_buf[len++] = (uint8_t)(s_cat_dog_targets[i].y & 0xFF);
                out_buf[len++] = (uint8_t)((s_cat_dog_targets[i].y >> 8) & 0xFF);
                out_buf[len++] = (uint8_t)(s_cat_dog_targets[i].w & 0xFF);
                out_buf[len++] = (uint8_t)((s_cat_dog_targets[i].w >> 8) & 0xFF);
                out_buf[len++] = (uint8_t)(s_cat_dog_targets[i].h & 0xFF);
                out_buf[len++] = (uint8_t)((s_cat_dog_targets[i].h >> 8) & 0xFF);
            }
        }
    }
    // 5. Read color-detection configuration / 5. 读取颜色识别配置
    else if (strcmp(param, "color_config") == 0)
    {
        out_buf[0] = s_color_mode;
        for (uint8_t i = 0; i < s_color_target_count && (1 + i) < max_len; i++)
        {
            out_buf[1 + i] = s_color_targets[i];
        }
        len = 1 + s_color_target_count;
    }
    // 5. Read color-detection results / 5. 读取颜色识别结果
    else if (strcmp(param, "color_result") == 0 ||
             (strlen(param) == 0 && s_project_type == I2C_PROJ_COLOR))
    {
        if (s_color_mode == 0)
        {
            if (s_color_block_count == 0)
            {
                out_buf[0] = 0;
                len = 1;
            }
            else
            {
                out_buf[0] = s_color_block_count;
                len = 1;
                for (uint8_t i = 0; i < s_color_block_count && len + 9 <= max_len; i++)
                {
                    out_buf[len++] = s_color_blocks[i].target_type;
                    out_buf[len++] = (uint8_t)(s_color_blocks[i].x & 0xFF);
                    out_buf[len++] = (uint8_t)((s_color_blocks[i].x >> 8) & 0xFF);
                    out_buf[len++] = (uint8_t)(s_color_blocks[i].y & 0xFF);
                    out_buf[len++] = (uint8_t)((s_color_blocks[i].y >> 8) & 0xFF);
                    out_buf[len++] = (uint8_t)(s_color_blocks[i].w & 0xFF);
                    out_buf[len++] = (uint8_t)((s_color_blocks[i].w >> 8) & 0xFF);
                    out_buf[len++] = (uint8_t)(s_color_blocks[i].h & 0xFF);
                    out_buf[len++] = (uint8_t)((s_color_blocks[i].h >> 8) & 0xFF);
                }
            }
        }
        else
        {
            if (!s_line_detected)
            {
                out_buf[0] = 0;
                len = 1;
            }
            else
            {
                out_buf[0] = 1;
                out_buf[1] = (uint8_t)(s_line_sx & 0xFF);
                out_buf[2] = (uint8_t)((s_line_sx >> 8) & 0xFF);
                out_buf[3] = (uint8_t)(s_line_sy & 0xFF);
                out_buf[4] = (uint8_t)((s_line_sy >> 8) & 0xFF);
                out_buf[5] = (uint8_t)(s_line_ex & 0xFF);
                out_buf[6] = (uint8_t)((s_line_ex >> 8) & 0xFF);
                out_buf[7] = (uint8_t)(s_line_ey & 0xFF);
                out_buf[8] = (uint8_t)((s_line_ey >> 8) & 0xFF);
                memcpy(&out_buf[9], &s_line_angle, 4);
                memcpy(&out_buf[13], &s_line_offset, 4);
                len = 17;
            }
        }
    }
    // 6. Face detection / 6. 人脸检测
    else if (strcmp(param, "human") == 0 || strcmp(param, "face") == 0 ||
             (strlen(param) == 0 && s_project_type == I2C_PROJ_FACE_DETECT))
    {
        if (s_face_detect_count == 0)
        {
            out_buf[0] = 0;
            len = 1;
        }
        else
        {
            out_buf[0] = s_face_detect_count;
            len = 1;
            for (uint8_t i = 0; i < s_face_detect_count && len + 9 <= max_len; i++)
            {
                out_buf[len++] = (uint8_t)(i + 1); // Face index / 人脸序号
                out_buf[len++] = (uint8_t)(s_face_detect_faces[i].x & 0xFF);
                out_buf[len++] = (uint8_t)((s_face_detect_faces[i].x >> 8) & 0xFF);
                out_buf[len++] = (uint8_t)(s_face_detect_faces[i].y & 0xFF);
                out_buf[len++] = (uint8_t)((s_face_detect_faces[i].y >> 8) & 0xFF);
                out_buf[len++] = (uint8_t)(s_face_detect_faces[i].w & 0xFF);
                out_buf[len++] = (uint8_t)((s_face_detect_faces[i].w >> 8) & 0xFF);
                out_buf[len++] = (uint8_t)(s_face_detect_faces[i].h & 0xFF);
                out_buf[len++] = (uint8_t)((s_face_detect_faces[i].h >> 8) & 0xFF);
            }
        }
    }
    // 7. Read face-recognition results / 7. 读取人脸识别结果
    else if (strcmp(param, "face_recognition") == 0 ||
             (strlen(param) == 0 && s_project_type == I2C_PROJ_FACE_RECOG))
    {
        if (s_face_recog_count == 0)
        {
            out_buf[0] = 0;
            len = 1;
        }
        else
        {
            out_buf[0] = s_face_recog_count;
            len = 1;
            for (uint8_t i = 0; i < s_face_recog_count && len + 10 <= max_len; i++)
            {
                out_buf[len++] = (uint8_t)(s_face_recog_targets[i].face_id & 0xFF);
                out_buf[len++] = (uint8_t)((s_face_recog_targets[i].face_id >> 8) & 0xFF);
                out_buf[len++] = (uint8_t)(s_face_recog_targets[i].x & 0xFF);
                out_buf[len++] = (uint8_t)((s_face_recog_targets[i].x >> 8) & 0xFF);
                out_buf[len++] = (uint8_t)(s_face_recog_targets[i].y & 0xFF);
                out_buf[len++] = (uint8_t)((s_face_recog_targets[i].y >> 8) & 0xFF);
                out_buf[len++] = (uint8_t)(s_face_recog_targets[i].w & 0xFF);
                out_buf[len++] = (uint8_t)((s_face_recog_targets[i].w >> 8) & 0xFF);
                out_buf[len++] = (uint8_t)(s_face_recog_targets[i].h & 0xFF);
                out_buf[len++] = (uint8_t)((s_face_recog_targets[i].h >> 8) & 0xFF);
            }
        }
    }
    // 7.4 Read the number of registered faces / 7.4 获取已注册人脸数量
    else if (strcmp(param, "face_count") == 0)
    {
        out_buf[0] = s_face_enrolled_count;
        len = 1;
    }
    // 9. QR-code detection / 9. 二维码识别
    else if (strcmp(param, "qrcode") == 0 ||
             (strlen(param) == 0 && s_project_type == I2C_PROJ_QR_CODE))
    {
        if (!s_qr_has_result || s_qr_str_len == 0)
        {
            out_buf[0] = 0;
            len = 1;
        }
        else
        {
            out_buf[0] = 1; // Total number of QR codes: 1 / 二维码总数：1
            uint16_t base = 1;
            out_buf[base + 0] = 1; // Index: 1 / 序号：1
            out_buf[base + 1] = (uint8_t)(s_qr_x & 0xFF);
            out_buf[base + 2] = (uint8_t)((s_qr_x >> 8) & 0xFF);
            out_buf[base + 3] = (uint8_t)(s_qr_y & 0xFF);
            out_buf[base + 4] = (uint8_t)((s_qr_y >> 8) & 0xFF);
            out_buf[base + 5] = (uint8_t)(s_qr_w & 0xFF);
            out_buf[base + 6] = (uint8_t)((s_qr_w >> 8) & 0xFF);
            out_buf[base + 7] = (uint8_t)(s_qr_h & 0xFF);
            out_buf[base + 8] = (uint8_t)((s_qr_h >> 8) & 0xFF);
            out_buf[base + 9] = (uint8_t)(s_qr_str_len & 0xFF);
            out_buf[base + 10] = (uint8_t)((s_qr_str_len >> 8) & 0xFF);
            memcpy(&out_buf[base + 11], s_qr_str, s_qr_str_len);
            len = base + 11 + s_qr_str_len;
        }
    }
    // 8. Image streaming or unknown project type / 8. 图像传输或未知项目类型
    else
    {
        out_buf[0] = 0;
        len = 1;
    }

    return len;
}

static uint32_t s_last_json_cmd_tick = 0;
static portMUX_TYPE s_i2c_fifo_spinlock = portMUX_INITIALIZER_UNLOCKED;

static void send_response_frames_safe(const uint8_t *resp_data, uint16_t resp_len)
{
    xSemaphoreTake(s_i2c_mutex, portMAX_DELAY);
    i2c_build_response_frames(resp_data, resp_len, s_tx_frames, &s_tx_frames_len);

    uint16_t total_frags = (uint16_t)((s_tx_frames_len + 31) / 32);
    if (total_frags == 0) total_frags = 1;

    i2c_dev_t *hw = I2C_LL_GET_HW(MYI2C_SLAVE_NUM);
    // Avoid resetting TX FIFO while an external I2C master transfer is actively in flight
    for (int wait_us = 0; wait_us < 10000 && hw->sr.bus_busy; wait_us += 50)
    {
        esp_rom_delay_us(50);
    }

    uint8_t first_chunk = (s_tx_frames_len > 32) ? 32 : (uint8_t)s_tx_frames_len;
    portENTER_CRITICAL(&s_i2c_fifo_spinlock);
    i2c_ll_txfifo_rst(hw);
    esp_rom_delay_us(5);
    i2c_ll_write_txfifo(hw, s_tx_frames, first_chunk);
    portEXIT_CRITICAL(&s_i2c_fifo_spinlock);

    for (uint16_t f = 1; f < total_frags; f++)
    {
        int timeout_us = 35000;
        while (hw->sr.txfifo_cnt > 0 && timeout_us > 0)
        {
            esp_rom_delay_us(50);
            timeout_us -= 50;
        }

        if (timeout_us <= 0)
        {
            break;
        }

        // At 100 kHz, sending 32 bytes followed by NACK and STOP takes about 100 us / I2C 速率为 100 kHz 时，发送 32 字节并完成 NACK 和 STOP 约需 100 微秒
        // Wait 150 us for STOP to complete and the hardware FIFO to become ready / 等待 150 微秒，确保 STOP 完成且硬件 FIFO 已就绪
        // STM32 delays 1000us (1ms) before reading next fragment, leaving ~850us clean safety margin.
        esp_rom_delay_us(150);

        uint8_t chunk = (s_tx_frames_len - f * 32 > 32) ? 32 : (uint8_t)(s_tx_frames_len - f * 32);
        i2c_ll_write_txfifo(hw, s_tx_frames + f * 32, chunk);
    }

    xSemaphoreGive(s_i2c_mutex);
}

// -----------------------------------------------------------------------------
// Parse JSON commands and build responses / 解析 JSON 命令并生成响应
// -----------------------------------------------------------------------------
static void process_i2c_json_command(const char *json_str, int len)
{
    // Fast path for frequent get_info queries; avoids cJSON heap allocation for microsecond-level responses / 高频 get_info 查询快速通道：无需分配 cJSON 堆内存，可在微秒级响应
    if (strstr(json_str, "\"get_info\""))
    {
        char param_str[32] = {0};
        const char *param_ptr = strstr(json_str, "\"params\":");
        if (param_ptr)
        {
            const char *pstart = strchr(param_ptr + 9, '\"');
            if (pstart)
            {
                pstart++;
                const char *pend = strchr(pstart, '\"');
                if (pend && (pend - pstart) < sizeof(param_str))
                {
                    memcpy(param_str, pstart, pend - pstart);
                    param_str[pend - pstart] = '\0';
                }
            }
        }

        uint8_t fast_resp_data[512];
        xSemaphoreTake(s_i2c_mutex, portMAX_DELAY);
        uint16_t fast_resp_len = build_get_info_data(param_str, fast_resp_data, sizeof(fast_resp_data));
        xSemaphoreGive(s_i2c_mutex);

        send_response_frames_safe(fast_resp_data, fast_resp_len);
        return;
    }

    // Fast path for vision_setting commands with microsecond-level parsing and no heap allocation / vision_setting 命令快速通道：无需堆内存分配，可在微秒级完成解析
    if (strstr(json_str, "\"vision_setting\""))
    {
        ESP_LOGI(TAG, "vision_setting cmd received: %s", json_str);
        bool ok = false;
        sensor_t *s = esp_camera_sensor_get();
        const char *pmode = strstr(json_str, "\"mode\":");
        const char *ptarget = strstr(json_str, "\"target\":");
        if (pmode || ptarget)
        {
            xSemaphoreTake(s_i2c_mutex, portMAX_DELAY);
            if (pmode) {
                s_color_mode = (uint8_t)atoi(pmode + 7);
            }
            if (ptarget)
            {
                const char *pbracket = strchr(ptarget, '[');
                if (pbracket)
                {
                    s_color_target_count = 0;
                    pbracket++;
                    while (*pbracket && *pbracket != ']' && s_color_target_count < 5)
                    {
                        if (*pbracket >= '0' && *pbracket <= '9')
                        {
                            s_color_targets[s_color_target_count++] = (uint8_t)(*pbracket - '0');
                        }
                        pbracket++;
                    }
                }
            }
            xSemaphoreGive(s_i2c_mutex);
            ok = true;
        }
        const char *phm = strstr(json_str, "\"hmirror\":");
        if (phm && s) { s->set_hmirror(s, atoi(phm + 10) ? 1 : 0); ok = true; }
        const char *pvf = strstr(json_str, "\"vflip\":");
        if (pvf && s) { s->set_vflip(s, atoi(pvf + 8) ? 1 : 0); ok = true; }

        uint8_t resp = ok ? 1 : 0;
        ESP_LOGI(TAG, "vision_setting resp ok=%d (resp=%d)", ok, resp);
        send_response_frames_safe(&resp, 1);
        return;
    }

    cJSON *root = cJSON_Parse(json_str);
    if (!root)
    {
        // Check for legacy direct-register writes such as [reg, val] / 检查是否为旧版直接写寄存器命令，例如 [reg, val]
        if (len >= 2)
        {
            switch_i2c_control((uint8_t)json_str[0], (uint8_t)json_str[1]);
        }
        return;
    }

    cJSON *cmd_item = cJSON_GetObjectItem(root, "command");
    if (!cmd_item || !cJSON_IsString(cmd_item))
    {
        cJSON_Delete(root);
        return;
    }

    const char *cmd = cmd_item->valuestring;
    cJSON *params = cJSON_GetObjectItem(root, "params");

    uint8_t resp_data[512];
    uint16_t resp_len = 0;

    // 3. General control commands / 3. 通用控制指令
    if (strcmp(cmd, "vision_setting") == 0)
    {
        bool ok = false;
        sensor_t *s = esp_camera_sensor_get();
        if (params && cJSON_IsObject(params))
        {
            cJSON *hmirror = cJSON_GetObjectItem(params, "hmirror");
            if (hmirror && cJSON_IsNumber(hmirror))
            {
                if (s) {
                    s->set_hmirror(s, hmirror->valueint ? 1 : 0);
                    ok = true;
                }
            }
            cJSON *vflip = cJSON_GetObjectItem(params, "vflip");
            if (vflip && cJSON_IsNumber(vflip))
            {
                if (s) {
                    s->set_vflip(s, vflip->valueint ? 1 : 0);
                    ok = true;
                }
            }
            cJSON *mode = cJSON_GetObjectItem(params, "mode");
            cJSON *target = cJSON_GetObjectItem(params, "target");
            if ((mode && cJSON_IsNumber(mode)) || (target && cJSON_IsArray(target)))
            {
                xSemaphoreTake(s_i2c_mutex, portMAX_DELAY);
                if (mode && cJSON_IsNumber(mode))
                {
                    s_color_mode = (uint8_t)mode->valueint;
                }
                if (target && cJSON_IsArray(target))
                {
                    s_color_target_count = 0;
                    int arr_sz = cJSON_GetArraySize(target);
                    for (int i = 0; i < arr_sz && i < 5; i++)
                    {
                        cJSON *elem = cJSON_GetArrayItem(target, i);
                        if (elem && cJSON_IsNumber(elem))
                        {
                            s_color_targets[s_color_target_count++] = (uint8_t)elem->valueint;
                        }
                    }
                }
                xSemaphoreGive(s_i2c_mutex);
                ok = true;
            }
        }
        resp_data[0] = ok ? 1 : 0;
        resp_len = 1;
    }
    // 3.3 Reset the module / 3.3 模块复位
    else if (strcmp(cmd, "module_reset") == 0)
    {
        cJSON_Delete(root);
        vTaskDelay(pdMS_TO_TICKS(100));
        esp_restart();
        return;
    }
    // 7.2 Register a face / 7.2 人脸注册
    else if (strcmp(cmd, "face_register") == 0)
    {
        xSemaphoreTake(s_i2c_mutex, portMAX_DELAY);
        s_last_register_done = false;
        s_last_register_success = false;
        s_last_registered_id = 0;
        xSemaphoreGive(s_i2c_mutex);

        bool triggered = false;
        if (xQueuevirtualKeystate)
        {
            int k = 2; // Enroll mode / 注册模式
            triggered = (xQueueSend(xQueuevirtualKeystate, &k, 0) == pdTRUE);
        }

        bool done = false;
        if (triggered)
        {
            for (int wait = 0; wait < 100; wait++)
            {
                vTaskDelay(pdMS_TO_TICKS(10));
                xSemaphoreTake(s_i2c_mutex, portMAX_DELAY);
                done = s_last_register_done;
                xSemaphoreGive(s_i2c_mutex);
                if (done) break;
            }
        }

        xSemaphoreTake(s_i2c_mutex, portMAX_DELAY);
        if (done && s_last_register_success)
        {
            resp_data[0] = 1;
            resp_data[1] = (uint8_t)(s_last_registered_id & 0xFF);
            resp_data[2] = (uint8_t)((s_last_registered_id >> 8) & 0xFF);
            resp_len = 3;
        }
        else
        {
            resp_data[0] = 0;
            resp_len = 1;
        }
        xSemaphoreGive(s_i2c_mutex);
    }
    // 7.3 Delete a face / 7.3 删除人脸
    else if (strcmp(cmd, "face_delete") == 0)
    {
        int32_t target_id = 0;
        if (params && cJSON_IsNumber(params))
        {
            target_id = (int32_t)params->valueint;
        }
        else if (params && cJSON_IsString(params))
        {
            target_id = atoi(params->valuestring);
        }

        xSemaphoreTake(s_i2c_mutex, portMAX_DELAY);
        s_face_delete_target = target_id;
        s_face_delete_done = false;
        s_face_delete_success = false;
        xSemaphoreGive(s_i2c_mutex);

        bool triggered = false;
        if (xQueuevirtualKeystate)
        {
            int k = 4; // Delete mode / 删除模式
            triggered = (xQueueSend(xQueuevirtualKeystate, &k, 0) == pdTRUE);
        }

        bool done = false;
        if (triggered)
        {
            for (int wait = 0; wait < 100; wait++)
            {
                vTaskDelay(pdMS_TO_TICKS(10));
                xSemaphoreTake(s_i2c_mutex, portMAX_DELAY);
                done = s_face_delete_done;
                xSemaphoreGive(s_i2c_mutex);
                if (done) break;
            }
        }

        xSemaphoreTake(s_i2c_mutex, portMAX_DELAY);
        resp_data[0] = (done && s_face_delete_success) ? 1 : 0;
        resp_len = 1;
        xSemaphoreGive(s_i2c_mutex);
    }
    // Handle get_info queries / 处理 get_info 查询
    else if (strcmp(cmd, "get_info") == 0)
    {
        const char *param_str = (params && cJSON_IsString(params)) ? params->valuestring : "";
        xSemaphoreTake(s_i2c_mutex, portMAX_DELAY);
        resp_len = build_get_info_data(param_str, resp_data, sizeof(resp_data));
        xSemaphoreGive(s_i2c_mutex);
    }
    else
    {
        resp_data[0] = 0;
        resp_len = 1;
    }

    cJSON_Delete(root);

    // Build and refresh the transmit buffer / 组装并刷新发送缓冲区
    send_response_frames_safe(resp_data, resp_len);
}

// Refresh the default response frame for hosts that read without sending a command / 刷新默认响应帧，供不发送指令的主机直接读取
static void refresh_default_tx_buffer(void)
{
    uint8_t resp_data[256];
    xSemaphoreTake(s_i2c_mutex, portMAX_DELAY);
    uint16_t resp_len = build_get_info_data("", resp_data, sizeof(resp_data));
    xSemaphoreGive(s_i2c_mutex);

    send_response_frames_safe(resp_data, resp_len);
}

// -----------------------------------------------------------------------------
// I2C slave task / I2C 从机任务
// -----------------------------------------------------------------------------
static void My_IIC_Slave_Task(void *arg)
{
    uint8_t rx_buf[512];
    int rx_total = 0;
    int empty_polls = 0;
    bool in_json = false;
    int brace_depth = 0;
    bool in_string = false;
    bool escaped = false;
    ESP_LOGI(TAG, "I2C Slave started on address 0x%02X, SDA=%d, SCL=%d (Standard 32-byte protocol)",
             Camera_IIC_ADDR, MYI2C_SLAVE_SDA, MYI2C_SLAVE_SCL);

    while (1)
    {
        uint8_t rx_byte = 0;
        // Request 1 byte so i2c_slave_read_buffer wakes & returns immediately upon arrival instead of blocking full tick timeout
        int n = i2c_slave_read_buffer(MYI2C_SLAVE_NUM, &rx_byte, 1, pdMS_TO_TICKS(10) > 0 ? pdMS_TO_TICKS(10) : 1);
        if (n > 0)
        {
            empty_polls = 0;
            if (rx_total < (int)sizeof(rx_buf) - 1)
            {
                rx_buf[rx_total++] = rx_byte;
                rx_buf[rx_total] = '\0';
            }
            else
            {
                // Overflow guard: reset parser state cleanly
                rx_total = 0;
                in_json = false;
                brace_depth = 0;
                in_string = false;
                escaped = false;
                continue;
            }

            if (!in_json)
            {
                if (rx_byte == '{')
                {
                    // Discard any leading noise before '{'
                    if (rx_total > 1)
                    {
                        rx_buf[0] = '{';
                        rx_buf[1] = '\0';
                        rx_total = 1;
                    }
                    in_json = true;
                    brace_depth = 1;
                    in_string = false;
                    escaped = false;
                }
            }
            else
            {
                if (escaped)
                {
                    escaped = false;
                }
                else if (rx_byte == '\\' && in_string)
                {
                    escaped = true;
                }
                else if (rx_byte == '"')
                {
                    in_string = !in_string;
                }
                else if (!in_string)
                {
                    if (rx_byte == '{')
                    {
                        brace_depth++;
                    }
                    else if (rx_byte == '}')
                    {
                        brace_depth--;
                        if (brace_depth == 0)
                        {
                            s_last_json_cmd_tick = xTaskGetTickCount();
                            process_i2c_json_command((const char *)rx_buf, rx_total);
                            rx_total = 0;
                            in_json = false;
                            in_string = false;
                            escaped = false;
                        }
                    }
                }
            }
        }
        else
        {
            if (rx_total > 0)
            {
                if (!in_json)
                {
                    // Legacy non-JSON binary register write (e.g., [reg, val])
                    process_i2c_json_command((const char *)rx_buf, rx_total);
                    rx_total = 0;
                    empty_polls = 0;
                }
                else if (++empty_polls >= 5)
                {
                    // Clear stale incomplete JSON fragment after 50ms of silence
                    rx_total = 0;
                    empty_polls = 0;
                    in_json = false;
                    brace_depth = 0;
                    in_string = false;
                    escaped = false;
                }
            }
        }
    }
}

static void i2c_slave_init(void)
{
    i2c_config_t conf_slave = {
        .mode = I2C_MODE_SLAVE,
        .sda_io_num = (gpio_num_t)MYI2C_SLAVE_SDA,
        .scl_io_num = (gpio_num_t)MYI2C_SLAVE_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .slave = {
            .addr_10bit_en = 0,
            .slave_addr = Camera_IIC_ADDR,
            .maximum_speed = 100000,
        },
        .clk_flags = 0,
    };
    i2c_param_config(MYI2C_SLAVE_NUM, &conf_slave);
    i2c_driver_install(MYI2C_SLAVE_NUM, conf_slave.mode, I2C_SLAVE_RX_BUF_LEN, I2C_SLAVE_TX_BUF_LEN, 0);
    i2c_dev_t *hw = I2C_LL_GET_HW(MYI2C_SLAVE_NUM);
    i2c_ll_set_txfifo_empty_thr(hw, 1);
}

void My_i2c_init(const QueueHandle_t key_state_i)
{
    xQueuevirtualKeystate = key_state_i;
    if (!s_i2c_mutex) {
        s_i2c_mutex = xSemaphoreCreateMutex();
    }
    i2c_slave_init();
    refresh_default_tx_buffer();
    xTaskCreatePinnedToCore(My_IIC_Slave_Task, "My_IIC_Slave_Task", 4 * 1024, NULL, 20, NULL, 0);
}
