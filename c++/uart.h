#ifndef __UART_H__
#define __UART_H__

#include <cstdint>

extern int g_uart_fd;
extern int g_uart_nbytes;
extern const uint32_t g_uart_buff_sz;
extern uint8_t g_uart_buff[1024];

int uart_init();
int uart_uninit();
int uart_read();

#endif /* __UART_H__ */