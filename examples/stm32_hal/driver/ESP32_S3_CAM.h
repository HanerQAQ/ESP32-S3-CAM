#ifndef ESP32_S3_CAM_H_
#define ESP32_S3_CAM_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ESP32_S3_CAM_MAX_COLOR_BLOCKS (16U)
#define ESP32_S3_CAM_MAX_TARGET_COLORS (5U)
#define ESP32_S3_CAM_HAND_GESTURE_MAX_COUNT (3U)
#define ESP32_S3_CAM_TARGET_DETECTION_MAX_COUNT (5U)
#define ESP32_S3_CAM_YOLO11N_CLASS_MAX_ID (79U)
#define ESP32_S3_CAM_POSE_MAX_COUNT (3U)
#define ESP32_S3_CAM_POSE_KEYPOINT_COUNT (17U)
#define ESP32_S3_CAM_APRILTAG_MAX_COUNT (3U)
#define ESP32_S3_CAM_QRCODE_MAX_COUNT (3U)
#define ESP32_S3_CAM_QRCODE_MAX_TEXT_LEN (96U)
#define ESP32_S3_CAM_FACE_RECOGNITION_MAX_COUNT (5U)
#define ESP32_S3_CAM_FACE_RECOGNITION_RECORD_SIZE (10U)

/**
 * @brief ESP32_S3_CAM I2C driver error code.
 */
typedef enum {
    ESP32_S3_CAM_OK = 0,                    /*!< Operation succeeded. */
    ESP32_S3_CAM_ERR_FAIL = -1,             /*!< Generic failure. */
    ESP32_S3_CAM_ERR_INVALID_ARG = -3,      /*!< Invalid argument. */
    ESP32_S3_CAM_ERR_INVALID_SIZE = -4,     /*!< Invalid frame or buffer size. */
    ESP32_S3_CAM_ERR_TIMEOUT = -5,          /*!< Receive timeout. */
    ESP32_S3_CAM_ERR_INVALID_RESPONSE = -6, /*!< Invalid response fields. */
    ESP32_S3_CAM_ERR_INVALID_CHECKSUM = -7, /*!< Frame checksum mismatch. */
} esp32_s3_cam_err_t;

/**
 * @brief I2C write callback.
 *
 * @param data Data buffer to write.
 * @param len Data length in bytes.
 * @param timeout_ms Write timeout in milliseconds. 0 means wait forever.
 * @return ESP32_S3_CAM_OK on success, otherwise an error code.
 */
typedef esp32_s3_cam_err_t (*esp32_s3_cam_i2c_write_cb_t)(const uint8_t *data, size_t len, uint32_t timeout_ms);

/**
 * @brief I2C read callback.
 *
 * @param data Destination buffer.
 * @param len Number of bytes to read.
 * @param timeout_ms Read timeout in milliseconds. 0 means wait forever.
 * @return ESP32_S3_CAM_OK on success, otherwise an error code.
 */
typedef esp32_s3_cam_err_t (*esp32_s3_cam_i2c_read_cb_t)(uint8_t *data, size_t len, uint32_t timeout_ms);

/**
 * @brief MCU-specific callbacks.
 */
typedef struct {
    esp32_s3_cam_i2c_write_cb_t write;                /*!< I2C write callback. */
    esp32_s3_cam_i2c_read_cb_t read;                  /*!< I2C read callback. */
} esp32_s3_cam_callbacks_t;

/**
 * @brief Protocol frame metadata.
 */
typedef struct {
    uint16_t data_len;            /*!< DATA field length. */
    uint16_t fragment_id;         /*!< Fragment id, starting from 1. */
    uint16_t total_fragments;     /*!< Total fragment count. */
} esp32_s3_cam_frame_header_t;

/**
 * @brief ESP32_S3_CAM client object.
 */
typedef struct {
    esp32_s3_cam_callbacks_t callbacks;          /*!< I2C callbacks. */
} esp32_s3_cam_handle_t;

typedef enum {
    ESP32_S3_CAM_VISION_MODE_COLOR = 0,
    ESP32_S3_CAM_VISION_MODE_LINE = 1,
} esp32_s3_cam_vision_mode_t;

typedef struct {
    uint8_t mode;                    /*!< 0: color recognition, 1: line tracking. */
    union {
        uint8_t registered_color_count;  /*!< Number of registered color classes (packet_len - 1). */
        uint8_t target_count;            /*!< Alias for registered_color_count. */
    };
    uint8_t targets[ESP32_S3_CAM_MAX_TARGET_COLORS]; /*!< Configured target color indexes. */
} esp32_s3_cam_color_config_t;

typedef struct {
    uint8_t index;       /*!< Color block index. */
    uint16_t x;          /*!< Bounding box X. */
    uint16_t y;          /*!< Bounding box Y. */
    uint16_t width;      /*!< Bounding box width. */
    uint16_t height;     /*!< Bounding box height. */
} esp32_s3_cam_color_block_t;

typedef struct {
    uint8_t count;      /*!< Detected color block count. */
    esp32_s3_cam_color_block_t blocks[ESP32_S3_CAM_MAX_COLOR_BLOCKS];
} esp32_s3_cam_color_result_t;

typedef struct {
    bool detected;       /*!< true if a line is detected. */
    uint16_t start_x;    /*!< Line start X. */
    uint16_t start_y;    /*!< Line start Y. */
    uint16_t end_x;      /*!< Line end X. */
    uint16_t end_y;      /*!< Line end Y. */
    float angle_deg;     /*!< Angle relative to the vertical screen axis, in degrees. */
    float offset;        /*!< Horizontal offset from the screen center, usually -1.0 to 1.0. */
} esp32_s3_cam_line_result_t;

typedef enum {
    ESP32_S3_CAM_HAND_GESTURE_ONE = 0,
    ESP32_S3_CAM_HAND_GESTURE_TWO = 1,
    ESP32_S3_CAM_HAND_GESTURE_THREE = 2,
    ESP32_S3_CAM_HAND_GESTURE_FOUR = 3,
    ESP32_S3_CAM_HAND_GESTURE_FIVE = 4,
    ESP32_S3_CAM_HAND_GESTURE_LIKE = 5,
    ESP32_S3_CAM_HAND_GESTURE_OK = 6,
    ESP32_S3_CAM_HAND_GESTURE_NO_GESTURE = 7,
    ESP32_S3_CAM_HAND_GESTURE_CALL = 8,
    ESP32_S3_CAM_HAND_GESTURE_DISLIKE = 9
} esp32_s3_cam_hand_gesture_type_t;

typedef struct {
    uint8_t gesture;     /*!< Gesture class id. */
    uint16_t x;          /*!< Bounding box X. */
    uint16_t y;          /*!< Bounding box Y. */
    uint16_t width;      /*!< Bounding box width. */
    uint16_t height;     /*!< Bounding box height. */
} esp32_s3_cam_hand_gesture_item_t;

typedef struct {
    uint8_t total_count;       /*!< Total detected gestures in DATA. */
    uint8_t parsed_count;      /*!< Parsed item count stored in items. */
    esp32_s3_cam_hand_gesture_item_t items[ESP32_S3_CAM_HAND_GESTURE_MAX_COUNT];
} esp32_s3_cam_hand_gesture_result_t;

typedef struct {
    uint8_t index;       /*!< Detection index. */
    uint16_t x;          /*!< Bounding box X. */
    uint16_t y;          /*!< Bounding box Y. */
    uint16_t width;      /*!< Bounding box width. */
    uint16_t height;     /*!< Bounding box height. */
} esp32_s3_cam_target_detection_item_t;

typedef struct {
    uint8_t total_count;       /*!< Total detected targets in DATA. */
    uint8_t parsed_count;      /*!< Parsed item count stored in items. */
    esp32_s3_cam_target_detection_item_t items[ESP32_S3_CAM_TARGET_DETECTION_MAX_COUNT];
} esp32_s3_cam_target_detection_result_t;

typedef struct {
    uint8_t index;       /*!< Keypoint index. */
    uint16_t x;          /*!< Keypoint X. */
    uint16_t y;          /*!< Keypoint Y. */
} esp32_s3_cam_pose_keypoint_t;

typedef struct {
    uint8_t index;       /*!< Pose index. */
    uint16_t x;          /*!< Bounding box X. */
    uint16_t y;          /*!< Bounding box Y. */
    uint16_t width;      /*!< Bounding box width. */
    uint16_t height;     /*!< Bounding box height. */
    uint8_t keypoint_count; /*!< Detected keypoint count in DATA. */
    esp32_s3_cam_pose_keypoint_t keypoints[ESP32_S3_CAM_POSE_KEYPOINT_COUNT];
} esp32_s3_cam_pose_item_t;

typedef struct {
    uint8_t total_count;       /*!< Total detected poses in DATA. */
    uint8_t parsed_count;      /*!< Parsed item count stored in items. */
    esp32_s3_cam_pose_item_t items[ESP32_S3_CAM_POSE_MAX_COUNT];
} esp32_s3_cam_pose_result_t;

typedef struct {
    uint16_t id;         /*!< AprilTag ID. */
    uint16_t x;          /*!< Bounding box X. */
    uint16_t y;          /*!< Bounding box Y. */
    uint16_t width;      /*!< Bounding box width. */
    uint16_t height;     /*!< Bounding box height. */
    float rotation;      /*!< In-plane rotation angle. */
    float x_translation; /*!< X translation. */
    float y_translation; /*!< Y translation. */
    float z_translation; /*!< Z translation. */
    float x_rotation;    /*!< Rotation around X axis. */
    float y_rotation;    /*!< Rotation around Y axis. */
    float z_rotation;    /*!< Rotation around Z axis. */
} esp32_s3_cam_apriltag_item_t;

typedef struct {
    uint8_t total_count;       /*!< Total recognized AprilTags in DATA. */
    uint8_t parsed_count;      /*!< Parsed item count stored in items. */
    esp32_s3_cam_apriltag_item_t items[ESP32_S3_CAM_APRILTAG_MAX_COUNT];
} esp32_s3_cam_apriltag_result_t;

typedef struct {
    uint8_t index;       /*!< QR code index. */
    uint16_t x;          /*!< Bounding box X. */
    uint16_t y;          /*!< Bounding box Y. */
    uint16_t width;      /*!< Bounding box width. */
    uint16_t height;     /*!< Bounding box height. */
    uint16_t text_len;   /*!< Original QR code text byte length. */
    char text[ESP32_S3_CAM_QRCODE_MAX_TEXT_LEN + 1U]; /*!< Null-terminated text buffer. */
} esp32_s3_cam_qrcode_item_t;

typedef struct {
    uint8_t total_count;       /*!< Total recognized QR codes in DATA. */
    uint8_t parsed_count;      /*!< Parsed item count stored in items. */
    esp32_s3_cam_qrcode_item_t items[ESP32_S3_CAM_QRCODE_MAX_COUNT];
} esp32_s3_cam_qrcode_result_t;


typedef struct {
    uint16_t id;         /*!< Face ID (0: stranger / unregistered, >=1: registered ID). */
    uint16_t x;          /*!< Bounding box X. */
    uint16_t y;          /*!< Bounding box Y. */
    uint16_t width;      /*!< Bounding box width. */
    uint16_t height;     /*!< Bounding box height. */
} esp32_s3_cam_face_recognition_item_t;

typedef struct {
    uint8_t total_count;       /*!< Total detected faces in DATA. */
    uint8_t parsed_count;      /*!< Parsed item count stored in items. */
    esp32_s3_cam_face_recognition_item_t items[ESP32_S3_CAM_FACE_RECOGNITION_MAX_COUNT];
} esp32_s3_cam_face_recognition_result_t;

/**
 * @brief Initialize a ESP32_S3_CAM handle.
 *
 * @param handle Client object.
 * @return ESP32_S3_CAM_OK on success, otherwise an error code.
 */
esp32_s3_cam_err_t esp32_s3_cam_init(esp32_s3_cam_handle_t *handle);

/**
 * @brief Register I2C write and read callbacks.
 *
 * @param handle Client object.
 * @param write_callback I2C write callback function.
 * @param read_callback I2C read callback function.
 * @return ESP32_S3_CAM_OK on success, otherwise an error code.
 */
esp32_s3_cam_err_t esp32_s3_cam_register_i2c_callbacks(esp32_s3_cam_handle_t *handle,
                                                 esp32_s3_cam_i2c_write_cb_t write_callback,
                                                 esp32_s3_cam_i2c_read_cb_t read_callback);

/**
 * @brief Send a JSON command (strictly no trailing null byte).
 *
 * @param handle Client object.
 * @param json Null-terminated UTF-8 JSON text.
 * @param timeout_ms Write timeout in milliseconds. 0 means wait forever.
 * @return ESP32_S3_CAM_OK on success, otherwise an error code.
 */
esp32_s3_cam_err_t esp32_s3_cam_send_json(esp32_s3_cam_handle_t *handle,
                                    const char *json,
                                    uint32_t timeout_ms);

/**
 * @brief Receive and parse one fixed-size 32-byte protocol frame.
 *
 * @param handle Client object.
 * @param data Destination buffer for the DATA field.
 * @param data_capacity Destination buffer size in bytes.
 * @param header Output frame header.
 * @param timeout_ms Read timeout in milliseconds. 0 means wait forever.
 * @return ESP32_S3_CAM_OK on success, otherwise an error code.
 */
esp32_s3_cam_err_t esp32_s3_cam_receive_frame(esp32_s3_cam_handle_t *handle,
                                        uint8_t *data,
                                        size_t data_capacity,
                                        esp32_s3_cam_frame_header_t *header,
                                        uint32_t timeout_ms);

/**
 * @brief Receive and assemble one complete protocol packet.
 *
 * @param handle Client object.
 * @param data Destination buffer for the assembled DATA fields.
 * @param data_capacity Destination buffer size in bytes.
 * @param packet_len Output assembled DATA length.
 * @param timeout_ms Read timeout for each frame in milliseconds. 0 means wait forever.
 * @return ESP32_S3_CAM_OK on success, otherwise an error code.
 */
esp32_s3_cam_err_t esp32_s3_cam_receive_packet(esp32_s3_cam_handle_t *handle,
                                         uint8_t *data,
                                         size_t data_capacity,
                                         size_t *packet_len,
                                         uint32_t timeout_ms);

/**
 * @brief Get the current color recognition or line tracking configuration.
 *
 * @param handle Client object.
 * @param config Output color/line vision configuration.
 * @param timeout_ms I2C operation timeout in milliseconds. 0 means wait forever.
 * @return ESP32_S3_CAM_OK on success, otherwise an error code.
 */
esp32_s3_cam_err_t esp32_s3_cam_get_color_config(esp32_s3_cam_handle_t *handle,
                                           esp32_s3_cam_color_config_t *config,
                                           uint32_t timeout_ms);

/**
 * @brief Set color recognition or line tracking mode.
 *
 * @param handle Client object.
 * @param mode Vision mode, see esp32_s3_cam_vision_mode_t.
 * @param targets Target color indexes. Line tracking mode accepts exactly one target.
 * @param target_count Number of entries in targets.
 * @param success Output command result reported by the device.
 * @param timeout_ms I2C operation timeout in milliseconds. 0 means wait forever.
 * @return ESP32_S3_CAM_OK on success, otherwise an error code.
 */
esp32_s3_cam_err_t esp32_s3_cam_set_vision_mode(esp32_s3_cam_handle_t *handle,
                                          uint8_t mode,
                                          const uint8_t *targets,
                                          size_t target_count,
                                          bool *success,
                                          uint32_t timeout_ms);

/**
 * @brief Get color recognition result with truncation protection.
 *
 * @param handle Client object.
 * @param result Output detected color blocks.
 * @param timeout_ms I2C operation timeout in milliseconds. 0 means wait forever.
 * @return ESP32_S3_CAM_OK on success, otherwise an error code.
 */
esp32_s3_cam_err_t esp32_s3_cam_get_color_result(esp32_s3_cam_handle_t *handle,
                                           esp32_s3_cam_color_result_t *result,
                                           uint32_t timeout_ms);

/**
 * @brief Get line tracking result.
 *
 * @param handle Client object.
 * @param result Output detected line data.
 * @param timeout_ms I2C operation timeout in milliseconds. 0 means wait forever.
 * @return ESP32_S3_CAM_OK on success, otherwise an error code.
 */
esp32_s3_cam_err_t esp32_s3_cam_get_line_result(esp32_s3_cam_handle_t *handle,
                                          esp32_s3_cam_line_result_t *result,
                                          uint32_t timeout_ms);

/**
 * @brief Get hand gesture recognition result.
 *
 * @param handle Client object.
 * @param result Output hand gesture detections.
 * @param timeout_ms I2C operation timeout in milliseconds. 0 means wait forever.
 * @return ESP32_S3_CAM_OK on success, otherwise an error code.
 */
esp32_s3_cam_err_t esp32_s3_cam_get_hand_gesture_result(esp32_s3_cam_handle_t *handle,
                                                  esp32_s3_cam_hand_gesture_result_t *result,
                                                  uint32_t timeout_ms);

/**
 * @brief Get cat detection result.
 *
 * @param handle Client object.
 * @param result Output detected cats.
 * @param timeout_ms I2C operation timeout in milliseconds. 0 means wait forever.
 * @return ESP32_S3_CAM_OK on success, otherwise an error code.
 */
/**
 * @brief Get cat and dog detection result (bounding box positions).
 *
 * @param handle Client object.
 * @param result Output detected cat/dog targets.
 * @param timeout_ms I2C operation timeout in milliseconds. 0 means wait forever.
 * @return ESP32_S3_CAM_OK on success, otherwise an error code.
 */
esp32_s3_cam_err_t esp32_s3_cam_get_cat_dog_result(esp32_s3_cam_handle_t *handle,
                                             esp32_s3_cam_target_detection_result_t *result,
                                             uint32_t timeout_ms);

esp32_s3_cam_err_t esp32_s3_cam_get_cat_result(esp32_s3_cam_handle_t *handle,
                                         esp32_s3_cam_target_detection_result_t *result,
                                         uint32_t timeout_ms);

/**
 * @brief Get dog detection result.
 *
 * @param handle Client object.
 * @param result Output detected dogs.
 * @param timeout_ms I2C operation timeout in milliseconds. 0 means wait forever.
 * @return ESP32_S3_CAM_OK on success, otherwise an error code.
 */
esp32_s3_cam_err_t esp32_s3_cam_get_dog_result(esp32_s3_cam_handle_t *handle,
                                         esp32_s3_cam_target_detection_result_t *result,
                                         uint32_t timeout_ms);

/**
 * @brief Get pedestrian detection result.
 *
 * @param handle Client object.
 * @param result Output detected pedestrians.
 * @param timeout_ms I2C operation timeout in milliseconds. 0 means wait forever.
 * @return ESP32_S3_CAM_OK on success, otherwise an error code.
 */
esp32_s3_cam_err_t esp32_s3_cam_get_pedestrian_result(esp32_s3_cam_handle_t *handle,
                                                esp32_s3_cam_target_detection_result_t *result,
                                                uint32_t timeout_ms);

/**
 * @brief Get YOLO11n COCO target detection result.
 *
 * @param handle Client object.
 * @param result Output detected targets.
 * @param timeout_ms I2C operation timeout in milliseconds. 0 means wait forever.
 * @return ESP32_S3_CAM_OK on success, otherwise an error code.
 */
esp32_s3_cam_err_t esp32_s3_cam_get_yolo11n_result(esp32_s3_cam_handle_t *handle,
                                             esp32_s3_cam_target_detection_result_t *result,
                                             uint32_t timeout_ms);

/**
 * @brief Get face detection result.
 *
 * @param handle Client object.
 * @param result Output detected faces.
 * @param timeout_ms I2C operation timeout in milliseconds. 0 means wait forever.
 * @return ESP32_S3_CAM_OK on success, otherwise an error code.
 */
esp32_s3_cam_err_t esp32_s3_cam_get_face_result(esp32_s3_cam_handle_t *handle,
                                          esp32_s3_cam_target_detection_result_t *result,
                                          uint32_t timeout_ms);

/**
 * @brief Get COCO pose detection result.
 *
 * @param handle Client object.
 * @param result Output pose detections.
 * @param timeout_ms I2C operation timeout in milliseconds. 0 means wait forever.
 * @return ESP32_S3_CAM_OK on success, otherwise an error code.
 */
esp32_s3_cam_err_t esp32_s3_cam_get_pose_result(esp32_s3_cam_handle_t *handle,
                                          esp32_s3_cam_pose_result_t *result,
                                          uint32_t timeout_ms);

/**
 * @brief Get AprilTag recognition result.
 *
 * @param handle Client object.
 * @param result Output recognized AprilTags.
 * @param timeout_ms I2C operation timeout in milliseconds. 0 means wait forever.
 * @return ESP32_S3_CAM_OK on success, otherwise an error code.
 */
esp32_s3_cam_err_t esp32_s3_cam_get_apriltag_result(esp32_s3_cam_handle_t *handle,
                                              esp32_s3_cam_apriltag_result_t *result,
                                              uint32_t timeout_ms);

/**
 * @brief Get QR code recognition result.
 *
 * @param handle Client object.
 * @param result Output recognized QR codes.
 * @param timeout_ms I2C operation timeout in milliseconds. 0 means wait forever.
 * @return ESP32_S3_CAM_OK on success, otherwise an error code.
 */
esp32_s3_cam_err_t esp32_s3_cam_get_qrcode_result(esp32_s3_cam_handle_t *handle,
                                            esp32_s3_cam_qrcode_result_t *result,
                                            uint32_t timeout_ms);

/**
 * @brief Set horizontal mirror.
 *
 * @param handle Client object.
 * @param enable true to enable horizontal mirror, false to disable.
 * @param success Output command result reported by the device.
 * @param timeout_ms I2C operation timeout in milliseconds. 0 means wait forever.
 * @return ESP32_S3_CAM_OK on success, otherwise an error code.
 */
esp32_s3_cam_err_t esp32_s3_cam_set_hmirror(esp32_s3_cam_handle_t *handle,
                                      bool enable,
                                      bool *success,
                                      uint32_t timeout_ms);

/**
 * @brief Set vertical flip.
 *
 * @param handle Client object.
 * @param enable true to enable vertical flip, false to disable.
 * @param success Output command result reported by the device.
 * @param timeout_ms I2C operation timeout in milliseconds. 0 means wait forever.
 * @return ESP32_S3_CAM_OK on success, otherwise an error code.
 */
esp32_s3_cam_err_t esp32_s3_cam_set_vflip(esp32_s3_cam_handle_t *handle,
                                    bool enable,
                                    bool *success,
                                    uint32_t timeout_ms);

/**
 * @brief Reset module (software reboot).
 *
 * The protocol does not define an I2C response for this command.
 *
 * @param handle Client object.
 * @param timeout_ms Write timeout in milliseconds. 0 means wait forever.
 * @return ESP32_S3_CAM_OK on success, otherwise an error code.
 */
esp32_s3_cam_err_t esp32_s3_cam_module_reset(esp32_s3_cam_handle_t *handle,
                                       uint32_t timeout_ms);

/**
 * @brief Get face recognition result.
 *
 * @param handle Client object.
 * @param result Output detected and recognized faces.
 * @param timeout_ms I2C operation timeout in milliseconds. 0 means wait forever.
 * @return ESP32_S3_CAM_OK on success, otherwise an error code.
 */
esp32_s3_cam_err_t esp32_s3_cam_get_face_recognition_result(esp32_s3_cam_handle_t *handle,
                                                      esp32_s3_cam_face_recognition_result_t *result,
                                                      uint32_t timeout_ms);

/**
 * @brief Register the primary face detected in the current camera frame.
 *
 * @param handle Client object.
 * @param success Output true if registration succeeded, false otherwise.
 * @param face_id Output assigned face ID if registration succeeded.
 * @param timeout_ms I2C operation timeout in milliseconds. 0 means wait forever.
 * @return ESP32_S3_CAM_OK on success, otherwise an error code.
 */
esp32_s3_cam_err_t esp32_s3_cam_face_register(esp32_s3_cam_handle_t *handle,
                                        bool *success,
                                        uint16_t *face_id,
                                        uint32_t timeout_ms);

/**
 * @brief Delete registered face by ID (or delete all faces if face_id is 0).
 *
 * @param handle Client object.
 * @param face_id Face ID to delete (0 = delete all faces).
 * @param success Output true if deletion succeeded, false otherwise.
 * @param timeout_ms I2C operation timeout in milliseconds. 0 means wait forever.
 * @return ESP32_S3_CAM_OK on success, otherwise an error code.
 */
esp32_s3_cam_err_t esp32_s3_cam_face_delete(esp32_s3_cam_handle_t *handle,
                                      uint16_t face_id,
                                      bool *success,
                                      uint32_t timeout_ms);

/**
 * @brief Get total count of registered faces.
 *
 * @param handle Client object.
 * @param count Output registered face count.
 * @param timeout_ms I2C operation timeout in milliseconds. 0 means wait forever.
 * @return ESP32_S3_CAM_OK on success, otherwise an error code.
 */
esp32_s3_cam_err_t esp32_s3_cam_get_face_count(esp32_s3_cam_handle_t *handle,
                                         uint8_t *count,
                                         uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* ESP32_S3_CAM_H_ */
