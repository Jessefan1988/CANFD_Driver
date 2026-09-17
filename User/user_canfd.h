#ifndef USER_CANFD_H
#define USER_CANFD_H

#include "stdint.h"
#include "gd32h7xx_can.h"

#define  BUFFER_SIZE    64U
#define  CANFD_LOC_ID	  0x050U
#define  CANFD_TGT_ID		0x010U

extern volatile uint8_t canfd_tx_data[];
extern volatile uint8_t canfd_rx_data[];

extern can_mailbox_descriptor_struct transmit_message;
extern can_mailbox_descriptor_struct receive_message;

extern uint16_t tx_errcnt;
extern uint16_t rx_errcnt;
extern uint16_t fd_data_phase_tx_errcnt;
extern uint16_t fd_data_phase_rx_errcnt;
extern uint32_t tdcv_reg;


void canfd_gpio_config(void);
void canfd_config(void);
void canfd_mailbox_config(void);
void canfd_err_interuput_enble(void);
void canfd_init(void);

#endif