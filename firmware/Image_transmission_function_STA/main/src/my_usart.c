#include <stdio.h>
#include <stdbool.h>
#include "my_usart.h"

// Send a byte sequence over UART0 (falls back to stdout when the driver is not installed)
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

// Send one byte over UART0
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

// Shared color-registration state kept for key/I2C compatibility
bool register_mode = false;
