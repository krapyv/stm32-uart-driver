#ifndef UART_H
#define UART_H

#include <stdint.h>
#include "stm32f411.h"
#include "core_cm4.h"
#include "ring_buffer.h"

#ifdef USE_APP_CONFIG
#include "app_config.h"
#endif

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

#ifndef USART2_GPIO_MANAGED_EXTERNALLY
#define USART2_INIT_GPIO()                            \
    do                                                \
    {                                                 \
        GPIOA->MODER &= ~((0x3U << 6) | (0x3U << 4)); \
        GPIOA->MODER |= (0x2U << 6) | (0x2U << 4);    \
        GPIOA->AFRL &= ~((0xFU << 12) | (0xFU << 8)); \
        GPIOA->AFRL |= (0x7U << 12) | (0x7U << 8);    \
    } while (0)
#else
#define USART2_INIT_GPIO() \
    do                     \
    {                      \
    } while (0)
#endif
void usart2_init();
void usart2_write_char(char c);
void usart2_write_string(char *string, int len);

#endif
