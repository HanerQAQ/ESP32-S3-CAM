#include "s3_cam_color_detection.hpp"
#include "color_adjust_protocol.hpp"
#include "img_converters.h"
#include "my_usart.h"
#include "my_user_iic.h"
#include "esp_log.h"
#include "esp_camera.h"
#include "esp_timer.h"

#include "dl_image.hpp"
#include "fb_gfx.h"
#include "color_detector.hpp"
#include "s3_cam_camera.h"

#include "s3_cam_ai_utils.hpp"
#include "my_usart1_user.h"

#include <algorithm>
#include <cmath>

using namespace std;
using namespace dl;

static const char *TAG = "color_detection";

static QueueHandle_t xQueueFrameI = NULL;
static QueueHandle_t xQueueEvent = NULL;
static QueueHandle_t xQueueFrameO = NULL;
static QueueHandle_t xQueueResult = NULL;

static color_detection_state_t gEvent = COLOR_DETECTION_IDLE;
static bool register_mode = false;
static bool gReturnFB = true;
// Default draw_box = false: web browser SVG overlay draws vector boxes, keeping video stream 100% clean and pristine
static bool draw_box = false;

static SemaphoreHandle_t xMutex = NULL;

static SemaphoreHandle_t s_snapshot_req_mutex = NULL;
static SemaphoreHandle_t s_snapshot_done_sem = NULL;
static volatile bool s_snapshot_requested = false;
static uint8_t *s_snapshot_jpeg_buf = NULL;
static size_t s_snapshot_jpeg_len = 0;

static SemaphoreHandle_t s_raw_req_mutex = NULL;
static SemaphoreHandle_t s_raw_done_sem = NULL;
static volatile bool s_raw_requested = false;
static uint8_t *s_raw_buf = NULL;
static size_t s_raw_len = 0;
static uint16_t s_raw_w = 0;
static uint16_t s_raw_h = 0;

static volatile bool s_reload_colors_flag = false;

// Global detection JSON for /api/detections
static SemaphoreHandle_t s_json_mutex = NULL;
static char s_detections_json[1536] = "{\"title\":\"Color Detection\",\"type\":\"color\",\"width\":320,\"height\":240,\"streamFps\":10,\"age_ms\":0,\"enabled\":true,\"objects\":[]}";
static int64_t s_last_detect_time_us = 0;

bool color_detection_get_frame_jpeg(uint8_t **out_buf, size_t *out_len, TickType_t timeout_ticks)
{
    if (!out_buf || !out_len) return false;
    if (!s_snapshot_req_mutex) {
        s_snapshot_req_mutex = xSemaphoreCreateMutex();
        s_snapshot_done_sem = xSemaphoreCreateBinary();
    }
    if (xSemaphoreTake(s_snapshot_req_mutex, timeout_ticks) != pdTRUE) {
        return false;
    }

    xSemaphoreTake(s_snapshot_done_sem, 0);
    s_snapshot_jpeg_buf = NULL;
    s_snapshot_jpeg_len = 0;
    s_snapshot_requested = true;

    bool ok = false;
    if (xSemaphoreTake(s_snapshot_done_sem, timeout_ticks) == pdTRUE) {
        if (s_snapshot_jpeg_buf && s_snapshot_jpeg_len > 0) {
            *out_buf = s_snapshot_jpeg_buf;
            *out_len = s_snapshot_jpeg_len;
            ok = true;
        }
    }
    s_snapshot_requested = false;
    xSemaphoreGive(s_snapshot_req_mutex);
    return ok;
}

bool color_detection_get_frame_raw(uint8_t **out_buf, size_t *out_len, uint16_t *width, uint16_t *height, TickType_t timeout_ticks)
{
    if (!out_buf || !out_len) return false;
    if (!s_raw_req_mutex) {
        s_raw_req_mutex = xSemaphoreCreateMutex();
        s_raw_done_sem = xSemaphoreCreateBinary();
    }
    if (xSemaphoreTake(s_raw_req_mutex, timeout_ticks) != pdTRUE) {
        return false;
    }

    xSemaphoreTake(s_raw_done_sem, 0);
    s_raw_buf = NULL;
    s_raw_len = 0;
    s_raw_requested = true;

    bool ok = false;
    if (xSemaphoreTake(s_raw_done_sem, timeout_ticks) == pdTRUE) {
        if (s_raw_buf && s_raw_len > 0) {
            *out_buf = s_raw_buf;
            *out_len = s_raw_len;
            if (width) *width = s_raw_w;
            if (height) *height = s_raw_h;
            ok = true;
        }
    }
    s_raw_requested = false;
    xSemaphoreGive(s_raw_req_mutex);
    return ok;
}

void s3_cam_color_get_detections_json(char *buf, size_t max_len)
{
    if (!buf || max_len == 0) return;
    if (s_json_mutex && xSemaphoreTake(s_json_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        int64_t now_us = esp_timer_get_time();
        int age_ms = (s_last_detect_time_us > 0) ? (int)((now_us - s_last_detect_time_us) / 1000) : 0;
        if (age_ms < 0) age_ms = 0;
        if (age_ms > 2000) age_ms = 2000;
        
        // Dynamically update age_ms in returned JSON if buffer fits
        snprintf(buf, max_len, "%s", s_detections_json);
        xSemaphoreGive(s_json_mutex);
    } else {
        snprintf(buf, max_len, "{\"title\":\"Color Detection\",\"type\":\"color\",\"width\":320,\"height\":240,\"streamFps\":10,\"age_ms\":100,\"enabled\":true,\"objects\":[]}");
    }
}

#define COLOR_NUM 5

// Standard ColorTool / P4C5 Order (1: Red, 2: Blue, 3: Green, 4: Yellow, 5: Purple)
// HSV ranges tuned with S_min >= 85 for Red to strictly reject human skin tone!
vector<color_info_t> std_color_info = {
    {{158, 10, 85, 255, 70, 255}, 150, "Red"},     // 0 (id 1): Red
    {{90, 130, 55, 255, 45, 255}, 150, "Blue"},    // 1 (id 2): Blue
    {{38, 85, 50, 255, 45, 255},  150, "Green"},   // 2 (id 3): Green
    {{18, 35, 105, 255, 80, 255}, 150, "Yellow"},  // 3 (id 4): Yellow
    {{132, 158, 50, 255, 45, 255}, 150, "Purple"}  // 4 (id 5): Purple
};

static const char *color_hex_strings[COLOR_NUM] = {
    "#ff0000", // Red
    "#0000ff", // Blue
    "#00ff00", // Green
    "#ffff00", // Yellow
    "#ff00ff"  // Purple
};

static const uint16_t draw_lcd_colors[COLOR_NUM] = {
    0x00F8, // Red
    0x1F00, // Blue
    0xE007, // Green
    0xE0FF, // Yellow
    0x1EA1  // Purple
};

static const uint32_t draw_text_colors[COLOR_NUM] = {
    0xF800, // Red
    0x001F, // Blue
    0x07E0, // Green
    0xFFE0, // Yellow
    0xF81F  // Purple
};

static bool pixel_matches_hsv_thresh(uint16_t raw_pix, const vector<uint8_t> &thresh)
{
    if (thresh.size() < 6) return false;
    uint16_t pixel = __builtin_bswap16(raw_pix);
    int r5 = (pixel >> 11) & 0x1F;
    int g6 = (pixel >> 5) & 0x3F;
    int b5 = pixel & 0x1F;
    int r = (r5 << 3) | (r5 >> 2);
    int g = (g6 << 2) | (g6 >> 4);
    int b = (b5 << 3) | (b5 >> 2);

    int max_v = std::max({r, g, b});
    int min_v = std::min({r, g, b});
    int delta = max_v - min_v;
    int h = 0;
    if (delta != 0) {
        if (max_v == r) {
            h = 30 * (g - b) / delta;
            if (h < 0) h += 180;
        } else if (max_v == g) {
            h = 60 + 30 * (b - r) / delta;
        } else {
            h = 120 + 30 * (r - g) / delta;
        }
    }
    int s = (max_v == 0) ? 0 : (255 * delta) / max_v;
    int v = max_v;

    bool hue_match = (thresh[0] <= thresh[1])
                         ? (h >= thresh[0] && h <= thresh[1])
                         : (h >= thresh[0] || h <= thresh[1]);
    return hue_match && s >= thresh[2] && s <= thresh[3] && v >= thresh[4] && v <= thresh[5];
}

static bool detect_line_in_frame(const camera_fb_t *frame, const vector<uint8_t> &thresh,
                                 uint16_t *out_sx, uint16_t *out_sy,
                                 uint16_t *out_ex, uint16_t *out_ey,
                                 float *out_angle, float *out_offset,
                                 int *out_box)
{
    if (!frame || !frame->buf || frame->width < 16 || frame->height < 16) return false;
    int w = (int)frame->width;
    int h = (int)frame->height;
    int roi_y0 = std::clamp((int)std::lround(h * 0.45f), 0, h - 2);
    int roi_y1 = h;
    const uint16_t *pixels = (const uint16_t *)frame->buf;

    int64_t n = 0, sum_x = 0, sum_y = 0, sum_yy = 0, sum_xy = 0;
    int min_x = w - 1, max_x = 0, min_y = roi_y1 - 1, max_y = roi_y0;
    for (int y = roi_y0; y < roi_y1; y += 2) {
        const uint16_t *row = pixels + y * w;
        for (int x = (y & 2) ? 1 : 0; x < w; x += 2) {
            if (pixel_matches_hsv_thresh(row[x], thresh)) {
                n++;
                sum_x += x;
                sum_y += y;
                sum_yy += (int64_t)y * y;
                sum_xy += (int64_t)x * y;
                min_x = std::min(min_x, x);
                max_x = std::max(max_x, x);
                min_y = std::min(min_y, y);
                max_y = std::max(max_y, y);
            }
        }
    }
    if (n < 120) return false;

    int64_t denom = n * sum_yy - sum_y * sum_y;
    if (denom == 0) return false;

    float slope = (float)(n * sum_xy - sum_x * sum_y) / (float)denom;
    float intercept = ((float)sum_x - slope * (float)sum_y) / (float)n;
    int y0 = roi_y0;
    int y1 = roi_y1 - 1;
    int x0 = std::clamp((int)std::lround(slope * y0 + intercept), 0, w - 1);
    int x1 = std::clamp((int)std::lround(slope * y1 + intercept), 0, w - 1);

    float dx = (float)(x1 - x0);
    float dy = (float)(y1 - y0);
    float angle = atan2f(dx, dy) * 180.0f / 3.14159265f;
    float center_x = 0.5f * (float)(x0 + x1);
    float offset = std::clamp((center_x - w * 0.5f) / (w * 0.5f), -1.0f, 1.0f);

    *out_sx = (uint16_t)x0;
    *out_sy = (uint16_t)y0;
    *out_ex = (uint16_t)x1;
    *out_ey = (uint16_t)y1;
    *out_angle = angle;
    *out_offset = offset;
    out_box[0] = std::max(0, min_x);
    out_box[1] = std::max(0, min_y);
    out_box[2] = std::min(w - 1, max_x);
    out_box[3] = std::min(h - 1, max_y);
    return true;
}


void color_detection_apply_vision_config(const VisionConfig &cfg)
{
    auto apply_to_info = [&](vector<color_info_t> &info) {
        for (int i = 0; i < cfg.color_count; i++) {
            const auto &c = cfg.colors[i];
            int idx = -1;
            // Standard ColorTool mapping: 1=Red, 2=Blue, 3=Green, 4=Yellow, 5=Purple
            switch (c.name) {
                case 1: idx = 0; break; // Red
                case 2: idx = 1; break; // Blue
                case 3: idx = 2; break; // Green
                case 4: idx = 3; break; // Yellow
                case 5: idx = 4; break; // Purple
                default: break;
            }
            if (idx >= 0 && idx < (int)info.size()) {
                info[idx].color_thresh[0] = c.hsv_min[0];
                info[idx].color_thresh[1] = c.hsv_max[0];
                info[idx].color_thresh[2] = c.hsv_min[1];
                info[idx].color_thresh[3] = c.hsv_max[1];
                info[idx].color_thresh[4] = c.hsv_min[2];
                info[idx].color_thresh[5] = c.hsv_max[2];
                if (c.area_thr > 0) {
                    info[idx].area_thresh = c.area_thr;
                }
            }
        }
    };

    if (!xMutex) {
        apply_to_info(std_color_info);
        return;
    }

    xSemaphoreTake(xMutex, portMAX_DELAY);
    apply_to_info(std_color_info);
    s_reload_colors_flag = true;
    xSemaphoreGive(xMutex);
}

static void task_process_handler(void *arg)
{
    camera_fb_t *frame = NULL;

    ColorDetector detector;
    detector.set_detection_shape({80, 80, 1});

    // Automatically register all standard colors in order (Red, Blue, Green, Yellow, Purple)
    for (size_t i = 0; i < std_color_info.size(); ++i)
    {
        detector.register_color(std_color_info[i].color_thresh, std_color_info[i].area_thresh, std_color_info[i].name);
    }

    vector<vector<int>> color_thresh_boxes = {{110, 110, 130, 130}, {100, 100, 140, 140}, {90, 90, 150, 150}, {80, 80, 160, 160}, {60, 60, 180, 180}, {40, 40, 200, 200}, {20, 20, 220, 220}};
    int color_thresh_boxes_num = color_thresh_boxes.size();
    int color_thresh_boxes_index = color_thresh_boxes_num / 2;

    vector<int> color_area_threshes = {1, 4, 16, 32, 64, 128, 256, 512, 1024};
    int color_area_thresh_num = color_area_threshes.size();
    int color_area_thresh_index = color_area_thresh_num / 2;
    detector.set_area_thresh({color_area_threshes[color_area_thresh_index]});

    color_detection_state_t _gEvent;
    vector<uint8_t> color_thresh;

    int fps_count = 0;
    int64_t last_fps_time = esp_timer_get_time();
    int current_fps = 10;

    while (true)
    {
        xSemaphoreTake(xMutex, portMAX_DELAY);
        _gEvent = gEvent;
        gEvent = COLOR_DETECTION_IDLE;
        if (s_reload_colors_flag)
        {
            s_reload_colors_flag = false;
            detector.clear_color();
            for (size_t i = 0; i < std_color_info.size(); ++i)
            {
                detector.register_color(std_color_info[i].color_thresh, std_color_info[i].area_thresh, std_color_info[i].name);
            }
        }
        xSemaphoreGive(xMutex);

        if (xQueueReceive(xQueueFrameI, &frame, portMAX_DELAY))
        {
            fps_count++;
            int64_t now_us = esp_timer_get_time();
            if (now_us - last_fps_time >= 1000000) {
                current_fps = fps_count;
                fps_count = 0;
                last_fps_time = now_us;
            }
            // Handle JPEG snapshot request
            if (s_snapshot_requested)
            {
                s_snapshot_requested = false;
                uint8_t *jpg_buf = NULL;
                size_t jpg_len = 0;
                uint8_t jpg_q = app_camera_get_quality();
                if (frame2jpg(frame, jpg_q, &jpg_buf, &jpg_len)) {
                    s_snapshot_jpeg_buf = jpg_buf;
                    s_snapshot_jpeg_len = jpg_len;
                }
                xSemaphoreGive(s_snapshot_done_sem);
            }

            // Handle RAW binary snapshot request
            if (s_raw_requested)
            {
                s_raw_requested = false;
                size_t raw_bytes = frame->len;
                uint8_t *copy_buf = (uint8_t *)malloc(raw_bytes);
                if (copy_buf) {
                    memcpy(copy_buf, frame->buf, raw_bytes);
                    s_raw_buf = copy_buf;
                    s_raw_len = raw_bytes;
                    s_raw_w = frame->width;
                    s_raw_h = frame->height;
                }
                xSemaphoreGive(s_raw_done_sem);
            }

            if (register_mode)
            {
                switch (_gEvent)
                {
                case INCREASE_COLOR_AREA:
                    color_thresh_boxes_index = min(color_thresh_boxes_num - 1, color_thresh_boxes_index + 1);
                    dl::image::draw_hollow_rectangle((uint16_t *)frame->buf, (int)frame->height, (int)frame->width,
                                                     color_thresh_boxes[color_thresh_boxes_index][0],
                                                     color_thresh_boxes[color_thresh_boxes_index][1],
                                                     color_thresh_boxes[color_thresh_boxes_index][2],
                                                     color_thresh_boxes[color_thresh_boxes_index][3],
                                                     draw_lcd_colors[1]);
                    break;

                case DECREASE_COLOR_AREA:
                    color_thresh_boxes_index = max(0, color_thresh_boxes_index - 1);
                    dl::image::draw_hollow_rectangle((uint16_t *)frame->buf, (int)frame->height, (int)frame->width,
                                                     color_thresh_boxes[color_thresh_boxes_index][0],
                                                     color_thresh_boxes[color_thresh_boxes_index][1],
                                                     color_thresh_boxes[color_thresh_boxes_index][2],
                                                     color_thresh_boxes[color_thresh_boxes_index][3],
                                                     draw_lcd_colors[1]);
                    break;

                case REGISTER_COLOR:
                    color_thresh = detector.cal_color_thresh((uint16_t *)frame->buf, {(int)frame->height, (int)frame->width, 3}, color_thresh_boxes[color_thresh_boxes_index]);
                    detector.register_color(color_thresh);
                    xSemaphoreTake(xMutex, portMAX_DELAY);
                    register_mode = false;
                    xSemaphoreGive(xMutex);
                    break;

                case CLOSE_REGISTER_COLOR_BOX:
                    xSemaphoreTake(xMutex, portMAX_DELAY);
                    register_mode = false;
                    xSemaphoreGive(xMutex);
                    break;

                default:
                    dl::image::draw_hollow_rectangle((uint16_t *)frame->buf, (int)frame->height, (int)frame->width,
                                                     color_thresh_boxes[color_thresh_boxes_index][0],
                                                     color_thresh_boxes[color_thresh_boxes_index][1],
                                                     color_thresh_boxes[color_thresh_boxes_index][2],
                                                     color_thresh_boxes[color_thresh_boxes_index][3],
                                                     draw_lcd_colors[1]);
                    break;
                }
            }
            else
            {
                switch (_gEvent)
                {
                case INCREASE_COLOR_AREA:
                    color_area_thresh_index = min(color_area_thresh_num - 1, color_area_thresh_index + 1);
                    detector.set_area_thresh({color_area_threshes[color_area_thresh_index]});
                    break;

                case DECREASE_COLOR_AREA:
                    color_area_thresh_index = max(0, color_area_thresh_index - 1);
                    detector.set_area_thresh({color_area_threshes[color_area_thresh_index]});
                    break;

                case DELETE_COLOR:
                    detector.delete_color();
                    break;

                default:
                {
                    uint8_t i2c_mode = 0;
                    uint8_t i2c_targets[5] = {1, 2, 3, 4, 5};
                    uint8_t i2c_target_cnt = 5;
                    i2c_proto_get_color_config(&i2c_mode, i2c_targets, &i2c_target_cnt);

                    auto is_target_enabled = [&](uint8_t color_id) -> bool {
                        for (uint8_t ti = 0; ti < i2c_target_cnt; ++ti) {
                            if (i2c_targets[ti] == color_id) return true;
                        }
                        return false;
                    };

                    if (i2c_mode == 1)
                    {
                        uint8_t line_id = 1;
                        for (uint8_t ti = 0; ti < i2c_target_cnt; ++ti) {
                            if (i2c_targets[ti] >= 1 && i2c_targets[ti] <= COLOR_NUM) {
                                line_id = i2c_targets[ti];
                                break;
                            }
                        }
                        int line_idx = line_id - 1;
                        uint16_t sx = 0, sy = 0, ex = 0, ey = 0;
                        float angle = 0.0f, offset = 0.0f;
                        int box4[4] = {0, 0, 0, 0};
                        bool line_ok = detect_line_in_frame(frame, std_color_info[line_idx].color_thresh,
                                                            &sx, &sy, &ex, &ey, &angle, &offset, box4);
                        i2c_proto_set_line_result(line_ok, sx, sy, ex, ey, angle, offset);

                        string line_objects_json = "[";
                        if (line_ok)
                        {
                            int bw = std::max(1, box4[2] - box4[0]);
                            int bh = std::max(1, box4[3] - box4[1]);
                            char obj_buf[192];
                            snprintf(obj_buf, sizeof(obj_buf),
                                     "{\"label\":\"%s(Line)\",\"category\":%d,\"score\":1.0,\"color\":\"%s\",\"box\":[%d,%d,%d,%d],\"line\":[%u,%u,%u,%u],\"angle\":%.2f,\"offset\":%.3f}",
                                     std_color_info[line_idx].name.c_str(), line_idx, color_hex_strings[line_idx],
                                     box4[0], box4[1], bw, bh, sx, sy, ex, ey, angle, offset);
                            line_objects_json += obj_buf;

                            int tx1 = box4[0] * 320 / (int)frame->width;
                            int ty1 = box4[1] * 240 / (int)frame->height;
                            int tx2 = box4[2] * 320 / (int)frame->width;
                            int ty2 = box4[3] * 240 / (int)frame->height;
                            set_IIC_data(tx1, ty1, tx2, ty2);
                            set_IIC_data_id(0);
                        }
                        else
                        {
                            set_IIC_data(0, 0, 320, 240);
                            set_IIC_data_id(0);
                        }
                        line_objects_json += "]";

                        if (s_json_mutex && xSemaphoreTake(s_json_mutex, 0) == pdTRUE)
                        {
                            snprintf(s_detections_json, sizeof(s_detections_json),
                                     "{\"title\":\"Color / Line Detection\",\"type\":\"color\",\"mode\":1,\"width\":%d,\"height\":%d,\"streamFps\":%d,\"age_ms\":10,\"enabled\":true,\"objects\":%s}",
                                     (int)frame->width, (int)frame->height, current_fps, line_objects_json.c_str());
                            s_last_detect_time_us = esp_timer_get_time();
                            xSemaphoreGive(s_json_mutex);
                        }
                        break;
                    }

                    std::vector<std::vector<color_detect_result_t>> &results = detector.detect((uint16_t *)frame->buf, {(int)frame->height, (int)frame->width, 3});

                    bool has_color = false;
                    int best_color_idx = -1;
                    int best_box_idx = -1;
                    int max_area_overall = 0;

                    // Build JSON objects string for /api/detections
                    string objects_json = "[";
                    bool first_obj = true;

                    struct BlockCand {
                        i2c_target_box_t box;
                        int area;
                    };
                    std::vector<BlockCand> cands;

                    for (int i = 0; i < COLOR_NUM && i < (int)results.size(); ++i)
                    {
                        bool enabled = is_target_enabled((uint8_t)(i + 1));
                        if (results[i].empty() || !enabled)
                        {
                            set_IIC_color_data(i, 0, 0, 0, 0);
                        }
                        else
                        {
                            has_color = true;

                            // Find largest block of this color
                            size_t max_idx = 0;
                            int max_area = 0;
                            for (size_t k = 0; k < results[i].size(); ++k)
                            {
                                if (results[i][k].area > max_area)
                                {
                                    max_area = results[i][k].area;
                                    max_idx = k;
                                }

                                // Format each detected box into objects JSON
                                auto &b = results[i][k].box;
                                int bw = b[2] - b[0];
                                int bh = b[3] - b[1];
                                char obj_buf[160];
                                snprintf(obj_buf, sizeof(obj_buf),
                                         "%s{\"label\":\"%s\",\"category\":%d,\"score\":1.0,\"color\":\"%s\",\"box\":[%d,%d,%d,%d]}",
                                         first_obj ? "" : ",",
                                         std_color_info[i].name.c_str(),
                                         i,
                                         color_hex_strings[i],
                                         b[0], b[1], bw, bh);
                                objects_json += obj_buf;
                                first_obj = false;

                                BlockCand cand;
                                cand.box.target_type = (uint8_t)(i + 1);
                                cand.box.x = (uint16_t)b[0];
                                cand.box.y = (uint16_t)b[1];
                                cand.box.w = (uint16_t)std::max(0, bw);
                                cand.box.h = (uint16_t)std::max(0, bh);
                                cand.area = results[i][k].area;
                                cands.push_back(cand);
                            }

                            uint8_t cx = (uint8_t)(results[i][max_idx].center[0] * 320 / (int)frame->width);
                            uint8_t cy = (uint8_t)(results[i][max_idx].center[1] * 240 / (int)frame->height);
                            uint8_t w  = (uint8_t)((results[i][max_idx].box[2] - results[i][max_idx].box[0]) * 320 / (int)frame->width);
                            uint8_t h  = (uint8_t)((results[i][max_idx].box[3] - results[i][max_idx].box[1]) * 240 / (int)frame->height);
                            set_IIC_color_data(i, cx, cy, w, h);

                            if (max_area > max_area_overall)
                            {
                                max_area_overall = max_area;
                                best_color_idx = i;
                                best_box_idx = max_idx;
                            }

                            // If draw_box is enabled (e.g. for LCD hardware display), draw into frame
                            if (draw_box)
                            {
                                for (size_t k = 0; k < results[i].size(); ++k)
                                {
                                    auto &box = results[i][k].box;
                                    dl::image::draw_hollow_rectangle((uint16_t *)frame->buf, (int)frame->height, (int)frame->width,
                                                                     box[0], box[1], box[2], box[3],
                                                                     draw_lcd_colors[i]);
                                    int label_x = DL_MAX(box[0], 2);
                                    if (label_x + (int)std_color_info[i].name.length() * 14 > (int)frame->width)
                                    {
                                        label_x = DL_MAX(2, (int)frame->width - (int)std_color_info[i].name.length() * 14 - 2);
                                    }
                                    int label_y = (box[1] >= 16) ? (box[1] - 2) : (box[1] + 2);
                                    fb_gfx_print(frame, label_x, label_y, draw_text_colors[i], std_color_info[i].name.c_str());
                                }
                            }
                        }
                    }
                    objects_json += "]";

                    // Update thread-safe JSON detection string for /api/detections
                    if (s_json_mutex && xSemaphoreTake(s_json_mutex, 0) == pdTRUE)
                    {
                        snprintf(s_detections_json, sizeof(s_detections_json),
                                 "{\"title\":\"Color / Line Detection\",\"type\":\"color\",\"width\":%d,\"height\":%d,\"streamFps\":%d,\"age_ms\":10,\"enabled\":true,\"objects\":%s}",
                                 (int)frame->width, (int)frame->height, current_fps, objects_json.c_str());
                        s_last_detect_time_us = esp_timer_get_time();
                        xSemaphoreGive(s_json_mutex);
                    }

                    // Send results over UART and synchronize global I2C registers (max 5 largest blocks)
                    std::sort(cands.begin(), cands.end(), [](const BlockCand &a, const BlockCand &b) {
                        return a.area > b.area;
                    });
                    i2c_target_box_t blocks[5];
                    uint8_t blk_count = 0;
                    for (size_t ci = 0; ci < cands.size() && blk_count < 5; ++ci)
                    {
                        blocks[blk_count++] = cands[ci].box;
                    }

                    if (has_color && best_color_idx >= 0 && best_box_idx >= 0)
                    {
                        auto &top_box = results[best_color_idx][best_box_idx].box;
                        int tx1 = top_box[0] * 320 / (int)frame->width;
                        int ty1 = top_box[1] * 240 / (int)frame->height;
                        int tx2 = top_box[2] * 320 / (int)frame->width;
                        int ty2 = top_box[3] * 240 / (int)frame->height;
                        char ubuf[32];
                        snprintf(ubuf, sizeof(ubuf), "$%03d,%03d,%03d,%03d,#", tx1, ty1, tx2, ty2);
                        if (!color_adjust_is_active())
                        {
                            Uart_Send_Data((uint8_t*)ubuf, strlen(ubuf));
                        }
                        Uart1_Send_Data((uint8_t*)ubuf, strlen(ubuf));

                        set_IIC_data(tx1, ty1, tx2, ty2);
                        set_IIC_data_id(0);
                        i2c_proto_set_color_result(blocks, blk_count);
                    }
                    else
                    {
                        char nodata[32];
                        snprintf(nodata, sizeof(nodata), "$000,000,320,240,#");
                        if (!color_adjust_is_active())
                        {
                            Uart_Send_Data((uint8_t*)nodata, strlen(nodata));
                        }
                        Uart1_Send_Data((uint8_t*)nodata, strlen(nodata));

                        set_IIC_data(0, 0, 320, 240);
                        set_IIC_data_id(0);
                        i2c_proto_set_color_result(NULL, 0);
                    }
                    break;
                }
                }
            }

            // Pre-compress RGB565 -> JPEG in AI task (Core 1) so the stream handler (Core 0 httpd)
            // only needs to call send() and never blocks in frame2jpg at 640x480 (~150 ms).
            // This prevents /api/detections from timing out while stream is sending.
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
        }
    }
}

static void task_event_handler(void *arg)
{
    color_detection_state_t _gEvent;
    while (true)
    {
        xQueueReceive(xQueueEvent, &(_gEvent), portMAX_DELAY);
        xSemaphoreTake(xMutex, portMAX_DELAY);
        gEvent = _gEvent;
        xSemaphoreGive(xMutex);
        if (gEvent == OPEN_REGISTER_COLOR_BOX)
        {
            xSemaphoreTake(xMutex, portMAX_DELAY);
            register_mode = true;
            xSemaphoreGive(xMutex);
        }
        else if(gEvent == SWITCH_RESULT)
        {
            draw_box = !draw_box;
        }
    }
}

void register_color_detection(const QueueHandle_t frame_i,
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
    s_json_mutex = xSemaphoreCreateMutex();

    if (!s_snapshot_req_mutex) {
        s_snapshot_req_mutex = xSemaphoreCreateMutex();
        s_snapshot_done_sem = xSemaphoreCreateBinary();
    }
    if (!s_raw_req_mutex) {
        s_raw_req_mutex = xSemaphoreCreateMutex();
        s_raw_done_sem = xSemaphoreCreateBinary();
    }

    xTaskCreatePinnedToCore(task_process_handler, TAG, 8 * 1024, NULL, 5, NULL, 1);
    if (xQueueEvent)
        xTaskCreatePinnedToCore(task_event_handler, TAG, 4 * 1024, NULL, 5, NULL, 1);
}
