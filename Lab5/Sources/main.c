/*!
** @file
** @version 1.0
** @brief  Main module.
**
**   This file contains the high-level code for the project.
**   It initialises appropriate hardware subsystems,
**   creates application threads, and then starts the OS.
**
**   An example of two threads communicating via a semaphore
**   is given that flashes the orange LED. These should be removed
**   when the use of threads and the RTOS is understood.
*/         
/*!
**  @addtogroup main_module main module documentation
**  @{
*/         
/* MODULE main */


// CPU module - contains low level hardware initialization routines
#include "Cpu.h"
#include "OS.h"
#include "types.h"
#include "PE_Types.h"
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

// Arbitrary thread stack size - big enough for stacking of interrupts and OS use.
#define THREAD_STACK_SIZE 100

// Baud rate defined
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
static void InitThread(void* pData);
static void PacketCheckerThread(void* pData);
static void AccelReadyThread(void* pData);
static void I2CReadCompleteThread(void* pData);
static void RTCThread(void* pData);
static void PITThread(void* pData);
static void PacketHandler(void);
static void InitialPackets(void);
void FTM0Callback(const TFTMChannel* const aFTMChannel);


// Variable declarations
TPacket Packet;                        /*!< Initial packet created */
volatile TFIFO TxFIFO, RxFIFO;         /*!< Initial FIFOs created */
volatile TAccelMode Protocol_Mode;     /*!< Initial protocol mode selected */

static TFTMChannel FTMTimer0;          /*!< Stores content of 1 second timer on FTM0 channel 0 */

volatile uint16union_t* NvTowerMode;   /*!< Pointer to tower mode */
volatile uint16union_t* NvTowerNumber; /*!< Pointer to tower number */

static uint16_t TowerMode = 1;         /*!< Initial tower mode */
static uint16_t TowerNumber = 954;     /*!< Initial tower number, last 4 digits of student number (0x03BA) */

static TAccelData accelerometerValues; /*!< Array to store accelerometer values */

static uint32_t InitThreadStack[THREAD_STACK_SIZE] __attribute__ ((aligned(0x08)));            /*!< The stack for the initialization thread. */
static uint32_t PacketCheckerThreadStack[THREAD_STACK_SIZE] __attribute__ ((aligned(0x08)));   /*!< The stack for the packet checking thread. */
static uint32_t AccelReadyThreadStack[THREAD_STACK_SIZE] __attribute__ ((aligned(0x08)));      /*!< The stack for the accelerometer data ready thread. */
static uint32_t I2CReadCompleteThreadStack[THREAD_STACK_SIZE] __attribute__ ((aligned(0x08))); /*!< The stack for the I2C read complete thread. */
static uint32_t RTCThreadStack[THREAD_STACK_SIZE] __attribute__ ((aligned(0x08)));             /*!< The stack for the RTC thread. */
static uint32_t PITThreadStack[THREAD_STACK_SIZE] __attribute__ ((aligned(0x08)));             /*!< The stack for the PIT thread. */

// ----------------------------------------
// Global Semaphores
// ----------------------------------------

OS_ECB *Data_Ready_Semaphore;    /*!< Binary semaphore for signaling that data is ready to be read on accelerometer */
OS_ECB *Read_Complete_Semaphore; /*!< Binary semaphore for signaling that data was read successfully */
OS_ECB *Update_Clock_Semaphore;  /*!< Binary semaphore for signaling RTC update */
OS_ECB *PIT_Semaphore;           /*!< Binary semaphore for signaling PIT interrupt */


/*! @brief Waits for a signal to turns the blue LED on, then waits a half a second, then signals for the blue LED to be turned off.
 *
 *  @param pData is not used but is required by the OS to create a thread.
 *  @note Assumes that LEDInit has been called successfully.
 */
static void InitThread(void* pData)
{
  for (;;)
  {
    OS_DisableInterrupts(); // Disable interrupts

    /*!< 1 second timer on FTM0 channel 0 setup */
    FTMTimer0.channelNb = 0;
    FTMTimer0.delayCount =  CPU_MCGFF_CLK_HZ_CONFIG_0;
    FTMTimer0.timerFunction = TIMER_FUNCTION_OUTPUT_COMPARE;
    FTMTimer0.ioType.outputAction = TIMER_OUTPUT_HIGH;
    FTMTimer0.userArguments =  &FTMTimer0;
    FTMTimer0.userFunction = (void* )&FTM0Callback;

    LEDs_Init(); // Initialize LED ports

    if (Packet_Init(BAUD_RATE, CPU_BUS_CLK_HZ) && Flash_Init()) // UART and flash initialization
      LEDs_On(LED_ORANGE); // Turn on Orange LED

    if (Flash_AllocateVar((void* )&NvTowerNumber, sizeof(*NvTowerNumber))) // Allocate flash memory
      Flash_Write16((uint16_t* )NvTowerNumber,TowerNumber); // Program initial tower number to flash

    if (Flash_AllocateVar((void* )&NvTowerMode, sizeof(*NvTowerMode))) // Allocate flash memory
      Flash_Write16((uint16_t* )NvTowerMode,TowerMode); // Program initial tower mode to flash

    RTC_Init(); // Initialize RTC
    RTC_Set(0,0,0); // Initialize time on tower

    PIT_Init(CPU_BUS_CLK_HZ); // Initialize PIT0
    PIT_Set(500000000*2,bTRUE); // Set PIT0 to a period of 1 second

    FTM_Init(); // Initialize FTM
    FTM_Set(&FTMTimer0); // Setup FTM0 timer - channel 0

    Accel_Init(); // Initialize accelerometer
    Accel_SetMode(ACCEL_POLL); // Set initial mode on accelerometer

    OS_EnableInterrupts(); // Enable interrupts

    InitialPackets(); // Startup packets

    OS_ThreadDelete(OS_PRIORITY_SELF); // Thread not accessed again
  }
}


/*! @brief Initial routine where startup values listed below are sent
 *
 *  @return void
 */
static void InitialPackets(void)
{
  Packet_Put(CMD_STARTUP,0,0,0); // Startup values
  Packet_Put(CMD_TWRVERSION,'v',1,0); // Version
  Packet_Put(CMD_TWRNUMBER,1,NvTowerNumber->s.Lo,NvTowerNumber->s.Hi); // Tower number
  Packet_Put(CMD_TWRMODE,1,NvTowerMode->s.Lo,NvTowerMode->s.Hi); // Tower mode
  Packet_Put(CMD_PROTOCOL,1,Protocol_Mode,0); // Protocol mode
}


static void PacketCheckerThread(void* pData)
{
  for (;;)
  {
    if (Packet_Get()) // Check for received packets from PC
      PacketHandler(); // Handle received packet
  }
}


/*! @brief Handles received packets based on received command
 *
 *  @return void
 */
static void PacketHandler(void)
{
  BOOL success = bFALSE; /*!< Initially success flag set to false */

  LEDs_On(LED_BLUE); // Turn on blue LED

  FTMTimer0.ioType.inputDetection = TIMER_OUTPUT_HIGH; // Start FTM0 timer - channel 0
  FTM_StartTimer(&FTMTimer0);

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
      RTC_Set(Packet_Parameter1,Packet_Parameter2,Packet_Parameter3); // Set time
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


/*! @brief Thread that looks after interrupts made by I2C when slave device data read is complete.
 *
 *  @param pData Thread parameter.
 *  @note Assumes that semaphores are created and communicate properly.
 */
static void I2CReadCompleteThread(void* pData)
{
  for (;;)
  {
    OS_SemaphoreWait(Read_Complete_Semaphore,0);

    // Send accelerometer data at 1.56Hz
    Packet_Put(CMD_ACCELVALUES,accelerometerValues.bytes[0],accelerometerValues.bytes[1],accelerometerValues.bytes[2]);
  }
}


/*! @brief Thread that looks after interrupts made by the accelerometer when new data is ready.
 *
 *  @param pData Thread parameter.
 *  @note Assumes that semaphores are created and communicate properly.
 */
static void AccelReadyThread(void* pData)
{
  for (;;)
  {
    OS_SemaphoreWait(Data_Ready_Semaphore,0);

    Accel_ReadXYZ(accelerometerValues.bytes); // Collect accelerometer data
    LEDs_Toggle(LED_GREEN); // Turn on green LED
  }
}


/*! @brief Thread that looks after interrupts made by the periodic interrupt timer.
 *
 *  @param pData Thread parameter.
 *  @note Assumes that semaphores are created and communicate properly.
 */
static void PITThread(void* pData)
{
  for (;;)
  {
    OS_SemaphoreWait(PIT_Semaphore,0);

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
}


/*! @brief Thread that looks after interrupts made by the real time clock.
 *
 *  @param pData Thread parameter.
 *  @note Assumes that semaphores are created and communicate properly.
 */
static void RTCThread(void* pData)
{
  for (;;)
  {
    OS_SemaphoreWait(Update_Clock_Semaphore,0);

    uint8_t hours, minutes, seconds; /*!< Variables to store current time */

    RTC_Get(&hours,&minutes,&seconds); // Get current time each second
    Packet_Put(CMD_TIME,hours,minutes,seconds); // Update time in PC
    LEDs_Toggle(LED_YELLOW); // Toggle yellow LED
  }
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
    FTMTimer0.ioType.inputDetection = TIMER_OUTPUT_DISCONNECT; // Stop timer interrupts
    FTM_StartTimer(&FTMTimer0); // Update timer setting
    LEDs_Off(LED_BLUE); // Turn off blue LED
  }
}
/*! @brief Initialises the hardware, sets up to threads, and starts the OS.
 *
 */
/*lint -save  -e970 Disable MISRA rule (6.3) checking. */
int main(void)
/*lint -restore Enable MISRA rule (6.3) checking. */
{
  OS_ERROR error; /*!< Thread content */

  // Initialise low-level clocks etc using Processor Expert code
  PE_low_level_init();

  // Initialize the RTOS
  OS_Init(CPU_CORE_CLK_HZ, false);

  // Create threads using OS_ThreadCreate();
  error = OS_ThreadCreate(InitThread, // Highest priority
                          NULL,
                          &InitThreadStack[THREAD_STACK_SIZE - 1],
                          0);

  error = OS_ThreadCreate(PITThread, // 4th highest priority
                          NULL,
                          &PITThreadStack[THREAD_STACK_SIZE - 1],
                          3);

  error = OS_ThreadCreate(RTCThread, // 5th highest priority
                          NULL,
                          &RTCThreadStack[THREAD_STACK_SIZE - 1],
                          4);

  error = OS_ThreadCreate(AccelReadyThread, // 6th highest priority
                          NULL,
                          &AccelReadyThreadStack[THREAD_STACK_SIZE - 1],
                          5);

  error = OS_ThreadCreate(I2CReadCompleteThread, // 7th highest priority
                          NULL,
                          &I2CReadCompleteThreadStack[THREAD_STACK_SIZE - 1],
                          6);

  error = OS_ThreadCreate(PacketCheckerThread, // Lowest priority
                          NULL,
                          &PacketCheckerThreadStack[THREAD_STACK_SIZE - 1],
                          7);


  // Start multithreading - never returns!
  OS_Start();
}

/*!
** @}
*/
