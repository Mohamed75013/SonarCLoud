/*! @file
 *
 *  @brief Routines to access the LEDs on the TWR-K70F120M.
 *
 *  This contains the functions for operating the LEDs.
 *
 *  @author Manujaya Kankanige & Smit Patel
 *  @date 2016-04-07
 */

/*!
 *  @addtogroup leds_module LEDs module documentation
 *  @{
 */

// Included header files
#include "types.h"
#include "MK70F12.h"
#include "LEDs.h"

BOOL LEDs_Init(void)
{
  SIM_SCGC5 |= SIM_SCGC5_PORTA_MASK; // Enable clock gate for PortA to enable pin routing

  PORTA_PCR10 = PORT_PCR_MUX(1); // Set up pin 10 - blue
  PORTA_PCR11 = PORT_PCR_MUX(1); // Set up pin 11 - orange
  PORTA_PCR29 = PORT_PCR_MUX(1); // Set up pin 29 - green
  PORTA_PCR28 = PORT_PCR_MUX(1); // Set up pin 28 - yellow

  GPIOA_PDDR = LED_BLUE | LED_ORANGE | LED_GREEN | LED_YELLOW; // Configure pins for LED outputs
  GPIOA_PSOR = LED_BLUE | LED_ORANGE | LED_GREEN | LED_YELLOW; // Turn off all LEDs
}

/*! @brief Turns LED on
 *  @param color - color to turn on
 */
void LEDs_On(const TLED color)
{
  GPIOA_PCOR |= color;
}

/*! @brief Turns LED off
 *  @param color - color to turn off
 */
void LEDs_Off(const TLED color)
{
  GPIOA_PSOR |= color;
}


/*! @brief Inverts LED's state ( ON to OFF and vice versa )
 *  @param color - toggled color
 */
void LEDs_Toggle(const TLED color)
{
  GPIOA_PTOR |= color;
}


/*!
 * @}
*/


