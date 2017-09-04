/*! @file
 *
 *  @brief Routines for controlling the Real Time Clock (RTC) on the TWR-K70F120M.
 *
 *  This contains the functions for operating the real time clock (RTC).
 *
 *  @author Manujaya Kankanige & Smit Patel
 *  @date 2016-05-01
 */

/*!
 *  @addtogroup rtc_module RTC module documentation
 *  @{
 */

// Included header files
#include "OS.h"
#include "PE_Types.h"
#include "types.h"
#include "IO_Map.h"

extern OS_ECB *Update_Clock_Semaphore; /*!< Binary semaphore for signaling clock update */

BOOL RTC_Init(void)
{
  SIM_SCGC6 |= SIM_SCGC6_RTC_MASK; // Enable clock gate for RTC module

  RTC_CR |= RTC_CR_OSCE_MASK; // Enable oscillator
  RTC_CR |= RTC_CR_SC16P_MASK | RTC_CR_SC2P_MASK; // Enable 18pF load

  RTC_IER |= RTC_IER_TSIE_MASK ; // Enable time seconds interrupt
  RTC_LR &= ~RTC_LR_CRL_MASK; // Lock control register

  NVICICPR2 |= NVIC_ICPR_CLRPEND(1 << 3); // Clear pending interrupts on RTC module
  NVICISER2 |= NVIC_ISER_SETENA(1 << 3); // Enable interrupts on RTC module

  Update_Clock_Semaphore = OS_SemaphoreCreate(0); // RTC semaphore initialized to 0

  return bTRUE; // RTC initialization complete
}


void RTC_Set(const uint8_t hours, const uint8_t minutes, const uint8_t seconds)
{
  RTC_SR &= ~RTC_SR_TCE_MASK; // Disable time counter
  RTC_TSR = (hours * 3600) + (minutes * 60) + seconds; // Put time value to time seconds register
  RTC_SR |= RTC_SR_TCE_MASK; // Re-enable timer counter
}


void RTC_Get(uint8_t* const hours, uint8_t* const minutes, uint8_t* const seconds)
{
  uint32_t timeTemp = RTC_TSR; /*!< Temporarily stores time value in register */

  *hours = (timeTemp / 3600) % 24; // Update hours value
  timeTemp %= 3600;
  *minutes = timeTemp / 60; // Update minutes value
  timeTemp %= 60;
  *seconds = timeTemp; // Update seconds value
}


void __attribute__ ((interrupt)) RTC_ISR(void)
{
  OS_ISREnter(); // Start of servicing interrupt
  OS_SemaphoreSignal(Update_Clock_Semaphore); // Signal RTC thread to update clock value
  OS_ISRExit(); // End of servicing interrupt
}


/*!
 * @}
*/

