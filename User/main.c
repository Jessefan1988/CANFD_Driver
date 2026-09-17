#include "gd32h7xx.h"
//#include "string.h"
#include "main.h"
//#include "stdio.h"
#include "config.h"
#include "systick.h"
#include "user_canfd.h"
#include "usart_debug.h"

int main(void)
{	
	SCB->VTOR = 0x08008000;		//中断向量表重映射
	sys_init();
	
	for(int i=0;i<64;i++)
		canfd_tx_data[i] = i;
	
	usart_print("=== GD32H759 CANFD Debug ===\r\n");
	while(1)
	{
		delay_1ms(1000);
		usart_print("tx_errcnt = %d\r\n", tx_errcnt);
		usart_print("rx_errcnt = %d\r\n", rx_errcnt);
		usart_print("fd_data_phase_tx_errcnt = %d\r\n", fd_data_phase_tx_errcnt);
		usart_print("fd_data_phase_rx_errcnt = %d\r\n", fd_data_phase_rx_errcnt);
		usart_print("tdcv_reg = %d\r\n", tdcv_reg);
	}
}


