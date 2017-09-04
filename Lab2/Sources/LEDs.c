/*
 * LEDs.c
 *
 *  @brief Routines to access the LEDs on the TWR-K70F120M.
 *
 *  This contains the functions for operating the LEDs.
 *
 *  Created on: Apr 7, 2016
 *      Author: Manujaya Kankanige and Smit Patel (Group 7)
 */

#include "types.h"
#include "MK70F12.h"
#include "LEDs.h"


#define PIN_INITIALIZE 0x100
#define PORT_INITIALIZE 0x30000C00

BOOL LEDs_Init(void)
{
  SIM_SCGC5 |= SIM_SCGC5_PORTA_MASK; // Turn on portA
  PORTA_PCR10 |= PIN_INITIALIZE; // Set up pin 10 - blue
  PORTA_PCR11 |= PIN_INITIALIZE; // Set up pin 11 - orange
  PORTA_PCR29 |= PIN_INITIALIZE; // Set up pin 29 - green
  PORTA_PCR28 |= PIN_INITIALIZE; // Set up pin 28 - yellow


  GPIOA_PCOR |= PORT_INITIALIZE; // Initialize port to turn on LED
  GPIOA_PSOR |= PORT_INITIALIZE;
  GPIOA_PDDR |= PORT_INITIALIZE;
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




