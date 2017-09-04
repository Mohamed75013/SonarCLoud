/* ###################################################################
**     Filename    : main.c
**     Project     : Lab3
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
** @version 3.0
** @brief
**         Main module.
**         This module contains user's application code.
** @author Manujaya Kankanige & Smit Patel
** @date 2016-03-26
*/

/*!
**  @addtogroup main_module main module documentation
**  @{
*/         
/* MODULE main */


// Included header files
#include "Cpu.h"
#include "types.h"
#include "PE_Types.h"
#include "PE_Error.h"
#include "PE_Const.h"
#include "IO_Map.h"
#include "UART.h"
#include "packet.h"
#include "FIFO.h"
#include "Flash.h"
#include "LEDs.h"
#include "PIT.h"
#include "RTC.h"
#include "timer.h"

// Definitions
#define BAUD_RATE 115200

// Protocol packet definitions
#define CMD_STARTUP 0x04
#define CMD_WRITEBYTE 0x07
#define CMD_READBYTE 0x08
#define CMD_TWRVERSION 0x09
#define CMD_TWRNUMBER 0x0B
#define CMD_TIME 0x0C
#define CMD_TWRMODE 0x0D
#define ACK_REQUEST_MASK 0x80

//Prototypes
void InitialPackets(void);
void PacketHandler(void);
void PITCallback(void* arg);
void RTCCallback(void* arg);
void FTM0Callback(const TTimer* const aTimer);

// Variable Declarations
TFIFO TxFIFO, RxFIFO;
TPacket Packet;

// 1 second timer on FTM0 channel 0
TTimer FTM_Timer = {0, CPU_MCGFF_CLK_HZ_CONFIG_0, TIMER_FUNCTION_OUTPUT_COMPARE, TIMER_OUTPUT_HIGH, (void* )&FTM0Callback, &FTM_Timer};

volatile uint16union_t* NvTowerMode;   /*!< Pointer to tower mode */
volatile uint16union_t* NvTowerNumber; /*!< Pointer to tower number */

static uint16_t TowerMode = 1;     /*!< Initial tower mode */
static uint16_t TowerNumber = 954; /*!< Initial tower number, last 4 digits of student number (0x03BA) */


/*lint -save  -e970 Disable MISRA rule (6.3) checking. */
int main(void)
/*lint -restore Enable MISRA rule (6.3) checking. */
{
  /* Write your local variable definition here */

  /*** Processor Expert internal initialization. DON'T REMOVE THIS CODE!!! ***/
  PE_low_level_init();
  /*** End of Processor Expert internal initialization.                    ***/

  /* Write your code here */

  EnterCritical(); // Enter critical initialization section

  LEDs_Init(); // Initialize LED ports

  if (Packet_Init(BAUD_RATE, CPU_BUS_CLK_HZ) && Flash_Init()) // UART and flash initialization
    LEDs_On(LED_ORANGE); // Turn on Orange LED

  if (Flash_AllocateVar((void* )&NvTowerNumber, sizeof(*NvTowerNumber))) // Allocate flash memory
    Flash_Write16((uint16_t* )NvTowerNumber,TowerNumber); // Program initial tower number to flash

  if (Flash_AllocateVar((void* )&NvTowerMode, sizeof(*NvTowerMode))) // Allocate flash memory
    Flash_Write16((uint16_t* )NvTowerMode,TowerMode); // Program initial tower mode to flash

  RTC_Init(&RTCCallback,NULL); // Initialize RTC

  PIT_Init(CPU_BUS_CLK_HZ,&PITCallback,NULL); // Initialize PIT0
  PIT_Set(500000000,bTRUE); // Store PIT0 with a period of 0.5 seconds

  Timer_Init(); // Initialize FTM
  Timer_Set(&FTM_Timer); // Setup FTM0 timer

  ExitCritical(); // Exit critical initialization section

  InitialPackets(); // Startup packets

  for (;;)
  {
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
  Packet_Put(CMD_STARTUP,0,0,0); // Startup values
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
  BOOL success = bFALSE; // Initially success flag set to false

  LEDs_On(LED_BLUE); // Turn on blue LED
  FTM_Timer.ioType.inputDetection = TIMER_OUTPUT_HIGH; // Start FTM0 timer
  Timer_Start(&FTM_Timer);

  switch (Packet_Command & 0x7F) // Check command excluding the acknowledgement bit
  {
    case CMD_STARTUP: // Command 0x04 : special - get startup values
         success = Packet_Put(CMD_STARTUP,0,0,0) &  // Get startup values (startup values, version, tower number)
	           Packet_Put(CMD_TWRVERSION,'v',1,0) &
 	           Packet_Put(CMD_TWRNUMBER,1,NvTowerNumber->s.Lo,NvTowerNumber->s.Hi) &
	           Packet_Put(CMD_TWRMODE,1,NvTowerMode->s.Lo,NvTowerMode->s.Hi);
 	 break;

    case CMD_WRITEBYTE: // Command 0x07 : flash - program byte
	 if (Packet_Parameter1 > 7) // If offset is greater than sector range, initialize flash
	   Flash_Erase();
	 else
	   success = Flash_Write8((uint8_t* )(FLASH_DATA_START + Packet_Parameter1),Packet_Parameter3); // Program received data to flash memory location given by offset
         break;

    case CMD_READBYTE: // Command 0x08 : flash - read byte
         success = Packet_Put(CMD_READBYTE,Packet_Parameter1,0,_FB(FLASH_DATA_START + Packet_Parameter1)); // Read byte from flash memory location given by offset
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

    case CMD_TIME: // Command 0x0C : set time
         RTC_Set(Packet_Parameter1,Packet_Parameter2,Packet_Parameter3);
         success = bTRUE;
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

  if (Packet_Command & ACK_REQUEST_MASK) // Check for acknowledgement request
  {
    if (!success)
      Packet_Command &= ~ACK_REQUEST_MASK; // Clear acknowledgement flag if packet handling was unsuccessful

    Packet_Put(Packet_Command,Packet_Parameter1,Packet_Parameter2,Packet_Parameter3); // Send packet
  }

}


/*! @brief User callback function for PIT
 *
 *  @param arg Pointer to the user argument to use with the user callback function
 *  @note Assumes PIT interrupt has occurred
 */
void PITCallback(void* arg)
{
  if (PIT_TFLG0 & PIT_TFLG_TIF_MASK) // Check if timeout has occurred (500 ms)
  {
    PIT_TFLG0 |= PIT_TFLG_TIF_MASK; // Clear timer interrupt flag
    LEDs_Toggle(LED_GREEN); // Toggle green LED
  }
}


/*! @brief User callback function for RTC
 *
 *  @param arg Pointer to the user argument to use with the user callback function
 *  @note Assumes RTC interrupt has occurred
 */
void RTCCallback(void* arg)
{
  uint8_t hours, minutes, seconds; /*!< Variables to store current time */

  RTC_Get(&hours,&minutes,&seconds); // Get current time each second
  Packet_Put(CMD_TIME,hours,minutes,seconds); // Update time in PC
  LEDs_Toggle(LED_YELLOW); // Toggle yellow LED
}


/*! @brief User callback function for FTM0
 *
 *  @param aTimer Structure containing the parameters used for setting up the timer channel
 *  @note Assumes FTM0 interrupt has occurred
 */
void FTM0Callback(const TTimer* const aTimer)
{
  if (FTM0_CnSC(aTimer->channelNb) & FTM_CnSC_CHF_MASK) // Check channel flag to see if and event occurred
  {
    FTM_Timer.ioType.inputDetection = TIMER_OUTPUT_DISCONNECT; // Stop timer interrupts
    Timer_Start(&FTM_Timer); // Update timer setting
    LEDs_Off(LED_BLUE); // Turn off blue LED
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
