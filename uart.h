#ifndef UART_H
#define UART_H

#include <stdint.h>
#include "stm32f411.h"
#include "core_cm4.h"
#include "ring_buffer.h"

void usart2_init();
void usart2_write_char(char c);
void usart2_write_string(char *string, int len);

#endif