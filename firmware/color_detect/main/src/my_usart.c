#include "stdio.h"
#include "my_usart.h"
#include "color_adjust_protocol.hpp"
#include "string.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "driver/gpio.h"

static const char *UARTTAG = "UART";

extern uint16_t wifi_Mode;    // Wi-Fi mode: 0=AP, 1=STA, 2=AP+STA
extern uint8_t sta_ip_connect[4]; // STA IP address bytes

static QueueHandle_t xQueuevirtualKeystate = NULL; // Virtual-key state queue
static myrecognizer_state_t recognizer_state = myDETECT;
static mycolor_detection_state_t mydetector_state = myCOLOR_DETECTION_IDLE;

const int uart_buffer_size = (1024 * 16);

// UART receive and transmit task
static void My_Uart_Task(void *arg)
{
    uint16_t temp_len = 512;
    uint8_t* temp_data = (uint8_t*) malloc(temp_len);

    while (1) 
    {
        const int rxBytes = uart_read_bytes(UART_MUN, temp_data, temp_len, pdMS_TO_TICKS(10) > 0 ? pdMS_TO_TICKS(10) : 1);
        if (rxBytes > 0)
        {
           Deal_uart_massage(temp_data, rxBytes);
        }

        vTaskDelay(1);
    }
    free(temp_data);
    vTaskDelete(NULL);
}

// Send a byte sequence over UART
int Uart_Send_Data(uint8_t* data, uint16_t len)
{
    int txBytes = 0;
    if (uart_is_driver_installed(UART_MUN)) {
        txBytes = uart_write_bytes(UART_MUN, data, len);
    } else {
        fwrite(data, 1, len, stdout);
        fflush(stdout);
        txBytes = len;
    }
    return txBytes;
}

// Send one byte over UART
int Uart_Send_Byte(uint8_t data)
{
    if (!uart_is_driver_installed(UART_MUN)) {
        putchar(data);
        fflush(stdout);
        return 1;
    }
    uint8_t data1 = data;
    int txBytes = uart_write_bytes(UART_MUN, &data1, 1);
    if (txBytes < 0) {
        putchar(data);
        fflush(stdout);
        txBytes = 1;
    }
    return txBytes;
}

void My_Uart_Init(const QueueHandle_t key_state_i)
{
    xQueuevirtualKeystate = key_state_i; 

    const uart_config_t uart_config = {
        .baud_rate = 921600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_driver_install(UART_MUN, uart_buffer_size, uart_buffer_size, 0, NULL, 0);
    uart_param_config(UART_MUN, &uart_config);
    uart_set_pin(UART_MUN, UART_GPIO_TXD, UART_GPIO_RXD, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    xTaskCreate(My_Uart_Task, "Uart_my_Task", 4 * 1024, NULL, 5, NULL);
}

static uint8_t SetRECV[40] = "\0";

void Deal_uart_massage(uint8_t *buff, uint16_t len)
{
    if (color_adjust_process_rx(buff, len)) {
        return;
    }

    if ((strncmp("sta_ip", (char*)buff, 6) == 0) || (strncmp("STA_IP", (char*)buff, 6) == 0))
    {
        if (sta_ip_connect[0] == 0)
        {
            sprintf((char*)SetRECV, "sta_ip:null\r\n");
            Uart_Send_Data(SetRECV, strlen((char*)SetRECV));
        }
        else
        {
            sprintf((char*)SetRECV, "sta_ip:%d.%d.%d.%d\r\n", sta_ip_connect[0], sta_ip_connect[1], sta_ip_connect[2], sta_ip_connect[3]);
            Uart_Send_Data(SetRECV, strlen((char*)SetRECV));
        }   
    }
    else if ((strncmp("ap_ip", (char*)buff, 5) == 0) || (strncmp("AP_IP", (char*)buff, 5) == 0))
    {
        if (wifi_Mode == 1)
        {
            sprintf((char*)SetRECV, "ap_ip:null\r\n");
            Uart_Send_Data(SetRECV, strlen((char*)SetRECV));
        }
        else
        {
            sprintf((char*)SetRECV, "ap_ip:192.168.4.1\r\n");
            Uart_Send_Data(SetRECV, strlen((char*)SetRECV));
        }   
    }
    else
    {
        Get_Key_massage(buff, len);
    }
    memset(SetRECV, 0, sizeof(SetRECV));
}

bool register_mode = false;

void Get_Key_massage(uint8_t *buff, uint16_t len)
{
    if ((strncmp("key_menu", (char*)buff, 8) == 0) || (strncmp("KEY_MENU", (char*)buff, 8) == 0))
    {
        mydetector_state = register_mode ? myCLOSE_REGISTER_COLOR_BOX : myOPEN_REGISTER_COLOR_BOX;
        register_mode = !register_mode;
        xQueueSend(xQueuevirtualKeystate, &mydetector_state, portMAX_DELAY);
        sprintf((char*)SetRECV, "KEY_MENU OK\r\n");
        Uart_Send_Data(SetRECV, strlen((char*)SetRECV));
    }
    else if ((strncmp("key_play", (char*)buff, 8) == 0) || (strncmp("KEY_PLAY", (char*)buff, 8) == 0))
    {
        mydetector_state = register_mode ? myREGISTER_COLOR : myDELETE_COLOR;
        register_mode = false;
        xQueueSend(xQueuevirtualKeystate, &mydetector_state, portMAX_DELAY);
        sprintf((char*)SetRECV, "KEY_PLAY OK\r\n");
        Uart_Send_Data(SetRECV, strlen((char*)SetRECV));
    }
    else if ((strncmp("key_upup", (char*)buff, 8) == 0) || (strncmp("KEY_UPUP", (char*)buff, 8) == 0))
    {
        mydetector_state = myINCREASE_COLOR_AREA;
        xQueueSend(xQueuevirtualKeystate, &mydetector_state, portMAX_DELAY);
        sprintf((char*)SetRECV, "KEY_UPUP OK\r\n");
        Uart_Send_Data(SetRECV, strlen((char*)SetRECV));
    }
    else if ((strncmp("key_down", (char*)buff, 8) == 0) || (strncmp("KEY_DOWN", (char*)buff, 8) == 0))
    {
        mydetector_state = myDECREASE_COLOR_AREA;
        xQueueSend(xQueuevirtualKeystate, &mydetector_state, portMAX_DELAY);
        sprintf((char*)SetRECV, "KEY_DOWN OK\r\n");
        Uart_Send_Data(SetRECV, strlen((char*)SetRECV));
    }
    else
    {
        sprintf((char*)SetRECV, "fail,command no found!\r\n");
        Uart_Send_Data(SetRECV, strlen((char*)SetRECV));
    }
}
