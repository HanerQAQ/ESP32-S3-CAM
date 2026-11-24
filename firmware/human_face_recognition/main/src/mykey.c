#include "mykey.h"
#include "my_usart.h"
#include "stdio.h"
#include "stdint.h"
#include "string.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "driver/gpio.h"
#include "my_user_iic.h"

#define u8 uint8_t
#define u16 uint16_t




const static char *TAG = "KEY";


uint16_t AI_Mode = 0; 
volatile u8 g_shote_flag = 0;// 0 when released; set to 1 on a short press / 未按下为 0，短按时置 1
volatile u8 g_long_flag = 0;// 0 when released; set to 1 on a long press / 未按下为 0，长按时置 1
volatile u8 g_double_flag = 0;// 0 when released; set to 1 on a double press / 未按下为 0，双击时置 1

static QueueHandle_t xQueuevirtualKeystate = NULL; // Virtual-key state queue / 虚拟按键状态队列
static myrecognizer_state_t recognizer_state = myDETECT;// Face-recognition state; detection is the default / 人脸识别状态，默认执行检测
static mycolor_detection_state_t mydetector_state = myCOLOR_DETECTION_IDLE;// Color-detection state / 颜色识别状态


static uint8_t key0_data_flag = 1; // Send once on a short press, decrement on a double press, and increment on a long press / 短按发送一次，双击减一，长按加一
static uint8_t key_state[100]={'\0'};// Key commands to send / 要发送的按键命令

static void Key_GPIO_Init(void)
{
    // Zero-initialize the configuration structure / 将配置结构体清零初始化
    gpio_config_t io_conf = {};
    // Disable the interrupt / 关闭中断
    io_conf.intr_type = GPIO_INTR_DISABLE;
    // Set the pin to input mode / 将引脚设为输入模式
    io_conf.mode = GPIO_MODE_INPUT;
    // Bit mask of the pins to configure / 要配置的引脚位掩码
    io_conf.pin_bit_mask = (1ULL<<KEY_GPIO_BOOT0);
    // Disable the pull-down resistor / 禁用下拉电阻
    io_conf.pull_down_en = 0;
    // Enable the pull-up resistor / 启用上拉电阻
    io_conf.pull_up_en = 1;
    // Apply the GPIO configuration / 应用 GPIO 配置
    gpio_config(&io_conf);
    
}


static void Key_Task(void *arg)
{
    //ESP_LOGD(TAG, "Start Key_Task with core:%d", xPortGetCoreID());
    while (1)
    {
        //printf("key i am coming\r\n");
        Key_Handle();
        Get_key_data();
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    vTaskDelete(NULL);
}


/**************************************************************************
Function: Long press detection
Input   : none
Output  : 0：No action；1：Long press for 2 seconds；
函数功能：长按检测
入口参数：无
返回  值：按键状态 0：无动作 1：长按2s
**************************************************************************/
uint8_t Long_Press(void)
{
  static uint16_t Long_Press_count,Long_Press;
  if(Long_Press==0&&KEY1==0) 
  {
    Long_Press_count++;   // Long-press flag is not set yet / 长按标志尚未置 1
  } 
  
  else   
  {
    Long_Press_count=0; 
  }                    
    
    if(Long_Press_count>150)		// Scan every 10 milliseconds / 每 10 毫秒扫描一次
    {
      Long_Press=1;	
      Long_Press_count=0;
      return 1;
     }				
    if(Long_Press==1)     // Set the long-press flag / 置位长按标志
    {
        Long_Press=0;
    }
    return 0;
}


/**************************************************************************
Function: Key scan
Input   : Double click the waiting time
Output  : 0：No action；1：click；2：Double click
函数功能：按键扫描
入口参数：双击等待时间
返回  值：按键状态 0：无动作 1：单击 2：双击 
**************************************************************************/
uint8_t click_N_Double (uint16_t time)
{
  static	uint8_t flag_key,count_key,double_key;	
  static	uint16_t count_single,Forever_count;
  if(KEY1==0)  Forever_count++;   // Long-press flag is not set yet / 长按标志尚未置 1
  else        Forever_count=0;
  if(0==KEY1&&0==flag_key)		flag_key=1;	// First key press / 第一次按下
  if(0==count_key)
  {
    if(flag_key==1) 
    {
        double_key++;
        count_key=1;			// Mark one key press / 标记按下一次
    }
    if(double_key==2) 
    {					// Two key presses detected / 检测到按下两次
        double_key=0;
        count_single=0;
        return 2;			// Command for a double press / 双击执行的命令
    }
  }
    if(1==KEY1)
    {
       flag_key=0,count_key=0;
    }

    if(1==double_key)
    {
      count_single++;
      if(count_single>time&&Forever_count<time)
      {
        double_key=0;
        count_single=0;	// Do not mark it as a double press after timeout / 超时后不标记为双击
        return 1;// Command for a single press / 单击执行的命令
      }
      if(Forever_count>time)
      {
        double_key=0;
        count_single=0;	
      }
    }	
    return 0;
}



// Call this function every 10 milliseconds / 每 10 毫秒调用一次
void Key_Handle(void)
{
    uint8_t tmp,tmp2;
    tmp=click_N_Double(50); 
    if(tmp==1)
    { 
        g_shote_flag = 1;// Short press / 短按 
            
    }
    if(tmp == 2)
    {
        g_double_flag = 1; // Double press / 双击
        
    }
    
    tmp2=Long_Press();                   
    if(tmp2==1) 
    {
        g_long_flag = 1; // Long press / 长按
        
    }


}


extern bool register_mode ;// Share the color-registration state with UART0 / 与 UART0 共用颜色注册状态

void  Key_data_upgrade(uint8_t keydata)
{
    
   if (keydata == 1 )// Example command: KEY_MENU / 命令示例：KEY_MENU
    {
        // // Face recognition / 人脸识别
        
            recognizer_state = myENROLL;// Enroll a face / 登记人脸
            xQueueSend(xQueuevirtualKeystate, &recognizer_state, portMAX_DELAY);
       
        //printf("KEY_MENU\r\n");
    }
    else if (keydata == 2)// Example command: KEY_PLAY / 命令示例：KEY_PLAY
    {
        // Face recognition / 人脸识别
       
            recognizer_state = myRECOGNIZE;// Recognize faces / 识别人脸
            xQueueSend(xQueuevirtualKeystate, &recognizer_state, portMAX_DELAY);
       
        //printf("keydata:2\r\n");
    }
    else if (keydata == 3)// Example command: KEY_UPUP / 命令示例：KEY_UPUP
    {
        // Face recognition / 人脸识别
       
            recognizer_state = myDETECT;// Detect faces / 检测人脸
            xQueueSend(xQueuevirtualKeystate, &recognizer_state, portMAX_DELAY);
        
        //printf("keydata:3\r\n");
    }
    else if (keydata == 4)// Example command: KEY_DOWN / 命令示例：KEY_DOWN
    {
        // Face recognition / 人脸识别
        
            recognizer_state = myDELETE;// Delete a registered face / 删除已注册人脸
            xQueueSend(xQueuevirtualKeystate, &recognizer_state, portMAX_DELAY);
       
        //printf("keydata:4\r\n");
    }
    

}


void Get_key_data(void)
{
    //printf("key data coming\r\n");
    if(g_shote_flag == 1)
    {
        g_shote_flag = 0;

        Key_data_upgrade(key0_data_flag);

        sprintf((char*)key_state,"key_down!! flag:%d\r\n",key0_data_flag);
        Uart_Send_Data(key_state,strlen((char*)key_state)); 

    }
    else if (g_double_flag == 1)
    {
        g_double_flag = 0;

        key0_data_flag -=1; // Decrement on a double press / 双击时减一
        if(key0_data_flag>4 || key0_data_flag ==0)
        {
            key0_data_flag = 4;
        } 

        sprintf((char*)key_state,"key_double!! \r\n");
        Uart_Send_Data(key_state,strlen((char*)key_state)); 

    }
    else if(g_long_flag == 1)
    {
        g_long_flag = 0;

        key0_data_flag = (key0_data_flag+1)%5; //1-4 // Increment on a long press / 长按时加一

        if(key0_data_flag == 0)
        {
            key0_data_flag = 1;
        }
        
        sprintf((char*)key_state,"key_long_down!! \r\n");
        Uart_Send_Data(key_state,strlen((char*)key_state)); 
    }
}



void Key_Init(const QueueHandle_t key_state_i)
{
    // Add the virtual-key queue / 引入虚拟按键队列
    xQueuevirtualKeystate = key_state_i; 

    Key_GPIO_Init();

    xTaskCreatePinnedToCore(Key_Task, "Key_Task", 2*1024, NULL, configMAX_PRIORITIES - 1, NULL, 1); // Run the task on Core 1 / 任务运行在 Core 1

    // Keep this task disabled because it causes errors in some modules / 此任务会导致部分模块异常，因此保持关闭
    //xTaskCreatePinnedToCore(Key_User_Task, "Key_User_Task", 2*1024, NULL, configMAX_PRIORITIES - 1, NULL, 1);// Run the task on Core 1 / 任务运行在 Core 1
    
}
