#include "ESP32_S3_CAM.h"
#include "ESP32_S3_CAM_port.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define ESP32_S3_CAM_FRAME_HEADER_H (0xAA)
#define ESP32_S3_CAM_FRAME_HEADER_L (0x55)
#define ESP32_S3_CAM_FRAME_HEADER_SIZE (8U)
#define ESP32_S3_CAM_FRAME_MAX_SIZE (32U)
#define ESP32_S3_CAM_MAX_FRAGMENTS (64U)
#define ESP32_S3_CAM_MAX_FRAME_DATA_SIZE (ESP32_S3_CAM_FRAME_MAX_SIZE - ESP32_S3_CAM_FRAME_HEADER_SIZE - 1U)
#define ESP32_S3_CAM_PACKET_BUFFER_SIZE (1024U)
#define ESP32_S3_CAM_COLOR_BLOCK_SIZE (9U)
#define ESP32_S3_CAM_LINE_RESULT_SIZE (17U)
#define ESP32_S3_CAM_HAND_GESTURE_RECORD_SIZE (9U)
#define ESP32_S3_CAM_TARGET_DETECTION_RECORD_SIZE (9U)
#define ESP32_S3_CAM_POSE_RECORD_HEADER_SIZE (10U)
#define ESP32_S3_CAM_POSE_KEYPOINT_RECORD_SIZE (5U)
#define ESP32_S3_CAM_APRILTAG_RECORD_SIZE (38U)
#define ESP32_S3_CAM_QRCODE_RECORD_HEADER_SIZE (11U)

static const char ESP32_S3_CAM_COLOR_CONFIG_COMMAND[] = "{\"command\":\"get_info\",\"params\":\"color_config\"}";
static const char ESP32_S3_CAM_COLOR_RESULT_COMMAND[] = "{\"command\":\"get_info\",\"params\":\"color_result\"}";
static const char ESP32_S3_CAM_HAND_GESTURE_COMMAND[] = "{\"command\":\"get_info\",\"params\":\"hand_gesture\"}";
static const char ESP32_S3_CAM_CAT_DOG_COMMAND[] = "{\"command\":\"get_info\",\"params\":\"cat_dog\"}";
static const char ESP32_S3_CAM_CAT_COMMAND[] = "{\"command\":\"get_info\",\"params\":\"cat\"}";
static const char ESP32_S3_CAM_DOG_COMMAND[] = "{\"command\":\"get_info\",\"params\":\"dog\"}";
static const char ESP32_S3_CAM_PEDESTRIAN_COMMAND[] = "{\"command\":\"get_info\",\"params\":\"pedestrian\"}";
static const char ESP32_S3_CAM_YOLO11N_COMMAND[] = "{\"command\":\"get_info\",\"params\":\"coco_detect\"}";
static const char ESP32_S3_CAM_FACE_COMMAND[] = "{\"command\":\"get_info\",\"params\":\"human\"}";
static const char ESP32_S3_CAM_POSE_COMMAND[] = "{\"command\":\"get_info\",\"params\":\"coco_pose\"}";
static const char ESP32_S3_CAM_APRILTAG_COMMAND[] = "{\"command\":\"get_info\",\"params\":\"apriltag_recognition\"}";
static const char ESP32_S3_CAM_QRCODE_COMMAND[] = "{\"command\":\"get_info\",\"params\":\"qrcode\"}";
static const char ESP32_S3_CAM_FACE_RECOGNITION_COMMAND[] = "{\"command\":\"get_info\",\"params\":\"face_recognition\"}";
static const char ESP32_S3_CAM_FACE_REGISTER_COMMAND[] = "{\"command\":\"face_register\"}";
static const char ESP32_S3_CAM_FACE_COUNT_COMMAND[] = "{\"command\":\"get_info\",\"params\":\"face_count\"}";
static const char ESP32_S3_CAM_MODULE_RESET_COMMAND[] = "{\"command\":\"module_reset\"}";


static void esp32_s3_cam_wait_response_ready(void)
{
    esp32_s3_cam_port_delay_ms(15);
}

static uint8_t esp32_s3_cam_checksum_update(const uint8_t *data, size_t len)
{
    uint8_t checksum = 0;

    for (size_t i = 0; i < len; i++) {
        checksum ^= data[i];
    }

    return (uint8_t)(checksum & 0xff);
}

static uint16_t esp32_s3_cam_read_u16_le(const uint8_t *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static float esp32_s3_cam_read_float_le(const uint8_t *data)
{
    float value = 0.0f;
    uint8_t bytes[sizeof(float)];

    bytes[0] = data[0];
    bytes[1] = data[1];
    bytes[2] = data[2];
    bytes[3] = data[3];
    memcpy(&value, bytes, sizeof(value));

    return value;
}

static void esp32_s3_cam_print_failed_frame(esp32_s3_cam_err_t err,
                                         const uint8_t *header_buf,
                                         size_t header_len,
                                         const uint8_t *payload_buf,
                                         size_t payload_len)
{
    printf("parse failed err=%d frame:", (int)err);

    for (size_t i = 0; i < header_len; i++) {
        printf(" %02X", header_buf[i]);
    }

    for (size_t i = 0; i < payload_len; i++) {
        printf(" %02X", payload_buf[i]);
    }

    printf("\r\n");
}

static esp32_s3_cam_err_t esp32_s3_cam_parse_frame_header(const uint8_t *header_buf,
                                                    size_t data_capacity,
                                                    esp32_s3_cam_frame_header_t *header)
{
    if (header_buf == NULL || header == NULL) {
        return ESP32_S3_CAM_ERR_INVALID_ARG;
    }

    if (header_buf[0] != ESP32_S3_CAM_FRAME_HEADER_H ||
        header_buf[1] != ESP32_S3_CAM_FRAME_HEADER_L) {
        return ESP32_S3_CAM_ERR_INVALID_RESPONSE;
    }

    header->data_len = (uint16_t)header_buf[2] | ((uint16_t)header_buf[3] << 8);
    header->fragment_id = (uint16_t)header_buf[4] | ((uint16_t)header_buf[5] << 8);
    header->total_fragments = (uint16_t)header_buf[6] | ((uint16_t)header_buf[7] << 8);

    if (header->data_len == 0 ||
        header->data_len > data_capacity ||
        header->data_len > ESP32_S3_CAM_MAX_FRAME_DATA_SIZE) {
        return ESP32_S3_CAM_ERR_INVALID_SIZE;
    }

    if (header->fragment_id == 0 ||
        header->total_fragments == 0 ||
        header->fragment_id > ESP32_S3_CAM_MAX_FRAGMENTS ||
        header->total_fragments > ESP32_S3_CAM_MAX_FRAGMENTS ||
        header->fragment_id > header->total_fragments) {
        return ESP32_S3_CAM_ERR_INVALID_RESPONSE;
    }

    return ESP32_S3_CAM_OK;
}

esp32_s3_cam_err_t esp32_s3_cam_receive_frame(esp32_s3_cam_handle_t *handle,
                                        uint8_t *data,
                                        size_t data_capacity,
                                        esp32_s3_cam_frame_header_t *header,
                                        uint32_t timeout_ms)
{
    uint8_t frame_buf[ESP32_S3_CAM_FRAME_MAX_SIZE] = {0};
    const uint8_t *payload_buf = &frame_buf[ESP32_S3_CAM_FRAME_HEADER_SIZE];
    uint8_t cal_checksum = 0;

    if (handle == NULL ||
        handle->callbacks.read == NULL ||
        data == NULL ||
        header == NULL) {
        return ESP32_S3_CAM_ERR_INVALID_ARG;
    }

    esp32_s3_cam_err_t ret = handle->callbacks.read(frame_buf, sizeof(frame_buf), timeout_ms);
    if (ret != ESP32_S3_CAM_OK) {
        return ret;
    }

    // 检查帧头同步字 0xAA 0x55。若发生偏移（如从机硬件 FIFO 偶发前置空字节），执行自动重同步
    if (frame_buf[0] != ESP32_S3_CAM_FRAME_HEADER_H || frame_buf[1] != ESP32_S3_CAM_FRAME_HEADER_L) {
        size_t sync_offset = 0;
        for (size_t i = 1; i < sizeof(frame_buf) - 1; i++) {
            if (frame_buf[i] == ESP32_S3_CAM_FRAME_HEADER_H &&
                frame_buf[i + 1] == ESP32_S3_CAM_FRAME_HEADER_L) {
                sync_offset = i;
                break;
            }
        }

        if (sync_offset > 0 && sync_offset < sizeof(frame_buf)) {
            size_t valid_bytes = sizeof(frame_buf) - sync_offset;
            memmove(frame_buf, &frame_buf[sync_offset], valid_bytes);

            // 补充读取 sync_offset 个字节填补帧尾并清空从机 FIFO 残留
            esp32_s3_cam_err_t extra_ret = handle->callbacks.read(&frame_buf[valid_bytes], sync_offset, timeout_ms);
            if (extra_ret != ESP32_S3_CAM_OK) {
                memset(&frame_buf[valid_bytes], 0, sync_offset);
            }
        } else {
            return ESP32_S3_CAM_ERR_INVALID_RESPONSE;
        }
    }

    ret = esp32_s3_cam_parse_frame_header(frame_buf, data_capacity, header);
    if (ret != ESP32_S3_CAM_OK) {
        return ret;
    }

    cal_checksum = esp32_s3_cam_checksum_update(payload_buf, header->data_len);

    if (cal_checksum != payload_buf[header->data_len]) {
        printf("checksum mismatch calculated=0x%02X received=0x%02X\r\n",
               cal_checksum,
               payload_buf[header->data_len]);

        esp32_s3_cam_print_failed_frame(ESP32_S3_CAM_ERR_INVALID_CHECKSUM,
                                     frame_buf,
                                     ESP32_S3_CAM_FRAME_HEADER_SIZE,
                                     payload_buf,
                                     (size_t)header->data_len + 1U);
        return ESP32_S3_CAM_ERR_INVALID_CHECKSUM;
    }
    memcpy(data, payload_buf, header->data_len);
    return ESP32_S3_CAM_OK;
}

esp32_s3_cam_err_t esp32_s3_cam_receive_packet(esp32_s3_cam_handle_t *handle,
                                         uint8_t *data,
                                         size_t data_capacity,
                                         size_t *packet_len,
                                         uint32_t timeout_ms)
{
    esp32_s3_cam_frame_header_t header;
    size_t offset = 0;
    uint16_t expected_fragment_id = 1;
    uint16_t total_fragments = 0;

    if (handle == NULL ||
        data == NULL ||
        packet_len == NULL ||
        data_capacity == 0) {
        return ESP32_S3_CAM_ERR_INVALID_ARG;
    }

    *packet_len = 0;

    do {
        esp32_s3_cam_err_t ret = esp32_s3_cam_receive_frame(handle,
                                                      &data[offset],
                                                      data_capacity - offset,
                                                      &header,
                                                      timeout_ms);
        esp32_s3_cam_port_delay_ms(1);
        if (ret != ESP32_S3_CAM_OK) {
            return ret;
        }

        if (header.fragment_id != expected_fragment_id) {
            return ESP32_S3_CAM_ERR_INVALID_RESPONSE;
        }

        if (total_fragments == 0) {
            total_fragments = header.total_fragments;
        } else if (header.total_fragments != total_fragments) {
            return ESP32_S3_CAM_ERR_INVALID_RESPONSE;
        }

        if (header.data_len > data_capacity - offset) {
            return ESP32_S3_CAM_ERR_INVALID_SIZE;
        }

        offset += header.data_len;
        *packet_len = offset;

        if (header.fragment_id < total_fragments && offset >= data_capacity) {
            return ESP32_S3_CAM_ERR_INVALID_SIZE;
        }

        expected_fragment_id++;
    } while ((uint16_t)(expected_fragment_id - 1) < total_fragments);

    return ESP32_S3_CAM_OK;
}

static esp32_s3_cam_err_t esp32_s3_cam_request_packet(esp32_s3_cam_handle_t *handle,
                                                const char *command,
                                                uint8_t *packet_data,
                                                size_t capacity,
                                                size_t *packet_len,
                                                uint32_t timeout_ms)
{
    esp32_s3_cam_err_t ret = ESP32_S3_CAM_ERR_FAIL;

    for (int retry = 0; retry < 2; retry++) {
        ret = esp32_s3_cam_send_json(handle, command, timeout_ms);
        if (ret != ESP32_S3_CAM_OK) {
            esp32_s3_cam_port_delay_ms(5);
            continue;
        }

        esp32_s3_cam_wait_response_ready();

        ret = esp32_s3_cam_receive_packet(handle, packet_data, capacity, packet_len, timeout_ms);
        if (ret == ESP32_S3_CAM_OK) {
            return ESP32_S3_CAM_OK;
        }

        esp32_s3_cam_port_delay_ms(5);
    }

    return ret;
}

esp32_s3_cam_err_t esp32_s3_cam_init(esp32_s3_cam_handle_t *handle)
{
    if (handle == NULL) {
        return ESP32_S3_CAM_ERR_INVALID_ARG;
    }

    memset(handle, 0, sizeof(*handle));

    return ESP32_S3_CAM_OK;
}

esp32_s3_cam_err_t esp32_s3_cam_register_i2c_callbacks(esp32_s3_cam_handle_t *handle,
                                                 esp32_s3_cam_i2c_write_cb_t write_callback,
                                                 esp32_s3_cam_i2c_read_cb_t read_callback)
{
    if (handle == NULL || write_callback == NULL || read_callback == NULL) {
        return ESP32_S3_CAM_ERR_INVALID_ARG;
    }

    handle->callbacks.write = write_callback;
    handle->callbacks.read = read_callback;

    return ESP32_S3_CAM_OK;
}

esp32_s3_cam_err_t esp32_s3_cam_send_json(esp32_s3_cam_handle_t *handle,
                                    const char *json,
                                    uint32_t timeout_ms)
{
    if (handle == NULL ||
        handle->callbacks.write == NULL ||
        json == NULL ||
        json[0] == '\0') {
        return ESP32_S3_CAM_ERR_INVALID_ARG;
    }

    /* Strictly send strlen(json) without trailing null byte */
    size_t json_len = strlen(json);
    return handle->callbacks.write((const uint8_t *)json, json_len, timeout_ms);
}

esp32_s3_cam_err_t esp32_s3_cam_get_color_config(esp32_s3_cam_handle_t *handle,
                                           esp32_s3_cam_color_config_t *config,
                                           uint32_t timeout_ms)
{
    uint8_t packet_data[ESP32_S3_CAM_PACKET_BUFFER_SIZE] = {0};
    size_t packet_len = 0;

    if (handle == NULL || config == NULL) {
        return ESP32_S3_CAM_ERR_INVALID_ARG;
    }

    esp32_s3_cam_err_t ret = esp32_s3_cam_request_packet(handle,
                                                   ESP32_S3_CAM_COLOR_CONFIG_COMMAND,
                                                   packet_data,
                                                   sizeof(packet_data),
                                                   &packet_len,
                                                   timeout_ms);
    if (ret != ESP32_S3_CAM_OK) {
        return ret;
    }

    if (packet_len < 1U) {
        return ESP32_S3_CAM_ERR_INVALID_SIZE;
    }

    memset(config, 0, sizeof(*config));
    config->mode = packet_data[0];
    /* Protocol: Registered color count is packet_len - 1 */
    config->registered_color_count = (uint8_t)(packet_len - 1U);

    if (config->mode > ESP32_S3_CAM_VISION_MODE_LINE ||
        config->registered_color_count > ESP32_S3_CAM_MAX_TARGET_COLORS) {
        return ESP32_S3_CAM_ERR_INVALID_RESPONSE;
    }

    for (uint8_t i = 0; i < config->registered_color_count; i++) {
        config->targets[i] = packet_data[1U + i];
    }

    return ESP32_S3_CAM_OK;
}

esp32_s3_cam_err_t esp32_s3_cam_set_vision_mode(esp32_s3_cam_handle_t *handle,
                                          uint8_t mode,
                                          const uint8_t *targets,
                                          size_t target_count,
                                          bool *success,
                                          uint32_t timeout_ms)
{
    char command[128] = {0};
    uint8_t packet_data[ESP32_S3_CAM_PACKET_BUFFER_SIZE] = {0};
    size_t packet_len = 0;
    size_t offset = 0;

    if (handle == NULL ||
        targets == NULL ||
        success == NULL ||
        mode > ESP32_S3_CAM_VISION_MODE_LINE ||
        target_count == 0U ||
        target_count > ESP32_S3_CAM_MAX_TARGET_COLORS ||
        (mode == ESP32_S3_CAM_VISION_MODE_LINE && target_count != 1U)) {
        return ESP32_S3_CAM_ERR_INVALID_ARG;
    }

    /* Protocol Section 8.3.1: key is "target", singular */
    offset = (size_t)snprintf(command,
                              sizeof(command),
                              "{\"command\":\"vision_setting\",\"params\":{\"mode\":%u,\"target\":[",
                              (unsigned int)mode);
    if (offset >= sizeof(command)) {
        return ESP32_S3_CAM_ERR_INVALID_SIZE;
    }

    for (size_t i = 0; i < target_count; i++) {
        int written = snprintf(&command[offset],
                               sizeof(command) - offset,
                               "%s%u",
                               (i == 0U) ? "" : ",",
                               (unsigned int)targets[i]);
        if (written < 0 || (size_t)written >= sizeof(command) - offset) {
            return ESP32_S3_CAM_ERR_INVALID_SIZE;
        }
        offset += (size_t)written;
    }

    if (snprintf(&command[offset], sizeof(command) - offset, "]}}") >= (int)(sizeof(command) - offset)) {
        return ESP32_S3_CAM_ERR_INVALID_SIZE;
    }

    esp32_s3_cam_err_t ret = esp32_s3_cam_send_json(handle, command, timeout_ms);
    if (ret != ESP32_S3_CAM_OK) {
        return ret;
    }

    esp32_s3_cam_wait_response_ready();

    ret = esp32_s3_cam_receive_packet(handle,
                                   packet_data,
                                   sizeof(packet_data),
                                   &packet_len,
                                   timeout_ms);
    if (ret != ESP32_S3_CAM_OK) {
        return ret;
    }

    if (packet_len < 1U) {
        return ESP32_S3_CAM_ERR_INVALID_SIZE;
    }

    if (packet_data[0] > 1U) {
        return ESP32_S3_CAM_ERR_INVALID_RESPONSE;
    }

    *success = (packet_data[0] == 1U);

    return ESP32_S3_CAM_OK;
}

esp32_s3_cam_err_t esp32_s3_cam_get_color_result(esp32_s3_cam_handle_t *handle,
                                           esp32_s3_cam_color_result_t *result,
                                           uint32_t timeout_ms)
{
    uint8_t packet_data[ESP32_S3_CAM_PACKET_BUFFER_SIZE] = {0};
    size_t packet_len = 0;

    if (handle == NULL || result == NULL) {
        return ESP32_S3_CAM_ERR_INVALID_ARG;
    }

    memset(result, 0, sizeof(*result));

    esp32_s3_cam_err_t ret = esp32_s3_cam_request_packet(handle,
                                                   ESP32_S3_CAM_COLOR_RESULT_COMMAND,
                                                   packet_data,
                                                   sizeof(packet_data),
                                                   &packet_len,
                                                   timeout_ms);
    if (ret != ESP32_S3_CAM_OK) {
        return ret;
    }

    if (packet_len < 1U) {
        return ESP32_S3_CAM_ERR_INVALID_SIZE;
    }

    uint8_t total_blocks = packet_data[0];
    if (total_blocks == 0U) {
        result->count = 0;
        return ESP32_S3_CAM_OK;
    }

    /* Truncation protection: safely parse up to available blocks and ESP32_S3_CAM_MAX_COLOR_BLOCKS */
    size_t available_blocks = (packet_len > 1U) ? ((packet_len - 1U) / ESP32_S3_CAM_COLOR_BLOCK_SIZE) : 0;
    uint8_t count_to_parse = total_blocks;
    if (count_to_parse > available_blocks) {
        count_to_parse = (uint8_t)available_blocks;
    }
    if (count_to_parse > ESP32_S3_CAM_MAX_COLOR_BLOCKS) {
        count_to_parse = ESP32_S3_CAM_MAX_COLOR_BLOCKS;
    }

    result->count = count_to_parse;

    for (uint8_t i = 0; i < count_to_parse; i++) {
        const uint8_t *block_data = &packet_data[1U + ((size_t)i * ESP32_S3_CAM_COLOR_BLOCK_SIZE)];
        result->blocks[i].index = block_data[0];
        result->blocks[i].x = esp32_s3_cam_read_u16_le(&block_data[1]);
        result->blocks[i].y = esp32_s3_cam_read_u16_le(&block_data[3]);
        result->blocks[i].width = esp32_s3_cam_read_u16_le(&block_data[5]);
        result->blocks[i].height = esp32_s3_cam_read_u16_le(&block_data[7]);
    }

    return ESP32_S3_CAM_OK;
}

esp32_s3_cam_err_t esp32_s3_cam_get_line_result(esp32_s3_cam_handle_t *handle,
                                          esp32_s3_cam_line_result_t *result,
                                          uint32_t timeout_ms)
{
    uint8_t packet_data[ESP32_S3_CAM_PACKET_BUFFER_SIZE] = {0};
    size_t packet_len = 0;

    if (handle == NULL || result == NULL) {
        return ESP32_S3_CAM_ERR_INVALID_ARG;
    }

    memset(result, 0, sizeof(*result));

    esp32_s3_cam_err_t ret = esp32_s3_cam_send_json(handle, ESP32_S3_CAM_COLOR_RESULT_COMMAND, timeout_ms);
    if (ret != ESP32_S3_CAM_OK) {
        return ret;
    }

    esp32_s3_cam_wait_response_ready();

    ret = esp32_s3_cam_receive_packet(handle,
                                   packet_data,
                                   sizeof(packet_data),
                                   &packet_len,
                                   timeout_ms);
    if (ret != ESP32_S3_CAM_OK) {
        return ret;
    }

    if (packet_len < 1U) {
        return ESP32_S3_CAM_ERR_INVALID_SIZE;
    }

    if (packet_data[0] > 1U) {
        return ESP32_S3_CAM_ERR_INVALID_RESPONSE;
    }

    result->detected = (packet_data[0] == 1U);
    if (!result->detected) {
        return ESP32_S3_CAM_OK;
    }

    if (packet_len < ESP32_S3_CAM_LINE_RESULT_SIZE) {
        return ESP32_S3_CAM_ERR_INVALID_SIZE;
    }

    result->start_x = esp32_s3_cam_read_u16_le(&packet_data[1]);
    result->start_y = esp32_s3_cam_read_u16_le(&packet_data[3]);
    result->end_x = esp32_s3_cam_read_u16_le(&packet_data[5]);
    result->end_y = esp32_s3_cam_read_u16_le(&packet_data[7]);
    result->angle_deg = esp32_s3_cam_read_float_le(&packet_data[9]);
    result->offset = esp32_s3_cam_read_float_le(&packet_data[13]);

    return ESP32_S3_CAM_OK;
}

static esp32_s3_cam_err_t esp32_s3_cam_parse_hand_gesture_data(const uint8_t *data,
                                                         size_t data_len,
                                                         esp32_s3_cam_hand_gesture_result_t *result)
{
    uint8_t parse_count = 0;

    if (data == NULL || result == NULL || data_len < 1U) {
        return ESP32_S3_CAM_ERR_INVALID_ARG;
    }

    memset(result, 0, sizeof(*result));
    result->total_count = data[0];

    if (result->total_count == 0U) {
        return ESP32_S3_CAM_OK;
    }

    if (data_len < 1U + ((size_t)result->total_count * ESP32_S3_CAM_HAND_GESTURE_RECORD_SIZE)) {
        return ESP32_S3_CAM_ERR_INVALID_SIZE;
    }

    parse_count = result->total_count;
    if (parse_count > ESP32_S3_CAM_HAND_GESTURE_MAX_COUNT) {
        parse_count = ESP32_S3_CAM_HAND_GESTURE_MAX_COUNT;
    }

    for (uint8_t i = 0; i < parse_count; i++) {
        const uint8_t *item_data = &data[1U + ((size_t)i * ESP32_S3_CAM_HAND_GESTURE_RECORD_SIZE)];
        if (item_data[0] > ESP32_S3_CAM_HAND_GESTURE_DISLIKE) {
            return ESP32_S3_CAM_ERR_INVALID_RESPONSE;
        }

        result->items[i].gesture = item_data[0];
        result->items[i].x = esp32_s3_cam_read_u16_le(&item_data[1]);
        result->items[i].y = esp32_s3_cam_read_u16_le(&item_data[3]);
        result->items[i].width = esp32_s3_cam_read_u16_le(&item_data[5]);
        result->items[i].height = esp32_s3_cam_read_u16_le(&item_data[7]);
    }

    result->parsed_count = parse_count;
    return ESP32_S3_CAM_OK;
}

esp32_s3_cam_err_t esp32_s3_cam_get_hand_gesture_result(esp32_s3_cam_handle_t *handle,
                                                  esp32_s3_cam_hand_gesture_result_t *result,
                                                  uint32_t timeout_ms)
{
    uint8_t packet_data[ESP32_S3_CAM_PACKET_BUFFER_SIZE] = {0};
    size_t packet_len = 0;

    if (handle == NULL || result == NULL) {
        return ESP32_S3_CAM_ERR_INVALID_ARG;
    }

    esp32_s3_cam_err_t ret = esp32_s3_cam_send_json(handle, ESP32_S3_CAM_HAND_GESTURE_COMMAND, timeout_ms);
    if (ret != ESP32_S3_CAM_OK) {
        return ret;
    }

    esp32_s3_cam_wait_response_ready();

    ret = esp32_s3_cam_receive_packet(handle,
                                   packet_data,
                                   sizeof(packet_data),
                                   &packet_len,
                                   timeout_ms);
    if (ret != ESP32_S3_CAM_OK) {
        return ret;
    }

    return esp32_s3_cam_parse_hand_gesture_data(packet_data, packet_len, result);
}

static esp32_s3_cam_err_t esp32_s3_cam_parse_target_detection_data(const uint8_t *data,
                                                             size_t data_len,
                                                             esp32_s3_cam_target_detection_result_t *result)
{
    uint8_t parse_count = 0;

    if (data == NULL || result == NULL || data_len < 1U) {
        return ESP32_S3_CAM_ERR_INVALID_ARG;
    }

    memset(result, 0, sizeof(*result));
    result->total_count = data[0];

    if (result->total_count == 0U) {
        return ESP32_S3_CAM_OK;
    }

    if (data_len < 1U + ((size_t)result->total_count * ESP32_S3_CAM_TARGET_DETECTION_RECORD_SIZE)) {
        return ESP32_S3_CAM_ERR_INVALID_SIZE;
    }

    parse_count = result->total_count;
    if (parse_count > ESP32_S3_CAM_TARGET_DETECTION_MAX_COUNT) {
        parse_count = ESP32_S3_CAM_TARGET_DETECTION_MAX_COUNT;
    }

    for (uint8_t i = 0; i < parse_count; i++) {
        const uint8_t *item_data = &data[1U + ((size_t)i * ESP32_S3_CAM_TARGET_DETECTION_RECORD_SIZE)];
        result->items[i].index = item_data[0];
        result->items[i].x = esp32_s3_cam_read_u16_le(&item_data[1]);
        result->items[i].y = esp32_s3_cam_read_u16_le(&item_data[3]);
        result->items[i].width = esp32_s3_cam_read_u16_le(&item_data[5]);
        result->items[i].height = esp32_s3_cam_read_u16_le(&item_data[7]);
    }

    result->parsed_count = parse_count;
    return ESP32_S3_CAM_OK;
}

static esp32_s3_cam_err_t esp32_s3_cam_parse_yolo11n_detection_data(const uint8_t *data,
                                                              size_t data_len,
                                                              esp32_s3_cam_target_detection_result_t *result)
{
    uint8_t parse_count = 0;

    if (data == NULL || result == NULL || data_len < 1U) {
        return ESP32_S3_CAM_ERR_INVALID_ARG;
    }

    memset(result, 0, sizeof(*result));
    result->total_count = data[0];

    if (result->total_count == 0U) {
        return ESP32_S3_CAM_OK;
    }

    if (data_len < 1U + ((size_t)result->total_count * ESP32_S3_CAM_TARGET_DETECTION_RECORD_SIZE)) {
        return ESP32_S3_CAM_ERR_INVALID_SIZE;
    }

    parse_count = result->total_count;
    if (parse_count > ESP32_S3_CAM_TARGET_DETECTION_MAX_COUNT) {
        parse_count = ESP32_S3_CAM_TARGET_DETECTION_MAX_COUNT;
    }

    for (uint8_t i = 0; i < parse_count; i++) {
        const uint8_t *item_data = &data[1U + ((size_t)i * ESP32_S3_CAM_TARGET_DETECTION_RECORD_SIZE)];
        if (item_data[0] > ESP32_S3_CAM_YOLO11N_CLASS_MAX_ID) {
            return ESP32_S3_CAM_ERR_INVALID_RESPONSE;
        }

        result->items[i].index = item_data[0];
        result->items[i].x = esp32_s3_cam_read_u16_le(&item_data[1]);
        result->items[i].y = esp32_s3_cam_read_u16_le(&item_data[3]);
        result->items[i].width = esp32_s3_cam_read_u16_le(&item_data[5]);
        result->items[i].height = esp32_s3_cam_read_u16_le(&item_data[7]);
    }

    result->parsed_count = parse_count;
    return ESP32_S3_CAM_OK;
}

static esp32_s3_cam_err_t esp32_s3_cam_get_target_detection_result(esp32_s3_cam_handle_t *handle,
                                                             const char *command,
                                                             esp32_s3_cam_target_detection_result_t *result,
                                                             uint32_t timeout_ms)
{
    uint8_t packet_data[ESP32_S3_CAM_PACKET_BUFFER_SIZE] = {0};
    size_t packet_len = 0;

    if (handle == NULL || command == NULL || result == NULL) {
        return ESP32_S3_CAM_ERR_INVALID_ARG;
    }

    esp32_s3_cam_err_t ret = esp32_s3_cam_send_json(handle, command, timeout_ms);
    if (ret != ESP32_S3_CAM_OK) {
        return ret;
    }

    esp32_s3_cam_wait_response_ready();

    ret = esp32_s3_cam_receive_packet(handle,
                                   packet_data,
                                   sizeof(packet_data),
                                   &packet_len,
                                   timeout_ms);
    if (ret != ESP32_S3_CAM_OK) {
        return ret;
    }

    return esp32_s3_cam_parse_target_detection_data(packet_data, packet_len, result);
}

esp32_s3_cam_err_t esp32_s3_cam_get_cat_dog_result(esp32_s3_cam_handle_t *handle,
                                             esp32_s3_cam_target_detection_result_t *result,
                                             uint32_t timeout_ms)
{
    return esp32_s3_cam_get_target_detection_result(handle, ESP32_S3_CAM_CAT_DOG_COMMAND, result, timeout_ms);
}

esp32_s3_cam_err_t esp32_s3_cam_get_cat_result(esp32_s3_cam_handle_t *handle,
                                         esp32_s3_cam_target_detection_result_t *result,
                                         uint32_t timeout_ms)
{
    return esp32_s3_cam_get_target_detection_result(handle, ESP32_S3_CAM_CAT_COMMAND, result, timeout_ms);
}

esp32_s3_cam_err_t esp32_s3_cam_get_dog_result(esp32_s3_cam_handle_t *handle,
                                         esp32_s3_cam_target_detection_result_t *result,
                                         uint32_t timeout_ms)
{
    return esp32_s3_cam_get_target_detection_result(handle, ESP32_S3_CAM_DOG_COMMAND, result, timeout_ms);
}

esp32_s3_cam_err_t esp32_s3_cam_get_pedestrian_result(esp32_s3_cam_handle_t *handle,
                                                esp32_s3_cam_target_detection_result_t *result,
                                                uint32_t timeout_ms)
{
    return esp32_s3_cam_get_target_detection_result(handle, ESP32_S3_CAM_PEDESTRIAN_COMMAND, result, timeout_ms);
}

esp32_s3_cam_err_t esp32_s3_cam_get_yolo11n_result(esp32_s3_cam_handle_t *handle,
                                             esp32_s3_cam_target_detection_result_t *result,
                                             uint32_t timeout_ms)
{
    uint8_t packet_data[ESP32_S3_CAM_PACKET_BUFFER_SIZE] = {0};
    size_t packet_len = 0;

    if (handle == NULL || result == NULL) {
        return ESP32_S3_CAM_ERR_INVALID_ARG;
    }

    esp32_s3_cam_err_t ret = esp32_s3_cam_send_json(handle, ESP32_S3_CAM_YOLO11N_COMMAND, timeout_ms);
    if (ret != ESP32_S3_CAM_OK) {
        return ret;
    }

    esp32_s3_cam_wait_response_ready();

    ret = esp32_s3_cam_receive_packet(handle,
                                   packet_data,
                                   sizeof(packet_data),
                                   &packet_len,
                                   timeout_ms);
    if (ret != ESP32_S3_CAM_OK) {
        return ret;
    }

    return esp32_s3_cam_parse_yolo11n_detection_data(packet_data, packet_len, result);
}

esp32_s3_cam_err_t esp32_s3_cam_get_face_result(esp32_s3_cam_handle_t *handle,
                                          esp32_s3_cam_target_detection_result_t *result,
                                          uint32_t timeout_ms)
{
    return esp32_s3_cam_get_target_detection_result(handle, ESP32_S3_CAM_FACE_COMMAND, result, timeout_ms);
}

static esp32_s3_cam_err_t esp32_s3_cam_parse_pose_data(const uint8_t *data,
                                                 size_t data_len,
                                                 esp32_s3_cam_pose_result_t *result)
{
    uint8_t parse_count = 0;
    size_t offset = 1U;

    if (data == NULL || result == NULL || data_len < 1U) {
        return ESP32_S3_CAM_ERR_INVALID_ARG;
    }

    memset(result, 0, sizeof(*result));
    result->total_count = data[0];

    if (result->total_count == 0U) {
        return ESP32_S3_CAM_OK;
    }

    parse_count = result->total_count;
    if (parse_count > ESP32_S3_CAM_POSE_MAX_COUNT) {
        parse_count = ESP32_S3_CAM_POSE_MAX_COUNT;
    }

    for (uint8_t i = 0; i < result->total_count; i++) {
        if (offset > data_len || data_len - offset < ESP32_S3_CAM_POSE_RECORD_HEADER_SIZE) {
            return ESP32_S3_CAM_ERR_INVALID_SIZE;
        }

        const uint8_t *item_data = &data[offset];
        uint8_t keypoint_count = item_data[9];
        size_t record_size = ESP32_S3_CAM_POSE_RECORD_HEADER_SIZE +
                             ((size_t)keypoint_count * ESP32_S3_CAM_POSE_KEYPOINT_RECORD_SIZE);

        if (keypoint_count > ESP32_S3_CAM_POSE_KEYPOINT_COUNT ||
            data_len - offset < record_size) {
            return ESP32_S3_CAM_ERR_INVALID_SIZE;
        }

        if (i < parse_count) {
            result->items[i].index = item_data[0];
            result->items[i].x = esp32_s3_cam_read_u16_le(&item_data[1]);
            result->items[i].y = esp32_s3_cam_read_u16_le(&item_data[3]);
            result->items[i].width = esp32_s3_cam_read_u16_le(&item_data[5]);
            result->items[i].height = esp32_s3_cam_read_u16_le(&item_data[7]);
            result->items[i].keypoint_count = keypoint_count;

            for (uint8_t k = 0; k < keypoint_count; k++) {
                const uint8_t *keypoint_data = &item_data[10U + ((size_t)k * ESP32_S3_CAM_POSE_KEYPOINT_RECORD_SIZE)];
                result->items[i].keypoints[k].index = keypoint_data[0];
                result->items[i].keypoints[k].x = esp32_s3_cam_read_u16_le(&keypoint_data[1]);
                result->items[i].keypoints[k].y = esp32_s3_cam_read_u16_le(&keypoint_data[3]);
            }
        }

        offset += record_size;
    }

    result->parsed_count = parse_count;
    return ESP32_S3_CAM_OK;
}

static esp32_s3_cam_err_t esp32_s3_cam_get_pose_like_result(esp32_s3_cam_handle_t *handle,
                                                      const char *command,
                                                      esp32_s3_cam_pose_result_t *result,
                                                      uint32_t timeout_ms)
{
    uint8_t packet_data[ESP32_S3_CAM_PACKET_BUFFER_SIZE] = {0};
    size_t packet_len = 0;

    if (handle == NULL || command == NULL || result == NULL) {
        return ESP32_S3_CAM_ERR_INVALID_ARG;
    }

    esp32_s3_cam_err_t ret = esp32_s3_cam_send_json(handle, command, timeout_ms);
    if (ret != ESP32_S3_CAM_OK) {
        return ret;
    }

    esp32_s3_cam_wait_response_ready();

    ret = esp32_s3_cam_receive_packet(handle,
                                   packet_data,
                                   sizeof(packet_data),
                                   &packet_len,
                                   timeout_ms);
    if (ret != ESP32_S3_CAM_OK) {
        return ret;
    }

    return esp32_s3_cam_parse_pose_data(packet_data, packet_len, result);
}

esp32_s3_cam_err_t esp32_s3_cam_get_pose_result(esp32_s3_cam_handle_t *handle,
                                          esp32_s3_cam_pose_result_t *result,
                                          uint32_t timeout_ms)
{
    return esp32_s3_cam_get_pose_like_result(handle, ESP32_S3_CAM_POSE_COMMAND, result, timeout_ms);
}

static esp32_s3_cam_err_t esp32_s3_cam_parse_apriltag_data(const uint8_t *data,
                                                     size_t data_len,
                                                     esp32_s3_cam_apriltag_result_t *result)
{
    uint8_t parse_count = 0;

    if (data == NULL || result == NULL || data_len < 1U) {
        return ESP32_S3_CAM_ERR_INVALID_ARG;
    }

    memset(result, 0, sizeof(*result));
    result->total_count = data[0];

    if (result->total_count == 0U) {
        return ESP32_S3_CAM_OK;
    }

    if (data_len < 1U + ((size_t)result->total_count * ESP32_S3_CAM_APRILTAG_RECORD_SIZE)) {
        return ESP32_S3_CAM_ERR_INVALID_SIZE;
    }

    parse_count = result->total_count;
    if (parse_count > ESP32_S3_CAM_APRILTAG_MAX_COUNT) {
        parse_count = ESP32_S3_CAM_APRILTAG_MAX_COUNT;
    }

    for (uint8_t i = 0; i < parse_count; i++) {
        const uint8_t *item_data = &data[1U + ((size_t)i * ESP32_S3_CAM_APRILTAG_RECORD_SIZE)];
        result->items[i].id = esp32_s3_cam_read_u16_le(&item_data[0]);
        result->items[i].x = esp32_s3_cam_read_u16_le(&item_data[2]);
        result->items[i].y = esp32_s3_cam_read_u16_le(&item_data[4]);
        result->items[i].width = esp32_s3_cam_read_u16_le(&item_data[6]);
        result->items[i].height = esp32_s3_cam_read_u16_le(&item_data[8]);
        result->items[i].rotation = esp32_s3_cam_read_float_le(&item_data[10]);
        result->items[i].x_translation = esp32_s3_cam_read_float_le(&item_data[14]);
        result->items[i].y_translation = esp32_s3_cam_read_float_le(&item_data[18]);
        result->items[i].z_translation = esp32_s3_cam_read_float_le(&item_data[22]);
        result->items[i].x_rotation = esp32_s3_cam_read_float_le(&item_data[26]);
        result->items[i].y_rotation = esp32_s3_cam_read_float_le(&item_data[30]);
        result->items[i].z_rotation = esp32_s3_cam_read_float_le(&item_data[34]);
    }

    result->parsed_count = parse_count;
    return ESP32_S3_CAM_OK;
}

static esp32_s3_cam_err_t esp32_s3_cam_get_apriltag_like_result(esp32_s3_cam_handle_t *handle,
                                                          const char *command,
                                                          esp32_s3_cam_apriltag_result_t *result,
                                                          uint32_t timeout_ms)
{
    uint8_t packet_data[ESP32_S3_CAM_PACKET_BUFFER_SIZE] = {0};
    size_t packet_len = 0;

    if (handle == NULL || command == NULL || result == NULL) {
        return ESP32_S3_CAM_ERR_INVALID_ARG;
    }

    esp32_s3_cam_err_t ret = esp32_s3_cam_send_json(handle, command, timeout_ms);
    if (ret != ESP32_S3_CAM_OK) {
        return ret;
    }

    esp32_s3_cam_wait_response_ready();

    ret = esp32_s3_cam_receive_packet(handle,
                                   packet_data,
                                   sizeof(packet_data),
                                   &packet_len,
                                   timeout_ms);
    if (ret != ESP32_S3_CAM_OK) {
        return ret;
    }

    return esp32_s3_cam_parse_apriltag_data(packet_data, packet_len, result);
}

esp32_s3_cam_err_t esp32_s3_cam_get_apriltag_result(esp32_s3_cam_handle_t *handle,
                                              esp32_s3_cam_apriltag_result_t *result,
                                              uint32_t timeout_ms)
{
    return esp32_s3_cam_get_apriltag_like_result(handle, ESP32_S3_CAM_APRILTAG_COMMAND, result, timeout_ms);
}

static esp32_s3_cam_err_t esp32_s3_cam_parse_qrcode_data(const uint8_t *data,
                                                   size_t data_len,
                                                   esp32_s3_cam_qrcode_result_t *result)
{
    uint8_t parse_count = 0;
    size_t offset = 1U;

    if (data == NULL || result == NULL || data_len < 1U) {
        return ESP32_S3_CAM_ERR_INVALID_ARG;
    }

    memset(result, 0, sizeof(*result));
    result->total_count = data[0];

    if (result->total_count == 0U) {
        return ESP32_S3_CAM_OK;
    }

    parse_count = result->total_count;
    if (parse_count > ESP32_S3_CAM_QRCODE_MAX_COUNT) {
        parse_count = ESP32_S3_CAM_QRCODE_MAX_COUNT;
    }

    for (uint8_t i = 0; i < result->total_count; i++) {
        if (offset > data_len || data_len - offset < ESP32_S3_CAM_QRCODE_RECORD_HEADER_SIZE) {
            return ESP32_S3_CAM_ERR_INVALID_SIZE;
        }

        const uint8_t *item_data = &data[offset];
        uint16_t text_len = esp32_s3_cam_read_u16_le(&item_data[9]);
        if (data_len - offset - ESP32_S3_CAM_QRCODE_RECORD_HEADER_SIZE < text_len) {
            return ESP32_S3_CAM_ERR_INVALID_SIZE;
        }

        if (i < parse_count) {
            size_t copy_len = text_len;
            if (copy_len > ESP32_S3_CAM_QRCODE_MAX_TEXT_LEN) {
                copy_len = ESP32_S3_CAM_QRCODE_MAX_TEXT_LEN;
            }

            result->items[i].index = item_data[0];
            result->items[i].x = esp32_s3_cam_read_u16_le(&item_data[1]);
            result->items[i].y = esp32_s3_cam_read_u16_le(&item_data[3]);
            result->items[i].width = esp32_s3_cam_read_u16_le(&item_data[5]);
            result->items[i].height = esp32_s3_cam_read_u16_le(&item_data[7]);
            result->items[i].text_len = text_len;
            memcpy(result->items[i].text, &item_data[11], copy_len);
            result->items[i].text[copy_len] = '\0';
        }

        offset += ESP32_S3_CAM_QRCODE_RECORD_HEADER_SIZE + (size_t)text_len;
    }

    result->parsed_count = parse_count;
    return ESP32_S3_CAM_OK;
}

esp32_s3_cam_err_t esp32_s3_cam_get_qrcode_result(esp32_s3_cam_handle_t *handle,
                                            esp32_s3_cam_qrcode_result_t *result,
                                            uint32_t timeout_ms)
{
    uint8_t packet_data[ESP32_S3_CAM_PACKET_BUFFER_SIZE] = {0};
    size_t packet_len = 0;

    if (handle == NULL || result == NULL) {
        return ESP32_S3_CAM_ERR_INVALID_ARG;
    }

    esp32_s3_cam_err_t ret = esp32_s3_cam_send_json(handle, ESP32_S3_CAM_QRCODE_COMMAND, timeout_ms);
    if (ret != ESP32_S3_CAM_OK) {
        return ret;
    }

    esp32_s3_cam_wait_response_ready();

    ret = esp32_s3_cam_receive_packet(handle,
                                   packet_data,
                                   sizeof(packet_data),
                                   &packet_len,
                                   timeout_ms);
    if (ret != ESP32_S3_CAM_OK) {
        return ret;
    }

    return esp32_s3_cam_parse_qrcode_data(packet_data, packet_len, result);
}


esp32_s3_cam_err_t esp32_s3_cam_set_hmirror(esp32_s3_cam_handle_t *handle,
                                      bool enable,
                                      bool *success,
                                      uint32_t timeout_ms)
{
    char command[64] = {0};
    uint8_t packet_data[ESP32_S3_CAM_PACKET_BUFFER_SIZE] = {0};
    size_t packet_len = 0;

    if (handle == NULL || success == NULL) {
        return ESP32_S3_CAM_ERR_INVALID_ARG;
    }

    snprintf(command, sizeof(command), "{\"command\":\"vision_setting\",\"params\":{\"hmirror\":%u}}", enable ? 1U : 0U);

    esp32_s3_cam_err_t ret = esp32_s3_cam_send_json(handle, command, timeout_ms);
    if (ret != ESP32_S3_CAM_OK) {
        return ret;
    }

    esp32_s3_cam_wait_response_ready();

    ret = esp32_s3_cam_receive_packet(handle, packet_data, sizeof(packet_data), &packet_len, timeout_ms);
    if (ret != ESP32_S3_CAM_OK) {
        return ret;
    }

    if (packet_len < 1U) {
        return ESP32_S3_CAM_ERR_INVALID_SIZE;
    }

    *success = (packet_data[0] == 1U);
    return ESP32_S3_CAM_OK;
}

esp32_s3_cam_err_t esp32_s3_cam_set_vflip(esp32_s3_cam_handle_t *handle,
                                    bool enable,
                                    bool *success,
                                    uint32_t timeout_ms)
{
    char command[64] = {0};
    uint8_t packet_data[ESP32_S3_CAM_PACKET_BUFFER_SIZE] = {0};
    size_t packet_len = 0;

    if (handle == NULL || success == NULL) {
        return ESP32_S3_CAM_ERR_INVALID_ARG;
    }

    snprintf(command, sizeof(command), "{\"command\":\"vision_setting\",\"params\":{\"vflip\":%u}}", enable ? 1U : 0U);

    esp32_s3_cam_err_t ret = esp32_s3_cam_send_json(handle, command, timeout_ms);
    if (ret != ESP32_S3_CAM_OK) {
        return ret;
    }

    esp32_s3_cam_wait_response_ready();

    ret = esp32_s3_cam_receive_packet(handle, packet_data, sizeof(packet_data), &packet_len, timeout_ms);
    if (ret != ESP32_S3_CAM_OK) {
        return ret;
    }

    if (packet_len < 1U) {
        return ESP32_S3_CAM_ERR_INVALID_SIZE;
    }

    *success = (packet_data[0] == 1U);
    return ESP32_S3_CAM_OK;
}

esp32_s3_cam_err_t esp32_s3_cam_module_reset(esp32_s3_cam_handle_t *handle,
                                       uint32_t timeout_ms)
{
    if (handle == NULL) {
        return ESP32_S3_CAM_ERR_INVALID_ARG;
    }

    return esp32_s3_cam_send_json(handle, ESP32_S3_CAM_MODULE_RESET_COMMAND, timeout_ms);
}

static esp32_s3_cam_err_t esp32_s3_cam_parse_face_recognition_data(const uint8_t *data,
                                                             size_t data_len,
                                                             esp32_s3_cam_face_recognition_result_t *result)
{
    uint8_t parse_count = 0;

    if (data == NULL || result == NULL || data_len < 1U) {
        return ESP32_S3_CAM_ERR_INVALID_ARG;
    }

    memset(result, 0, sizeof(*result));
    result->total_count = data[0];

    if (result->total_count == 0U) {
        return ESP32_S3_CAM_OK;
    }

    if (data_len < 1U + ((size_t)result->total_count * ESP32_S3_CAM_FACE_RECOGNITION_RECORD_SIZE)) {
        return ESP32_S3_CAM_ERR_INVALID_SIZE;
    }

    parse_count = result->total_count;
    if (parse_count > ESP32_S3_CAM_FACE_RECOGNITION_MAX_COUNT) {
        parse_count = ESP32_S3_CAM_FACE_RECOGNITION_MAX_COUNT;
    }

    for (uint8_t i = 0; i < parse_count; i++) {
        const uint8_t *item_data = &data[1U + ((size_t)i * ESP32_S3_CAM_FACE_RECOGNITION_RECORD_SIZE)];
        result->items[i].id = esp32_s3_cam_read_u16_le(&item_data[0]);
        result->items[i].x = esp32_s3_cam_read_u16_le(&item_data[2]);
        result->items[i].y = esp32_s3_cam_read_u16_le(&item_data[4]);
        result->items[i].width = esp32_s3_cam_read_u16_le(&item_data[6]);
        result->items[i].height = esp32_s3_cam_read_u16_le(&item_data[8]);
    }

    result->parsed_count = parse_count;
    return ESP32_S3_CAM_OK;
}

esp32_s3_cam_err_t esp32_s3_cam_get_face_recognition_result(esp32_s3_cam_handle_t *handle,
                                                      esp32_s3_cam_face_recognition_result_t *result,
                                                      uint32_t timeout_ms)
{
    uint8_t packet_data[ESP32_S3_CAM_PACKET_BUFFER_SIZE] = {0};
    size_t packet_len = 0;

    if (handle == NULL || result == NULL) {
        return ESP32_S3_CAM_ERR_INVALID_ARG;
    }

    esp32_s3_cam_err_t ret = esp32_s3_cam_send_json(handle, ESP32_S3_CAM_FACE_RECOGNITION_COMMAND, timeout_ms);
    if (ret != ESP32_S3_CAM_OK) {
        return ret;
    }

    esp32_s3_cam_wait_response_ready();

    ret = esp32_s3_cam_receive_packet(handle,
                                   packet_data,
                                   sizeof(packet_data),
                                   &packet_len,
                                   timeout_ms);
    if (ret != ESP32_S3_CAM_OK) {
        return ret;
    }

    return esp32_s3_cam_parse_face_recognition_data(packet_data, packet_len, result);
}

esp32_s3_cam_err_t esp32_s3_cam_face_register(esp32_s3_cam_handle_t *handle,
                                        bool *success,
                                        uint16_t *face_id,
                                        uint32_t timeout_ms)
{
    uint8_t packet_data[ESP32_S3_CAM_PACKET_BUFFER_SIZE] = {0};
    size_t packet_len = 0;

    if (handle == NULL || success == NULL || face_id == NULL) {
        return ESP32_S3_CAM_ERR_INVALID_ARG;
    }

    *success = false;
    *face_id = 0U;

    esp32_s3_cam_err_t ret = esp32_s3_cam_send_json(handle, ESP32_S3_CAM_FACE_REGISTER_COMMAND, timeout_ms);
    if (ret != ESP32_S3_CAM_OK) {
        return ret;
    }

    esp32_s3_cam_wait_response_ready();

    ret = esp32_s3_cam_receive_packet(handle,
                                   packet_data,
                                   sizeof(packet_data),
                                   &packet_len,
                                   timeout_ms);
    if (ret != ESP32_S3_CAM_OK) {
        return ret;
    }

    if (packet_len < 1U) {
        return ESP32_S3_CAM_ERR_INVALID_SIZE;
    }

    *success = (packet_data[0] == 1U);
    if (*success && packet_len >= 3U) {
        *face_id = esp32_s3_cam_read_u16_le(&packet_data[1]);
    }

    return ESP32_S3_CAM_OK;
}

esp32_s3_cam_err_t esp32_s3_cam_face_delete(esp32_s3_cam_handle_t *handle,
                                      uint16_t face_id,
                                      bool *success,
                                      uint32_t timeout_ms)
{
    char command[64] = {0};
    uint8_t packet_data[ESP32_S3_CAM_PACKET_BUFFER_SIZE] = {0};
    size_t packet_len = 0;

    if (handle == NULL || success == NULL) {
        return ESP32_S3_CAM_ERR_INVALID_ARG;
    }

    *success = false;
    snprintf(command, sizeof(command), "{\"command\":\"face_delete\",\"params\":%u}", (unsigned int)face_id);

    esp32_s3_cam_err_t ret = esp32_s3_cam_send_json(handle, command, timeout_ms);
    if (ret != ESP32_S3_CAM_OK) {
        return ret;
    }

    esp32_s3_cam_wait_response_ready();

    ret = esp32_s3_cam_receive_packet(handle,
                                   packet_data,
                                   sizeof(packet_data),
                                   &packet_len,
                                   timeout_ms);
    if (ret != ESP32_S3_CAM_OK) {
        return ret;
    }

    if (packet_len < 1U) {
        return ESP32_S3_CAM_ERR_INVALID_SIZE;
    }

    *success = (packet_data[0] == 1U);
    return ESP32_S3_CAM_OK;
}

esp32_s3_cam_err_t esp32_s3_cam_get_face_count(esp32_s3_cam_handle_t *handle,
                                         uint8_t *count,
                                         uint32_t timeout_ms)
{
    uint8_t packet_data[ESP32_S3_CAM_PACKET_BUFFER_SIZE] = {0};
    size_t packet_len = 0;

    if (handle == NULL || count == NULL) {
        return ESP32_S3_CAM_ERR_INVALID_ARG;
    }

    *count = 0U;

    esp32_s3_cam_err_t ret = esp32_s3_cam_send_json(handle, ESP32_S3_CAM_FACE_COUNT_COMMAND, timeout_ms);
    if (ret != ESP32_S3_CAM_OK) {
        return ret;
    }

    esp32_s3_cam_wait_response_ready();

    ret = esp32_s3_cam_receive_packet(handle,
                                   packet_data,
                                   sizeof(packet_data),
                                   &packet_len,
                                   timeout_ms);
    if (ret != ESP32_S3_CAM_OK) {
        return ret;
    }

    if (packet_len < 1U) {
        return ESP32_S3_CAM_ERR_INVALID_SIZE;
    }

    *count = packet_data[0];
    return ESP32_S3_CAM_OK;
}
