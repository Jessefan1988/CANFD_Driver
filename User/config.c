#include "config.h"
#include "systick.h" 
#include "gd32h7xx_gpio.h"
#include "usart_debug.h"
#include "user_canfd.h"

/**********************************************************************************
 #-FUNCTION:        sys_init()
 #-OBJECT  :        系统初始化
 #-CREATE DATE  :
 #-UPDATE DATE  :
 #-AUTHOR  :
 **********************************************************************************/
void sys_init(void)
{
		cache_enable();
		systick_config();
		/****** LED GPIO Init ******/
		gpio_deinit(GPIOD);
		rcu_periph_clock_enable(RCU_GPIOD);
		gpio_mode_set(GPIOD, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, GPIO_PIN_3);
		gpio_output_options_set(GPIOD, GPIO_OTYPE_PP, GPIO_OSPEED_12MHZ, GPIO_PIN_3);
		gpio_bit_reset(GPIOD, GPIO_PIN_3);	//turn on the LED

		/****** UART Init ******/
		usart_debug_init();
		
		/****** CAN_FD Init ******/	
		canfd_init();
}

void cache_enable(void)
{
    /* Enable I-Cache */
    SCB_EnableICache();

    /* Enable D-Cache */
    SCB_EnableDCache();
}





