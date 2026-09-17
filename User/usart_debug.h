
#ifndef __USART_DEBUG_H
#define __USART_DEBUG_H

#include "gd32h7xx.h"
#include <stdio.h>
#include <stdarg.h>

/* 串口编号 — UART4 */
#define DEBUG_USART              UART4

/* 波特率 */
#define DEBUG_USART_BAUDRATE     115200

/* GPIO 引脚定义: PB5(RX) + PB6(TX), AF14 */
#define DEBUG_USART_TX_PIN       GPIO_PIN_6
#define DEBUG_USART_RX_PIN       GPIO_PIN_5
#define DEBUG_USART_GPIO_PORT    GPIOB
#define DEBUG_USART_GPIO_CLK     RCU_GPIOB
#define DEBUG_USART_AF           GPIO_AF_14

/* 函数声明 */
void usart_debug_init(void);
void usart_send_byte(uint8_t ch);
void usart_send_string(const char *str);
void usart_send_int(int num);
void usart_send_uint(unsigned int num);
void usart_send_hex(uint32_t hex);
void usart_print(const char *fmt, ...);

#endif /* __USART_DEBUG_H */