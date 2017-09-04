/* ###################################################################
**     Filename    : main.c
**     Project     : Lab2
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
** @version 2.0
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
#include "Flash.h"
#include "LEDs.h"
#include "PE_Types.h"
#include "PE_Error.h"
#include "PE_Const.h"
#include "IO_Map.h"

#define BAUD_RATE 115200

#define CMD_STARTUP 0x04
#define CMD_TWRVERSION 0x09
#define CMD_TWRNUMBER 0x0B
#define CMD_WRITEBYTE 0x07
#define CMD_READBYTE 0x08
#define CMD_TWRMODE 0x0D

//Prototypes
void InitialPackets(void);
void PacketHandler(void);

// Variable Declarations
TFIFO TxFIFO, RxFIFO;
TPacket Packet;

volatile uint16union_t* NvTowerMode; // Pointer to tower mode
volatile uint16union_t* NvTowerNumber; // Pointer to tower number

static uint16_t TowerMode = 1; // Initial tower mode
static uint16_t TowerNumber = 954; // Initial tower number, last 4 digits of student number (0x03BA)


/*lint -save  -e970 Disable MISRA rule (6.3) checking. */
int main(void)
/*lint -restore Enable MISRA rule (6.3) checking. */
{
  /* Write your local variable definition here */

  /*** Processor Expert internal initialization. DON'T REMOVE THIS CODE!!! ***/
  PE_low_level_init();
  /*** End of Processor Expert internal initialization.                    ***/

  /* Write your code here */

  LEDs_Init(); // Initialize LED ports

  if (Packet_Init(BAUD_RATE, CPU_BUS_CLK_HZ) && Flash_Init()) // UART and flash initialization
    LEDs_On(LED_ORANGE); // Turn on Orange LED

  if (Flash_AllocateVar((void* )&NvTowerNumber, sizeof(*NvTowerNumber))) // Allocate flash memory
    Flash_Write16((uint16_t* )NvTowerNumber,TowerNumber); // Program initial tower number to flash

  if (Flash_AllocateVar((void* )&NvTowerMode, sizeof(*NvTowerMode))) // Allocate flash memory
    Flash_Write16((uint16_t* )NvTowerMode,TowerMode); // Program initial tower mode to flash

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
  Packet_Put(CMD_STARTUP, 0,0,0); // Startup values
  Packet_Put(CMD_TWRVERSION,'v',1,0); // Version
  Packet_Put(CMD_TWRNUMBER,1,NvTowerNumber->s.Lo,NvTowerNumber->s.Hi); // Tower number
  Packet_Put(CMD_TWRMODE,1,NvTowerMode->s.Lo,NvTowerMode->s.Hi); // Tower mode
}

/*! @brief Handles received packets based on received command
 *
 *  @return void
 */
void PacketHandler(void)
{
  BOOL success = bFALSE;

  switch (Packet_Command & 0x7F) // Check command excluding the acknowledgement bit
  {
    case CMD_STARTUP: // Command 0x04 : special - get startup values
                success = Packet_Put(CMD_STARTUP, 0,0,0) &  // Get startup values (startup values, version, tower number, tower mode)
	                  Packet_Put(CMD_TWRVERSION,'v',1,0) &
 	                  Packet_Put(CMD_TWRNUMBER,1,NvTowerNumber->s.Lo,NvTowerNumber->s.Hi) &
			  Packet_Put(CMD_TWRMODE,1,NvTowerMode->s.Lo,NvTowerMode->s.Hi);
 		break;

    case CMD_TWRVERSION: // Command 0x09 : special - get version
                success = Packet_Put(CMD_TWRVERSION,'v',1,0); // Get version
 		break;

    case CMD_TWRNUMBER: // Command 0x0B : special - get or set tower number
                if (Packet_Parameter1 == 1) // Selection to get tower number
		  success = Packet_Put(CMD_TWRNUMBER,1,NvTowerNumber->s.Lo,NvTowerNumber->s.Hi); // Tower number
	        else if (Packet_Parameter1 == 2) // Selection to set tower number
	        {
	          if (Flash_AllocateVar((void* )&NvTowerNumber, sizeof(*NvTowerNumber))) // Allocate flash memory to store new tower number
	            success = Flash_Write16((uint16_t* )NvTowerNumber, Packet_Parameter23); // Program new tower number to flash memory
	        }
 		break;

    case CMD_WRITEBYTE: // Command 0x07 : flash - program byte
		if (Packet_Parameter1 > 7) // If offset is greater than sector range, initialize flash
		  Flash_Erase();
		else
	          success = Flash_Write8((uint8_t* )(FLASH_DATA_START + Packet_Parameter1),Packet_Parameter3); // Program received data to flash memory location given by offset
     		break;

    case CMD_READBYTE: // Command 0x08 : flash - read byte
                success = Packet_Put(CMD_READBYTE,Packet_Parameter1,0,FlashRead8(Packet_Parameter1)); // Read byte from flash memory location given by offset
     	        break;

    case CMD_TWRMODE: // Command 0x0D : get or set tower mode
                if (Packet_Parameter1 == 1) // Selection to get tower mode
                  success = Packet_Put(CMD_TWRMODE,1,NvTowerMode->s.Lo,NvTowerMode->s.Hi);
                else if (Packet_Parameter1 == 2) // Selection to set tower mode
                {
                  if (Flash_AllocateVar((void* )&NvTowerMode, sizeof(*NvTowerMode))) // Allocate flash memory to store new tower mode
                    success = Flash_Write16((uint16_t* )NvTowerMode, Packet_Parameter23); // Program new tower mode to flash memory
                }
 	        break;

    default: // Command not recognized
                break;

  }

  if (Packet_Command & 0x80) // Check for acknowledgement request
  {
    if (!success)
       Packet_Command &= 0x7F; // Clear acknowledgement flag if packet handling was unsuccessful

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
