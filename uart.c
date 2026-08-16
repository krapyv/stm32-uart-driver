#include "uart.h"
#include <stdio.h>
#include <stdint.h>

uint8_t raw_tx_buffer[64];

void DMA1_Stream6_IRQHandler(void)
{
    if (DMA1->HISR & (1U << 21U))
    {
        // since all the interruot clearing bits in the HIFCR/LIFCR are write-1-to-clear and the reserved bits has their reset values at 0, we can use direct write instead of RMW
        DMA1->HIFCR = (1U << 21U);
    }
}

#if ((TARGET_UART_MODE == UART_MODE_RX_ONLY) || (TARGET_UART_MODE == UART_MODE_TX_RX))
RingBuffer_t rx_buffer;
uint8_t raw_rx_storage[16];

void USART2_IRQHandler(void)
{
    uint32_t sr_snapshot = USART2->SR;

    // first of all, check whether the interrupt has been caused by the receiver
    // for this, check bit 5 RXNE of USART2_SR (if 1 - Received data is ready to be read, 0 - data is not received)
    if (sr_snapshot & (1UL << 5U))
    {
        // if the result > 0 (true), then the bit is 1
        uint8_t temp = (uint8_t)USART2->DR; // explicitly converting 32-bit sequence to 8 bit

        // check for FE (Frame Error) to prevent saving a corrupt (de-synchronized) data
        if (!(sr_snapshot & (1UL << 1U)))
        {
            ring_buffer_push(&rx_buffer, temp);
        }
    }

    // ORE and FE errors handler

    // it is good, but since the bitwise math is our main tool, we can rewrite this expression with it
    // if ((sr_snapshot & (1 << 3)) || (sr_snapshot & (1 << 1)))

    // we do not care which EXACTLY error bit (ORE or FE) has been set
    // in either case we are going to read SR (already done at the top) and DR
    if (sr_snapshot & ((1UL << 3U) | (1UL << 1U)))
    {
        // we are reading USART2_DR only when the first block of code has not been executed
        // but even without that if block and with unconditional reading of DR register, it would be completely fine (because DR has been clearned or has stale data)
        if (!(sr_snapshot & (1UL << 5U)))
        {
            volatile uint32_t clear = USART2->DR;
            (void)clear; // to prevent the compiler from warning about this unused var
        }
    }
}
#endif

void usart2_dma_tx_init(void)
{
    // enable the DMA1 clock
    RCC->AHB1ENR |= (1U << 21U);

    /* ----- DMA configuration ----- */

    // USART_TX is the stream 6 channel 4 of DMA1

    // CHSEL for channel 4 is 100
    // 100 = 2^2 = 4 = 0x4
    DMA1->S6CR |= (0x4 << 25);

    // MSIZE bits 14:13 = PSIZE bits 12:11 = byte (8-bit) = 00
    // for clearing: 11 11 = 2^3 + 2^2 + 2^1 + 2^0 = 8 + 4 + 2 + 1 = 15 = 0xF
    DMA1->S6CR &= ~(0xF << 11);

    // MINC bit 10
    // 1 - memory adress pointer is incremented after each data transfer
    DMA1->S6CR |= (1U << 10U);

    // PINC bit 9
    // 0 - peripheral address pointer is fixed
    DMA1->S6CR &= ~(1U << 9U);

    // DIR[1:0] bits 7:6
    // memory-to-peripheral - 01

    // for clearing: 11 = 0x3
    DMA1->S6CR &= ~(0x3 << 6);

    // for setting: 01 = 0x1
    DMA1->S6CR |= (0x1 << 6);

    // Peripheral address register
    DMA1->S6PAR = (uintptr_t)&USART2->DR;

    // Memory 0 address register
    DMA1->S6M0AR = (uintptr_t)raw_tx_buffer;

    // 64 bytes = 0x40
    DMA1->S6NDTR = 0x40;

    // clear stale TC flag
    // CTCIF6 = bit 21
    DMA1->HIFCR = (1U << 21U);

    // TCIE bit 4
    // 1 - TC interrupt enabled
    DMA1->S6CR |= (1 << 4);

    // USART2 DMA transmitter enable
    // CR[3] -> CR[0] is CR1, CR[1] is CR2 and CR[2] is CR3
    USART2->CR[2] |= (1U << 7U);

    // DMA1_Stream6 has the position of 17 in the vector table

    // enable NVIC for DMA1_Stream6_IRQn
    // uint32_t ISER[8] = 256 interrupts
    // position 17 is ISER[0] bit 17

    NVIC->ISER[0] = (1 << 17); // with NVIC registers we are using direct write, not RMW

    // stream enable
    DMA1->S6CR |= (1U << 0U);

    /* ----- DMA configuration ----- */
}

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

    // wait for the idle frame (preamble) to be fully set
    while (!(USART2->SR & (1 << 6)))
    {
    }

#if (TARGET_UART_MODE == UART_MODE_RX_ONLY) || (TARGET_UART_MODE == UART_MODE_TX_RX)
    NVIC->ISER[1] = (1UL << 6U); // bit 6 is responsible for the interrupt position 38
#endif
}

void usart2_write_char(char c)
{
    while (!(USART2->SR & (1UL << 7U)))
    {
    }

    USART2->DR = c;
}

void usart2_write_string(char *string, int len)
{
    // if len is positive, send exactly 'len' characters (printf)
    // if len is negative (-1), fall back to standard null-terminator string tracking

    int i = 0;
    while ((len >= 0U && i < len) || (len < 0U && string[i] != '\0'))
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