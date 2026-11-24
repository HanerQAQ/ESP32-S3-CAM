#include "stdio.h"
#include "my_usart.h"
#include "my_usart1_user.h"
#include "string.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "driver/gpio.h"

static const char *UARTTAG = "UART1_USER";

const int uart1_buffer_size = (1024 * 2);

extern uint8_t sta_ip_connect[4]; // STA IP address bytes / STA 模式下的 IP 地址字节
extern uint16_t wifi_Mode;    // Wi-Fi mode: 0=AP, 1=STA, 2=AP+STA / Wi-Fi 模式：0=AP，1=STA，2=AP+STA

static QueueHandle_t xQueuevirtualKeystate = NULL;
static bool s_uart1_installed = false; // Virtual-key state queue / 虚拟按键状态队列
static myrecognizer_state_t recognizer_state = myDETECT;// Face-recognition state; detection is the default / 人脸识别状态，默认执行检测

// UART receive and transmit task for the PH2.0 connector / PH2.0 接口的串口收发任务
static void My_Uart1_user_Task(void *arg)
{
    //ESP_LOGI(TAG, "Start Uart0_Rx_Task with core:%d", xPortGetCoreID());
    uint16_t temp_len = 255;
    uint8_t* temp_data = (uint8_t*) malloc(temp_len);
    

    while (1) 
    {
        // Read data from UART / 从 UART 读取数据 
        const int rxBytes = uart_read_bytes(UART1_MUN, temp_data, temp_len, 1);  // Timeout is specified in RTOS ticks / 超时时间以 RTOS tick 为单位
        if (rxBytes > 0)
        {
           //Uart1_Send_Data(temp_data,rxBytes);// Echo received data directly / 直接回显接收数据)
           Deal_uart1_massage(temp_data,rxBytes);
           uart_flush(UART1_MUN);// Clear the UART receive buffer / 清空串口接收缓冲区
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
    free(temp_data);
    vTaskDelete(NULL);
}



// Send a byte sequence over UART / 通过串口发送一串数据
int Uart1_Send_Data(uint8_t* data, uint16_t len)
{
    if (!uart_is_driver_installed(UART1_MUN)) return 0;
    const int txBytes = uart_write_bytes(UART1_MUN, data, len);
    return txBytes;
}

// Send one byte over UART / 通过串口发送一个字节
int Uart1_Send_Byte(uint8_t data)
{
    if (!uart_is_driver_installed(UART1_MUN)) return 0;
    uint8_t data1 = data;
    const int txBytes = uart_write_bytes(UART1_MUN, &data1, 1);
    return txBytes;
}


// Initialize UART1 at 115200 baud / 初始化 UART1，波特率为 115200 
// A mode-selection key may be added later; keep related handling together / 后续可能增加模式选择按键，相关处理尽量放在一起
void My_Uart1_user_Init(const QueueHandle_t key_state_i)
{
    s_uart1_installed = true;
    xQueuevirtualKeystate = key_state_i;

#if CONFIG_SPIRAM_MODE_OCT
    // Protect Octal PSRAM bus (GPIO33-37) from UART1 pin collision
    ESP_LOGW(UARTTAG, "Octal PSRAM enabled: bypassing UART1 on GPIO35/36 to prevent memory bus corruption");
    s_uart1_installed = false;
    return;
#else
    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_driver_install(UART1_MUN, uart1_buffer_size, uart1_buffer_size, 0, NULL, 0);
    uart_param_config(UART1_MUN, &uart_config);
    uart_set_pin(UART1_MUN, UART1_GPIO_TXD, UART1_GPIO_RXD, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    xTaskCreate(My_Uart1_user_Task, "Uart1_my_Task", 4 * 1024, NULL, 5, NULL);
#endif
}



static uint8_t SetRECV[40]="\0";
void Deal_uart1_massage(uint8_t *buff,uint16_t len)// Process data received over UART / 处理串口接收的数据
{
    uint8_t data[len];
    uint8_t  length = 0;
    
    if ((strncmp("sta_ip",(char*)buff,6)==0) ||(strncmp("STA_IP",(char*)buff,6)==0))
    {
        if (sta_ip_connect[0] == 0)// No IP address is available / 当前没有 IP 地址
        {
            sprintf((char*)SetRECV,"sta_ip:null\r\n");
            Uart_Send_Data(SetRECV,strlen((char*)SetRECV));
        }
        else
        {
            sprintf((char*)SetRECV,"sta_ip:%d.%d.%d.%d\r\n",sta_ip_connect[0],sta_ip_connect[1],sta_ip_connect[2],sta_ip_connect[3]);
            Uart_Send_Data(SetRECV,strlen((char*)SetRECV));
        }   
    }


   else if  ((strncmp("ap_ip",(char*)buff,5)==0) ||(strncmp("AP_IP",(char*)buff,5)==0))
    {
        if (wifi_Mode == 1)// No IP is available in station-only mode / 仅启用 STA 模式且当前没有 IP 地址
        {
            sprintf((char*)SetRECV,"ap_ip:null\r\n");
            Uart_Send_Data(SetRECV,strlen((char*)SetRECV));
        }
        else
        {
            sprintf((char*)SetRECV,"ap_ip:192.168.4.1\r\n"); // Use the fixed access-point address / 使用固定的 AP 地址
            Uart_Send_Data(SetRECV,strlen((char*)SetRECV));
        }   
    }

    else // Virtual-key state queue / 虚拟按键状态队列閫夋嫨
    {
        uart1_Get_Key_massage(buff,len);
    }
    memset(SetRECV,0,sizeof(SetRECV));// Clear the receive buffer / 清空接收缓冲区

}

extern bool register_mode;// Color-registration state, synchronized with UART0 / 颜色注册状态，与 UART0 保持同步


void uart1_Get_Key_massage(uint8_t *buff,uint16_t len)
{
    uint8_t data[len];
    uint8_t  length = 0;

    if ((strncmp("key_menu",(char*)buff,8)==0) ||(strncmp("KEY_MENU",(char*)buff,8)==0))// Example command: KEY_MENU / 命令示例：KEY_MENU
    {
        // // Face recognition / 人脸识别
       
        recognizer_state = myENROLL;// Enroll a face / 登记人脸
        xQueueSend(xQueuevirtualKeystate, &recognizer_state, portMAX_DELAY);

        sprintf((char*)SetRECV,"KEY_MENU OK\r\n");
        Uart1_Send_Data(SetRECV,strlen((char*)SetRECV)); // Send the setting confirmation / 回传设置确认信息
    }
    else if ((strncmp("key_play",(char*)buff,8)==0) ||(strncmp("KEY_PLAY",(char*)buff,8)==0))// Example command: KEY_PLAY / 命令示例：KEY_PLAY
    {
        
        recognizer_state = myRECOGNIZE;// Recognize faces / 识别人脸
        xQueueSend(xQueuevirtualKeystate, &recognizer_state, portMAX_DELAY);
       
        sprintf((char*)SetRECV,"KEY_PLAY OK\r\n");
        Uart1_Send_Data(SetRECV,strlen((char*)SetRECV)); // Send the setting confirmation / 回传设置确认信息
    }
    else if ((strncmp("key_upup",(char*)buff,8)==0) ||(strncmp("KEY_UPUP",(char*)buff,8)==0))// Example command: KEY_UPUP / 命令示例：KEY_UPUP
    {
        
        recognizer_state = myDETECT;// Detect faces / 检测人脸
        xQueueSend(xQueuevirtualKeystate, &recognizer_state, portMAX_DELAY);
       
        sprintf((char*)SetRECV,"KEY_UPUP OK\r\n");
        Uart1_Send_Data(SetRECV,strlen((char*)SetRECV)); // Send the setting confirmation / 回传设置确认信息
    }
    else if ((strncmp("key_down",(char*)buff,8)==0) ||(strncmp("KEY_DOWN",(char*)buff,8)==0))// Example command: KEY_DOWN / 命令示例：KEY_DOWN
    {
        
        recognizer_state = myDELETE;// Delete a registered face / 删除已注册人脸
        xQueueSend(xQueuevirtualKeystate, &recognizer_state, portMAX_DELAY);
        sprintf((char*)SetRECV,"KEY_DOWN OK\r\n");
        Uart1_Send_Data(SetRECV,strlen((char*)SetRECV)); // Send the setting confirmation / 回传设置确认信息
    }

     else // The microcontroller must send this module only configuration-protocol messages / 单片机只能向该模块发送配置协议消息
    {
        sprintf((char*)SetRECV,"fail,command no found!\r\n");
        Uart1_Send_Data(SetRECV,strlen((char*)SetRECV)); // Send the response / 回传响应
    }


}

