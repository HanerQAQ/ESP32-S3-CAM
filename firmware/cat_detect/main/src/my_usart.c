#include "stdio.h"
#include "my_usart.h"
#include "string.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "driver/gpio.h"

static const char *UARTTAG = "UART";

extern uint16_t wifi_Mode;    // Wi-Fi mode: 0=AP, 1=STA, 2=AP+STA / Wi-Fi 模式：0=AP，1=STA，2=AP+STA
extern uint8_t sta_ip_connect[4]; // STA IP address bytes / STA 模式下的 IP 地址字节


static QueueHandle_t xQueuevirtualKeystate = NULL; // Virtual-key state queue / 虚拟按键状态队列
static myrecognizer_state_t recognizer_state = myDETECT;// Face-recognition state; detection is the default / 人脸识别状态，默认执行检测

static mycolor_detection_state_t mydetector_state = myCOLOR_DETECTION_IDLE;// Color-detection state / 颜色识别状态


const int uart_buffer_size = (1024 * 2);


// UART receive and transmit task / 串口接收和发送任务
static void My_Uart_Task(void *arg)
{
    //ESP_LOGI(UARTTAG, "Start Uart0_Rx_Task with core:%d", xPortGetCoreID());
    uint16_t temp_len = 255;
    uint8_t* temp_data = (uint8_t*) malloc(temp_len);
    

    while (1) 
    {
        // Read data from UART / 从 UART 读取数据 
        const int rxBytes = uart_read_bytes(UART_MUN, temp_data, temp_len, 1);  // Timeout is specified in RTOS ticks / 超时时间以 RTOS tick 为单位
        if (rxBytes > 0)
        {
           //Uart_Send_Data(temp_data,rxBytes);// Echo received data directly / 直接回显接收数据)
           Deal_uart_massage(temp_data,rxBytes);
           uart_flush(UART_MUN);// Clear the UART receive buffer / 清空串口接收缓冲区
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
    free(temp_data);
    vTaskDelete(NULL);
}


// Send a byte sequence over UART / 通过串口发送一串数据
// Send a byte sequence over UART / 通过串口发送一串数据
int Uart_Send_Data(uint8_t* data, uint16_t len)
{
    if (!uart_is_driver_installed(UART_MUN)) {
        fwrite(data, 1, len, stdout);
        fflush(stdout);
        return len;
    }
    int txBytes = uart_write_bytes(UART_MUN, data, len);
    if (txBytes < 0) {
        fwrite(data, 1, len, stdout);
        fflush(stdout);
        txBytes = len;
    }
    return txBytes;
}

// Send one byte over UART / 通过串口发送一个字节
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
    // Add the virtual-key queue / 引入虚拟按键队列
     xQueuevirtualKeystate = key_state_i; 


    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        //.source_clk = UART_SCLK_DEFAULT,
    };
    uart_driver_install(UART_MUN, uart_buffer_size, uart_buffer_size, 0,NULL, 0);
    uart_param_config(UART_MUN, &uart_config);
    uart_set_pin(UART_MUN, UART_GPIO_TXD, UART_GPIO_RXD, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    
    //printf("uart_init_ok\r\n");
    xTaskCreate(My_Uart_Task, "Uart_my_Task", 5*1024, NULL, 5, NULL);
}



static uint8_t SetRECV[40]="\0";
void Deal_uart_massage(uint8_t *buff,uint16_t len)// Process data received over UART / 处理串口接收的数据
{
    uint8_t data[len];
    uint8_t  length = 0;


    // Print the IP address / 打印 IP 地址
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
    

    // Print the IP address / 打印 IP 地址
    else if ((strncmp("ap_ip",(char*)buff,5)==0) ||(strncmp("AP_IP",(char*)buff,5)==0))
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
    else // Virtual-key state queue / 虚拟按键状态队列选择
    {
        Get_Key_massage(buff, len);
    }


    memset(SetRECV,0,sizeof(SetRECV));// Clear the receive buffer / 清空接收缓冲区

}

bool register_mode = false;// Color-registration state for backward compatibility / 颜色注册状态（兼容共享变量）

void Get_Key_massage(uint8_t *buff, uint16_t len)
{
    sprintf((char*)SetRECV, "fail,command no found!\r\n");
    Uart_Send_Data(SetRECV, strlen((char*)SetRECV));
}

