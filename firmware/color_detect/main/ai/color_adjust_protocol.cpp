#include "color_adjust_protocol.hpp"
#include "s3_cam_color_detection.hpp"
#include "my_usart.h"
#include "cJSON.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_timer.h"


#include <algorithm>
#include <cstdlib>
#include <cstring>

static const char *TAG = "ColorAdjust";
static const char *NVS_NAMESPACE = "vision_cfg";
static const char *NVS_KEY = "settings_v4";

namespace {
constexpr int MAX_COLORS = 5;
constexpr int MAX_TARGETS = 5;

SemaphoreHandle_t g_cfg_mutex = nullptr;
VisionConfig g_cfg;
uint32_t g_last_tool_activity_ms = 0;
color_adjust::PayloadUnpacker g_payload_unpacker;
std::string g_rx_accum;

inline uint16_t read_le16(const uint8_t *data)
{
    return static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8);
}

inline void write_le16(uint8_t *data, uint16_t value)
{
    data[0] = value & 0xff;
    data[1] = (value >> 8) & 0xff;
}

uint32_t get_time_ms()
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

void mark_activity()
{
    g_last_tool_activity_ms = get_time_ms();
}

bool get_int(const cJSON *obj, const char *key, int min, int max, int *out)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!cJSON_IsNumber(item) || item->valuedouble != item->valueint ||
        item->valueint < min || item->valueint > max) {
        return false;
    }
    *out = item->valueint;
    return true;
}

bool parse_u8_array3(const cJSON *obj, const char *key, int max, uint8_t out[3])
{
    const cJSON *array = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!cJSON_IsArray(array) || cJSON_GetArraySize(array) != 3) {
        return false;
    }
    for (int i = 0; i < 3; i++) {
        const cJSON *item = cJSON_GetArrayItem(array, i);
        if (!cJSON_IsNumber(item) || item->valuedouble != item->valueint ||
            item->valueint < 0 || item->valueint > max) {
            return false;
        }
        out[i] = static_cast<uint8_t>(item->valueint);
    }
    return true;
}

bool parse_optional_int(const cJSON *obj, const char *key, int min, int max, int *out)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!item) {
        return true;
    }
    return get_int(obj, key, min, max, out);
}

bool parse_color_config_item(const cJSON *obj, ColorConfig *config)
{
    if (!cJSON_IsObject(obj) || !config) {
        return false;
    }

    const cJSON *name_item = cJSON_GetObjectItemCaseSensitive(obj, "index");
    int name = -1;
    if (cJSON_IsNumber(name_item) && name_item->valuedouble == name_item->valueint) {
        name = name_item->valueint;
    } else {
        const char *name_text = cJSON_GetStringValue(name_item);
        if (name_text) {
            name = atoi(name_text);
        }
    }

    int area_threshold = config->area_thr > 0 ? config->area_thr : 64;
    if (name < 1 || name > MAX_COLORS ||
        !parse_u8_array3(obj, "hsv_min", 255, config->hsv_min) ||
        !parse_u8_array3(obj, "hsv_max", 255, config->hsv_max)) {
        return false;
    }

    // box_color is optional; if present parse it
    if (cJSON_GetObjectItemCaseSensitive(obj, "box_color")) {
        if (!parse_u8_array3(obj, "box_color", 255, config->box_color)) {
            return false;
        }
    }

    // area_thr is optional
    if (!parse_optional_int(obj, "area_thr", 1, 65535, &area_threshold)) {
        return false;
    }

    // Check HSV sanity:
    // Hue is 0..180 (wrap-around allowed, e.g. min 155, max 12 for red)
    if (config->hsv_min[0] > 180 || config->hsv_max[0] > 180 ||
        config->hsv_min[1] > config->hsv_max[1] ||
        config->hsv_min[2] > config->hsv_max[2]) {
        return false;
    }

    config->name = static_cast<uint8_t>(name);
    config->area_thr = area_threshold;
    return true;
}

int find_color_index_by_name(const VisionConfig &config, uint8_t name)
{
    for (int i = 0; i < config.color_count; i++) {
        if (config.colors[i].name == name) {
            return i;
        }
    }
    return -1;
}

bool parse_settings_json_internal(const std::string &json, const VisionConfig &base_config, VisionConfig *config)
{
    cJSON *root = cJSON_ParseWithLength(json.data(), json.size());
    if (!root) {
        return false;
    }

    bool valid = false;
    const cJSON *params = cJSON_GetObjectItemCaseSensitive(root, "params");
    if (params && config) {
        VisionConfig parsed = base_config;
        const cJSON *colors = nullptr;

        if (cJSON_IsArray(params)) {
            colors = params;
            valid = true;
        } else if (cJSON_IsObject(params)) {
            int mode = parsed.mode;
            valid = parse_optional_int(params, "mode", 0, 1, &mode);
            parsed.mode = mode;

            const cJSON *targets = cJSON_GetObjectItemCaseSensitive(params, "target");
            if (valid && targets) {
                const int target_count = cJSON_GetArraySize(targets);
                valid = cJSON_IsArray(targets) && target_count > 0 && target_count <= MAX_TARGETS;
                parsed.target_count = static_cast<uint8_t>(target_count);
                for (int i = 0; valid && i < target_count; i++) {
                    const cJSON *target = cJSON_GetArrayItem(targets, i);
                    valid = cJSON_IsNumber(target) && target->valuedouble == target->valueint &&
                            target->valueint >= 1 && target->valueint <= MAX_COLORS;
                    if (valid) {
                        parsed.targets[i] = static_cast<uint8_t>(target->valueint);
                    }
                }
            }

            colors = cJSON_GetObjectItemCaseSensitive(params, "color_config");
            if (!colors) {
                colors = cJSON_GetObjectItemCaseSensitive(params, "colors");
            }
        }
        if (valid && colors) {
            const int color_count = cJSON_GetArraySize(colors);
            valid = cJSON_IsArray(colors) && color_count > 0 && color_count <= MAX_COLORS;
            VisionConfig updated = parsed;
            for (int i = 0; valid && i < color_count; i++) {
                const cJSON *item = cJSON_GetArrayItem(colors, i);
                ColorConfig color = {};
                const cJSON *name_item = cJSON_GetObjectItemCaseSensitive(item, "index");
                int name = -1;
                if (cJSON_IsNumber(name_item) && name_item->valuedouble == name_item->valueint) {
                    name = name_item->valueint;
                } else {
                    const char *name_text = cJSON_GetStringValue(name_item);
                    if (name_text) {
                        name = atoi(name_text);
                    }
                }
                int old_idx = name >= 0 ? find_color_index_by_name(parsed, static_cast<uint8_t>(name)) : -1;
                if (old_idx >= 0) {
                    color = parsed.colors[old_idx];
                }
                valid = parse_color_config_item(item, &color);
                if (valid) {
                    if (old_idx >= 0) {
                        updated.colors[old_idx] = color;
                    } else if (updated.color_count < MAX_COLORS) {
                        updated.colors[updated.color_count++] = color;
                    }
                }
            }
            if (valid) {
                parsed = updated;
            }
        }

        if (valid) {
            *config = parsed;
        }
    }

    cJSON_Delete(root);
    return valid;
}

bool extract_json_command(std::string &accum, std::string &json)
{
    size_t start = accum.find('{');
    if (start == std::string::npos) {
        accum.clear();
        return false;
    }
    if (start > 0) {
        accum.erase(0, start);
    }

    int depth = 0;
    bool in_string = false;
    bool escape = false;
    for (size_t i = 0; i < accum.size(); i++) {
        char ch = accum[i];
        if (escape) {
            escape = false;
            continue;
        }
        if (in_string && ch == '\\') {
            escape = true;
            continue;
        }
        if (ch == '"') {
            in_string = !in_string;
            continue;
        }
        if (in_string) {
            continue;
        }
        if (ch == '{') {
            depth++;
        } else if (ch == '}') {
            depth--;
            if (depth == 0) {
                json = accum.substr(0, i + 1);
                accum.erase(0, i + 1);
                return true;
            }
        }
    }
    return false;
}

bool send_protocol_payload(uint8_t func, const uint8_t *payload, size_t payload_len)
{
    std::vector<std::vector<uint8_t>> frames;
    if (!color_adjust::pack_payload(func, payload, payload_len, frames)) {
        ESP_LOGE(TAG, "pack_payload failed for len %u", (unsigned)payload_len);
        return false;
    }

    for (const auto &f : frames) {
        color_adjust_send_bytes(f.data(), f.size());
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    return true;
}

bool send_frame_jpeg()
{
    uint8_t *jpg_buf = nullptr;
    size_t jpg_len = 0;
    if (!color_detection_get_frame_jpeg(&jpg_buf, &jpg_len, pdMS_TO_TICKS(1500))) {
        ESP_LOGW(TAG, "Failed to capture camera frame for ColorTool");
        return false;
    }

    bool sent = send_protocol_payload(0x00, jpg_buf, jpg_len);
    free(jpg_buf);
    return sent;
}

bool handle_protocol_command(const std::string &json)
{
    cJSON *root = cJSON_ParseWithLength(json.data(), json.size());
    if (!root) {
        return false;
    }

    bool handled = false;
    const cJSON *command = cJSON_GetObjectItemCaseSensitive(root, "command");
    const cJSON *params = cJSON_GetObjectItemCaseSensitive(root, "params");
    const char *command_text = cJSON_GetStringValue(command);

    if (command_text && strcmp(command_text, "vision_setting") == 0 && params) {
        mark_activity();

        if (cJSON_IsString(params)) {
            const char *param_text = cJSON_GetStringValue(params);
            if (param_text && strcmp(param_text, "get_img_frame") == 0) {
                handled = true;
                send_frame_jpeg();
            } else if (param_text && (strcmp(param_text, "get_color_config") == 0 ||
                                      strcmp(param_text, "get_setting") == 0 ||
                                      strcmp(param_text, "get_settings") == 0 ||
                                      strcmp(param_text, "read_setting") == 0 ||
                                      strcmp(param_text, "read_settings") == 0)) {
                handled = true;
                std::string resp = color_adjust::get_settings_json();
                send_protocol_payload(0x01, reinterpret_cast<const uint8_t *>(resp.data()), resp.size());
            }
        } else if (cJSON_IsObject(params) || cJSON_IsArray(params)) {
            handled = true;
            bool ok = color_adjust::save_settings_json(json);
            const char *resp = ok ?
                "{\"command\":\"vision_setting\",\"params\":{\"is_save\":true}}" :
                "{\"command\":\"vision_setting\",\"params\":{\"is_save\":false}}";
            send_protocol_payload(0x02, reinterpret_cast<const uint8_t *>(resp), strlen(resp));
        }
    }

    cJSON_Delete(root);
    return handled;
}

} // namespace

namespace color_adjust {

uint8_t checksum(const uint8_t *data, size_t len)
{
    uint8_t value = 0;
    if (!data) return value;
    for (size_t i = 0; i < len; i++) {
        value ^= data[i];
    }
    return value;
}

bool pack_frame(const Frame &frame, std::vector<uint8_t> &out)
{
    if (frame.payload.size() > MAX_PAYLOAD_SIZE ||
        frame.fragment_id == 0 ||
        frame.total_fragments == 0 ||
        frame.fragment_id > frame.total_fragments) {
        return false;
    }

    uint16_t data_len = static_cast<uint16_t>(1 + frame.payload.size());
    out.assign(FRAME_OVERHEAD + frame.payload.size(), 0);
    out[0] = HEADER_0;
    out[1] = HEADER_1;
    write_le16(out.data() + 2, data_len);
    write_le16(out.data() + 4, frame.fragment_id);
    write_le16(out.data() + 6, frame.total_fragments);
    out[8] = frame.func;
    if (!frame.payload.empty()) {
        memcpy(out.data() + 9, frame.payload.data(), frame.payload.size());
    }
    out[9 + frame.payload.size()] = checksum(out.data() + 8, data_len);
    return true;
}

bool pack_payload(uint8_t func, const uint8_t *payload, size_t payload_len, std::vector<std::vector<uint8_t>> &out)
{
    if (!payload && payload_len > 0) {
        return false;
    }

    size_t total_fragments = payload_len == 0 ? 1 : (payload_len + MAX_PAYLOAD_SIZE - 1) / MAX_PAYLOAD_SIZE;
    if (total_fragments > UINT16_MAX) {
        return false;
    }

    out.clear();
    out.reserve(total_fragments);
    size_t offset = 0;
    for (uint16_t frag = 1; frag <= static_cast<uint16_t>(total_fragments); frag++) {
        size_t chunk_len = std::min(MAX_PAYLOAD_SIZE, payload_len - offset);
        Frame frame = {};
        frame.func = func;
        frame.fragment_id = frag;
        frame.total_fragments = static_cast<uint16_t>(total_fragments);
        if (chunk_len > 0) {
            frame.payload.assign(payload + offset, payload + offset + chunk_len);
        }

        std::vector<uint8_t> packed;
        if (!pack_frame(frame, packed)) {
            out.clear();
            return false;
        }
        out.push_back(std::move(packed));
        offset += chunk_len;
    }
    return true;
}

bool unpack_frame(const uint8_t *data, size_t len, Frame &frame)
{
    if (!data || len < FRAME_OVERHEAD || data[0] != HEADER_0 || data[1] != HEADER_1) {
        return false;
    }

    uint16_t data_len = read_le16(data + 2);
    if (data_len == 0 || data_len > 1 + MAX_PAYLOAD_SIZE || len != static_cast<size_t>(data_len) + 9) {
        return false;
    }

    uint16_t fragment_id = read_le16(data + 4);
    uint16_t total_fragments = read_le16(data + 6);
    if (fragment_id == 0 || total_fragments == 0 || fragment_id > total_fragments) {
        return false;
    }

    uint8_t expected = checksum(data + 8, data_len);
    if (expected != data[8 + data_len]) {
        return false;
    }

    frame.func = data[8];
    frame.fragment_id = fragment_id;
    frame.total_fragments = total_fragments;
    frame.payload.assign(data + 9, data + 8 + data_len);
    return true;
}

bool assemble_payload(const std::vector<Frame> &frames, Message &message)
{
    if (frames.empty()) {
        return false;
    }

    const uint8_t func = frames.front().func;
    const uint16_t total_fragments = frames.front().total_fragments;
    if (total_fragments == 0 || frames.size() != total_fragments) {
        return false;
    }

    size_t payload_len = 0;
    for (size_t i = 0; i < frames.size(); i++) {
        const Frame &frame = frames[i];
        if (frame.func != func ||
            frame.total_fragments != total_fragments ||
            frame.fragment_id != static_cast<uint16_t>(i + 1)) {
            return false;
        }
        payload_len += frame.payload.size();
    }

    message.func = func;
    message.payload.clear();
    message.payload.reserve(payload_len);
    for (const auto &frame : frames) {
        message.payload.insert(message.payload.end(), frame.payload.begin(), frame.payload.end());
    }
    return true;
}

void StreamUnpacker::reset()
{
    m_buffer.clear();
}

size_t StreamUnpacker::pending_size() const
{
    return m_buffer.size();
}

void StreamUnpacker::feed(const uint8_t *data, size_t len, std::vector<Frame> &frames)
{
    frames.clear();
    if (!data || len == 0) {
        return;
    }

    m_buffer.insert(m_buffer.end(), data, data + len);
    while (true) {
        auto header = std::find(m_buffer.begin(), m_buffer.end(), HEADER_0);
        if (header == m_buffer.end()) {
            m_buffer.clear();
            return;
        }
        if (header != m_buffer.begin()) {
            m_buffer.erase(m_buffer.begin(), header);
        }
        if (m_buffer.size() < FRAME_OVERHEAD) {
            return;
        }
        if (m_buffer[1] != HEADER_1) {
            m_buffer.erase(m_buffer.begin());
            continue;
        }

        uint16_t data_len = read_le16(m_buffer.data() + 2);
        size_t frame_len = static_cast<size_t>(data_len) + 9;
        if (data_len == 0 || data_len > 1 + MAX_PAYLOAD_SIZE) {
            m_buffer.erase(m_buffer.begin());
            continue;
        }
        if (m_buffer.size() < frame_len) {
            return;
        }

        Frame frame;
        if (unpack_frame(m_buffer.data(), frame_len, frame)) {
            frames.push_back(std::move(frame));
            m_buffer.erase(m_buffer.begin(), m_buffer.begin() + frame_len);
        } else {
            m_buffer.erase(m_buffer.begin());
        }
    }
}

void PayloadUnpacker::reset()
{
    m_stream_unpacker.reset();
    m_fragments.clear();
}

bool PayloadUnpacker::has_pending_data() const
{
    return m_stream_unpacker.pending_size() > 0 || !m_fragments.empty();
}

void PayloadUnpacker::feed(const uint8_t *data, size_t len, std::vector<Message> &messages)
{
    messages.clear();
    std::vector<Frame> frames;
    m_stream_unpacker.feed(data, len, frames);
    for (const auto &frame : frames) {
        handle_frame(frame, messages);
    }
}

void PayloadUnpacker::handle_frame(const Frame &frame, std::vector<Message> &messages)
{
    if (frame.total_fragments == 1) {
        Message message;
        message.func = frame.func;
        message.payload = frame.payload;
        messages.push_back(std::move(message));
        return;
    }

    if (frame.fragment_id == 1) {
        m_fragments.clear();
    } else if (m_fragments.empty() ||
               m_fragments.front().func != frame.func ||
               m_fragments.front().total_fragments != frame.total_fragments ||
               frame.fragment_id != static_cast<uint16_t>(m_fragments.size() + 1)) {
        m_fragments.clear();
        return;
    }

    m_fragments.push_back(frame);
    if (m_fragments.size() == frame.total_fragments) {
        Message message;
        if (assemble_payload(m_fragments, message)) {
            messages.push_back(std::move(message));
        }
        m_fragments.clear();
    }
}

VisionConfig default_config()
{
    VisionConfig cfg = {};
    cfg.mode = 0;
    cfg.target_count = 4;
    cfg.targets[0] = 1; // Red
    cfg.targets[1] = 2; // Blue
    cfg.targets[2] = 3; // Green
    cfg.targets[3] = 4; // Yellow
    cfg.color_count = 5;

    // Standard ColorTool Order (1: Red, 2: Blue, 3: Green, 4: Yellow, 5: Purple)
    // 1: Red: [158, 10, 85, 255, 70, 255] (S_min>=85 strictly rejects human skin tone)
    cfg.colors[0] = {1, {158, 85, 70}, {10, 255, 255}, {255, 0, 0}, 150};
    // 2: Blue: [90, 130, 55, 255, 45, 255]
    cfg.colors[1] = {2, {90, 55, 45}, {130, 255, 255}, {0, 0, 255}, 150};
    // 3: Green: [38, 85, 50, 255, 45, 255]
    cfg.colors[2] = {3, {38, 50, 45}, {85, 255, 255}, {0, 255, 0}, 150};
    // 4: Yellow: [18, 35, 105, 255, 80, 255] (S_min>=105 strictly rejects desk/paper/glare)
    cfg.colors[3] = {4, {18, 105, 80}, {35, 255, 255}, {255, 255, 0}, 150};
    // 5: Purple: [132, 158, 50, 255, 45, 255]
    cfg.colors[4] = {5, {132, 50, 45}, {158, 255, 255}, {160, 32, 240}, 150};

    return cfg;
}

bool load_config_from_nvs(VisionConfig *cfg)
{
    if (!cfg) return false;
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) return false;

    size_t len = sizeof(VisionConfig);
    err = nvs_get_blob(handle, NVS_KEY, cfg, &len);
    nvs_close(handle);
    return (err == ESP_OK && len == sizeof(VisionConfig));
}

bool save_config_to_nvs(const VisionConfig &cfg)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) return false;

    err = nvs_set_blob(handle, NVS_KEY, &cfg, sizeof(cfg));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err == ESP_OK;
}

bool is_config_valid(const VisionConfig &cfg)
{
    if (cfg.color_count == 0 || cfg.color_count > MAX_COLORS) {
        return false;
    }

    bool color_names[MAX_COLORS + 1] = {};
    for (int i = 0; i < cfg.color_count; i++) {
        const auto &color = cfg.colors[i];
        if (color.name < 1 || color.name > MAX_COLORS || color_names[color.name] ||
            color.hsv_min[0] > 180 || color.hsv_max[0] > 180 ||
            color.hsv_min[1] > color.hsv_max[1] || color.hsv_min[2] > color.hsv_max[2] ||
            color.area_thr <= 0 || color.area_thr > 65535) {
            return false;
        }

        // Sanity check to prevent swapped colors (1=Red, 2=Blue, 3=Green, 4=Yellow, 5=Purple)
        if (color.name == 2) { // Blue: nominal hue ~80-140, cannot be yellow/orange (<60)
            if (color.hsv_max[0] < 70 || color.hsv_min[0] < 60) return false;
        } else if (color.name == 4) { // Yellow: nominal hue ~15-40, cannot be blue (>70)
            if (color.hsv_min[0] > 60 || color.hsv_max[0] > 70) return false;
        }

        color_names[color.name] = true;
    }
    return true;
}

VisionConfig get_current_config()
{
    if (g_cfg_mutex) {
        xSemaphoreTake(g_cfg_mutex, portMAX_DELAY);
    }
    VisionConfig copy = g_cfg;
    if (g_cfg_mutex) {
        xSemaphoreGive(g_cfg_mutex);
    }
    return copy;
}

void set_current_config(const VisionConfig &cfg)
{
    if (g_cfg_mutex) {
        xSemaphoreTake(g_cfg_mutex, portMAX_DELAY);
    }
    g_cfg = cfg;
    if (g_cfg_mutex) {
        xSemaphoreGive(g_cfg_mutex);
    }
}

std::string get_settings_json()
{
    const VisionConfig config = get_current_config();

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "command", "vision_setting");
    cJSON *params = cJSON_AddArrayToObject(root, "params");
    for (int i = 0; i < config.color_count; i++) {
        const auto &color = config.colors[i];
        cJSON *item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "index", color.name);
        cJSON *hsv_min = cJSON_AddArrayToObject(item, "hsv_min");
        cJSON *hsv_max = cJSON_AddArrayToObject(item, "hsv_max");
        cJSON *box_color = cJSON_AddArrayToObject(item, "box_color");
        for (int j = 0; j < 3; j++) {
            cJSON_AddItemToArray(hsv_min, cJSON_CreateNumber(color.hsv_min[j]));
            cJSON_AddItemToArray(hsv_max, cJSON_CreateNumber(color.hsv_max[j]));
            cJSON_AddItemToArray(box_color, cJSON_CreateNumber(color.box_color[j]));
        }
        cJSON_AddNumberToObject(item, "area_thr", color.area_thr);
        cJSON_AddItemToArray(params, item);
    }

    char *text = cJSON_PrintUnformatted(root);
    std::string json = text ? text : "";
    cJSON_free(text);
    cJSON_Delete(root);
    return json;
}

bool save_settings_json(const std::string &json)
{
    VisionConfig current = get_current_config();
    VisionConfig updated = current;
    if (!parse_settings_json_internal(json, current, &updated)) {
        ESP_LOGE(TAG, "parse_settings_json failed");
        return false;
    }
    if (!is_config_valid(updated)) {
        ESP_LOGE(TAG, "invalid vision config");
        return false;
    }
    if (!save_config_to_nvs(updated)) {
        ESP_LOGE(TAG, "save_config_to_nvs failed");
        return false;
    }
    VisionConfig loaded = {};
    if (!load_config_from_nvs(&loaded) || memcmp(&updated, &loaded, sizeof(VisionConfig)) != 0) {
        ESP_LOGE(TAG, "verify loaded NVS config failed");
        return false;
    }

    set_current_config(loaded);
    color_detection_apply_vision_config(loaded);
    ESP_LOGI(TAG, "Settings successfully saved and applied to detector");
    return true;
}

} // namespace color_adjust

extern "C" void color_adjust_init(void)
{
    if (!g_cfg_mutex) {
        g_cfg_mutex = xSemaphoreCreateMutex();
    }

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    VisionConfig default_cfg = color_adjust::default_config();
    VisionConfig loaded_cfg = {};
    if (color_adjust::load_config_from_nvs(&loaded_cfg) && color_adjust::is_config_valid(loaded_cfg)) {
        color_adjust::set_current_config(loaded_cfg);
        color_detection_apply_vision_config(loaded_cfg);
        ESP_LOGI(TAG, "Loaded saved color thresholds from NVS");
    } else {
        color_adjust::set_current_config(default_cfg);
        color_adjust::save_config_to_nvs(default_cfg);
        color_detection_apply_vision_config(default_cfg);
        ESP_LOGI(TAG, "Initialized default color thresholds into NVS");
    }
}

extern "C" bool color_adjust_is_active(void)
{
    if (g_last_tool_activity_ms == 0) return false;
    uint32_t now = get_time_ms();
    return (now - g_last_tool_activity_ms) < 4000;
}

extern "C" int color_adjust_send_bytes(const uint8_t *data, size_t len)
{
    if (!data || len == 0) return 0;
    return Uart_Send_Data((uint8_t*)data, (uint16_t)len);
}

extern "C" bool color_adjust_process_rx(const uint8_t *data, size_t len)
{
    if (!data || len == 0) return false;

    // Check if this looks like a protocol frame or vision setting JSON
    bool looks_like_protocol = (len >= 2 && data[0] == color_adjust::HEADER_0 && data[1] == color_adjust::HEADER_1);
    bool looks_like_json = (std::memchr(data, '{', len) != nullptr);

    std::vector<color_adjust::Message> messages;
    g_payload_unpacker.feed(data, len, messages);
    if (!messages.empty()) {
        mark_activity();
        for (const auto &msg : messages) {
            if (!msg.payload.empty()) {
                std::string json(reinterpret_cast<const char *>(msg.payload.data()), msg.payload.size());
                handle_protocol_command(json);
            }
        }
        return true;
    }

    if (g_payload_unpacker.has_pending_data()) {
        mark_activity();
        return true;
    }

    // Direct JSON accumulation
    g_rx_accum.append(reinterpret_cast<const char *>(data), len);
    if (g_rx_accum.size() > 4096) {
        g_rx_accum.erase(0, g_rx_accum.size() - 4096);
    }

    std::string json;
    bool handled_json = false;
    while (extract_json_command(g_rx_accum, json)) {
        if (handle_protocol_command(json)) {
            handled_json = true;
        }
    }

    if (handled_json || looks_like_protocol || looks_like_json || (g_rx_accum.find('{') != std::string::npos)) {
        mark_activity();
        return true;
    }

    return false;
}
