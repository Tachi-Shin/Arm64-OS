#include <stdarg.h>
#include <stdint.h>
#define TRUE  1
#define FALSE 0

#define UART_BASE 0xFE215040U
#define UART_TX   (UART_BASE + 0U*4U)
#define UART_LSR  (UART_BASE + 5U*4U)
#define THR_EMPTY  0x20U

void uart_putchar(char c){
    volatile uint32_t * const uart = (uint32_t *)UART_TX;
    volatile uint32_t * const status = (uint32_t *)UART_LSR;
    while ( !(*status & THR_EMPTY) ) ;
    *uart = c;
}

void print_message(const char* s, ...){
    va_list ap;
    va_start (ap, s);

    while (*s) {
        if (*s == '%' && *(s+1) == 'x') {
            unsigned long v = va_arg(ap, unsigned long);
            _Bool print_started = FALSE;
            int i;

            s += 2;
            for (i = 15; i >= 0; i--) {
                unsigned long x = (v & 0xFUL << i*4) >> i*4;
                if (print_started || x != 0U || i == 0) {
                    print_started = TRUE;
                    uart_putchar(x < 10 ? ('0' + x) : ('a' + x - 10));
                }
            }
        } else {
            if (*s == '\n') {
                uart_putchar('\r');
            }
            uart_putchar(*s++);
        }
    }

    va_end (ap);
}

void main(void){
    while(1){
        print_message("Hello World!\n");
    }
}