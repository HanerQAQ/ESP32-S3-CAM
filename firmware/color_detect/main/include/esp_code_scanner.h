#pragma once

#include "esp_err.h"



struct esp_image_scanner_s;
/** Opaque image-scanner object / 不透明的图像扫描器对象 */
typedef struct esp_image_scanner_s esp_image_scanner_t;



typedef enum{
    ESP_CODE_SCANNER_MODE_FAST = 0,
    /* Additional scanning modes / 更多扫描模式 */
}esp_code_scanner_mode_t;

typedef enum{
    ESP_CODE_SCANNER_IMAGE_GRAY = 0,
    ESP_CODE_SCANNER_IMAGE_RGB565,
    ESP_CODE_SCANNER_IMAGE_YUV422,
    /* Additional image formats / 更多图像格式 */
}esp_code_scanner_image_format_t;

typedef struct
{
    esp_code_scanner_mode_t mode;
    esp_code_scanner_image_format_t fmt;
    uint32_t width;  /*!< Image width / 图像宽度 */
    uint32_t height; /*!< Image height / 图像高度 */
    /* Additional configuration options / 更多配置选项 */
} esp_code_scanner_config_t;

typedef enum{
    ESP_CODE_SCANNER_SYMBOL_CODE39,
    ESP_CODE_SCANNER_SYMBOL_CODE128,
    ESP_CODE_SCANNER_SYMBOL_QR
}esp_code_scanner_symbol_type_t;


typedef struct esp_code_scanner_symbol_t esp_code_scanner_symbol_t;
struct esp_code_scanner_symbol_t
{
    const char *type_name;
    const char *data;
    uint32_t datalen;
    esp_code_scanner_symbol_t* next;
};


/**
 * @brief      Start a code scanner / 启动码扫描器
 *             Call this function first / 必须首先调用此函数，
 *             It returns an esp_image_scanner_t handle for the other interface functions. / 它会返回 esp_image_scanner_t 句柄，供接口中的其他函数使用。
 *             Call esp_code_scanner_cleanup when the operation is complete. / 操作完成后必须调用 esp_code_scanner_cleanup 释放资源。
 *
 * @return
 *     - `esp_image_scanner_t`
 *     - NULL if an error occurs / 出错时返回 NULL
 */
esp_image_scanner_t* esp_code_scanner_create();

/**
 * @brief      Set the code-scanner configuration / 设置码扫描器配置
 *
 * @param[in]  config   Configuration options / 配置项，参见 `esp_code_scanner_config_t`
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL if an error occurs / 出错时返回 ESP_FAIL
 */
esp_err_t esp_code_scanner_set_config(esp_image_scanner_t* scanner, const esp_code_scanner_config_t config);

/**
 * @brief      Scan image data / 扫描图像数据
 *
 * @param[in]  scanner  Image-scanner handle / 图像扫描器句柄
 * @param[in]  image_data     Image data; only grayscale images are supported / 图像数据；目前只支持灰度图像
 *
 * @return
 *  	- number of decoded symbol 
 */
int esp_code_scanner_scan_image(esp_image_scanner_t* scanner, const uint8_t *image_data);

/**
 * @brief      Release the scanner instance and its configuration / 释放扫描器实例及其配置
 *
 * @param[in]  scanner  Code-scanner handle / 码扫描器句柄
 *
 */
void esp_code_scanner_destroy(esp_image_scanner_t* scanner);

/**
 * @brief      Get scan results / 获取扫描结果
 *
 * @param[in]  scanner  Code-scanner handle / 码扫描器句柄
 *
 * @return
 *  	- `const esp_code_scanner_symbol_t`
 *  	- Returns NULL if no data is recognized
 */
esp_code_scanner_symbol_t esp_code_scanner_result(esp_image_scanner_t* scanner);