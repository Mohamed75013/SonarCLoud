/*
 * UART.c
 *
 *  @brief I/O routines for UART communications on the TWR-K70F120M.
 *
 *  Created on: Mar 26, 2016
 *      Author: Manujaya Kankanige and Smit Patel (Group 7)
 */

#include "types.h"
#include "MK70F12.h"
#include "FIFO.h"

extern TFIFO TxFIFO;
extern TFIFO RxFIFO;


BOOL UART_Init(const uint32_t baudRate, const uint32_t moduleClk)
{
  uint16_t sbr,     	      /*!< The SBR value */
	   baudRateDivisor;   /*!< The SBR and BRFD value as a whole number */

  uint8_t brfa;      	      /*!< The BRFA bits */


 SIM_SCGC4 |= SIM_SCGC4_UART2_MASK; // Enable clock gate for UART2 (set bit 12)
 SIM_SCGC5 |= SIM_SCGC5_PORTE_MASK; // Enable clock gate for PortE to enable pin routing (set bit 13)
 PORTE_PCR16 = PORT_PCR_MUX(3); // Transmitter pin select, MUX = 3 (0x300)
 PORTE_PCR17 = PORT_PCR_MUX(3); // Receiver pin select, MUX = 3 (0x300)

 UART2_C1 = 0x00; // Configuration for UART
 UART2_C2 |= UART_C2_TE_MASK; // Enable transmitting
 UART2_C2 |= UART_C2_RE_MASK; // Enable receiving (0x0C)

 baudRateDivisor = (moduleClk * 2) / baudRate; // Calculation to deduce SBR and BRFA (1092)
 UART_C4_BRFA(baudRateDivisor % 32); // BRFA set to 4 where BRFD is 0.125

 sbr = baudRateDivisor / 32; // Value for baud rate setting(SBR) is calculated (34)
 UART2_BDH |= 0x1F & (sbr >>8); // 5 most significant bits of SBR is written to baud rate register(high)
 UART2_BDL = sbr; // 8 least significant bits of SBR is written to baud rate register(low)

 FIFO_Init(&RxFIFO); // Initialize receiver FIFO
 FIFO_Init(&TxFIFO); // Initialize transmitter FIFO

  return bTRUE; // Successful UART initialization
}

void UART_Poll(void)
{
  if (UART2_S1 & UART_S1_RDRF_MASK) // Receive data register full flag set
    FIFO_Put(&RxFIFO, UART2_D); // Put byte in UART data register to RxFIFO
  if (UART2_S1 & UART_S1_TDRE_MASK) // Transmit data register empty flag set
    FIFO_Get(&TxFIFO, (uint8_t *) &UART2_D); // Get byte from UART data register TxFIFO
}


BOOL UART_InChar(uint8_t * const dataPtr)
{
  return FIFO_Get(&RxFIFO, dataPtr); // Gets oldest byte from receiver FIFO buffer
}


BOOL UART_OutChar(const uint8_t data)
{
  return FIFO_Put(&TxFIFO, data); // Puts byte in transmit FIFO buffer
}






