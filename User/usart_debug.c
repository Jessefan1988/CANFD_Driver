
/**
 * @file    usart_debug.c
 * @brief   GD32H7 UART4 调试串口初始化与打印函数实现
 *          引脚: PB5(RX) + PB6(TX), AF14
 *          波特率: 115200, 数据位:8, 停止位:1, 无校验
 */

#include "usart_debug.h"

/* ============================================================
 *  1. USART 初始化函数
 * ============================================================
 *  调用方式: 在 main() 最开始调用一次即可
 *  内部完成:
 *    - 使能 GPIOB 和 UART4 时钟
 *    - 配置 PB5(复用推挽=RX), PB6(复用推挽=TX), AF14
 *    - 配置 UART4: 115200bps, 8数据位, 1停止位, 无校验
 *    - 使能发送器和接收器, 使能 UART4
 */
void usart_debug_init(void)
{
    /* ---------- 1. 使能 GPIOB 和 UART4 外设时钟 ---------- */
    rcu_periph_clock_enable(DEBUG_USART_GPIO_CLK);
    rcu_periph_clock_enable(RCU_UART4);

    /* ---------- 2. 配置 PB5 为复用功能 (RX), AF14 ---------- */
    gpio_mode_set(DEBUG_USART_GPIO_PORT, GPIO_MODE_AF,
                  GPIO_PUPD_PULLUP, DEBUG_USART_RX_PIN);
    gpio_af_set(DEBUG_USART_GPIO_PORT, DEBUG_USART_AF, DEBUG_USART_RX_PIN);

    /* ---------- 3. 配置 PB6 为复用功能 (TX), AF14 ---------- */
    gpio_mode_set(DEBUG_USART_GPIO_PORT, GPIO_MODE_AF,
                  GPIO_PUPD_NONE, DEBUG_USART_TX_PIN);
    gpio_af_set(DEBUG_USART_GPIO_PORT, DEBUG_USART_AF, DEBUG_USART_TX_PIN);

    /* ---------- 4. 配置 UART4 参数 ---------- */
    /* 先禁用 UART4 */
    usart_disable(DEBUG_USART);

    /* 波特率 */
    usart_baudrate_set(DEBUG_USART, DEBUG_USART_BAUDRATE);

    /* 过采样 16 倍 (默认就是 16, 显式配置一下更清晰) */
    usart_oversample_config(DEBUG_USART, USART_OVSMOD_16);

    /* 数据位 8 位 */
    usart_word_length_set(DEBUG_USART, USART_WL_8BIT);

    /* 停止位 1 位 */
    usart_stop_bit_set(DEBUG_USART, USART_STB_1BIT);

    /* 无校验 */
    usart_parity_config(DEBUG_USART, USART_PM_NONE);

    /* 使能发送器和接收器 */
    usart_transmit_config(DEBUG_USART, USART_TRANSMIT_ENABLE);
    usart_receive_config(DEBUG_USART, USART_RECEIVE_ENABLE);

    /* 使能 UART4 */
    usart_enable(DEBUG_USART);
}

/* ============================================================
 *  2. 底层发送函数
 * ============================================================
 *  等待发送数据寄存器为空(TBE 标志), 然后写入数据
 */
void usart_send_byte(uint8_t ch)
{
    /* 等待上次发送完成 — GD32H7 用 USART_FLAG_TBE */
    while (0 == usart_flag_get(DEBUG_USART, USART_FLAG_TBE));
    /* 写入数据寄存器 */
    usart_data_transmit(DEBUG_USART, ch);
}

/* ============================================================
 *  3. 发送字符串函数
 * ============================================================
 *  逐字符调用 usart_send_byte 发送, 遇到 '\0' 停止
 */
void usart_send_string(const char *str)
{
    while (*str) {
        usart_send_byte(*str++);
    }
}

/* ============================================================
 *  4. 发送有符号整型函数
 * ============================================================
 *  处理负号, 逐位提取数字并转为 ASCII 发送
 */
void usart_send_int(int num)
{
    if (num < 0) {
        usart_send_byte('-');
        num = -num;
    }
    if (num == 0) {
        usart_send_byte('0');
        return;
    }
    /* 先算出位数, 从高位到低位发送 */
    int divisor = 1;
    while (num / divisor > 9) {
        divisor *= 10;
    }
    while (divisor > 0) {
        usart_send_byte((num / divisor) % 10 + '0');
        divisor /= 10;
    }
}

/* ============================================================
 *  5. 发送无符号整型函数
 * ============================================================
 */
void usart_send_uint(unsigned int num)
{
    if (num == 0) {
        usart_send_byte('0');
        return;
    }
    unsigned int divisor = 1;
    while (num / divisor > 9) {
        divisor *= 10;
    }
    while (divisor > 0) {
        usart_send_byte((num / divisor) % 10 + '0');
        divisor /= 10;
    }
}

/* ============================================================
 *  6. 发送十六进制函数
 * ============================================================
 *  固定 8 位十六进制 (32位), 每 4 位转一个 hex 字符
 */
void usart_send_hex(uint32_t hex)
{
    const char hex_chars[] = "0123456789abcdef";
    int i;
    /* 跳过前导零 */
    int leading = 1;
    for (i = 28; i >= 0; i -= 4) {
        char c = (hex >> i) & 0xF;
        if (c != 0) leading = 0;
        if (!leading) {
            usart_send_byte(hex_chars[c]);
        }
    }
    if (leading) {
        usart_send_byte('0');
    }
}

/* ============================================================
 *  7. 格式化打印函数 (类 printf)
 * ============================================================
 *  支持的格式符: %s %d %u %x %c %p
 *  内部使用 vsnprintf 实现, 然后逐字符通过 usart_send_byte 发出
 *
 *  使用示例:
 *    usart_print("Hello, World!\r\n");
 *    usart_print("counter = %d\r\n", counter);
 *    usart_print("value = %x, hex = %08x\r\n", val, addr);
 */
void usart_print(const char *fmt, ...)
{
    char buf[256];
    va_list args;

    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    usart_send_string(buf);
}

/* ============================================================
 *  8. 重定向 printf 到串口 (可选)
 * ============================================================
 *  如果你希望直接使用标准库的 printf() 函数,
 *  只需要在代码最顶层(全局作用域)添加以下 __attribute__((weak)) 函数:
 *
 *  int __attribute__((weak)) __io_putchar(int ch)
 *  {
 *      usart_send_byte((uint8_t)ch);
 *      return ch;
 *  }
 *
 *  添加后, 直接用 printf("val = %d\r\n", x); 即可输出到串口。
 *  注意: 需要在 Keil/IAR/STM32CubeIDE 中勾选 "Use MicroLIB"
 *        或在编译选项加 -u _printf_float (如果要用 %f)
 */