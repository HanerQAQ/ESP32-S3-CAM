#include "app_mywifi.h"
#include "wifi_config_page.h"
#include "cJSON.h"
// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD

//

// Licensed under the Apache License, Version 2.0 (the "License");

// you may not use this file except in compliance with the License.

// You may obtain a copy of the License at

//

//     http://www.apache.org/licenses/LICENSE-2.0

//

// Unless required by applicable law or agreed to in writing, software

// distributed under the License is distributed on an "AS IS" BASIS,

// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.

// See the License for the specific language governing permissions and

// limitations under the License.



#include "app_myhttpd.hpp"



#include <list>
#include <string.h>
#include <strings.h>
#include <stdlib.h>

#include "esp_http_server.h"

#include "esp_timer.h"

#include "img_converters.h"

#include "fb_gfx.h"


#include "sdkconfig.h"



#include "s3_cam_camera.h"

#include "lwip/sockets.h"

#include "esp_wifi.h"



#if defined(ARDUINO_ARCH_ESP32) && defined(CONFIG_ARDUHAL_ESP_LOG)

#include "esp32-hal-log.h"

#define TAG ""

#else

#include "esp_log.h"

static const char *TAG = "camera_httpd";

#endif



static QueueHandle_t xQueueFrameI = NULL;

static QueueHandle_t xQueueFrameO = NULL;

static bool gReturnFB = true;

static volatile int s_stream_clients = 0;
static volatile uint32_t s_stream_frame_count = 0;
static volatile uint32_t s_last_jpg_len = 0;



static int8_t detection_enabled = 0;

static int8_t recognition_enabled = 0;

static int8_t is_enrolling = 0;



typedef struct

{

    httpd_req_t *req;

    size_t len;

} jpg_chunking_t;



#define PART_BOUNDARY "123456789000000000000987654321"

static const char *_STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;

#define _STREAM_PART "Content-Type: image/jpeg\r\nContent-Length: %u\r\nX-Timestamp: %d.%06d\r\n\r\n"

#define _STREAM_PACKET_HEADER "\r\n--" PART_BOUNDARY "\r\n" _STREAM_PART

// Frames always originate from the camera driver's buffer pool, regardless of pixel format
static void release_frame(camera_fb_t *frame)
{
    if (!frame)
    {
        return;
    }
    if (xQueueFrameO && xQueueSend(xQueueFrameO, &frame, 0) == pdTRUE)
    {
        return;
    }
    if (gReturnFB)
    {
        esp_camera_fb_return(frame);
        return;
    }
    free(frame->buf);
    free(frame);
}



httpd_handle_t stream_httpd = NULL;

httpd_handle_t camera_httpd = NULL;



static esp_err_t capture_handler(httpd_req_t *req)

{

    ESP_LOGI(TAG, "capture_handler entered");

    camera_fb_t *frame = NULL;

    esp_err_t res = ESP_OK;

    app_camera_capture_acquire();

    app_camera_ensure_ready();

    app_camera_frame_lock();

    if (xQueueReceive(xQueueFrameI, &frame, pdMS_TO_TICKS(8000)))

    {

        httpd_resp_set_type(req, "image/jpeg");

        httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");

        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");



        char ts[32];

        snprintf(ts, 32, "%lld.%06ld", frame->timestamp.tv_sec, frame->timestamp.tv_usec);

        httpd_resp_set_hdr(req, "X-Timestamp", (const char *)ts);



        if (frame->format == PIXFORMAT_JPEG)

        {

            res = httpd_resp_send(req, (const char *)frame->buf, frame->len);

            release_frame(frame);

        }

        else

        {

            uint8_t *jpg_buf = NULL;
            size_t jpg_len = 0;
            uint8_t jpg_q = app_camera_get_quality();
            if (frame->width >= 640 && jpg_q > 90) {
                jpg_q = 90;
            } else if (jpg_q > 95) {
                jpg_q = 95;
            }

            if (!frame2jpg(frame, jpg_q, &jpg_buf, &jpg_len))

            {

                ESP_LOGE(TAG, "capture: frame2jpg failed!");

                res = ESP_FAIL;

            }

            release_frame(frame);
            app_camera_frame_unlock();

            if (jpg_buf)

            {

                if (res == ESP_OK)

                {

                    ESP_LOGI(TAG, "capture: frame converted to JPEG %u bytes", (unsigned)jpg_len);

                    res = httpd_resp_send(req, (const char *)jpg_buf, jpg_len);

                }

                free(jpg_buf);

            }

            app_camera_capture_release();

            return res;

        }

    }

    else

    {

        app_camera_frame_unlock();

        app_camera_capture_release();

        ESP_LOGE(TAG, "Camera capture failed (timeout)");

        httpd_resp_send_500(req);

        return ESP_FAIL;

    }



    app_camera_frame_unlock();

    app_camera_capture_release();

    return res;

}

static esp_err_t capture_binary_handler(httpd_req_t *req)

{

    camera_fb_t *frame = NULL;

    app_camera_capture_acquire();

    app_camera_ensure_ready();

    app_camera_frame_lock();

    if (!xQueueReceive(xQueueFrameI, &frame, pdMS_TO_TICKS(8000)))

    {

        app_camera_frame_unlock();

        app_camera_capture_release();

        ESP_LOGE(TAG, "capture_binary: camera capture failed (timeout)");

        httpd_resp_send_500(req);

        return ESP_FAIL;

    }

    httpd_resp_set_type(req, "application/octet-stream");

    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=camera_raw.bin");

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    esp_err_t res = httpd_resp_send(req, (const char *)frame->buf, frame->len);

    release_frame(frame);

    app_camera_frame_unlock();

    app_camera_capture_release();

    return res;

}



static esp_err_t stream_redirect_handler(httpd_req_t *req)
{
    char redirect_url[96];
    char host_hdr[48] = { 0 };
    snprintf(host_hdr, sizeof(host_hdr), "%s", app_mywifi_get_ap_ip());
    if (httpd_req_get_hdr_value_str(req, "Host", host_hdr, sizeof(host_hdr)) != ESP_OK || host_hdr[0] == '\0')
    {
        snprintf(host_hdr, sizeof(host_hdr), "%s", app_mywifi_get_ap_ip());
    }
    char *colon = strchr(host_hdr, ':');
    if (colon) *colon = '\0';
    snprintf(redirect_url, sizeof(redirect_url), "http://%s:81/stream", host_hdr);
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", redirect_url);
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t stream_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "stream_handler entered");
    s_stream_clients = s_stream_clients + 1;
    app_camera_capture_acquire();
    app_camera_ensure_ready();

    camera_fb_t *frame = NULL;
    struct timeval _timestamp;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len = 0;
    uint8_t *_jpg_buf = NULL;
    char part_buf[160];

    int sockfd = httpd_req_to_sockfd(req);
    if (sockfd >= 0)
    {
        int nodelay = 1;
        setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));
        struct timeval to;
        to.tv_sec = 2; // Use a 2-second timeout to avoid deadlock during prolonged network congestion / 设置 2 秒超时，避免长时间网络拥塞导致死锁
        to.tv_usec = 0;
        setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &to, sizeof(to));
    }

    // Clear queued old frames so streaming starts with the current live image / 清除积压帧，确保推流首帧是当前实时画面
    camera_fb_t *stale_fb = NULL;
    app_camera_frame_lock();
    while (xQueueReceive(xQueueFrameI, &stale_fb, 0) == pdTRUE)
    {
        release_frame(stale_fb);
    }
    app_camera_frame_unlock();

    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if (res != ESP_OK)
    {
        s_stream_clients = s_stream_clients - 1;
        if (s_stream_clients < 0) s_stream_clients = 0;
        app_camera_capture_release();
        return res;
    }

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    int send_consecutive_errs = 0;
    while (true)
    {
        frame = NULL;
        camera_fb_t *temp = NULL;
        bool got_frame = false;
        for (int wait_slice = 0; wait_slice < 20; wait_slice++)
        {
            app_camera_frame_lock();
            if (xQueueReceive(xQueueFrameI, &frame, pdMS_TO_TICKS(100)) == pdTRUE)
            {
                got_frame = true;
                break;
            }
            app_camera_frame_unlock();
            // Exit promptly if the client closed the TCP socket while waiting
            if (sockfd >= 0)
            {
                char peek_ch;
                int pr = recv(sockfd, &peek_ch, 1, MSG_PEEK | MSG_DONTWAIT);
                if (pr == 0 || (pr < 0 && errno != EAGAIN && errno != EWOULDBLOCK))
                {
                    ESP_LOGI(TAG, "stream: client disconnected while waiting for frame");
                    got_frame = false;
                    goto stream_exit;
                }
            }
        }
        if (!got_frame)
        {
            ESP_LOGW(TAG, "stream: queue timeout, waiting next frame");
            continue;
        }

        // Drop queued old frames so each send uses the newest image with minimal latency / 清除积压帧，每次只压缩并发送最新画面以降低延迟
        while (xQueueReceive(xQueueFrameI, &temp, 0) == pdTRUE)
        {
            release_frame(frame);
            frame = temp;
        }

        _timestamp.tv_sec = frame->timestamp.tv_sec;
        _timestamp.tv_usec = frame->timestamp.tv_usec;

        if (frame->format == PIXFORMAT_JPEG)
        {
            _jpg_buf = frame->buf;
            _jpg_buf_len = frame->len;
        }
        else
        {
            uint8_t jpg_q = app_camera_get_quality();
            if (frame->width >= 640 && jpg_q > 90) {
                jpg_q = 90;
            } else if (jpg_q > 95) {
                jpg_q = 95;
            }
            if (!frame2jpg(frame, jpg_q, &_jpg_buf, &_jpg_buf_len))
            {
                ESP_LOGE(TAG, "JPEG compression failed");
                res = ESP_FAIL;
            }

            release_frame(frame);
            frame = NULL;
        }

        if (res == ESP_OK)
        {
            s_last_jpg_len = (uint32_t)_jpg_buf_len;
            size_t hlen = snprintf(part_buf, sizeof(part_buf), _STREAM_PACKET_HEADER, _jpg_buf_len, (int)_timestamp.tv_sec, (int)_timestamp.tv_usec);
            res = httpd_resp_send_chunk(req, part_buf, hlen);
        }

        if (res == ESP_OK)
        {
            res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
            if (res == ESP_OK)
            {
                s_stream_frame_count = s_stream_frame_count + 1;
            }
        }

        if (frame)
        {
            release_frame(frame);
            frame = NULL;
            _jpg_buf = NULL;
        }
        else if (_jpg_buf != NULL)
        {
            free(_jpg_buf);
            _jpg_buf = NULL;
        }

        app_camera_frame_unlock();

        if (res != ESP_OK)
        {
            send_consecutive_errs++;
            if (send_consecutive_errs >= 3)
            {
                ESP_LOGW(TAG, "stream_handler: chunk send failed 3 consecutive times, closing client connection");
                break;
            }
            res = ESP_OK;
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        else
        {
            send_consecutive_errs = 0;
        }
    }

stream_exit:
    s_stream_clients = s_stream_clients - 1;
    if (s_stream_clients < 0) s_stream_clients = 0;
    app_camera_capture_release();
    ESP_LOGI(TAG, "stream_handler exited");
    return ESP_OK;
}



static esp_err_t parse_get(httpd_req_t *req, char **obuf)

{

    char *buf = NULL;

    size_t buf_len = 0;



    buf_len = httpd_req_get_url_query_len(req) + 1;

    if (buf_len > 1)

    {

        buf = (char *)malloc(buf_len);

        if (!buf)

        {

            httpd_resp_send_500(req);

            return ESP_FAIL;

        }

        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK)

        {

            *obuf = buf;

            return ESP_OK;

        }

        free(buf);

    }

    httpd_resp_send_404(req);

    return ESP_FAIL;

}



static esp_err_t cmd_handler(httpd_req_t *req)

{

    char *buf = NULL;

    char variable[32];

    char value[32];



    if (parse_get(req, &buf) != ESP_OK ||

        httpd_query_key_value(buf, "var", variable, sizeof(variable)) != ESP_OK ||

        httpd_query_key_value(buf, "val", value, sizeof(value)) != ESP_OK)

    {

        free(buf);

        httpd_resp_send_404(req);

        return ESP_FAIL;

    }

    free(buf);



    int val = atoi(value);

    ESP_LOGI(TAG, "%s = %d", variable, val);

    sensor_t *s = esp_camera_sensor_get();

    int res = 0;



        if (!strcmp(variable, "framesize"))
    {
        if (app_camera_set_resolution((framesize_t)val) == ESP_OK) {
            res = 0;
        } else {
            res = -1;
        }
    }
    else if (!strcmp(variable, "quality"))
    {
        res = (app_camera_set_quality((uint8_t)val) == ESP_OK) ? 0 : -1;
    }

    else if (!strcmp(variable, "contrast"))

        res = s->set_contrast(s, val);

    else if (!strcmp(variable, "brightness"))

        res = s->set_brightness(s, val);

    else if (!strcmp(variable, "saturation"))

        res = s->set_saturation(s, val);

    else if (!strcmp(variable, "gainceiling"))

        res = s->set_gainceiling(s, (gainceiling_t)val);

    else if (!strcmp(variable, "colorbar"))

        res = s->set_colorbar(s, val);

    else if (!strcmp(variable, "awb"))

        res = s->set_whitebal(s, val);

    else if (!strcmp(variable, "agc"))

        res = s->set_gain_ctrl(s, val);

    else if (!strcmp(variable, "aec"))

        res = s->set_exposure_ctrl(s, val);

    else if (!strcmp(variable, "hmirror"))

    {

        if (s->set_hmirror) {

            s->set_hmirror(s, val);

        }

        s->status.hmirror = val;

        res = 0;

    }

    else if (!strcmp(variable, "vflip"))

    {

        if (s->set_vflip) {

            s->set_vflip(s, val);

        }

        s->status.vflip = val;

        res = 0;

    }

    else if (!strcmp(variable, "awb_gain"))

        res = s->set_awb_gain(s, val);

    else if (!strcmp(variable, "agc_gain"))

        res = s->set_agc_gain(s, val);

    else if (!strcmp(variable, "aec_value"))

        res = s->set_aec_value(s, val);

    else if (!strcmp(variable, "aec2"))

        res = s->set_aec2(s, val);

    else if (!strcmp(variable, "dcw"))

        res = s->set_dcw(s, val);

    else if (!strcmp(variable, "bpc"))

        res = s->set_bpc(s, val);

    else if (!strcmp(variable, "wpc"))

        res = s->set_wpc(s, val);

    else if (!strcmp(variable, "raw_gma"))

        res = s->set_raw_gma(s, val);

    else if (!strcmp(variable, "lenc"))

        res = s->set_lenc(s, val);

    else if (!strcmp(variable, "special_effect"))

        res = s->set_special_effect(s, val);

    else if (!strcmp(variable, "wb_mode"))

        res = s->set_wb_mode(s, val);

    else if (!strcmp(variable, "ae_level"))

        res = s->set_ae_level(s, val);

#ifdef CONFIG_LED_ILLUMINATOR_ENABLED

    else if (!strcmp(variable, "led_intensity"))

        led_duty = val;

#endif



    else if (!strcmp(variable, "face_detect"))

    {

        detection_enabled = val;

        if (!detection_enabled)

        {

            recognition_enabled = 0;

        }

    }

    else if (!strcmp(variable, "face_enroll"))

        is_enrolling = val;

    else if (!strcmp(variable, "face_recognize"))

    {

        recognition_enabled = val;

        if (recognition_enabled)

        {

            detection_enabled = val;

        }

    }

    else

    {

        res = -1;

    }



    if (res)

    {

        return httpd_resp_send_500(req);

    }



    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    return httpd_resp_send(req, NULL, 0);

}



static int print_reg(char *p, sensor_t *s, uint16_t reg, uint32_t mask)

{

    return sprintf(p, "\"0x%x\":%u,", reg, s->get_reg(s, reg, mask));

}



static esp_err_t status_handler(httpd_req_t *req)

{

    static char json_response[1280];



    sensor_t *s = esp_camera_sensor_get();

    char *p = json_response;

    *p++ = '{';



    if (s->id.PID == OV5640_PID || s->id.PID == OV3660_PID)

    {

        for (int reg = 0x3400; reg < 0x3406; reg += 2)

        {

            p += print_reg(p, s, reg, 0xFFF); // 12-bit register / 12 位寄存器

        }

        p += print_reg(p, s, 0x3406, 0xFF);



        p += print_reg(p, s, 0x3500, 0xFFFF0); // 16-bit register / 16 位寄存器

        p += print_reg(p, s, 0x3503, 0xFF);

        p += print_reg(p, s, 0x350a, 0x3FF);  // 10-bit register / 10 位寄存器

        p += print_reg(p, s, 0x350c, 0xFFFF); // 16-bit register / 16 位寄存器



        for (int reg = 0x5480; reg <= 0x5490; reg++)

        {

            p += print_reg(p, s, reg, 0xFF);

        }



        for (int reg = 0x5380; reg <= 0x538b; reg++)

        {

            p += print_reg(p, s, reg, 0xFF);

        }



        for (int reg = 0x5580; reg < 0x558a; reg++)

        {

            p += print_reg(p, s, reg, 0xFF);

        }

        p += print_reg(p, s, 0x558a, 0x1FF); // 9-bit register / 9 位寄存器

    }

    else

    {

        p += print_reg(p, s, 0xd3, 0xFF);

        p += print_reg(p, s, 0x111, 0xFF);

        p += print_reg(p, s, 0x132, 0xFF);

    }



    p += sprintf(p, "\"board\":\"%s\",", CAMERA_MODULE_NAME);

    p += sprintf(p, "\"xclk\":%u,", s->xclk_freq_hz / 1000000);

    p += sprintf(p, "\"pixformat\":%u,", s->pixformat);

    p += sprintf(p, "\"framesize\":%u,", s->status.framesize);

    p += sprintf(p, "\"quality\":%u,", (s->status.quality > 0) ? s->status.quality : 10);

    p += sprintf(p, "\"brightness\":%d,", s->status.brightness);

    p += sprintf(p, "\"contrast\":%d,", s->status.contrast);

    p += sprintf(p, "\"saturation\":%d,", s->status.saturation);

    p += sprintf(p, "\"sharpness\":%d,", s->status.sharpness);

    p += sprintf(p, "\"special_effect\":%u,", s->status.special_effect);

    p += sprintf(p, "\"wb_mode\":%u,", s->status.wb_mode);

    p += sprintf(p, "\"awb\":%u,", s->status.awb);

    p += sprintf(p, "\"awb_gain\":%u,", s->status.awb_gain);

    p += sprintf(p, "\"aec\":%u,", s->status.aec);

    p += sprintf(p, "\"aec2\":%u,", s->status.aec2);

    p += sprintf(p, "\"ae_level\":%d,", s->status.ae_level);

    p += sprintf(p, "\"aec_value\":%u,", s->status.aec_value);

    p += sprintf(p, "\"agc\":%u,", s->status.agc);

    p += sprintf(p, "\"agc_gain\":%u,", s->status.agc_gain);

    p += sprintf(p, "\"gainceiling\":%u,", s->status.gainceiling);

    p += sprintf(p, "\"bpc\":%u,", s->status.bpc);

    p += sprintf(p, "\"wpc\":%u,", s->status.wpc);

    p += sprintf(p, "\"raw_gma\":%u,", s->status.raw_gma);

    p += sprintf(p, "\"lenc\":%u,", s->status.lenc);

    p += sprintf(p, "\"hmirror\":%u,", s->status.hmirror);

    p += sprintf(p, "\"dcw\":%u,", s->status.dcw);

    p += sprintf(p, "\"colorbar\":%u", s->status.colorbar);

#ifdef CONFIG_LED_ILLUMINATOR_ENABLED

    p += sprintf(p, ",\"led_intensity\":%u", led_duty);

#else

    p += sprintf(p, ",\"led_intensity\":%d", -1);

#endif

    p += sprintf(p, ",\"face_detect\":%u", detection_enabled);

    p += sprintf(p, ",\"face_enroll\":%u,", is_enrolling);

    p += sprintf(p, "\"face_recognize\":%u", recognition_enabled);

    p += sprintf(p, ",\"stream_clients\":%d,\"stream_frames\":%lu", s_stream_clients, (unsigned long)s_stream_frame_count);
    wifi_ap_record_t ap_info;
    int sta_rssi = (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) ? ap_info.rssi : 0;
    p += sprintf(p, ",\"frames_captured\":%lu,\"last_jpeg\":%lu,\"sta_rssi\":%d",
                 (unsigned long)app_camera_get_frame_count(), (unsigned long)s_last_jpg_len, sta_rssi);

    *p++ = '}';

    *p++ = 0;

    httpd_resp_set_type(req, "application/json");

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    return httpd_resp_send(req, json_response, strlen(json_response));

}







static esp_err_t xclk_handler(httpd_req_t *req)

{

    char *buf = NULL;

    char _xclk[32];



    if (parse_get(req, &buf) != ESP_OK ||

        httpd_query_key_value(buf, "xclk", _xclk, sizeof(_xclk)) != ESP_OK)

    {

        free(buf);

        httpd_resp_send_404(req);

        return ESP_FAIL;

    }

    free(buf);



    int xclk = atoi(_xclk);

    ESP_LOGI(TAG, "Set XCLK: %d MHz", xclk);



    sensor_t *s = esp_camera_sensor_get();

    int res = s->set_xclk(s, LEDC_TIMER_0, xclk);

    if (res)

    {

        return httpd_resp_send_500(req);

    }



    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    return httpd_resp_send(req, NULL, 0);

}



static esp_err_t reg_handler(httpd_req_t *req)

{

    char *buf = NULL;

    char _reg[32];

    char _mask[32];

    char _val[32];



    if (parse_get(req, &buf) != ESP_OK ||

        httpd_query_key_value(buf, "reg", _reg, sizeof(_reg)) != ESP_OK ||

        httpd_query_key_value(buf, "mask", _mask, sizeof(_mask)) != ESP_OK ||

        httpd_query_key_value(buf, "val", _val, sizeof(_val)) != ESP_OK)

    {

        free(buf);

        httpd_resp_send_404(req);

        return ESP_FAIL;

    }

    free(buf);



    int reg = atoi(_reg);

    int mask = atoi(_mask);

    int val = atoi(_val);

    ESP_LOGI(TAG, "Set Register: reg: 0x%02x, mask: 0x%02x, value: 0x%02x", reg, mask, val);



    sensor_t *s = esp_camera_sensor_get();

    int res = s->set_reg(s, reg, mask, val);

    if (res)

    {

        return httpd_resp_send_500(req);

    }



    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    return httpd_resp_send(req, NULL, 0);

}



static esp_err_t greg_handler(httpd_req_t *req)

{

    char *buf = NULL;

    char _reg[32];

    char _mask[32];



    if (parse_get(req, &buf) != ESP_OK ||

        httpd_query_key_value(buf, "reg", _reg, sizeof(_reg)) != ESP_OK ||

        httpd_query_key_value(buf, "mask", _mask, sizeof(_mask)) != ESP_OK)

    {

        free(buf);

        httpd_resp_send_404(req);

        return ESP_FAIL;

    }

    free(buf);



    int reg = atoi(_reg);

    int mask = atoi(_mask);

    sensor_t *s = esp_camera_sensor_get();

    int res = s->get_reg(s, reg, mask);

    if (res < 0)

    {

        return httpd_resp_send_500(req);

    }

    ESP_LOGI(TAG, "Get Register: reg: 0x%02x, mask: 0x%02x, value: 0x%02x", reg, mask, res);



    char buffer[20];

    const char *val = itoa(res, buffer, 10);

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    return httpd_resp_send(req, val, strlen(val));

}



static int parse_get_var(char *buf, const char *key, int def)

{

    char _int[16];

    if (httpd_query_key_value(buf, key, _int, sizeof(_int)) != ESP_OK)

    {

        return def;

    }

    return atoi(_int);

}



static esp_err_t pll_handler(httpd_req_t *req)

{

    char *buf = NULL;



    if (parse_get(req, &buf) != ESP_OK)

    {

        free(buf);

        httpd_resp_send_404(req);

        return ESP_FAIL;

    }



    int bypass = parse_get_var(buf, "bypass", 0);

    int mul = parse_get_var(buf, "mul", 0);

    int sys = parse_get_var(buf, "sys", 0);

    int root = parse_get_var(buf, "root", 0);

    int pre = parse_get_var(buf, "pre", 0);

    int seld5 = parse_get_var(buf, "seld5", 0);

    int pclken = parse_get_var(buf, "pclken", 0);

    int pclk = parse_get_var(buf, "pclk", 0);

    free(buf);



    ESP_LOGI(TAG, "Set Pll: bypass: %d, mul: %d, sys: %d, root: %d, pre: %d, seld5: %d, pclken: %d, pclk: %d", bypass, mul, sys, root, pre, seld5, pclken, pclk);

    sensor_t *s = esp_camera_sensor_get();

    int res = s->set_pll(s, bypass, mul, sys, root, pre, seld5, pclken, pclk);

    if (res)

    {

        return httpd_resp_send_500(req);

    }



    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    return httpd_resp_send(req, NULL, 0);

}



static esp_err_t win_handler(httpd_req_t *req)

{

    char *buf = NULL;



    if (parse_get(req, &buf) != ESP_OK)

    {

        free(buf);

        httpd_resp_send_404(req);

        return ESP_FAIL;

    }



    int startX = parse_get_var(buf, "sx", 0);

    int startY = parse_get_var(buf, "sy", 0);

    int endX = parse_get_var(buf, "ex", 0);

    int endY = parse_get_var(buf, "ey", 0);

    int offsetX = parse_get_var(buf, "offx", 0);

    int offsetY = parse_get_var(buf, "offy", 0);

    int totalX = parse_get_var(buf, "tx", 0);

    int totalY = parse_get_var(buf, "ty", 0);

    int outputX = parse_get_var(buf, "ox", 0);

    int outputY = parse_get_var(buf, "oy", 0);

    bool scale = parse_get_var(buf, "scale", 0) == 1;

    bool binning = parse_get_var(buf, "binning", 0) == 1;

    free(buf);



    ESP_LOGI(TAG, "Set Window: Start: %d %d, End: %d %d, Offset: %d %d, Total: %d %d, Output: %d %d, Scale: %u, Binning: %u", startX, startY, endX, endY, offsetX, offsetY, totalX, totalY, outputX, outputY, scale, binning);

    sensor_t *s = esp_camera_sensor_get();

    int res = s->set_res_raw(s, startX, startY, endX, endY, offsetX, offsetY, totalX, totalY, outputX, outputY, scale, binning);

    if (res)

    {

        return httpd_resp_send_500(req);

    }



    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    return httpd_resp_send(req, NULL, 0);

}



[[maybe_unused]] static esp_err_t index_handler(httpd_req_t *req)

{

    ESP_LOGI(TAG, "index_handler entered");

    extern const unsigned char index_ov2640_html_gz_start[] asm("_binary_index_ov2640_html_gz_start");

    extern const unsigned char index_ov2640_html_gz_end[] asm("_binary_index_ov2640_html_gz_end");

    size_t index_ov2640_html_gz_len = index_ov2640_html_gz_end - index_ov2640_html_gz_start;



    extern const unsigned char index_ov3660_html_gz_start[] asm("_binary_index_ov3660_html_gz_start");

    extern const unsigned char index_ov3660_html_gz_end[] asm("_binary_index_ov3660_html_gz_end");

    size_t index_ov3660_html_gz_len = index_ov3660_html_gz_end - index_ov3660_html_gz_start;



    extern const unsigned char index_ov5640_html_gz_start[] asm("_binary_index_ov5640_html_gz_start");

    extern const unsigned char index_ov5640_html_gz_end[] asm("_binary_index_ov5640_html_gz_end");

    size_t index_ov5640_html_gz_len = index_ov5640_html_gz_end - index_ov5640_html_gz_start;



    extern const unsigned char index_gc2145_html_gz_start[] asm("_binary_index_gc2145_html_gz_start");

    extern const unsigned char index_gc2145_html_gz_end[] asm("_binary_index_gc2145_html_gz_end");

    size_t index_gc2145_html_gz_len = index_gc2145_html_gz_end - index_gc2145_html_gz_start;



    httpd_resp_set_type(req, "text/html");

    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");

    sensor_t *s = esp_camera_sensor_get();

    if (s != NULL)

    {

        if (s->id.PID == OV3660_PID)

        {

            return httpd_resp_send(req, (const char *)index_ov3660_html_gz_start, index_ov3660_html_gz_len);

        }

        else if (s->id.PID == OV5640_PID)

        {

            return httpd_resp_send(req, (const char *)index_ov5640_html_gz_start, index_ov5640_html_gz_len);

        }

         else if (s->id.PID == GC2145_PID)

        {

            return httpd_resp_send(req, (const char *)index_gc2145_html_gz_start, index_gc2145_html_gz_len);

        }

        else

        {

            return httpd_resp_send(req, (const char *)index_ov2640_html_gz_start, index_ov2640_html_gz_len);

        }

    }

    else

    {

        ESP_LOGE(TAG, "Camera sensor not found");

        return httpd_resp_send_500(req);

    }

}



static esp_err_t monitor_handler(httpd_req_t *req)

{

    extern const unsigned char monitor_html_gz_start[] asm("_binary_monitor_html_gz_start");

    extern const unsigned char monitor_html_gz_end[] asm("_binary_monitor_html_gz_end");

    size_t monitor_html_gz_len = monitor_html_gz_end - monitor_html_gz_start;

    httpd_resp_set_type(req, "text/html");

    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");

    return httpd_resp_send(req, (const char *)monitor_html_gz_start, monitor_html_gz_len);

}



httpd_config_t myconfig = HTTPD_DEFAULT_CONFIG();


extern const unsigned char index_html_gz_start[] asm("_binary_index_html_gz_start");
extern const unsigned char index_html_gz_end[] asm("_binary_index_html_gz_end");

extern const unsigned char loading_jpg_gz_start[] asm("_binary_loading_jpg_gz_start");
extern const unsigned char loading_jpg_gz_end[] asm("_binary_loading_jpg_gz_end");

extern const unsigned char favicon_ico_gz_start[] asm("_binary_favicon_ico_gz_start");
extern const unsigned char favicon_ico_gz_end[] asm("_binary_favicon_ico_gz_end");

extern const unsigned char index_js_gz_start[] asm("_binary_index_js_gz_start");
extern const unsigned char index_js_gz_end[] asm("_binary_index_js_gz_end");

extern const unsigned char index_css_gz_start[] asm("_binary_index_css_gz_start");
extern const unsigned char index_css_gz_end[] asm("_binary_index_css_gz_end");


/* Wi-Fi Configuration & Provisioning Handlers */
static esp_err_t modern_index_handler(httpd_req_t *req);
static esp_err_t wifi_config_html_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    return httpd_resp_send(req, wifi_config_html, strlen(wifi_config_html));
}

static esp_err_t smart_root_handler(httpd_req_t *req) {
    char host[64] = {0};
    bool is_ap_host = false;
    if (httpd_req_get_hdr_value_str(req, "Host", host, sizeof(host)) == ESP_OK) {
        char *colon = strchr(host, ':');
        if (colon) *colon = '\0';
        const char *ap_ip = app_mywifi_get_ap_ip();
        is_ap_host = (strcmp(host, ap_ip) == 0);
    }

    /* AP host or STA not connected: go to the provisioning page.
     * The live camera view stays reachable via the router IP (/) and /cam. */
    if (is_ap_host || !app_mywifi_is_sta_connected()) {
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", "/wifi");
        return httpd_resp_send(req, NULL, 0);
    }

    return modern_index_handler(req);
}

#define WIFI_HTTP_BODY_SIZE         512
#define WIFI_HTTP_BODY_TIMEOUT_US   5000000

static esp_err_t receive_http_body(httpd_req_t *req, char *body, size_t capacity)
{
    if (req->content_len == 0 || req->content_len >= capacity) return ESP_ERR_INVALID_SIZE;
    size_t offset = 0;
    int64_t deadline = esp_timer_get_time() + WIFI_HTTP_BODY_TIMEOUT_US;
    while (offset < req->content_len) {
        if (esp_timer_get_time() >= deadline) return ESP_ERR_TIMEOUT;
        int received = httpd_req_recv(req, body + offset, req->content_len - offset);
        if (received == HTTPD_SOCK_ERR_TIMEOUT) return ESP_ERR_TIMEOUT;
        if (received <= 0) return ESP_FAIL;
        offset += received;
    }
    body[offset] = '\0';
    if (memchr(body, '\0', offset)) return ESP_ERR_INVALID_ARG;
    return ESP_OK;
}

static cJSON *parse_http_json(const char *body)
{
    /* cJSON strings have no length: reject decoded NULs rather than truncate credentials. */
    for (const char *p = body; *p; ++p) {
        if (*p == '\\' && p[1]) {
            if (p[1] == 'u' && strncmp(p + 2, "0000", 4) == 0) return NULL;
            ++p;
        }
    }
    cJSON *root = cJSON_ParseWithOpts(body, NULL, true);
    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return NULL;
    }
    return root;
}

static esp_err_t wifi_scan_handler(httpd_req_t *req)
{
    char query[64] = {0};
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char val[8] = {0};
        if (httpd_query_key_value(query, "start", val, sizeof(val)) == ESP_OK &&
            strcmp(val, "1") == 0) {
            app_mywifi_scan_trigger();
        }
    }
    char *json = app_mywifi_scan_json();
    if (!json) json = strdup("[]");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_hdr(req, "X-Scan-Running", app_mywifi_scan_running() ? "1" : "0");
    esp_err_t ret = httpd_resp_send(req, json, strlen(json));
    free(json);
    return ret;
}

static esp_err_t wifi_submit_handler(httpd_req_t *req)
{
    char body[WIFI_HTTP_BODY_SIZE];
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    esp_err_t ret = receive_http_body(req, body, sizeof(body));
    if (ret != ESP_OK) {
        httpd_resp_set_hdr(req, "Connection", "close");
        const char *error = ret == ESP_ERR_TIMEOUT ?
            "{\"success\":false,\"error\":\"Request body timeout\"}" :
            "{\"success\":false,\"error\":\"Invalid or incomplete body (maximum 511 bytes)\"}";
        httpd_resp_sendstr(req, error);
        return ESP_FAIL;
    }
    cJSON *root = parse_http_json(body);
    if (!root) return httpd_resp_sendstr(req, "{\"success\":false,\"error\":\"JSON parse error\"}");
    const cJSON *ssid = cJSON_GetObjectItemCaseSensitive(root, "ssid");
    const cJSON *password = cJSON_GetObjectItemCaseSensitive(root, "password");
    const cJSON *use_saved = cJSON_GetObjectItemCaseSensitive(root, "use_saved");
    const cJSON *bssid_item = cJSON_GetObjectItemCaseSensitive(root, "bssid");
    const cJSON *channel_item = cJSON_GetObjectItemCaseSensitive(root, "channel");
    uint8_t bssid[6] = { 0 };
    uint8_t channel = 0;
    bool has_bssid = false;
    bool bssid_valid = true;
    if (bssid_item) {
        unsigned int b[6];
        bssid_valid = false;
        if (cJSON_IsString(bssid_item) && bssid_item->valuestring &&
            strlen(bssid_item->valuestring) == 17 &&
            sscanf(bssid_item->valuestring, "%2x:%2x:%2x:%2x:%2x:%2x",
                   &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]) == 6 &&
            cJSON_IsNumber(channel_item)) {
            int ch = channel_item->valueint;
            if (ch > 0 && ch <= 14) {
                for (int i = 0; i < 6; ++i) bssid[i] = (uint8_t)b[i];
                channel = (uint8_t)ch;
                has_bssid = true;
                bssid_valid = true;
            }
        }
    }
    const char *response = NULL;
    if (!bssid_valid) {
        response = "{\"success\":false,\"error\":\"Invalid bssid/channel\"}";
    } else if (cJSON_IsTrue(use_saved)) {
        char saved_ssid[33] = { 0 };
        char saved_passwd[65] = { 0 };
        bool ok = app_mywifi_load_sta_creds(saved_ssid, sizeof(saved_ssid),
                                            saved_passwd, sizeof(saved_passwd)) == ESP_OK &&
                  saved_ssid[0] != '\0';
        if (ok && cJSON_IsString(ssid) && ssid->valuestring && ssid->valuestring[0] &&
            strcmp(ssid->valuestring, saved_ssid) != 0) {
            ok = false;
        }
        if (!ok) {
            response = "{\"success\":false,\"error\":\"No matching saved Wi-Fi credentials\"}";
        } else {
            ret = has_bssid ?
                  app_mywifi_connect_sta_ex(saved_ssid, saved_passwd, bssid, channel) :
                  app_mywifi_connect_sta(saved_ssid, saved_passwd);
            response = ret == ESP_OK ? "{\"success\":true,\"state\":\"connecting\",\"saved\":true}" :
                                      "{\"success\":false,\"error\":\"Failed to start Wi-Fi connection\"}";
        }
        memset(saved_passwd, 0, sizeof(saved_passwd));
    } else {
        const char *passwd = cJSON_IsString(password) ? password->valuestring : "";
        if (!cJSON_IsString(ssid) || !ssid->valuestring || ssid->valuestring[0] == '\0' ||
            strlen(ssid->valuestring) > 32) {
            response = "{\"success\":false,\"error\":\"SSID must be a nonempty string of at most 32 bytes\"}";
        } else if ((password && !cJSON_IsString(password)) || strlen(passwd) > 64) {
            response = "{\"success\":false,\"error\":\"Password must be a string of at most 64 bytes (empty for open networks)\"}";
        } else {
            /* Wi-Fi owns persistence: credentials are saved only after GOT_IP. */
            ret = has_bssid ?
                  app_mywifi_connect_sta_ex(ssid->valuestring, passwd, bssid, channel) :
                  app_mywifi_connect_sta(ssid->valuestring, passwd);
            response = ret == ESP_OK ? "{\"success\":true,\"state\":\"connecting\"}" :
                                      "{\"success\":false,\"error\":\"Failed to start Wi-Fi connection\"}";
        }
    }
    cJSON_Delete(root);
    return httpd_resp_sendstr(req, response);
}

static esp_err_t wifi_status_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    bool connected = app_mywifi_is_sta_connected();
    bool connecting = app_mywifi_is_sta_connecting();
    int reason = app_mywifi_get_disconnect_reason();
    const char *ip = app_mywifi_get_sta_ip();
    bool has_ip = ip && ip[0] != '\0';

    /* Never report "connected" without an IP: the UI would show an empty STA
     * address and stop polling before DHCP completes. */
    const char *state = "disconnected";
    if (connected && has_ip) {
        state = "connected";
        app_mywifi_notify_status_delivered();
    } else if (connecting || connected) {
        state = "connecting";
    } else if (reason > 0) {
        state = "failed";
    }

    cJSON_AddStringToObject(root, "state", state);
    cJSON_AddBoolToObject(root, "connected", connected && has_ip);
    cJSON_AddBoolToObject(root, "connecting", connecting || (connected && !has_ip));
    cJSON_AddStringToObject(root, "ip", has_ip ? ip : "");
    cJSON_AddStringToObject(root, "ssid", app_mywifi_get_sta_ssid());
    cJSON_AddNumberToObject(root, "reason", reason);
    cJSON_AddStringToObject(root, "message", app_mywifi_get_disconnect_message());
    cJSON_AddBoolToObject(root, "ap_active", app_mywifi_is_ap_active());
    cJSON_AddStringToObject(root, "ap_ip", app_mywifi_get_ap_ip());
    cJSON_AddStringToObject(root, "ap_ssid", app_mywifi_get_ap_ssid());
    if (connected) {
        uint8_t sta_channel = 0;
        wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
        if (esp_wifi_get_channel(&sta_channel, &second) == ESP_OK && sta_channel > 0) {
            cJSON_AddNumberToObject(root, "sta_channel", sta_channel);
            cJSON_AddStringToObject(root, "sta_band", "2.4G");
        }
    }

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    esp_err_t ret = httpd_resp_send(req, json_str, strlen(json_str));
    free(json_str);
    return ret;
}

static esp_err_t wifi_prepare_handler(httpd_req_t *req)
{
    char body[WIFI_HTTP_BODY_SIZE];
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    esp_err_t ret = receive_http_body(req, body, sizeof(body));
    if (ret != ESP_OK) {
        httpd_resp_set_hdr(req, "Connection", "close");
        return httpd_resp_sendstr(req, "{\"success\":false,\"error\":\"Invalid body\"}");
    }
    cJSON *root = parse_http_json(body);
    if (!root) return httpd_resp_sendstr(req, "{\"success\":false,\"error\":\"JSON parse error\"}");

    const cJSON *channel = cJSON_GetObjectItemCaseSensitive(root, "channel");
    esp_err_t err = ESP_ERR_INVALID_ARG;
    if (cJSON_IsNumber(channel)) {
        err = app_mywifi_prepare_ap_channel((uint8_t)channel->valueint);
    }
    cJSON_Delete(root);
    return httpd_resp_sendstr(req, err == ESP_OK ? "{\"success\":true}" : "{\"success\":false}");
}

static esp_err_t wifi_saved_handler(httpd_req_t *req) {
    char ssid[33] = { 0 };
    char passwd[65] = { 0 };
    bool saved = app_mywifi_load_sta_creds(ssid, sizeof(ssid), passwd, sizeof(passwd)) == ESP_OK &&
                 ssid[0] != '\0';
    bool has_password = passwd[0] != '\0';
    memset(passwd, 0, sizeof(passwd)); /* Never expose the stored password. */

    cJSON *root = cJSON_CreateObject();
    if (!root) return ESP_ERR_NO_MEM;
    cJSON_AddBoolToObject(root, "saved", saved);
    if (saved) {
        cJSON_AddStringToObject(root, "ssid", ssid);
        cJSON_AddBoolToObject(root, "has_password", has_password);
    }
    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json_str) return ESP_ERR_NO_MEM;

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    esp_err_t ret = httpd_resp_send(req, json_str, strlen(json_str));
    free(json_str);
    return ret;
}

static esp_err_t wifi_reset_handler(httpd_req_t *req) {
    app_mywifi_clear_sta_creds();
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    const char *resp = "{\"success\":true,\"message\":\"Wi-Fi credentials cleared\"}";
    return httpd_resp_send(req, resp, strlen(resp));
}

/* Modern Vue Web Frontend Handler */
static esp_err_t modern_index_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    return httpd_resp_send(req, (const char *)index_html_gz_start, index_html_gz_end - index_html_gz_start);
}

static esp_err_t loading_jpg_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=86400");
    return httpd_resp_send(req, (const char *)loading_jpg_gz_start, loading_jpg_gz_end - loading_jpg_gz_start);
}

static esp_err_t favicon_ico_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "image/x-icon");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=86400");
    return httpd_resp_send(req, (const char *)favicon_ico_gz_start, favicon_ico_gz_end - favicon_ico_gz_start);
}

static esp_err_t index_js_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/javascript");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=300");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, (const char *)index_js_gz_start, index_js_gz_end - index_js_gz_start);
}

static esp_err_t index_css_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/css");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=300");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, (const char *)index_css_gz_start, index_css_gz_end - index_css_gz_start);
}

/* Measured MJPEG rate, updated at most once per second */
static uint32_t get_stream_fps(void)
{
    static uint32_t s_fps_last_frames = 0;
    static int64_t s_fps_last_us = 0;
    static uint32_t s_fps_value = 0;

    int64_t now = esp_timer_get_time();
    if (s_fps_last_us == 0) {
        s_fps_last_us = now;
        s_fps_last_frames = s_stream_frame_count;
        return 0;
    }

    int64_t elapsed = now - s_fps_last_us;
    if (elapsed >= 1000000) {
        uint32_t frames_now = s_stream_frame_count;
        s_fps_value = (uint32_t)((int64_t)(frames_now - s_fps_last_frames) * 1000000 / elapsed);
        s_fps_last_frames = frames_now;
        s_fps_last_us = now;
    }
    return s_fps_value;
}

/* /api/get_camera_info Handler (Two-level resolution support: 320x240 & 640x480) */
static esp_err_t get_camera_info_handler(httpd_req_t *req)
{
    char buf[768];
    framesize_t cur_res = app_camera_get_resolution();
    int cur_fmt = (cur_res == FRAMESIZE_VGA) ? 1 : 0;
    uint32_t cur_w = (cur_res == FRAMESIZE_VGA) ? 640 : 320;
    uint32_t cur_h = (cur_res == FRAMESIZE_VGA) ? 480 : 240;
    uint8_t cur_q = app_camera_get_quality();
    uint32_t cur_fps = get_stream_fps();

    snprintf(buf, sizeof(buf),
             "{\"cameras\":[{\"index\":0,\"src\":\":81/stream\",\"currentFrameRate\":%u,"
             "\"currentImageFormat\":%d,\"currentImageFormatDescription\":\"JPEG %ux%u\","
             "\"currentQuality\":%u,\"currentResolution\":{\"width\":%u,\"height\":%u},"
             "\"imageFormats\":["
             "{\"id\":0,\"description\":\"JPEG 320x240\",\"quality\":{\"min\":1,\"max\":100,\"step\":1,\"default\":80}},"
             "{\"id\":1,\"description\":\"JPEG 640x480\",\"quality\":{\"min\":1,\"max\":100,\"step\":1,\"default\":60}}"
             "]}]}",
             (unsigned)cur_fps, cur_fmt, (unsigned)cur_w, (unsigned)cur_h, (unsigned)cur_q,
             (unsigned)cur_w, (unsigned)cur_h);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_sendstr(req, buf);
}

/* /api/capture_image & /api/frame.jpg Handler */
static esp_err_t capture_image_handler(httpd_req_t *req)
{
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=camera_0_image.jpg");
    return capture_handler(req);
}

/* Default /api/detections Handler */
static esp_err_t default_detections_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_sendstr(req, "{\"title\":\"Image Transmission STA Mode(\xe5\x9b\xbe\xe5\x83\x8f\xe5\x9b\x9e\xe4\xbc\xa0STA\xe6\xa8\xa1\xe5\xbc\x8f)\",\"type\":\"image_transmission\",\"enabled\":false,\"objects\":[]}");
}

/* /api/set_camera_config Handler */
static esp_err_t camera_settings_handler(httpd_req_t *req)
{
    char buf[256] = {};
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret > 0) {
        cJSON *root = cJSON_Parse(buf);
        if (root) {
            cJSON *item = cJSON_GetObjectItem(root, "image_format");
            if (!item) item = cJSON_GetObjectItem(root, "currentImageFormat");
            if (item && item->type == cJSON_Number) {
                int fmt = item->valueint;
                framesize_t target_size = (fmt == 1) ? FRAMESIZE_VGA : FRAMESIZE_QVGA;
                if (app_camera_set_resolution(target_size) == ESP_OK) {
                }
            }

            cJSON *q = cJSON_GetObjectItem(root, "quality");
            if (!q) q = cJSON_GetObjectItem(root, "jpeg_quality");
            if (!q) q = cJSON_GetObjectItem(root, "currentQuality");
            if (q && q->type == cJSON_Number) {
                int qual = q->valueint;
                if (qual >= 1 && qual <= 100) {
                    app_camera_set_quality((uint8_t)qual);
                    ESP_LOGI(TAG, "Quality updated via API to %d", qual);
                }
            }

            cJSON_Delete(root);
        }
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
}

void register_httpd(const QueueHandle_t frame_i, const QueueHandle_t frame_o, const bool return_fb)

{

    xQueueFrameI = frame_i;

    xQueueFrameO = frame_o;

    gReturnFB = return_fb;



    

    myconfig.max_uri_handlers = 48;

    myconfig.stack_size = 10240;

    myconfig.lru_purge_enable = true;
    myconfig.max_open_sockets = 7;



    httpd_uri_t index_uri = {

        .uri = "/",

        .method = HTTP_GET,

        .handler = smart_root_handler,

        .user_ctx = NULL};



    httpd_uri_t status_uri = {

        .uri = "/status",

        .method = HTTP_GET,

        .handler = status_handler,

        .user_ctx = NULL};



    httpd_uri_t cmd_uri = {

        .uri = "/control",

        .method = HTTP_GET,

        .handler = cmd_handler,

        .user_ctx = NULL};



    httpd_uri_t capture_uri = {

        .uri = "/capture",

        .method = HTTP_GET,

        .handler = capture_handler,

        .user_ctx = NULL};



    httpd_uri_t stream_uri = {

        .uri = "/stream",  // MJPEG stream endpoint / MJPEG 图像流接口

        .method = HTTP_GET,

        .handler = stream_handler,

        .user_ctx = NULL};

    httpd_uri_t stream_redirect_uri = {
        .uri = "/stream",
        .method = HTTP_GET,
        .handler = stream_redirect_handler,
        .user_ctx = NULL};



    httpd_uri_t xclk_uri = {

        .uri = "/xclk",

        .method = HTTP_GET,

        .handler = xclk_handler,

        .user_ctx = NULL};



    httpd_uri_t reg_uri = {

        .uri = "/reg",

        .method = HTTP_GET,

        .handler = reg_handler,

        .user_ctx = NULL};



    httpd_uri_t greg_uri = {

        .uri = "/greg",

        .method = HTTP_GET,

        .handler = greg_handler,

        .user_ctx = NULL};



    httpd_uri_t pll_uri = {

        .uri = "/pll",

        .method = HTTP_GET,

        .handler = pll_handler,

        .user_ctx = NULL};



    httpd_uri_t win_uri = {

        .uri = "/resolution",

        .method = HTTP_GET,

        .handler = win_handler,

        .user_ctx = NULL};



    httpd_uri_t monitor_uri = {

        .uri = "/monitor",

        .method = HTTP_GET,

        .handler = monitor_handler,

        .user_ctx = NULL};

    httpd_uri_t favicon_uri = {
        .uri = "/favicon.ico",
        .method = HTTP_GET,
        .handler = favicon_ico_handler,
        .user_ctx = NULL};





    ESP_LOGI(TAG, "Starting web server on port: '%d'", myconfig.server_port);

    
    /* Modern Single-Page Frontend & Assets */
    
    /* Wi-Fi Provisioning URIs */
    httpd_uri_t wifi_page_uri = { .uri = "/wifi", .method = HTTP_GET, .handler = wifi_config_html_handler, .user_ctx = NULL };
    httpd_uri_t cam_page_uri = { .uri = "/cam", .method = HTTP_GET, .handler = modern_index_handler, .user_ctx = NULL };
    httpd_uri_t vue_page_uri = { .uri = "/vue", .method = HTTP_GET, .handler = modern_index_handler, .user_ctx = NULL };
    httpd_uri_t scan_uri = { .uri = "/scan", .method = HTTP_GET, .handler = wifi_scan_handler, .user_ctx = NULL };
    httpd_uri_t submit_uri = { .uri = "/submit", .method = HTTP_POST, .handler = wifi_submit_handler, .user_ctx = NULL };
    httpd_uri_t connect_uri = { .uri = "/connect", .method = HTTP_POST, .handler = wifi_submit_handler, .user_ctx = NULL };
    httpd_uri_t status_wifi_uri = { .uri = "/connect/status", .method = HTTP_GET, .handler = wifi_status_handler, .user_ctx = NULL };
    httpd_uri_t saved_wifi_uri = { .uri = "/api/wifi/saved", .method = HTTP_GET, .handler = wifi_saved_handler, .user_ctx = NULL };
    httpd_uri_t prepare_wifi_uri = { .uri = "/api/wifi/prepare", .method = HTTP_POST, .handler = wifi_prepare_handler, .user_ctx = NULL };
    httpd_uri_t reset_wifi_uri = { .uri = "/api/wifi/reset", .method = HTTP_POST, .handler = wifi_reset_handler, .user_ctx = NULL };
    httpd_uri_t reset_wifi_get_uri = { .uri = "/api/wifi/reset", .method = HTTP_GET, .handler = wifi_reset_handler, .user_ctx = NULL };

    httpd_uri_t index_html_uri = { .uri = "/index.html", .method = HTTP_GET, .handler = smart_root_handler, .user_ctx = NULL };
    httpd_uri_t loading_jpg_uri = { .uri = "/loading.jpg", .method = HTTP_GET, .handler = loading_jpg_handler, .user_ctx = NULL };
    httpd_uri_t assets_js_uri = { .uri = "/assets/index.js", .method = HTTP_GET, .handler = index_js_handler, .user_ctx = NULL };
    httpd_uri_t assets_css_uri = { .uri = "/assets/index.css", .method = HTTP_GET, .handler = index_css_handler, .user_ctx = NULL };

    /* Modern REST APIs (Two-level resolution & settings) */
    httpd_uri_t get_info_uri = { .uri = "/api/get_camera_info", .method = HTTP_GET, .handler = get_camera_info_handler, .user_ctx = NULL };
    httpd_uri_t capture_img_uri = { .uri = "/api/capture_image", .method = HTTP_GET, .handler = capture_image_handler, .user_ctx = NULL };
    httpd_uri_t frame_jpg_uri = { .uri = "/api/frame.jpg", .method = HTTP_GET, .handler = capture_image_handler, .user_ctx = NULL };
    httpd_uri_t capture_bin_uri = { .uri = "/api/capture_binary", .method = HTTP_GET, .handler = capture_binary_handler, .user_ctx = NULL };
    httpd_uri_t set_cfg_uri = { .uri = "/api/set_camera_config", .method = HTTP_POST, .handler = camera_settings_handler, .user_ctx = NULL };
    httpd_uri_t set_res_uri = { .uri = "/api/resolution", .method = HTTP_POST, .handler = camera_settings_handler, .user_ctx = NULL };
    httpd_uri_t detections_uri = { .uri = "/api/detections", .method = HTTP_GET, .handler = default_detections_handler, .user_ctx = NULL };

    if (httpd_start(&camera_httpd, &myconfig) == ESP_OK)

    {

        httpd_register_uri_handler(camera_httpd, &index_uri);
        httpd_register_uri_handler(camera_httpd, &index_html_uri);
        httpd_register_uri_handler(camera_httpd, &wifi_page_uri);
        httpd_register_uri_handler(camera_httpd, &cam_page_uri);
        httpd_register_uri_handler(camera_httpd, &vue_page_uri);
        httpd_register_uri_handler(camera_httpd, &scan_uri);
        httpd_register_uri_handler(camera_httpd, &submit_uri);
        httpd_register_uri_handler(camera_httpd, &connect_uri);
        httpd_register_uri_handler(camera_httpd, &status_wifi_uri);
        httpd_register_uri_handler(camera_httpd, &saved_wifi_uri);
        httpd_register_uri_handler(camera_httpd, &prepare_wifi_uri);
        httpd_register_uri_handler(camera_httpd, &reset_wifi_uri);
        httpd_register_uri_handler(camera_httpd, &reset_wifi_get_uri);

        httpd_register_uri_handler(camera_httpd, &loading_jpg_uri);
        httpd_register_uri_handler(camera_httpd, &assets_js_uri);
        httpd_register_uri_handler(camera_httpd, &assets_css_uri);
        httpd_register_uri_handler(camera_httpd, &get_info_uri);
        httpd_register_uri_handler(camera_httpd, &capture_img_uri);
        httpd_register_uri_handler(camera_httpd, &frame_jpg_uri);
        httpd_register_uri_handler(camera_httpd, &capture_bin_uri);
        httpd_register_uri_handler(camera_httpd, &set_cfg_uri);
        httpd_register_uri_handler(camera_httpd, &set_res_uri);
        httpd_register_uri_handler(camera_httpd, &detections_uri);


        httpd_register_uri_handler(camera_httpd, &cmd_uri);

        httpd_register_uri_handler(camera_httpd, &status_uri);

        httpd_register_uri_handler(camera_httpd, &capture_uri);



        httpd_register_uri_handler(camera_httpd, &xclk_uri);

        httpd_register_uri_handler(camera_httpd, &reg_uri);

        httpd_register_uri_handler(camera_httpd, &greg_uri);

        httpd_register_uri_handler(camera_httpd, &pll_uri);

        httpd_register_uri_handler(camera_httpd, &win_uri);



        httpd_register_uri_handler(camera_httpd, &monitor_uri);

        httpd_register_uri_handler(camera_httpd, &stream_redirect_uri);
        httpd_register_uri_handler(camera_httpd, &favicon_uri);

    }



    httpd_config_t config_stream = HTTPD_DEFAULT_CONFIG();

    config_stream.server_port = 81;

    config_stream.ctrl_port = 32769;

    config_stream.max_open_sockets = 4;

    config_stream.stack_size = 10240;

    config_stream.core_id = 0;

    config_stream.task_priority = 6;

    config_stream.lru_purge_enable = true;

    ESP_LOGI(TAG, "Starting stream server on port: '%d'", config_stream.server_port);

    if (httpd_start(&stream_httpd, &config_stream) == ESP_OK)

    {

        httpd_register_uri_handler(stream_httpd, &stream_uri);

    }

}




