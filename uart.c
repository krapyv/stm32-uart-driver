#include "uart.h"

#if ((TARGET_UART_MODE == UART_MODE_RX_ONLY) || (TARGET_UART_MODE == UART_MODE_TX_RX))
RingBuffer_t rx_buffer;
uint8_t raw_rx_storage[16];

void USART2_IRQHandler(void)
{
    uint32_t sr_snapshot = USART2->SR;

    // first of all, check whether the interrupt has been caused by the receiver
    // for this, check bit 5 RXNE of USART2_SR (if 1 - Received data is ready to be read, 0 - data is not received)
    if (sr_snapshot & (1 << 5))
    {
        // if the result > 0 (true), then the bit is 1
        uint8_t temp = (uint8_t)USART2->DR; // explicitly converting 32-bit sequence to 8 bit

        // check for FE (Frame Error) to prevent saving a corrupt (de-synchronized) data
        if (!(sr_snapshot & (1 << 1)))
        {
            ring_buffer_push(&rx_buffer, temp);
        }
    }

    // ORE and FE errors handler

    // it is good, but since the bitwise math is our main tool, we can rewrite this expression with it
    // if ((sr_snapshot & (1 << 3)) || (sr_snapshot & (1 << 1)))

    // we do not care which EXACTLY error bit (ORE or FE) has been set
    // in either case we are going to read SR (already done at the top) and DR
    if (sr_snapshot & ((1 << 3) | (1 << 1)))
    {
        // we are reading USART2_DR only when the first block of code has not been executed
        // but even without that if block and with unconditional reading of DR register, it would be completely fine (because DR has been clearned or has stale data)
        if (!(sr_snapshot & (1 << 5)))
        {
            volatile uint32_t clear = USART2->DR;
            (void)clear; // to prevent the compiler from warning about this unused var
        }
    }
}
#endif

void usart2_init()
{
#if ((TARGET_UART_MODE == UART_MODE_RX_ONLY) || (TARGET_UART_MODE == UART_MODE_TX_RX))

    // initialize the ring buffer
    (void)ring_buffer_init(&rx_buffer, raw_rx_storage, 16);
#endif
    USART2_ENABLE_CLK();

    // next, set the GPIOA_MODER to AF (10 or 0x2)
    // since we are using PA2 and PA3, we need to set bits 2-3 and 4-5

    USART2_INIT_GPIO();

    // now we need to initialize USART registers

    // according to the formula USARTDIV = clock frequency / 8 * (2 - OVER8) * Baud Rate

    USART2->BRR = TARGET_UART_BRR;

    // USART enabling
    uint32_t cr1_config = (1UL << 13U);

#if (TARGET_UART_MODE == UART_MODE_TX_ONLY)
    cr1_config |= (1UL << 3U); // TE
#elif (TARGET_UART_MODE == UART_MODE_RX_ONLY)
    cr1_config |= (1UL << 5U) | (1UL << 2U); // RXNEIE, RX
#elif (TARGET_UART_MODE == UART_MODE_TX_RX)
    cr1_config |= (1U << 5UL) | (1UL << 3U) | (1UL << 2U);
#endif
    // enabling USART, RXNEIE, TE, RE
    USART2->CR[0] = cr1_config;

#if (TARGET_UART_MODE == UART_MODE_RX_ONLY) || (TARGET_UART_MODE == UART_MODE_TX_RX)
    NVIC->ISER[1] = (1UL << 6U); // bit 6 is responsible for the interrupt position 38
#endif
}

void usart2_write_char(char c)
{
    while (!(USART2->SR & (1 << 7)))
    {
    }

    USART2->DR = c;
}

void usart2_write_string(char *string, int len)
{
    // if len is positive, send exactly 'len' characters (printf)
    // if len is negative (-1), fall back to standard null-terminator string tracking

    int i = 0;
    while ((len >= 0 && i < len) || (len < 0 && string[i] != '\0'))
    {
        if (string[i] == '\n')
        {
            usart2_write_char('\r');
        }
        usart2_write_char(string[i]);
        i++;
    }
}

int _write(int file, char *ptr, int len)
{
    usart2_write_string(ptr, len);
    return len;
}