# USART2 Driver

This driver handles USART2 peripheral functionality: it supports interrupt-driven RX, DMA-handled TX, incorporation of the custom ring buffer data structure.
Compile-time macros in app_config.h select which branches are compiled in - RX-only, TX-only, or RX + TX - avoiding unused interrupt handlers and DMA configuration being linked into the library.
Built on top of the custom ring buffer library for interrupt-safe byte queuing between the USART2 ISR and the main loop.

## Project Structure

```text
├── app_config.h    # Compile-time macros selecting RX-only, TX-only, or RX+TX build
└── reusable_drivers/
    ├── core/   # ARM Cortex-M4 and STM32F411 register definitions
    │   ├── core_cm4.h      # Register layout definitions for NVIC and SysTick architectures
    │   └── stm32f411.h     # Memory boundaries and register definitions for AHB/APB peripherals
    ├── periph/     # Portable peripheral drivers
    │   ├── uart.c      # USART2 register configuration and DMA1 Stream 6 transfer invocation
    │   └── uart.h      # USART2 bit definitions, control macros, and function declarations
    └── utils/     # Additional utility drivers
        ├── ring_buffer.c      # Ring buffer initialization and control functions
        └── ring_buffer.h      # Ring buffer control struct, and function declarations

```

## Public API

### usart2_init
Initializes the ring buffer, GPIO pins, USART registers and RX interrupts.

Takes zero parameters. Depends on the compile-time macros defined in app_config.h.

### usart2_dma_tx_init
Configures DMA and enables DMA1 Stream6 interrupts.

Takes zero parameters. 

### usart2_write_char
Writes the passed character to the USART2 DR.

Parameters:

- c     A data character that is about to be written

Blocks until TXE (bit 7 of SR) is set before writing to DR.
Returns after the byte is written to DR. The byte may still be in the shift register and not yet fully transmitted on the wire.

### usart2_write_string
Writes the string to the USART2 DR using the usart2_write_char.

Parameters:
- string    Pointer to the string (an array of characters)
- len       Length of the passed string

Supports length defined strings: if len is positive, send exactly 'len' characters.
If len is negative (-1), fall back to standard null-terminator string tracking.

Returns after all bytes have been written to DR via usart2_write_char. The last byte may still be in the shift register at the point of return.

## Architectural Decisions

1. TCIE before EN

The TCIE interrupt flag should be set before the DMA Stream becomes enabled because if the program contains other interrupts and the main thread of the program gets preempted after the EN is set but before TCIE interrupt flag is enabled, the program can lose the first TCIE flag set.

2. TCIF6 vs SR TC

This implementation of the DMA TX uses the TCIF6 polling as the "success" condition - TCIF6 is set once the DMA moved the data from the RAM to the peripheral's DR. The DMA does not wait for the data to be moved to the shift register and then on the lines.
SR TC on the other hand gets set once the last bit of the data payload has left the shift register and is on the lines.
The initial idea to track the successfulness of the transfer was to poll TCIF6 and if it is set, then to poll SR TC. But for the transmit of a 64-byte buffer with no CPU involvment and TC interrupt signal completion, the SR TC chasing is out of scope. It was decided that this functionality is moved to the later phases.

3. Write-order constraint

The full DMA config sequence has been placed before EN = 1. The majority of DMA register bits can only be written while EN = 0, otherwise no change is possible. The EN gates the configuration fields because the stream is a live AHB bus master mid-arbitration, and letting CHSEL/DIR/size change underneath a running transfer would leave the hardware in an undefined state (which byte width, which peripheral, halfway through).

4. app_config.h compile-time branching

The RX/TX/RX+TX branches are separate builds because, for example, the RX and RX+TX branches require the initialization of the ring buffer meanwhile the TX path does not use it at all. So the compile-time branching gates parts of the functionality and registers that are not used by one path but required by the other.
Without compile-time branching, a TX-only user would have an unused ring buffer and RX interrupt handler linked into the binary, wasting SRAM and registering an ISR that never fires.

## Known Limitations

1. No usart2_dma_tx_send(ptr, len) function at the moment. The DMA TX path has no real usage in the driver: a user wanting DMA TX must manually populate raw_tx_buffer and call usart2_dma_tx_init() directly - there is no function to trigger a new transfer after initialization.
It is set to be written once the future project(s) require it.

2. Hardcoded PA2 and PA3 as TX2 and RX2 pins respectively. The future version of the driver should allow the selection of the available pins, not hardcoded values.

3. No DMA for receiving. The driver still uses the interrupt-driven RX functionality. The DMA RX is the next step.
