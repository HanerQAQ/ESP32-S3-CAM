#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the serial threshold adjustment module & NVS configuration.
 */
void color_adjust_init(void);

/**
 * @brief Feed raw bytes from UART into the protocol parser.
 * @return true if the data was recognized as part of a vision protocol packet or JSON command.
 */
bool color_adjust_process_rx(const uint8_t *data, size_t len);

/**
 * @brief Returns true if ColorTool was active recently (within last 4 seconds).
 * When active, periodic $xxx,# debug stream on UART0 is suppressed to avoid collision.
 */
bool color_adjust_is_active(void);

/**
 * @brief Send raw bytes out over UART0 (and USB Serial JTAG if installed).
 */
int color_adjust_send_bytes(const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#include <string>
#include <vector>

struct ColorConfig {
    uint8_t name;        // 1..5 corresponding to color index
    uint8_t hsv_min[3];  // [H_min, S_min, V_min]
    uint8_t hsv_max[3];  // [H_max, S_max, V_max]
    uint8_t box_color[3];// [R, G, B]
    int32_t area_thr;
};

struct VisionConfig {
    int32_t mode;        // 0: color_detect, 1: line_tracking
    uint8_t target_count;
    uint8_t targets[5];
    uint8_t color_count;
    ColorConfig colors[5];
};

namespace color_adjust {

static constexpr uint8_t HEADER_0 = 0xAA;
static constexpr uint8_t HEADER_1 = 0x55;
static constexpr size_t MAX_FRAME_SIZE = 1024;
static constexpr size_t FRAME_OVERHEAD = 10;
static constexpr size_t MAX_PAYLOAD_SIZE = MAX_FRAME_SIZE - FRAME_OVERHEAD;

struct Frame {
    uint8_t func = 0;
    uint16_t fragment_id = 1;
    uint16_t total_fragments = 1;
    std::vector<uint8_t> payload;
};

struct Message {
    uint8_t func = 0;
    std::vector<uint8_t> payload;
};

uint8_t checksum(const uint8_t *data, size_t len);
bool pack_frame(const Frame &frame, std::vector<uint8_t> &out);
bool pack_payload(uint8_t func, const uint8_t *payload, size_t payload_len, std::vector<std::vector<uint8_t>> &out);
bool unpack_frame(const uint8_t *data, size_t len, Frame &frame);
bool assemble_payload(const std::vector<Frame> &frames, Message &message);

class StreamUnpacker {
public:
    void reset();
    void feed(const uint8_t *data, size_t len, std::vector<Frame> &frames);
    size_t pending_size() const;

private:
    std::vector<uint8_t> m_buffer;
};

class PayloadUnpacker {
public:
    void reset();
    void feed(const uint8_t *data, size_t len, std::vector<Message> &messages);
    bool has_pending_data() const;

private:
    void handle_frame(const Frame &frame, std::vector<Message> &messages);

    StreamUnpacker m_stream_unpacker;
    std::vector<Frame> m_fragments;
};

VisionConfig default_config();
bool load_config_from_nvs(VisionConfig *cfg);
bool save_config_to_nvs(const VisionConfig &cfg);
bool is_config_valid(const VisionConfig &cfg);
VisionConfig get_current_config();
void set_current_config(const VisionConfig &cfg);
std::string get_settings_json();
bool save_settings_json(const std::string &json);

} // namespace color_adjust
#endif
