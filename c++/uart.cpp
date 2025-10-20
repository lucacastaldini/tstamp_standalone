#include <cstdio>
#include <fcntl.h>
#include <termios.h> 
#include <unistd.h>
#include <errno.h>

#include "uart.h"
#include <cstring> 
#include <sys/select.h> // Include for select

int g_uart_fd = -1;
int g_uart_nbytes = -1;
const uint32_t g_uart_buff_sz = 1024;
uint8_t g_uart_buff[1024];

int uart_init() {

    g_uart_fd = open("/dev/ttyPS1", O_RDWR | O_NOCTTY | O_NDELAY);

    if(g_uart_fd < 0){
        fprintf(stderr, "Failed to open uart.\n");
        return -1;
    }

    struct termios settings;
    tcgetattr(g_uart_fd, &settings);

    /*  CONFIGURE THE UART
    *  The flags (defined in /usr/include/termios.h - see http://pubs.opengroup.org/onlinepubs/007908799/xsh/termios.h.html):
    *       Baud rate:- B1200, B2400, B4800, B9600, B19200, B38400, B57600, B115200, B230400, B460800, B500000, B576000, B921600, B1000000, B1152000, B1500000, B2000000, B2500000, B3000000, B3500000, B4000000
    *       CSIZE:- CS5, CS6, CS7, CS8
    *       CLOCAL - Ignore modem status lines
    *       CREAD - Enable receiver
    *       IGNPAR = Ignore characters with parity errors
    *       ICRNL - Map CR to NL on input (Use for ASCII comms where you want to auto correct end of line characters - don't use for bianry comms!)
    *       PARENB - Parity enable
    *       PARODD - Odd parity (else even) */

    /* Set baud rate - default set to 9600Hz */
    speed_t baud_rate = B9600;

    /* Baud rate fuctions
    * cfsetospeed - Set output speed
    * cfsetispeed - Set input speed
    * cfsetspeed  - Set both output and input speed */

    cfsetspeed(&settings, baud_rate);

    settings.c_cflag &= ~PARENB; /* no parity */
    settings.c_cflag &= ~CSTOPB; /* 1 stop bit */
    settings.c_cflag &= ~CSIZE;
    settings.c_cflag |= CS8 | CLOCAL; /* 8 bits */
    settings.c_lflag = ICANON; /* canonical mode */
    settings.c_oflag &= ~OPOST; /* raw output */

    /* Setting attributes */
    tcflush(g_uart_fd, TCIFLUSH);
    tcsetattr(g_uart_fd, TCSANOW, &settings);
    
    // Enable buffering
    fcntl(g_uart_fd, F_SETFL, 0);

    return 0;
    
}

int uart_uninit() {

	if (g_uart_fd < 0) {
		return -1;
	}

    tcflush(g_uart_fd, TCIFLUSH);
    close(g_uart_fd);
    
    g_uart_fd = -1;
    g_uart_nbytes = -1;
    
    return 0;
}

int uart_read() {
    if (g_uart_fd < 0) {
        return -1;
    }

    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(g_uart_fd, &read_fds);

    struct timeval timeout;
    timeout.tv_sec = 5;  // Timeout di 1 secondo
    timeout.tv_usec = 0;

    int ret = select(g_uart_fd + 1, &read_fds, NULL, NULL, &timeout);

    if (ret > 0) {
        if (FD_ISSET(g_uart_fd, &read_fds)) {
            g_uart_nbytes = ::read(g_uart_fd, (void*)g_uart_buff, g_uart_buff_sz);

            if (g_uart_nbytes < 0) {
                // An actual error occurred
                fprintf(stderr, "UART read error: %s\n", strerror(errno));
                return -1;
            }
            return g_uart_nbytes;
        }
    } else if (ret == 0) {
        // Timeout, senza dati
        return -1;
    } else {
        // Errore nella chiamata select
        fprintf(stderr, "Select error: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}