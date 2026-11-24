#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "sdkconfig.h"
#include "esp_err.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_chip_info.h"
#include "esp_system.h"
#include "esp_flash.h"

#define UART_MUN         UART_NUM_0
#define UART_GPIO_TXD   (GPIO_NUM_43)
#define UART_GPIO_RXD   (GPIO_NUM_44)

int Uart_Send_Data(uint8_t* data, uint16_t len);
int Uart_Send_Byte(uint8_t data);

typedef enum
{
    myIDLE = 0,
    myDETECT,
    myENROLL,
    myRECOGNIZE,
    myDELETE,
} myrecognizer_state_t;

typedef enum
{
    Nornal_AI = 0,
    Cat_Dog_AI,
    FACE_AI,
    COLOR_AI,
    REFACE_AI,
    QR_AI,
    AI_MAX
} myAI_mode_t;

typedef enum
{
    myCOLOR_DETECTION_IDLE = 0,
    myOPEN_REGISTER_COLOR_BOX,
    myCLOSE_REGISTER_COLOR_BOX,
    myREGISTER_COLOR,
    myDELETE_COLOR,
    myINCREASE_COLOR_AREA,
    myDECREASE_COLOR_AREA,
    mySWITCH_RESULT,
} mycolor_detection_state_t;

#ifdef __cplusplus
}
#endif
