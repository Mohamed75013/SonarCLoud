/*! @file
 *
 *  @brief I/O routines for the K70 I2C interface.
 *
 *  This contains the functions for operating the I2C (inter-integrated circuit) module.
 *
 *  @author Manujaya Kankanige & Smit Patel
 *  @date 2016-05-07
 */

/*!
 *  @addtogroup i2c_module I2C module documentation
 *  @{
 */

// Included header files
#include "Cpu.h"
#include "PE_Types.h"
#include "types.h"
#include "MK70F12.h"
#include "LEDs.h"
#include "I2C.h"
#include "stdlib.h"

//Definitions
#define READ_WRITE 0x01

//Prototypes
static void Start(void);
static void Wait(void);
static void Stop(void);

static char SlaveAddress;          /*!< Private global variable to store 8-bit slave address */
static uint8_t I2CReadSequence[3]; /*!< Store the 3 values for interrupt reading */

static uint8_t SequencePosition;   /*!< The current position during interrupt reading */
static uint8_t* DataPtr;           /*!< A pointer to store the bytes that are read */
static uint8_t NumBytes;           /*!< The number of bytes to read */
static uint8_t DataCounter = 0;    /*!< A counter for the number of reads or writes requested */

static void* ReadCompleteUserArgumentsGlobal;      /*!< Private global pointer to the user arguments to use with the user callback function */
static void (*ReadCompleteCallbackGlobal)(void *); /*!< Private global pointer to data ready user callback function */


BOOL I2C_Init(const TI2CModule* const aI2CModule, const uint32_t moduleClk)
{
  ReadCompleteUserArgumentsGlobal = aI2CModule->readCompleteCallbackArguments; // userArguments made globally(private) accessible
  ReadCompleteCallbackGlobal = aI2CModule->readCompleteCallbackFunction; // userFunction made globally(private) accessible

  // Arrays to store multiplier and scl divider values
  uint8_t mult[] = {1,2,4};
  uint16_t scl[] = {20,22,24,26,28,30,34,40,28,32,36,40,44,48,56,68,
                    48,56,64,72,80,88,104,128,80,96,112,128,144,160,192,240,
  		    160,192,224,256,288,320,384,480,320,384,448,512,576,640,
  		    768,960,640,768,896,1024,1152,1280,1536,1920,1280,1536,
  		    1792,2048,2304,2560,3072,3840};

  uint8_t sclDivCount,                /*!< Counter for looping through scl divider array */
          mulCount,                   /*!< Counter for looping through multiplier array */
	  multiplier,                 /*!< Selected count value of multiplier */
	  sclDivider;                 /*!< Selected count value of scl divider */
  uint32_t baudRateError = 100000;    /*!< Difference between calculated baud rate and required barud rate */
  uint32_t selectedBaudRate;          /*!< Baud rate of current calculation */

  SIM_SCGC4 |= SIM_SCGC4_IIC0_MASK ; // Enable clock gate for I2C module

  PORTE_PCR18 |= PORT_PCR_MUX(4) | PORT_PCR_ODE_MASK; // I2C0_SDA (data line) and open drain
  PORTE_PCR19 |= PORT_PCR_MUX(4) | PORT_PCR_ODE_MASK; // I2C0_SCL (clock line) and open drain

  // Loop to find the baud rate
  for (sclDivCount=0; sclDivCount < (sizeof(scl)/sizeof(uint16_t))-1; sclDivCount++)
  {
    for (mulCount=0; mulCount < (sizeof(mult)/sizeof(uint8_t))-1; mulCount++)
    {
      if (abs(selectedBaudRate - aI2CModule->baudRate) < baudRateError) // Check if baud rate is closer to required baud rate, than before
      {
	selectedBaudRate = moduleClk / (mult[mulCount] * scl[sclDivCount]); // Calculate baud rate
        baudRateError = abs(selectedBaudRate - aI2CModule->baudRate); // Calculate difference in required baud rate and calculated baud rate
        multiplier = mulCount; // Count for selected multiplier
        sclDivider = sclDivCount; // Count for selected scl divider value
      }
    }
  }

  I2C0_F = I2C_F_MULT(mult[multiplier]) | I2C_F_ICR(sclDivider); // Set baud rate

  I2C0_C1 |= I2C_C1_IICEN_MASK; // I2C enable

  NVICICPR0 = NVIC_ICPR_CLRPEND(1 << 24); // Clear any pending interrupts on I2C0
  NVICISER0 = NVIC_ISER_SETENA(1 << 24); // Enable interrupts on I2C0
}


void I2C_SelectSlaveDevice(const uint8_t slaveAddress)
{
  SlaveAddress = slaveAddress; // Store the slave address globally(private)
}


/*! @brief Select master mode and transmit mode to start communication
 */
static void Start(void)
{
  I2C0_C1 |= I2C_C1_MST_MASK;
  I2C0_C1 |= I2C_C1_TX_MASK;
}


/*! @brief Wait until interrupt flag is set and clear it.
 */
static void Wait(void)
{
  while (!((I2C0_S & I2C_S_IICIF_MASK) == I2C_S_IICIF_MASK)){}
  I2C0_S |= I2C_S_IICIF_MASK;
}


/*! @brief Stop communication by clearing master mode
 */
static void Stop(void)
{
  I2C0_C1 &= ~I2C_C1_MST_MASK;
}


void I2C_Write(const uint8_t registerAddress, const uint8_t data)
{
  while ((I2C0_S & I2C_S_BUSY_MASK) == I2C_S_BUSY_MASK) // Wait till bus is idle
  {}

  Start(); // START signal, master-transmit mode

  I2C0_D = (SlaveAddress << 1)  & ~READ_WRITE; // Send slave address with write bit
  Wait(); // Wait for ACK

  I2C0_D = registerAddress; // Send slave register address
  Wait(); // Wait for ACK

  I2C0_D = data; // Write data
  Wait(); // Wait for ACK

  Stop(); // STOP signal
}


void I2C_PollRead(const uint8_t registerAddress, uint8_t* const data, const uint8_t nbBytes)
{

  uint8_t dataCount; /*!< Counter for counting data bytes read */

  while ((I2C0_S & I2C_S_BUSY_MASK) == I2C_S_BUSY_MASK) // Wait till bus is idle
  {}

  Start(); // START signal, master-transmit mode

  I2C0_D = (SlaveAddress << 1) & ~READ_WRITE; // Send slave address with write bit
  Wait(); // Wait for ACK

  I2C0_D = registerAddress; // Send slave register address
  Wait(); // Wait for ACK

  I2C0_C1 |= I2C_C1_RSTA_MASK; // Repeat start

  I2C0_D = (SlaveAddress << 1) | READ_WRITE;// Send slave address with read bit
  Wait(); // Wait for ACK

  I2C0_C1 &= ~I2C_C1_TX_MASK; // Receive mode
  I2C0_C1 &= ~I2C_C1_TXAK_MASK; // Turn on ACK from master

  data[0] = I2C0_D; // Dummy read
  Wait(); // Wait for ACK

  for (dataCount =0; dataCount < nbBytes -1; dataCount ++)
  {
    data[dataCount] = I2C0_D; // Read bytes until second last byte is to be read
    Wait(); // Wait for ACK
  }

  I2C0_C1 |= I2C_C1_TXAK_MASK; // NACK from master

  data[dataCount++] = I2C0_D; // Read second last byte
  Wait();

  Stop(); // STOP signal

  data[dataCount++] = I2C0_D; // Read last byte
}


void I2C_IntRead(const uint8_t registerAddress, uint8_t* const data, const uint8_t nbBytes)
{
  while ((I2C0_S & I2C_S_BUSY_MASK) == I2C_S_BUSY_MASK) // Wait till bus is idle
  {}
  I2C0_S |= I2C_S_IICIF_MASK; // Clear interrupt flag

  I2C0_C1 |= I2C_C1_IICIE_MASK; // I2C interrupt enable
  DataPtr = data; // Array to store values into, is made globally accessible
  NumBytes = nbBytes; // Number of bytes to read
  I2CReadSequence[0] = (SlaveAddress << 1) & ~READ_WRITE; // Send slave address with write bit
  I2CReadSequence[1] = registerAddress; // Register Address
  I2CReadSequence[2] = (SlaveAddress << 1) | READ_WRITE;// Send slave address with read bit
  SequencePosition = 0; // Initialize position

  Start(); // Start signal
  I2C0_D = I2CReadSequence[0]; // Send slave address with write bit
}


void __attribute__ ((interrupt)) I2C_ISR(void)
{
  if (!(I2C0_S & I2C_S_IICIF_MASK)) // Return if flag was set from different interrupt
    return;

  I2C0_S |= I2C_S_IICIF_MASK; // Clear interrupt flag

  if (I2C0_S & I2C_S_BUSY_MASK) // Bus is busy
  {
    if (I2C0_C1 & I2C_C1_TX_MASK) // In transmit mode
    {
      if (!(I2C0_S & I2C_S_RXAK_MASK)) // Check if ACK received
      {
        SequencePosition++; // Move to next item in sequence
        if (SequencePosition == 2)
        {
          I2C0_C1 |=  I2C_C1_RSTA_MASK; // Restart signal
          I2C0_D = I2CReadSequence[SequencePosition]; // Send slave address with read bit
        }
        else if (SequencePosition == 3)
        {
          I2C0_C1 &= ~I2C_C1_TX_MASK; // Receive mode
    	  I2C0_C1 &= ~I2C_C1_TXAK_MASK; // Turn on ACK from master
    	  DataPtr[DataCounter] = I2C0_D; // Dummy read
        }
    	else
      	  I2C0_D = I2CReadSequence[SequencePosition]; // Send slave register address

        return;
      }
      else // No ACK received
      {
        Stop(); // Stop signal
    	return;
      }
    }

    else if (!(I2C0_C1 & I2C_C1_TX_MASK) && (DataCounter <NumBytes)) // In receive mode
    {
      if (DataCounter == (NumBytes -1)) // Check if last byte to be read
      {
    	Stop();
        DataPtr[DataCounter] = I2C0_D; // Read last byte
    	DataCounter = 0; // Reset data counter
    	SequencePosition = 0; // Reset position counter

    	I2C0_C1 &= ~I2C_C1_IICIE_MASK; // I2C interrupt disable
        I2C0_S |= I2C_S_IICIF_MASK; // Clear interrupt flag

        (*ReadCompleteCallbackGlobal)(ReadCompleteUserArgumentsGlobal); // Accelerometer read complete callback function
        return;
      }
      else if (DataCounter == (NumBytes -2)) // Check if second last byte to be read
        I2C0_C1 |= I2C_C1_TXAK_MASK; // NACK from master

        DataPtr[DataCounter] = I2C0_D; // Read data
        DataCounter++; // Increment data counter
    }
  }
}


/*!
 * @}
*/
