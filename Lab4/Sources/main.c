/* ###################################################################
**     Filename    : main.c
**     Project     : Lab4
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
** @version 4.0
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

// CPU module - contains low level hardware initialization routines
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
#include "FTM.h"
#include "accel.h"

// Definitions
#define BAUD_RATE 115200

// Protocol packet definitions
#define CMD_STARTUP 0x04
#define CMD_WRITEBYTE 0x07
#define CMD_READBYTE 0x08
#define CMD_TWRVERSION 0x09
#define CMD_PROTOCOL 0x0A
#define CMD_TWRNUMBER 0x0B
#define CMD_TIME 0x0C
#define CMD_TWRMODE 0x0D
#define CMD_ACCELVALUES 0x10
#define ACK_REQUEST_MASK 0x80

//Prototypes
void InitialPackets(void);
void PacketHandler(void);
void ReadCompleteCallback(void* arg);
void DataReadyCallback(void* arg);
void PITCallback(void* arg);
void RTCCallback(void* arg);
void FTM0Callback(const TFTMChannel* const aFTMChannel);


// Variable Declarations
TPacket Packet;                                 /*!< Initial packet created */
volatile TFIFO TxFIFO, RxFIFO;                  /*!< Initial FIFOs created */
volatile TAccelMode Protocol_Mode = ACCEL_POLL; /*!< Initial protocol mode selected */

static TFTMChannel FTM_Timer = {0, CPU_MCGFF_CLK_HZ_CONFIG_0, TIMER_FUNCTION_OUTPUT_COMPARE, TIMER_OUTPUT_HIGH, (void* )&FTM0Callback, &FTM_Timer}; // 1 second timer on FTM0 channel 0
static TAccelSetup Accelerometer = {CPU_BUS_CLK_HZ, (void* )&DataReadyCallback, &Accelerometer, (void* )ReadCompleteCallback, NULL}; // Accelerometer initial content

volatile uint16union_t* NvTowerMode;   /*!< Pointer to tower mode */
volatile uint16union_t* NvTowerNumber; /*!< Pointer to tower number */

static uint16_t TowerMode = 1;     /*!< Initial tower mode */
static uint16_t TowerNumber = 954; /*!< Initial tower number, last 4 digits of student number (0x03BA) */

static TAccelData accelerometerValues; /*!< Array to store accelerometer values */

/*lint -save  -e970 Disable MISRA rule (6.3) checking. */
int main(void)
/*lint -restore Enable MISRA rule (6.3) checking. */
{
  /* Write your local variable definition here */

  /*** Processor Expert internal initialization. DON'T REMOVE THIS CODE!!! ***/
  PE_low_level_init();
  /*** End of Processor Expert internal initialization.                    ***/

  /* Write your code here */

  __DI(); // Disable interrupts

  LEDs_Init(); // Initialize LED ports

  if (Packet_Init(BAUD_RATE, CPU_BUS_CLK_HZ) && Flash_Init()) // UART and flash initialization
    LEDs_On(LED_ORANGE); // Turn on Orange LED

  if (Flash_AllocateVar((void* )&NvTowerNumber, sizeof(*NvTowerNumber))) // Allocate flash memory
    Flash_Write16((uint16_t* )NvTowerNumber,TowerNumber); // Program initial tower number to flash

  if (Flash_AllocateVar((void* )&NvTowerMode, sizeof(*NvTowerMode))) // Allocate flash memory
    Flash_Write16((uint16_t* )NvTowerMode,TowerMode); // Program initial tower mode to flash

  RTC_Init(&RTCCallback,NULL); // Initialize RTC
  RTC_Set(0,0,0); // Initialize time on tower

  PIT_Init(CPU_BUS_CLK_HZ,&PITCallback,NULL); // Initialize PIT0
  PIT_Set(500000000*2,bTRUE); // Store PIT0 with a period of 1 second

  FTM_Init(); // Initialize FTM
  FTM_Set(&FTM_Timer); // Setup FTM0 timer

  Accel_Init(&Accelerometer); // Initialize accelerometer
  Accel_SetMode(ACCEL_POLL); // Set initial mode on accelerometer

  __EI(); // Enable interrupts

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
  Packet_Put(CMD_PROTOCOL,1,Protocol_Mode,0); // Protocol mode
}

/*! @brief Handles received packets based on received command
 *
 *  @return void
 */
void PacketHandler(void)
{
  BOOL success = bFALSE; /*!< Initially success flag set to false */

  LEDs_On(LED_BLUE); // Turn on blue LED

  FTM_Timer.ioType.inputDetection = TIMER_OUTPUT_HIGH; // Start FTM0 timer
  FTM_StartTimer(&FTM_Timer);

  switch (Packet_Command & ~ACK_REQUEST_MASK) // Check command excluding the ACK bit
  {
    case CMD_STARTUP: // Command 0x04 : special - get startup values
      success = Packet_Put(CMD_STARTUP,0,0,0) &  // Get startup values (startup values, version, tower number)
      Packet_Put(CMD_TWRVERSION,'v',1,0) &
      Packet_Put(CMD_TWRNUMBER,1,NvTowerNumber->s.Lo,NvTowerNumber->s.Hi) &
      Packet_Put(CMD_TWRMODE,1,NvTowerMode->s.Lo,NvTowerMode->s.Hi);
      Packet_Put(CMD_PROTOCOL,1,Protocol_Mode,0); // Protocol mode
      break;

    case CMD_WRITEBYTE: // Command 0x07 : flash - program byte
      if (Packet_Parameter1 > 7) // If offset is greater than sector range, erase flash
        success = Flash_Erase();
      else
	success = Flash_Write8((uint8_t* )(FLASH_DATA_START + Packet_Parameter1),Packet_Parameter3); // Program received data to flash memory location given by offset
      break;

    case CMD_READBYTE: // Command 0x08 : flash - read byte
      success = Packet_Put(Packet_Parameter1,0,0,_FB(FLASH_DATA_START + Packet_Parameter1)); // Read byte from flash memory location given by offset
      break;

    case CMD_TWRVERSION: // Command 0x09 : special - get version
      success = Packet_Put(CMD_TWRVERSION,'v',1,0); // Get version
      break;

    case CMD_PROTOCOL: // Command 0x0A : protocol - get or set mode - synchronous or asynchronous
      if (Packet_Parameter1 == 1) // Selection to get protocol mode
        success = Packet_Put(CMD_PROTOCOL,1,Protocol_Mode,0); // Protocol mode
      else if (Packet_Parameter1 == 2) // Selection to set protocol mode
      {
 	if (Packet_Parameter2 == 0) // Selection for asynchronous mode
 	{
 	  Protocol_Mode = ACCEL_POLL;
 	  success = Packet_Put(CMD_PROTOCOL,1,Protocol_Mode,0); // Protocol mode
 	  Accel_SetMode(ACCEL_POLL); // Set accelerometer for polling method
 	}
 	else if (Packet_Parameter2 == 1) // Selection for synchronous mode
 	{
 	  Protocol_Mode = ACCEL_INT;
 	  success = Packet_Put(CMD_PROTOCOL,1,Protocol_Mode,0); // Protocol mode
 	  Accel_SetMode(ACCEL_INT); // Set accelerometer for interrupt method
 	}
      }
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

  if (Packet_Command & ACK_REQUEST_MASK) // Check for ACK request
  {
    if (!success)
      Packet_Command &= ~ACK_REQUEST_MASK; // Clear ACK flag if packet handling was unsuccessful

    Packet_Put(Packet_Command,Packet_Parameter1,Packet_Parameter2,Packet_Parameter3); // Send packet
  }
}


/*! @brief I2C read complete callback function
 *
 *  @param arg Pointer to the user argument to use with the user callback function
 *  @note Assumes I2C interrupt has occurred
 */
void ReadCompleteCallback(void* arg)
{
  // Send accelerometer data at 1.56Hz
  Packet_Put(CMD_ACCELVALUES,accelerometerValues.bytes[0],accelerometerValues.bytes[1],accelerometerValues.bytes[2]);
}


/*! @brief User callback function for accelerometer when new data is available
 *
 *  @param arg Pointer to the user argument to use with the user callback function
 *  @note Assumes data ready interrupt has occurred
 */
void DataReadyCallback(void* arg)
{
  Accel_ReadXYZ(accelerometerValues.bytes); // Collect accelerometer data
  LEDs_Toggle(LED_GREEN); // Turn on green LED
}


/*! @brief User callback function for PIT
 *
 *  @param arg Pointer to the user argument to use with the user callback function
 *  @note Assumes PIT interrupt has occurred
 */
void PITCallback(void* arg)
{
  static TAccelData lastAccelerometerValues; /*!< Array to store previous accelerometer data */
  uint8_t axisCount; /*!< Variables to store axis number */

  if (PIT_TFLG0 & PIT_TFLG_TIF_MASK) // Check if timeout has occurred (1 second) - 1Hz frequency
  {
    PIT_TFLG0 |= PIT_TFLG_TIF_MASK; // Clear timer interrupt flag
    if (Protocol_Mode == ACCEL_POLL) // Only read accelerometer if in polling mode
    {
      Accel_ReadXYZ(accelerometerValues.bytes); // Collect accelerometer data
      LEDs_Toggle(LED_GREEN); // Toggle green LED

      // Send accelerometer data every second only if there is a difference from last time
      if ((lastAccelerometerValues.bytes[0] != accelerometerValues.bytes[0]) ||
          (lastAccelerometerValues.bytes[1] != accelerometerValues.bytes[1]) ||
          (lastAccelerometerValues.bytes[2] != accelerometerValues.bytes[2]))
        Packet_Put(CMD_ACCELVALUES,accelerometerValues.bytes[0],accelerometerValues.bytes[1],accelerometerValues.bytes[2]);

      for (axisCount=0; axisCount < 3; axisCount++) // Transfer data from new data array to old data array
      {
        lastAccelerometerValues.bytes[axisCount] = accelerometerValues.bytes[axisCount];
      }
    }
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
 *  @param aFTMChannel Structure containing the parameters used for setting up the timer channel
 *  @note Assumes FTM0 interrupt has occurred
 */
void FTM0Callback(const TFTMChannel* const aFTMChannel)
{
  if (FTM0_CnSC(aFTMChannel->channelNb) & FTM_CnSC_CHF_MASK) // Check channel flag to see if an event occurred
  {
    FTM_Timer.ioType.inputDetection = TIMER_OUTPUT_DISCONNECT; // Stop timer interrupts
    FTM_StartTimer(&FTM_Timer); // Update timer setting
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
