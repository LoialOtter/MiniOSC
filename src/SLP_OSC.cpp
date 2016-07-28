//#define OSC_DEBUG_SLP

#include "SLP_OSC.h"
#include "crc16.h"
#include "ets_sys.h"
#include "uart_register.h"
#include "cbuf_malloc.h"
#include "os_type.h"
#include "user_interface.h"


#define SLP_BOUNDARY_BYTE 0x5C

static void uart0_intr_handler(void *para);
static void uart0_Txd(char c);
static void uart0_init(void);

static SLP_OSC_Class *slp_osc_peripheral;

#ifndef OSC_DEBUG_SLP
void SLP_OSC_Class::_tx(const char *buffer, size_t len) {
  int i;
  uint16_t crc = crc16(0, (uint8_t*)buffer, len);
  uart0_Txd(SLP_BOUNDARY_BYTE);
  uart0_Txd(0x10);
  uart0_Txd(len >> 2); // maximum 1024
  uart0_Txd(crc >> 8);
  uart0_Txd(crc);
  for (i = 0; i < len; i++) {
    uart0_Txd(buffer[i]);
  }
  for (; i & 0x3; i++) uart0_Txd(0);
  uart0_Txd(SLP_BOUNDARY_BYTE);
}
#else
const static char hex_chars[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
void SLP_OSC_Class::_tx(const char *buffer, size_t len) {
  int i;
  uint16_t crc = crc16(0, (uint8_t*)buffer, len);
  uart0_Txd('|');
  uart0_Txd(' ');
  uart0_Txd(hex_chars[(len>>8)&0x0F]);
  uart0_Txd(hex_chars[(len>>4)&0x0F]);
  uart0_Txd(hex_chars[(len)&0x0F]);
  uart0_Txd(' ');
  //uart0_Txd(len >> 2); // maximum 1024
  //uart0_Txd(crc >> 8);
  //uart0_Txd(crc);
  uart0_Txd(hex_chars[(crc>>12)&0x0F]);
  uart0_Txd(hex_chars[(crc>> 8)&0x0F]);
  uart0_Txd(hex_chars[(crc>> 4)&0x0F]);
  uart0_Txd(hex_chars[(crc    )&0x0F]);
  uart0_Txd(' ');
  for (i = 0; i < len; i++) {
    if (buffer[i] >= ' ' && buffer[i] <= '~') uart0_Txd(buffer[i]);
    else {
      uart0_Txd('\\');
      uart0_Txd(hex_chars[(buffer[i]>>4)&0xF]);
      uart0_Txd(hex_chars[(buffer[i])&0xF]);
    }
  }
  for (; i & 0x3; i++) uart0_Txd('.');
  //uart0_Txd(SLP_BOUNDARY_BYTE);
  uart0_Txd('|');
  uart0_Txd('\r');
  uart0_Txd('\n');
}
#endif /* OSC_DEBUG_SLP */


SLP_OSC_Class::SLP_OSC_Class(void) {
  uart0_init();
  slp_osc_peripheral = this;
}


void SLP_OSC_Class::rx(uint8_t *payload, size_t len) {
  Receive((char*)payload, len, this);
}



//============================================================================ Lowlevel System ==============================================================================
typedef enum {
  MESSAGE_DELIMITER=0,
  MESSAGE_ENDPOINT,
  MESSAGE_LENGTH,
  MESSAGE_DATA,
  MESSAGE_CRC1,
  MESSAGE_CRC2,
  MESSAGE_END
} message_section_type;

typedef struct {
  // this is used as an overflow for transmitting more than 127 bytes (the peripheral FIFO size)
#define TX_BUF_SIZE 256
#define TX_BUF_MASK (TX_BUF_SIZE-1)
  u8  tx_buf[TX_BUF_SIZE];
  u16 tx_buf_write_point = 0;
  u16 tx_buf_read_point = 0;
  
  // buffer for receiving messages - note that this is accessed within the interrupt
  // no provisions for reading outside the interrupt
#define RX_BUF_SIZE 512
#define RX_BUF_MASK (RX_BUF_SIZE-1)
  u8 rx_buf[RX_BUF_SIZE];
  u16 rx_buf_write_point;
  u16 rx_buf_read_point;


  // OS event to handle receving a message
#define RX_TASK_PRIORITY       5
#define RX_MESSAGE_QUEUE_SIZE  1
  os_event_t rx_message_queue[RX_MESSAGE_QUEUE_SIZE];

  message_section_type message_section;
  uint8  message_endpoint;
  uint16 message_length;
  uint16 message_crc;
  uint8_t* message_data;
  uint16 message_write_point;
} slp_internal_type;


// buffer for holding the finished OSC blocks in
#define SLP_OSC_BUFFER_SIZE 1024
char slp_osc_buffer_raw[SLP_OSC_BUFFER_SIZE];
cbuf_malloc_type slp_osc_buffer = {
  .buf = slp_osc_buffer_raw,
  .length = SLP_OSC_BUFFER_SIZE,
  .write_point = 0
};


slp_internal_type slp_int;

void ICACHE_FLASH_ATTR rx_task(os_event_t *events);

static void uart0_init(void) {
  //_uart.rx_enabled = false;
  //_uart.tx_enabled = false;
  //uart_set_debug(UART_NO);
  
  ETS_UART_INTR_ATTACH(uart0_intr_handler, 0);

  slp_int.message_section = MESSAGE_DELIMITER;
  
  //Enable TxD pin
  PIN_PULLUP_DIS(PERIPHS_IO_MUX_U0TXD_U);
  PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, FUNC_U0TXD);
  
  //Set baud rate and other serial parameters to 115200,n,8,1
  //uart_div_modify(0, UART_CLK_FREQ/BIT_RATE_115200);
  //uart_div_modify(0, UART_CLK_FREQ/100000); // 100kbit
  //uart_div_modify(0, UART_CLK_FREQ/2000000); // 2mbit
  
  //U0D = (ESP8266_CLOCK / 500000);
  //      1 stop bit   8 data bits     Reset both RX/TX FIFOs
  U0C0 = (1<<UCSBN) | (3<<UCBN) |   (1<<UCRXRST) | (1<<UCTXRST);
  // finish reset
  U0C0 = (1<<UCSBN) | (3<<UCBN);

  
  //Clear pending interrupts
  U0IC = 0xFFFF;
  
  //Install our own putchar handler
  //os_install_putc1((void *)stdoutPutchar);
  
  //enable rx_interrupt
  U0IE = UIFF;
  //SET_PERI_REG_MASK(UART_INT_ENA(0), UART_RXFIFO_FULL_INT_ENA);
  //CLEAR_PERI_REG_MASK(UART_INT_ENA(0), UART_TXFIFO_EMPTY_INT_ENA);

  //system_os_task(rx_task, RX_TASK_PRIORITY, slp_int.rx_message_queue, RX_MESSAGE_QUEUE_SIZE);
  ets_task(rx_task, RX_TASK_PRIORITY, slp_int.rx_message_queue, RX_MESSAGE_QUEUE_SIZE);

  ETS_UART_INTR_ENABLE();
}




static ICACHE_FLASH_ATTR void uart0_Txd(char c) {
  if ((((READ_PERI_REG(UART_STATUS(0)) >> UART_TXFIFO_CNT_S) & UART_TXFIFO_CNT) >= 126) || (READ_PERI_REG(UART_INT_ENA(0)) & UART_TXFIFO_EMPTY_INT_ENA)) {
    // if TXFIFO_EMPTY interrupt not set we're entering because the fifo is full; make sure it's set
    // add the character onto the buffer
    slp_int.tx_buf[slp_int.tx_buf_write_point++] = c;
    slp_int.tx_buf_write_point &= TX_BUF_MASK;
  
    if (!(READ_PERI_REG(UART_INT_ENA(0)) & UART_TXFIFO_EMPTY_INT_ENA)) {
      SET_PERI_REG_MASK(UART_INT_ENA(0), UART_TXFIFO_EMPTY_INT_ENA);
    }
  }
  else {
    //Send the character
    WRITE_PERI_REG(UART_FIFO(0), c);
  }
}






ICACHE_FLASH_ATTR void rx_task(os_event_t *events) {
  uint8 c;
  while (slp_int.rx_buf_write_point != slp_int.rx_buf_read_point) {
    c = slp_int.rx_buf[slp_int.rx_buf_read_point++];
    slp_int.rx_buf_read_point &= RX_BUF_MASK;
    
    switch (slp_int.message_section) {
    case MESSAGE_DELIMITER: // looking for delimiter (SLP_BOUNDARY_BYTE)
    case MESSAGE_ENDPOINT:
      if (c == SLP_BOUNDARY_BYTE) continue;
      else if (c == 0x10) {
        slp_int.message_endpoint = c;
        slp_int.message_section = MESSAGE_LENGTH;
      }
      else slp_int.message_section = MESSAGE_DELIMITER;
      break;
      
    case MESSAGE_LENGTH:
      slp_int.message_length = c << 2; // message length is *4 and only counts data bytes
      slp_int.message_section = MESSAGE_CRC1;
      break;
      
    case MESSAGE_CRC1:
      slp_int.message_crc = c << 8;
      slp_int.message_section = MESSAGE_CRC2;
      break;
      
    case MESSAGE_CRC2:
      slp_int.message_crc |= c;
      slp_int.message_section = MESSAGE_DATA;
      slp_int.message_data = (u8*)cbuf_malloc(&slp_osc_buffer, slp_int.message_length);
      break;
      
    case MESSAGE_DATA:
      slp_int.message_data[slp_int.message_write_point++] = c;
      if (slp_int.message_write_point >= slp_int.message_length) {
        slp_int.message_section = MESSAGE_DELIMITER;
        // enqueue message
        if (slp_osc_peripheral) {
          slp_osc_peripheral->rx(slp_int.message_data, slp_int.message_length);
        }
          
        slp_int.message_write_point = 0;
        slp_int.message_length = 0;
      }
      break;
      
    default:
      slp_int.message_section = MESSAGE_DELIMITER;
      if (c == SLP_BOUNDARY_BYTE) {
        slp_int.message_section = MESSAGE_ENDPOINT;
      }
    }
  }
}



static ICACHE_FLASH_ATTR void uart0_intr_handler(void *para) {
  /* uart0 and uart1 intr combine togther, when interrupt occur, see reg 0x3ff20020, bit2, bit0 represents
   * uart1 and uart0 respectively
   */
  if ((READ_PERI_REG(UART_INT_ST(UART0)) & UART_RXFIFO_FULL_INT_ST) == UART_RXFIFO_FULL_INT_ST) {
    slp_int.rx_buf[slp_int.rx_buf_write_point++] = UART_FIFO(UART0) & 0xFF;
    slp_int.rx_buf_write_point &= RX_BUF_MASK; // make sure the buffer doesn't extend past the size
    //rx_buf_write_point++; rx_buf_write_point &= RX_BUF_MASK;
    //system_os_post(RX_TASK_PRIORITY, 0, 0);
    ets_post(RX_TASK_PRIORITY, 0, 0);
    WRITE_PERI_REG(UART_INT_CLR(0), UART_RXFIFO_FULL_INT_CLR);
  }

  // if the txfifo is empty
  if (UART_TXFIFO_EMPTY_INT_ST == (READ_PERI_REG(UART_INT_ST(UART0)) & UART_TXFIFO_EMPTY_INT_ST)) {
    // load up the fifo with the buffered data
    while ((slp_int.tx_buf_write_point != slp_int.tx_buf_read_point) && (((READ_PERI_REG(UART_STATUS(0))>>UART_TXFIFO_CNT_S)&UART_TXFIFO_CNT)<126)) {
      WRITE_PERI_REG(UART_FIFO(0), slp_int.tx_buf[slp_int.tx_buf_read_point++]);
      slp_int.tx_buf_read_point &= TX_BUF_MASK;
    }
    // if the fifo buffer is full, great, remove the interrupt
    if (slp_int.tx_buf_write_point == slp_int.tx_buf_read_point) {
      CLEAR_PERI_REG_MASK(UART_INT_ENA(0), UART_TXFIFO_EMPTY_INT_ENA);
    }
  }
}

