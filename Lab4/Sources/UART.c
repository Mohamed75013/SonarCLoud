/*! @file
 *
 *  @brief I/O routines for UART communications on the TWR-K70F120M.
 *
 *  This contains the functions for operating the UART (serial port).
 *
 *  @author Manujaya Kankanige & Smit Patel
 *  @date 2016-03-26
 */

/*!
 *  @addtogroup uart_module UART module documentation
 *  @{
 */

// Included header files
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

  SIM_SCGC4 |= SIM_SCGC4_UART2_MASK; // Enable clock gate for UART2 module
  SIM_SCGC5 |= SIM_SCGC5_PORTE_MASK; // Enable clock gate for PortE to enable pin routing
  PORTE_PCR16 = PORT_PCR_MUX(3); // Transmitter pin select
  PORTE_PCR17 = PORT_PCR_MUX(3); // Receiver pin select

  UART2_C1 = 0x00; // Configuration for UART

  UART2_C2 |= UART_C2_TE_MASK; // Enable transmitting
  UART2_C2 |= UART_C2_RE_MASK; // Enable receiving (0x0C)
  UART2_C2 |= UART_C2_RIE_MASK; // Receive(RDRF) interrupt enabled

  baudRateDivisor = (moduleClk * 2) / baudRate; // Calculation to deduce SBR and BRFA

  brfa = baudRateDivisor % 32; // Calculation of BRFA
  UART2_C4 = UART_C4_BRFA(brfa); // BRFA values is stored into the register

  sbr = baudRateDivisor / 32; // Value for baud rate setting(SBR) is calculated

  UART2_BDH |= 0x1F & (sbr >> 8); // 5 most significant bits of SBR is written to baud rate register(high)
  UART2_BDL = sbr; // 8 least significant bits of SBR is written to baud rate register(low)

  NVICICPR1 = NVIC_ICPR_CLRPEND(1 << 17); // Clear any pending interrupts on UART2
  NVICISER1 = NVIC_ISER_SETENA(1 << 17);  // Enable interrupts on UART2

  FIFO_Init(&RxFIFO); // Initialize receiver FIFO
  FIFO_Init(&TxFIFO); // Initialize transmitter FIFO

  return bTRUE;
}


BOOL UART_InChar(uint8_t * const dataPtr)
{
  return FIFO_Get(&RxFIFO, dataPtr); // Gets oldest byte from receiver FIFO buffer
}


BOOL UART_OutChar(const uint8_t data)
{
  UART2_C2 |= UART_C2_TIE_MASK; // Transmit(TDRE) interrupt enabled
  return FIFO_Put(&TxFIFO, data); // Puts byte in transmit FIFO buffer
}


void __attribute__ ((interrupt)) UART_ISR(void)
{
  if (UART2_S1 & UART_S1_RDRF_MASK) // Clear RDRF flag by reading it
    FIFO_Put(&RxFIFO,UART2_D); // Place data in receiver FIFO

  if (UART2_C2 & UART_C2_TIE_MASK) // Only respond if transmit(TDRE) interrupt is enabled
  {
    if (UART2_S1 & UART_S1_TDRE_MASK)// Clear TDRE flag by reading it
    {
      if (!FIFO_Get(&TxFIFO,(uint8_t* )&UART2_D)) // Check if transmitter FIFO is empty
        UART2_C2 &= ~UART_C2_TIE_MASK; // Transmit(TDRE) interrupt disabled
    }
  }
}


/*!
 * @}
*/
