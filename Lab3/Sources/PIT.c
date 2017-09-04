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
#include "types.h"
#include "MK70F12.h"
#include "LEDs.h"
#include "PIT.h"

void* UserArguments_Global;           /*!< Private global pointer to the user arguments to use with the user callback function */
void (*PIT_Callback_Global)(void *);  /*!< Private global pointer to PIT user callback function */
static uint32_t ModuleClk_Global_MHz; /*!< Module clock value in MHz */

BOOL PIT_Init(const uint32_t moduleClk, void (*userFunction)(void *), void *userArguments)
{
  ModuleClk_Global_MHz = moduleClk / 1000000; // Conversion of module clock to MHz
  UserArguments_Global = userArguments; // userArguments made globally(private) accessible
  PIT_Callback_Global = userFunction; // userFunction made globally(private) accessible

  SIM_SCGC6 |=  SIM_SCGC6_PIT_MASK; // Enable module in system integration module register
  PIT_MCR &= ~PIT_MCR_FRZ_MASK; // Run timers when in debug mode (PIT_MCR = 0)
  PIT_MCR &= ~PIT_MCR_MDIS_MASK; // Enable clock for PIT

  NVICICPR2 |= NVIC_ICPR_CLRPEND(1 << 4); // Clear pending interrupts on PIT module
  NVICISER2 |= NVIC_ISER_SETENA(1 << 4); // Enable interrupts on PIT module
  return bTRUE;
}


void PIT_Set(const uint32_t period, const BOOL restart)
{
  if (restart)
  {
    PIT_Enable(bFALSE); // Disable PIT0
    PIT_LDVAL0 = ((period/1000) * ModuleClk_Global_MHz) -1; // Setup timer0 for 12500000 cycles
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
  (*PIT_Callback_Global)(UserArguments_Global); // PIT ISR callback function
}


/*!
 * @}
*/

