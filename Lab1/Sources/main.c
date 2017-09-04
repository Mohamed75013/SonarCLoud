/* ###################################################################
**     Filename    : main.c
**     Project     : Lab1
**     Processor   : MK70FN1M0VMJ12
**     Version     : Driver 01.01
**     Compiler    : GNU C Compiler
**     Date/Time   : 2015-07-20, 13:27, # CodeGen: 0
**     Abstract    :
**         Main module.
**         This module contains user's application code.
**     Settings    :
**     Contents    :
**         No public methods
**
** ###################################################################*/
/*!
** @file main.c
** @version 1.0
** @brief
**         Main module.
**         This module contains user's application code.
*/         
/*!
**  @addtogroup main_module main module documentation
**  @{
*/         
/* MODULE main */


// CPU module - contains low level hardware initialization routines
#include "Cpu.h"
#include "UART.h"
#include "packet.h"
#include "FIFO.h"

#define BAUD_RATE 38400

//Prototypes
void InitialPackets(void);
void PacketHandler(void);


TFIFO TxFIFO, RxFIFO;
static uint8_t LSB = 0xBA; /*!< Least significant bits of tower number */
static uint8_t MSB = 0x03; /*!< Most significant bits of tower number */

/*lint -save  -e970 Disable MISRA rule (6.3) checking. */
int main(void)
/*lint -restore Enable MISRA rule (6.3) checking. */
{
  /* Write your local variable definition here */

  /*** Processor Expert internal initialization. DON'T REMOVE THIS CODE!!! ***/
  PE_low_level_init();
  /*** End of Processor Expert internal initialization.                    ***/

  /* Write your code here */

  Packet_Init(BAUD_RATE, CPU_BUS_CLK_HZ); // Initialization routine
  InitialPackets(); // Startup packets

  for (;;)
  {
    UART_Poll(); // Poll for RDRF or TDRE flag
    if (Packet_Get()) // Check for received packets from PC
      PacketHandler(); // Call packet handling function
  }

  /*** Don't write any code pass this line, or it will be deleted during code generation. ***/
  /*** RTOS startup code. Macro PEX_RTOS_START is defined by the RTOS component. DON'T MODIFY THIS CODE!!! ***/
  #ifdef PEX_RTOS_START
    PEX_RTOS_START();                  /* Startup of the selected RTOS. Macro is defined by the RTOS component. */
  #endif
  /*** End of RTOS startup code.  ***/
  /*** Processor Expert end of main routine. DON'T MODIFY THIS CODE!!! ***/
  for(;;){}
  /*** Processor Expert end of main routine. DON'T WRITE CODE BELOW!!! ***/
} /*** End of main routine. DO NOT MODIFY THIS TEXT!!! ***/

/* END main */


/*! @brief Initial routine where startup values listed below are sent
 *
 *  @return void
 */
void InitialPackets(void)
{
  Packet_Put(0x04,0x00,0x00,0x00); // Startup values
  Packet_Put(0x09,'v',1,0); // Version
  Packet_Put(0x0B,0x01,LSB,MSB); // Tower number
}

/*! @brief Handles received packets based on received command
 *
 *  @return void
 */
void PacketHandler(void)
{
  BOOL successful = bFALSE;

   switch (Packet_Command & 0x7F) // Check command excluding the acknowledgement bit
   {
     case 0x04: successful = Packet_Put(0x04,0x00,0x00,0x00) &
	                     Packet_Put(0x09,'v',1,0) &
 		             Packet_Put(0x0B,0x01,LSB,MSB); // Get startup values (startup values, version, tower number)
 		break;

     case 0x09: successful = Packet_Put(0x09,'v',1,0); // Get version
 		break;

     case 0x0B: if (Packet_Parameter1 == 0x01) // Selection to get tower number
		  successful = Packet_Put(0x0B,0x01,LSB,MSB); // Tower number
	        else if (Packet_Parameter1 == 0x02) // Selection to set tower number
	        {
		  LSB = Packet_Parameter2; // New least significant bits
		  MSB = Packet_Parameter3; // New most significant bits
	          successful = Packet_Put(0x0B,0x01,LSB,MSB); // New tower number
	        }
 		break;

     default:  break; // Command not recognized

   }

   if (Packet_Command & 0x80) // Check for acknowledgement request
   {
     if (!successful)
     {
       Packet_Command &= 0x7F; // Clear acknowledgement flag if packet handling was unsuccessful
     }
     Packet_Put(Packet_Command,Packet_Parameter1,Packet_Parameter2,Packet_Parameter3); // Send packet
   }

}

/*!
** @}
*/
/*
** ###################################################################
**
**     This file was created by Processor Expert 10.5 [05.21]
**     for the Freescale Kinetis series of microcontrollers.
**
** ###################################################################
*/
