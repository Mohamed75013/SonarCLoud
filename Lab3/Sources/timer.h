/*! @file
 *
 *  @brief Routines for setting up the flexible timer module (FTM) on the TWR-K70F120M.
 *
 *  This contains the functions for operating the flexible timer module (FTM).
 *
 *  @author PMcL
 *  @date 2015-09-04
 */

#ifndef TIMER_H
#define TIMER_H

// new types
#include "types.h"

typedef enum
{
  TIMER_FUNCTION_INPUT_CAPTURE,
  TIMER_FUNCTION_OUTPUT_COMPARE
} TTimerFunction;

typedef enum
{
  TIMER_OUTPUT_DISCONNECT,
  TIMER_OUTPUT_TOGGLE,
  TIMER_OUTPUT_LOW,
  TIMER_OUTPUT_HIGH
} TTimerOutputAction;

typedef enum
{
  TIMER_INPUT_OFF,
  TIMER_INPUT_RISING,
  TIMER_INPUT_FALLING,
  TIMER_INPUT_ANY
} TTimerInputDetection;

typedef struct
{
  uint8_t channelNb;
  uint16_t initialCount;
  TTimerFunction timerFunction;
  union
  {
    TTimerOutputAction outputAction;
    TTimerInputDetection inputDetection;
  } ioType;
  void (*userFunction)(void*);
  void *userArguments;
} TTimer;


/*! @brief Sets up the FTM before first use.
 *
 *  Enables the FTM as a free running 16-bit counter.
  *  @return BOOL - TRUE if the FTM was successfully initialized.
 */
BOOL Timer_Init();

/*! @brief Sets up a timer channel.
 *
 *  @param aTimer is a structure containing the parameters to be used in setting up the timer channel.
 *    channelNb is the channel number of the FTM to use.
 *    initialCount is the compare value for an output compare event.
 *    timerFunction is used to set the timer up as either an input capture or an output compare.
 *    ioType is a union that depends on the setting of the channel as input capture or output compare:
 *      outputAction is the action to take on a successful output compare.
 *      inputDetection is the type of input capture detection.
 *    userFunction is a pointer to a user callback function.
 *    userArguments is a pointer to the user arguments to use with the user callback function.
 *  @return BOOL - TRUE if the timer was set up successfully.
 *  @note Assumes the FTM has been initialized.
 */
BOOL Timer_Set(const TTimer* const aTimer);


/*! @brief Starts a timer if set up for output compare.
 *
 *  @param aTimer is a structure containing the parameters to be used in setting up the timer channel.
 *  @return BOOL - TRUE if the timer was started successfully.
 *  @note Assumes the FTM has been initialized.
 */
BOOL Timer_Start(const TTimer* const aTimer);


/*! @brief Interrupt service routine for the FTM.
 *
 *  If a timer channel was set up as output compare, then the user callback function will be called.
 *  @note Assumes the FTM has been initialized.
 */
void __attribute__ ((interrupt)) FTM0_ISR(void);

#endif
