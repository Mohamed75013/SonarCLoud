/*! @file
 *
 *  @brief Routines for controlling Periodic Interrupt Timer (PIT) on the TWR-K70F120M.
 *
 *  This contains the functions for operating the periodic interrupt timer (PIT).
 *
 *  @author Manujaya Kankanige & Smit Patel
 *  @date 2016-05-01
 */

/*!
 *  @addtogroup pit_module PIT module documentation
 *  @{
 */

// Included header files
#include "OS.h"
#include "types.h"
#include "MK70F12.h"
#include "LEDs.h"
#include "PIT.h"

// Variable declarations
static uint32_t ModuleClkMHz; /*!< Module clock value in MHz */
extern OS_ECB *PIT_Semaphore; /*!< Binary semaphore for signaling clock update */

BOOL PIT_Init(const uint32_t moduleClk)
{
  ModuleClkMHz = moduleClk / 1000000; // Conversion of module clock to MHz

  SIM_SCGC6 |=  SIM_SCGC6_PIT_MASK; // Enable clock gate for PIT module
  PIT_MCR &= ~PIT_MCR_FRZ_MASK; // Run timers when in debug mode (PIT_MCR = 0)
  PIT_MCR &= ~PIT_MCR_MDIS_MASK; // Enable clock for PIT

  NVICICPR2 |= NVIC_ICPR_CLRPEND(1 << 4); // Clear pending interrupts on PIT module
  NVICISER2 |= NVIC_ISER_SETENA(1 << 4); // Enable interrupts on PIT module

  PIT_Semaphore = OS_SemaphoreCreate(0); // PIT semaphore initialized to 0

  return bTRUE;
}


void PIT_Set(const uint32_t period, const BOOL restart)
{
  if (restart)
  {
    PIT_Enable(bFALSE); // Disable PIT0
    PIT_LDVAL0 = ((period/1000) * ModuleClkMHz) -1; // Setup timer0 for 12500000 cycles
    PIT_Enable(bTRUE); // Enable PIT0
  }
  PIT_TCTRL0 |= PIT_TCTRL_TIE_MASK; // Enable interrupts for PIT0
}


void PIT_Enable(const BOOL enable)
{
  if (enable)
    PIT_TCTRL0 |= PIT_TCTRL_TEN_MASK; // Enable PIT0
  else
    PIT_TCTRL0 &= ~PIT_TCTRL_TEN_MASK; // Disable PIT0
}


void __attribute__ ((interrupt)) PIT_ISR(void)
{
  OS_ISREnter(); // Start of servicing interrupt
  OS_SemaphoreSignal(PIT_Semaphore); // Signal PIT thread to tell it can run
  OS_ISRExit(); // End of servicing interrupt
}


/*!
 * @}
*/
