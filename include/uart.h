#ifndef __UART_H__
#define __UART_H__

#include <stdint.h>

extern int g_uart_fd;
extern int g_uart_nbytes;
extern const uint32_t g_uart_buff_sz;
extern uint8_t g_uart_buff[1024];

int uart_init(void);
int uart_uninit(void);
int uart_read(void);

#endif /* __UART_H__ */
