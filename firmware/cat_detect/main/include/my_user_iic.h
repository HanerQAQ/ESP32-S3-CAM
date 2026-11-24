#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "sdkconfig.h"
#include "esp_err.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include <stdint.h>
#include <stdbool.h>

// Define safe I2C slave pins away from Octal PSRAM IO33–37 / 定义 I2C 从机引脚并避开 Octal PSRAM 的 IO33–37
#define MYI2C_SLAVE_NUM  I2C_NUM_0
#define MYI2C_SLAVE_SDA  (47)
#define MYI2C_SLAVE_SCL  (48)

// I2C device address: 0x52 (7-bit slave address) / I2C 设备地址：0x52（7 位从机地址）
#define Camera_IIC_ADDR  (0x52)

#define I2C_SLAVE_RX_BUF_LEN (1024)
#define I2C_SLAVE_TX_BUF_LEN (1024)

// Define project types / 定义项目类型
typedef enum {
    I2C_PROJ_CAT_DOG = 1,
    I2C_PROJ_COLOR = 2,
    I2C_PROJ_FACE_DETECT = 3,
    I2C_PROJ_FACE_RECOG = 4,
    I2C_PROJ_IMAGE_TRANS = 5,
    I2C_PROJ_QR_CODE = 6
} i2c_project_type_t;

// Shared detection-box structure for cat/dog, face, and color detection / 猫狗、人脸和颜色检测共用的目标框结构
typedef struct {
    uint8_t target_type; // Cat: 1, Dog: 2; faces: 1..N; colors: 1=red, 2=yellow, 3=green, 4=blue, 5=purple / 猫：1，狗：2；人脸：1..N；颜色：1=红、2=黄、3=绿、4=蓝、5=紫
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
} i2c_target_box_t;

// Face recognition / 人脸识别目标结构体 (含 2 字节 face_id)
typedef struct {
    uint16_t face_id;    // 0 means an unregistered stranger; >=1 is a registered face ID / 0 表示未注册的陌生人，>=1 表示已注册的人脸编号
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
} i2c_face_recog_target_t;

// Initialization API / 初始化接口
void My_i2c_init(const QueueHandle_t key_state_i);
void i2c_proto_set_project_type(i2c_project_type_t proj_type);

// 1. Update cat and dog detection results / 1. 更新猫狗检测结果
void i2c_proto_set_cat_dog_result(const i2c_target_box_t *targets, uint8_t count);

// 2. Update color-detection results / 2. 更新颜色识别结果
void i2c_proto_set_color_result(const i2c_target_box_t *blocks, uint8_t count);
void i2c_proto_set_line_result(bool detected, uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, float angle, float offset);
void i2c_proto_get_color_config(uint8_t *out_mode, uint8_t *out_targets, uint8_t *out_count);

// 3. Update face-detection results / 3. 更新人脸检测结果
void i2c_proto_set_face_detect_result(const i2c_target_box_t *faces, uint8_t count);

// 4. Update face-recognition results / 4. 更新人脸识别结果
void i2c_proto_set_face_recog_result(const i2c_face_recog_target_t *faces, uint8_t count);
void i2c_proto_set_face_enrolled_count(uint8_t count);
void i2c_proto_set_register_result(bool success, uint16_t new_id);
void i2c_proto_set_face_delete_target(int32_t target_id);
int32_t i2c_proto_get_face_delete_target(void);
void i2c_proto_set_face_delete_result(bool success);

// 5. Update QR-code detection results / 5. 更新二维码识别结果
void i2c_proto_set_qr_result(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const char *text, uint16_t len);
void i2c_proto_clear_qr_result(void);

// Keep legacy APIs for backward compatibility / 保留旧接口以兼容现有代码
void set_IIC_face_data(uint8_t cx, uint8_t cy, uint8_t w, uint8_t h);
void set_IIC_color_data(int color_idx, uint8_t cx, uint8_t cy, uint8_t w, uint8_t h);
void set_IIC_data(int16_t lx, int16_t ly, int16_t rx, int16_t ry);
void set_IIC_data_id(int16_t temp_id);
void Get_IIC_Key_massage(uint8_t keydata);
void switch_i2c_control(uint8_t REG_flag, uint8_t data);
void recv_i2c_data(uint8_t REG_flag);
int I2C_Send_Byte(uint8_t data);
int I2C_Send_Data(uint8_t* data, uint16_t len);

#ifdef __cplusplus
}
#endif