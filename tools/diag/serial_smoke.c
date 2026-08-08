#include <conio.h>
#include <stdio.h>

#ifndef V9X_BUILD_ID
#define V9X_BUILD_ID "local"
#endif

#define V9X_COM1_BASE 0x03f8u
#define V9X_UART_THR  (V9X_COM1_BASE + 0u)
#define V9X_UART_DLL  (V9X_COM1_BASE + 0u)
#define V9X_UART_IER  (V9X_COM1_BASE + 1u)
#define V9X_UART_DLM  (V9X_COM1_BASE + 1u)
#define V9X_UART_FCR  (V9X_COM1_BASE + 2u)
#define V9X_UART_LCR  (V9X_COM1_BASE + 3u)
#define V9X_UART_MCR  (V9X_COM1_BASE + 4u)
#define V9X_UART_LSR  (V9X_COM1_BASE + 5u)

static void v9x_serial_initialize(void)
{
    outp(V9X_UART_IER, 0x00u);
    outp(V9X_UART_LCR, 0x80u);
    outp(V9X_UART_DLL, 0x0cu);
    outp(V9X_UART_DLM, 0x00u);
    outp(V9X_UART_LCR, 0x03u);
    outp(V9X_UART_FCR, 0x07u);
    outp(V9X_UART_MCR, 0x0bu);
}

static int v9x_serial_write_byte(unsigned char value)
{
    unsigned long remaining = 1000000ul;

    while (remaining != 0ul) {
        if ((inp(V9X_UART_LSR) & 0x20u) != 0u) {
            outp(V9X_UART_THR, value);
            return 1;
        }
        --remaining;
    }
    return 0;
}

static int v9x_serial_write(const char *text)
{
    while (*text != '\0') {
        if (!v9x_serial_write_byte((unsigned char)*text)) {
            return 0;
        }
        ++text;
    }
    return 1;
}

int main(void)
{
    v9x_serial_initialize();
    if (!v9x_serial_write("V9X-SERIAL-SMOKE v2 build=" V9X_BUILD_ID "\r\n")) {
        puts("COM1 UART transmit register did not become ready.");
        return 1;
    }

    puts("Velocity9x COM1 smoke line sent at 9600 8N1.");
    return 0;
}
