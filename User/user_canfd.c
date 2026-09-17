//////////////////////////////////////////////////////////////////////////////////
// Create Date      :   2026/9/16
// Author           :   Jesse Fan
// Module Name      :   user_canfd
// Target Devices   :   gd32h7xx
// Description      :   User driver of CANFD
//////////////////////////////////////////////////////////////////////////////////
#include "user_canfd.h"
#include "gd32h7xx_can.h"
#include "gd32h7xx_gpio.h"

volatile uint8_t canfd_tx_data[BUFFER_SIZE] = {0};
volatile uint8_t canfd_rx_data[BUFFER_SIZE] = {0};

can_mailbox_descriptor_struct transmit_message;
can_mailbox_descriptor_struct receive_message;

static uint16_t s_can_busoff_count      = 0;
static uint16_t s_can_recovery_count    = 0;
static uint16_t s_can_err_warning_count = 0;
static uint16_t s_can_err_summary_count = 0;

uint16_t tx_errcnt = 0;
uint16_t rx_errcnt = 0;
uint16_t fd_data_phase_tx_errcnt = 0;
uint16_t fd_data_phase_rx_errcnt = 0;
uint32_t tdcv_reg = 0;

void canfd_gpio_config(void)
{
    /* configure CAN1 or CAN2 clock source */
    rcu_can_clock_config(IDX_CAN1, RCU_CANSRC_APB2_DIV2);//APB2_DIV2 is 150MHz,if APB2 is 300MHz
//		rcu_can_clock_config(IDX_CAN1, RCU_CANSRC_APB2);//APB2 is 300MHz
    /* enable CAN clock */
    rcu_periph_clock_enable(RCU_CAN1);
    /* enable CAN port clock */
    rcu_periph_clock_enable(RCU_GPIOB);
    /* configure CAN1_RX GPIO */
    gpio_output_options_set(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_60MHZ, GPIO_PIN_12);
    gpio_mode_set(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_12);
    gpio_af_set(GPIOB, GPIO_AF_9, GPIO_PIN_12);
    /* configure CAN1_TX GPIO */
    gpio_output_options_set(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_60MHZ, GPIO_PIN_13);
    gpio_mode_set(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_13);
    gpio_af_set(GPIOB, GPIO_AF_9, GPIO_PIN_13);

}

void canfd_config(void)
{
    can_parameter_struct can_parameter;
    can_fd_parameter_struct fd_parameter;

    /* initialize CAN register */
    can_deinit(CAN1);
    /* initialize CAN */
    can_struct_para_init(CAN_INIT_STRUCT, &can_parameter);
    can_struct_para_init(CAN_FD_INIT_STRUCT, &fd_parameter);

    /* initialize CAN parameters */
    can_parameter.internal_counter_source = CAN_TIMER_SOURCE_BIT_CLOCK;
    can_parameter.self_reception = DISABLE;
    can_parameter.mb_tx_order = CAN_TX_HIGH_PRIORITY_MB_FIRST;
    can_parameter.mb_tx_abort_enable = ENABLE;
    can_parameter.local_priority_enable = DISABLE;
    can_parameter.mb_rx_ide_rtr_type = CAN_IDE_RTR_FILTERED;
    can_parameter.mb_remote_frame = CAN_STORE_REMOTE_REQUEST_FRAME;
    can_parameter.rx_private_filter_queue_enable = DISABLE;
    can_parameter.edge_filter_enable = DISABLE;
    can_parameter.protocol_exception_enable = DISABLE;
    can_parameter.rx_filter_order = CAN_RX_FILTER_ORDER_MAILBOX_FIRST;
    can_parameter.memory_size = CAN_MEMSIZE_32_UNIT;
    /* filter configuration */
    can_parameter.mb_public_filter = 0xFFFFFFFFU;

		/* baud rate 1Mbps, sample point at 80% */
    can_parameter.resync_jump_width = 1U;
    can_parameter.prop_time_segment = 2U;
    can_parameter.time_segment_1 = 5U;
    can_parameter.time_segment_2 = 2U;
    can_parameter.prescaler = 15U; //150MHz/(15*10)=1MHz

    /* initialize CAN */
    can_init(CAN1, &can_parameter);

    /* FD parameter configurations */
    fd_parameter.bitrate_switch_enable = ENABLE;
    fd_parameter.iso_can_fd_enable = ENABLE;
    fd_parameter.mailbox_data_size = CAN_MAILBOX_DATA_SIZE_64_BYTES;
    fd_parameter.tdc_enable = ENABLE;
    fd_parameter.tdc_offset = 6U;
		
		/* FD sample point at 73.3%. Total_tp=1+6+4+4=15 */
    fd_parameter.resync_jump_width = 3U;
    fd_parameter.prop_time_segment = 6U;
    fd_parameter.time_segment_1 = 4U;
    fd_parameter.time_segment_2 = 4U;

		/* FD baud rate 5Mbps */
    fd_parameter.prescaler = 2U; //150MHz/(2*Total_tp)=5MHz

    can_fd_config(CAN1, &fd_parameter);

    /* configure CAN1 NVIC */
    nvic_irq_enable(CAN1_Message_IRQn, 5U, 0U);

    /* enable CAN MB1 interrupt,receive interrupt */
    can_interrupt_enable(CAN1, CAN_INT_MB1);
		
    /* CAN Mode select */		
    can_operation_mode_enter(CAN1, CAN_NORMAL_MODE);
		
		/* enable auto busoff recovery */
		can_auto_busoff_recovery_enable(CAN1);
}

void canfd_mailbox_config(void)
{
		/* initialize CAN mailbox */
		can_struct_para_init(CAN_MDSC_STRUCT, &transmit_message);
		can_struct_para_init(CAN_MDSC_STRUCT, &receive_message);
		/* initialize transmit message */
		transmit_message.rtr = 0U;
		transmit_message.ide = 0U;
		transmit_message.code = CAN_MB_TX_STATUS_DATA;
		transmit_message.brs = 1U;
		transmit_message.fdf = 1U;
		transmit_message.esi = 0U;
		transmit_message.prio = 0U;
		transmit_message.data_bytes = BUFFER_SIZE;
		/* tx message content */
		transmit_message.data = (uint32_t *)(canfd_tx_data);
		transmit_message.id = CANFD_TGT_ID;
		/* configure TX mailbox (Mailbox 0) */
		can_mailbox_config(CAN1, 0U, &transmit_message);
		
		/* rx message content */
		receive_message.rtr = 0U;
		receive_message.ide = 0U;
		receive_message.code = CAN_MB_RX_STATUS_EMPTY;
		/* rx mailbox */
		receive_message.id = CANFD_LOC_ID;
		receive_message.data = (uint32_t *)canfd_rx_data;
		can_mailbox_config(CAN1, 1U, &receive_message);
		/* configure private filter for RX mailbox 1, matching ID = CANFD_LOC_ID */
		can_private_filter_config(CAN1, 1U, CANFD_LOC_ID);
}

void canfd_err_interuput_enble(void)
{

		/* enable can error interrupt */
		can_interrupt_enable(CAN1, CAN_INT_ERR_SUMMARY);
		can_interrupt_enable(CAN1, CAN_INT_BUSOFF);
		can_interrupt_enable(CAN1, CAN_INT_BUSOFF_RECOVERY);
		can_interrupt_enable(CAN1, CAN_INT_RX_WARNING);
		can_interrupt_enable(CAN1, CAN_INT_TX_WARNING);

		nvic_irq_enable(CAN1_Error_IRQn, 6, 0);
	
		/* read TDCV reg, used to test */
		tdcv_reg = GET_FDCTL_TDCV(CAN_FDCTL(CAN1));
}

void canfd_init(void)
{
		canfd_gpio_config();
		canfd_config();
		canfd_mailbox_config();
		canfd_err_interuput_enble(); 
}

/* Mailbox 1 (接收邮箱)中断服务函数，函数代码仅供参考 */
void CAN1_Message_IRQHandler(void)
{
    /* 检查是否是 Mailbox 1 (接收邮箱) 触发的中断 */
    if (can_interrupt_flag_get(CAN1, CAN_INT_FLAG_MB1)) 
    {
        /* 清除 MB1 中断标志位（必须最先清除，防止重复进入） */
        can_interrupt_flag_clear(CAN1, CAN_INT_FLAG_MB1);
        
        /* 从 Mailbox 1 读取数据。
           库函数会自动把硬件收到的数据搬入全局的 canfd_rx_data 数组，
           并将真实的数据字节数存入全局的 receive_message.data_bytes 中 */
        can_mailbox_receive_data_read(CAN1, 1U, &receive_message);
        
        /* 判断接收到的数据长度是否为 8 字节 */
        if (receive_message.data_bytes == 8U) 
        {
            /* 逐个比对数据内容是否为 0x00 ~ 0x07 */
            if ((canfd_rx_data[0] == 0x00U) && 
                (canfd_rx_data[1] == 0x01U) && 
                (canfd_rx_data[2] == 0x02U) && 
                (canfd_rx_data[3] == 0x03U) && 
                (canfd_rx_data[4] == 0x04U) && 
                (canfd_rx_data[5] == 0x05U) && 
                (canfd_rx_data[6] == 0x06U) && 
                (canfd_rx_data[7] == 0x07U)) 
            {
                /* 匹配成功！
                   因为 transmit_message 已经是全局变量且参数已准备好，
                   直接调用配置函数即可触发 MB0 发送 64 字节数据,检查 Mailbox 0 是否空闲 */
                if (can_mailbox_code_get(CAN1, 0U) == CAN_MB_TX_STATUS_INACTIVE)  
								{
										can_mailbox_config(CAN1, 0U, &transmit_message); 
								}
            }
        }
    }
}

/* CANFD错误中断服务函数，函数代码仅供参考 */
void CAN1_Error_IRQHandler(void)
{
    can_error_counter_struct errcnt;
		
		/* 读TDCV寄存器，检查当前回环延迟 */
		tdcv_reg = GET_FDCTL_TDCV(CAN_FDCTL(CAN1));

    /* 逐个检查中断标志位 */
    FlagStatus busoff_flag       = can_interrupt_flag_get(CAN1, CAN_INT_FLAG_BUSOFF);
    FlagStatus recovery_flag     = can_interrupt_flag_get(CAN1, CAN_INT_FLAG_BUSOFF_RECOVERY);
    FlagStatus tx_warning_flag   = can_interrupt_flag_get(CAN1, CAN_INT_FLAG_TX_WARNING);
    FlagStatus rx_warning_flag   = can_interrupt_flag_get(CAN1, CAN_INT_FLAG_RX_WARNING);
    FlagStatus err_summary_flag  = can_interrupt_flag_get(CAN1, CAN_INT_FLAG_ERR_SUMMARY);
    FlagStatus err_summary_fd    = can_interrupt_flag_get(CAN1, CAN_INT_FLAG_ERR_SUMMARY_FD);

    /* 处理 BusOff */
    if (busoff_flag == SET)
		{
        s_can_busoff_count++;

        /* 读取错误计数器，定位错误类型 */
        can_error_counter_get(CAN1, &errcnt);

        /* 清除 BusOff 中断标志 */
        can_interrupt_flag_clear(CAN1, CAN_INT_FLAG_BUSOFF);

        /* 手动恢复：停止 CAN -> 清零错误计数器 -> 重新启动 */
        can_operation_mode_enter(CAN1, CAN_INACTIVE_MODE);
        for (volatile uint32_t i = 0; i < 100; i++);

        can_error_counter_struct clear_cnt = {0};
        can_error_counter_config(CAN1, &clear_cnt);

        can_operation_mode_enter(CAN1, CAN_NORMAL_MODE);
        can_auto_busoff_recovery_enable(CAN1);

        /* 重新使能错误中断（进入模式可能被清除） */
        can_interrupt_enable(CAN1, CAN_INT_ERR_SUMMARY);
        can_interrupt_enable(CAN1, CAN_INT_BUSOFF);
        can_interrupt_enable(CAN1, CAN_INT_BUSOFF_RECOVERY);

        s_can_recovery_count++;
    }

    /* 处理硬件自动恢复完成标志 */
    if (recovery_flag == SET)
		{
        can_interrupt_flag_clear(CAN1, CAN_INT_FLAG_BUSOFF_RECOVERY);
    }

    /* 处理 TX 错误警告 (TX errcnt >= 128, 进入 Error Passive) */
    if (tx_warning_flag == SET)
		{
        s_can_err_warning_count++;
        can_error_counter_get(CAN1, &errcnt);
        can_interrupt_flag_clear(CAN1, CAN_INT_FLAG_TX_WARNING);
    }

    /* 处理 RX 错误警告 (RX errcnt >= 128, 进入 Error Passive) */
    if (rx_warning_flag == SET)
		{
        s_can_err_warning_count++;
        can_error_counter_get(CAN1, &errcnt);
        can_interrupt_flag_clear(CAN1, CAN_INT_FLAG_RX_WARNING);
    }

    /* 处理 CAN FD 错误汇总中断 */
    if (err_summary_fd == SET)
		{
        can_error_counter_get(CAN1, &errcnt);
        can_interrupt_flag_clear(CAN1, CAN_INT_FLAG_ERR_SUMMARY_FD);
    }

    /* 处理错误汇总中断（瞬态错误：位错误/填充错误/CRC错误/格式错误/ACK错误） */
    if (err_summary_flag == SET)
		{
				s_can_err_summary_count++;
        can_error_counter_get(CAN1, &errcnt);
				if(errcnt.tx_errcnt >= 1U) tx_errcnt++;
				if(errcnt.rx_errcnt >= 1U) rx_errcnt++;
				if(errcnt.fd_data_phase_tx_errcnt >= 1U) fd_data_phase_tx_errcnt++;
				if(errcnt.fd_data_phase_rx_errcnt >= 1U) fd_data_phase_rx_errcnt++;
			
        can_interrupt_flag_clear(CAN1, CAN_INT_FLAG_ERR_SUMMARY);
    }
		
}


