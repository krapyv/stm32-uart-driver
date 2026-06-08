#ifndef UART_H
#define UART_H

#include <stdint.h>
#include "stm32f411.h"
#include "core_cm4.h"
#include "ring_buffer.h"

#ifndef USART2_CLK_MANAGED_EXTERNALLY
#define USART2_ENABLE_CLK()         \
    do                              \
    {                               \
        RCC->AHB1ENR |= 1U;         \
        RCC->APB1ENR |= (1U << 17); \
    } while (0)
#else
#define USART2_ENABLE_CLK() \
    do                      \
    {                       \
    } while (0) /* clocks managed by system_clocks_init() */
#endif

void usart2_init();
void usart2_write_char(char c);
void usart2_write_string(char *string, int len);

#endif
