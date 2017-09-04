/*! @file
 *
 *  @brief Routines for setting up the FlexTimer module (FTM) on the TWR-K70F120M.
 *
 *  This contains the functions for operating the FlexTimer module (FTM).
 *
 *  @author Manujaya Kankanige & Smit Patel
 *  @date 2016-05-10
 */

/*!
 *  @addtogroup ftm_module FTM module documentation
 *  @{
 */

#include "types.h"
#include "IO_Map.h"
#include "FTM.h"

static void* UserArgumentsGlobal;           /*!< Private global pointer to the user arguments to use with the user callback function */
static void (*FTM0CallbackGlobal)(void *); /*!< Private global pointer to FTM user callback function */


BOOL FTM_Init()
{
  SIM_SCGC6 |= SIM_SCGC6_FTM0_MASK; // Enable clock gate for FTM0 module

  FTM0_CNTIN = ~FTM_CNTIN_INIT_MASK; // Ensure the counter is a free running counter
  FTM0_MOD = FTM_MOD_MOD_MASK;
  FTM0_CNT = ~FTM_CNT_COUNT_MASK;

  FTM0_SC |= FTM_SC_CLKS(2); // Use system fixed frequency clock for the counter (0x10)

  NVICICPR1 |= NVIC_ICPR_CLRPEND(1 << 30); // Clear pending interrupts on FTM0 module
  NVICISER1 |= NVIC_ISER_SETENA(1 << 30); // Enable interrupts on FTM0 module

  return bTRUE; // FTM initialization complete
}


BOOL FTM_Set(const TFTMChannel* const aFTMChannel)
{
  if (aFTMChannel->timerFunction == TIMER_FUNCTION_INPUT_CAPTURE)
    FTM0_CnSC(aFTMChannel->channelNb) &= ~(FTM_CnSC_MSB_MASK | FTM_CnSC_MSA_MASK); // Set function for input capture(00)

  else // timerFunction is TIMER_FUNCTION_OUTPUT_CAPTURE
  {
    FTM0_CnSC(aFTMChannel->channelNb) &= ~FTM_CnSC_MSB_MASK; // Set function for output compare(01)
    FTM0_CnSC(aFTMChannel->channelNb) |= FTM_CnSC_MSA_MASK;
  }

  switch (aFTMChannel->ioType.inputDetection)
  {
    case 1: // inputDetection is TIMER_INPUT_RISING
      FTM0_CnSC(aFTMChannel->channelNb) &= ~FTM_CnSC_ELSB_MASK; // Set function for rising edge only(01)
      FTM0_CnSC(aFTMChannel->channelNb) |= FTM_CnSC_ELSA_MASK;
      break;

    case 2: // inputDetection is TIMER_INPUT_FALLING
      FTM0_CnSC(aFTMChannel->channelNb) |= FTM_CnSC_ELSB_MASK; // Set function for falling edge only(10)
      FTM0_CnSC(aFTMChannel->channelNb) &= ~FTM_CnSC_ELSA_MASK;
      break;

    case 3: // inputDetection is TIMER_INPUT_ANY
      FTM0_CnSC(aFTMChannel->channelNb) |= FTM_CnSC_ELSB_MASK | FTM_CnSC_ELSA_MASK; // Set function for any edge(11)
      break;

    default:
      FTM0_CnSC(aFTMChannel->channelNb) &= ~(FTM_CnSC_ELSB_MASK | FTM_CnSC_ELSA_MASK); // Unused input capture(00)
      break;
  }
  UserArgumentsGlobal = aFTMChannel->userArguments; // userArguments made globally(private) accessible
  FTM0CallbackGlobal = aFTMChannel->userFunction; // userFunction made globally(private) accessible
}


BOOL FTM_StartTimer(const TFTMChannel* const aFTMChannel)
{
  if (aFTMChannel->channelNb < 8)  // Check for valid channel
  {
    if (aFTMChannel->timerFunction == TIMER_FUNCTION_OUTPUT_COMPARE) // Only start for output compare mode
    {
      FTM0_CnSC(aFTMChannel->channelNb) &= ~FTM_CnSC_CHF_MASK; // Clear channel flag
      FTM0_CnSC(aFTMChannel->channelNb) |= FTM_CnSC_CHIE_MASK; // Enable channel interrupts
      FTM0_CnV(aFTMChannel->channelNb) =  FTM0_CNT + (aFTMChannel->delayCount); // Set initial count

      return bTRUE; // Timer start successful
    }
  }
  return bFALSE; // Invalid channel, timer failed to start
}


void __attribute__ ((interrupt)) FTM0_ISR(void)
{
  (*FTM0CallbackGlobal)(UserArgumentsGlobal); // FTM ISR callback function
}


/*!
 * @}
*/
