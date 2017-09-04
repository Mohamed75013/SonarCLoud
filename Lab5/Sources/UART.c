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
#include "OS.h"
#include "PE_Types.h"
#include "types.h"
#include "MK70F12.h"
#include "FIFO.h"


// Arbitrary thread stack size - big enough for stacking of interrupts and OS use.
#define THREAD_STACK_SIZE 100

// Prototypes
static void ReceiveThread(void* pData);
static void TransmitThread(void* pData);

// Variable Declarations
static uint32_t TransmitThreadStack[THREAD_STACK_SIZE] __attribute__ ((aligned(0x08))); /*!< The stack for the transmit thread. */
static uint32_t ReceiveThreadStack[THREAD_STACK_SIZE] __attribute__ ((aligned(0x08))); /*!< The stack for the receive thread */
static OS_ECB *TransmitSemaphore; /*!< Binary semaphore for signaling that data transmission */
static OS_ECB *ReceiveSemaphore;  /*!< Binary semaphore for signaling receiving of data */

extern TFIFO TxFIFO;
extern TFIFO RxFIFO;


BOOL UART_Init(const uint32_t baudRate, const uint32_t moduleClk)
{
  OS_ERROR error;             /*!< Thread content */

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
  UART2_C2 |= UART_C2_RIE_MASK; // Receive(RDRF) interrupt enable
  UART2_C2 &= ~UART_C2_TIE_MASK; // Transmission complete interrupt enable

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

  ReceiveSemaphore = OS_SemaphoreCreate(0); // Receive semaphore initialized to 0
  TransmitSemaphore = OS_SemaphoreCreate(0); // Transmit semaphore initialized to 0

  error = OS_ThreadCreate(ReceiveThread, // 2nd highest priority thread
                          NULL,
                          &ReceiveThreadStack[THREAD_STACK_SIZE - 1],
                          1);

  error = OS_ThreadCreate(TransmitThread, // 3rd highest priority thread
                          NULL,
                          &TransmitThreadStack[THREAD_STACK_SIZE - 1],
                          2);
  return bTRUE;
}


BOOL UART_InChar(uint8_t * const dataPtr)
{
  return FIFO_Get(&RxFIFO, dataPtr); // Gets oldest byte from RxFIFO
}


BOOL UART_OutChar(const uint8_t data)
{
  FIFO_Put(&TxFIFO, data); // Put byte into TxFIFO
  UART2_C2 |= UART_C2_TIE_MASK; // Enable transmit interrupt
}


/*! @brief Thread that looks after receiving data.
 *
 *  @param pData Thread parameter.
 *  @note Assumes that semaphores are created and communicate properly.
 */
static void ReceiveThread(void* pData)
{
  for (;;)
  {
    OS_SemaphoreWait(ReceiveSemaphore, 0); // Wait for receive semaphore to signal
    FIFO_Put(&RxFIFO, UART2_D); // Put byte into RxFIFO
    UART2_C2 |= UART_C2_RIE_MASK; // Re-enable receive interrupt
  }
}


/*! @brief Thread that looks after transmitting data.
 *
 *  @param pData Thread parameter.
 *  @note Assumes that semaphores are created and communicate properly.
 */
void TransmitThread(void *data)
{
  for (;;)
  {
    OS_SemaphoreWait(TransmitSemaphore, 0); // Wait for transmit semaphore to signal
    if (UART2_S1 & UART_S1_TDRE_MASK) // Clear TDRE flag by reading it
    {
      FIFO_Get(&TxFIFO,(uint8_t* )&UART2_D);
      UART2_C2 |= UART_C2_TIE_MASK; // Re-enable transmission interrupt
    }
  }
}


void __attribute__ ((interrupt)) UART_ISR(void)
{
  OS_ISREnter(); // Start of servicing interrupt

  if (UART2_S1 & UART_S1_RDRF_MASK) // Clear RDRF flag by reading it
  {
    UART2_C2 &= ~UART_C2_RIE_MASK; // Receive interrupt disabled
    OS_SemaphoreSignal(ReceiveSemaphore); // Signal receive thread
  }

  if (UART2_C2 & UART_C2_TIE_MASK) // Clear TDRE flag by reading it
  {
    UART2_C2 &= ~UART_C2_TIE_MASK; // Transmit interrupt disabled
    OS_SemaphoreSignal(TransmitSemaphore); // Signal transmit thread
  }

  OS_ISRExit(); // End of servicing interrupt
}

/*!
 ** @}
 */
