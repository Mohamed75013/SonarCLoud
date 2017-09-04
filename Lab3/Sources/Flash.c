/*! @file
 *
 *  @brief Routines for erasing and writing to the Flash.
 *
 *  This contains the functions needed for accessing the internal Flash.
 *
 *  @author Manujaya Kankanige & Smit Patel
 *  @date 2016-04-07
 */

/*!
 *  @addtogroup flash_module flash module documentation
 *  @{
 */

// Included header files
#include "types.h"
#include "MK70F12.h"
#include "packet.h"
#include "Flash.h"


#define ACCERR_FPVIOL_ERROR (FTFE_FSTAT & (FTFE_FSTAT_FPVIOL_MASK & FTFE_FSTAT_ACCERR_MASK)) // Bits showing ACCER Error or FPVIOL Error
#define CHECK_BIT(x,n) ((x >> (n)) & 1) // Macro to compare each bit with LSB


typedef struct
{
  uint8_t Command;        /*!< Stores FTFE command */
  uint8_t FlashAddress1;  /*!< Stores starting address bits [23:16] */
  uint8_t FlashAddress2;  /*!< Stores starting address bits [15:8]  */
  uint8_t FlashAddress3;  /*!< Stores starting address bits [7:0]   */
  uint8_t DataByte0;      /*!< Stores phrase bits [63:56] */
  uint8_t DataByte1;      /*!< Stores phrase bits [55:48] */
  uint8_t DataByte2;      /*!< Stores phrase bits [47:40] */
  uint8_t DataByte3;      /*!< Stores phrase bits [39:32] */
  uint8_t DataByte4;      /*!< Stores phrase bits [31:24] */
  uint8_t DataByte5;      /*!< Stores phrase bits [23:16] */
  uint8_t DataByte6;      /*!< Stores phrase bits [15:8]  */
  uint8_t DataByte7;      /*!< Stores phrase bits [7:0]   */
} TFCCOB;


// Prototypes
static BOOL EraseSector(const uint32_t address);
static BOOL LaunchCommand(TFCCOB* commonCommandObject);
static BOOL AllocateVar(volatile void **data, const uint8_t end,const uint8_t size);
static BOOL WritePhrase(const uint32_t address, const uint64_t data);

TFCCOB Fccob; /*!< Structure containing the items going into the FCCOB registers */
static uint8_t Occupied = 0x00; /*!< Occupied space in flash memory where each bit represents a byte in a phrase */


BOOL Flash_Init()
{
  while(!(FTFE_FSTAT & FTFE_FSTAT_CCIF_MASK)) {} // Wait for command completion
  if(ACCERR_FPVIOL_ERROR) // Check for ACCERR flag and FPVIOL flag
    FTFE_FSTAT = FTFE_FSTAT_ACCERR_MASK | FTFE_FSTAT_FPVIOL_MASK; // Clear past errors (0x30)

  return bTRUE;
}


BOOL Flash_AllocateVar(volatile void** variable, const uint8_t size)
{
  uint8_t position = 0;		/*!< Position in the 8 byte sector */
  uint8_t availableSlots = 0;	/*!< Number of available positions */
  uint8_t found = 0 ;           /*!< Number of empty locations found */

  while(position < 8) // Cycle through the locations in the sector to find free space to allocate data
  {
    if(CHECK_BIT(Occupied,position++) == 0) // Macro to compare each bit to find available positions
    {
      availableSlots++; // Increase counter variable if the current array slot is empty
      found++;
    }
    else
    {
      found = 0; // No empty locations found
      continue; // Return to the start of the loop to increment count
    }


    if((availableSlots % size == 0) && (found == size))
    {
      if (AllocateVar(variable,position-1,size)) // Place variable into the allocated free spaces
        break;
      else
	return bFALSE;
    }
  }
  return bTRUE;
}


/*! @brief Allocates space for a non-volatile variable in the Flash memory
 *
 *  @param data is the address of a pointer to a variable that is to be allocated space in flash memory
 *  @param end is the position of the free starting location in the sector (position-1)
 *  @param size is the size, in bytes, of the variable that is to be allocated space in the flash memory (1, 2 or 4)
 *
 *  @return BOOL - TRUE if the variable was allocated space in the Flash memory
 */
static BOOL AllocateVar(volatile void **data, const uint8_t end,const uint8_t size)
{
  switch (size)
  {
    case 1:
	   *(volatile uint8_t**) data = &(_FB(FLASH_DATA_START+end)); // Starting address to place data(1 byte)
	   Occupied |= 0x1 << end; // Set flag to signal one byte being occupied
	   break;

    case 2:
	   *(volatile uint16_t**) data = &(_FH(FLASH_DATA_START+(end-1))); // Starting address to place data(2 bytes)
 	   Occupied |= 0x3 << end-1; // Set flag to signal two bytes being occupied
	   break;

    case 4:
	   *(volatile uint32_t**) data = &(_FW(FLASH_DATA_START+(end-3))); // Starting address to place data(4 bytes)
	   Occupied |= 0xF << end-3; // Set flag to signal four bytes being occupied
	   break;

    default:
            return bFALSE; // Error
  }
  return bTRUE; // Address declaration was successfully completed
}


/*! @brief Writes the 64 bit phrase and the 32 bit address to the structure
 *
 *  @return BOOL - TRUE if the Flash "data" sector was erased successfully
 *  @param address is the starting address of the data to be written at
 *  @param data is the 64 bit phrase
 */
static BOOL WritePhrase(const uint32_t address, const uint64_t data)
{
  Flash_Erase(); // Erase content in flash memory before writing into it

  Fccob.Command = FTFE_FCCOB0_CCOBn(0x07);; // Command to program phrase
  Fccob.FlashAddress1 = address >> 16; // Bits [23:16] of starting address
  Fccob.FlashAddress2 = address >> 8; // Bits [15:8] of starting address
  Fccob.FlashAddress3 = address; // Bits [7:0] of starting address

  Fccob.DataByte0 = data >> 56; // Store the phrase (separated into bytes) in the fccob structure
  Fccob.DataByte1 = data >> 48;
  Fccob.DataByte2 = data >> 40;
  Fccob.DataByte3 = data >> 32;
  Fccob.DataByte4 = data >> 24;
  Fccob.DataByte5 = data >> 16;
  Fccob.DataByte6 = data >> 8;
  Fccob.DataByte7 = data;

  return (LaunchCommand(&Fccob));
}

BOOL Flash_Write32(volatile uint32_t* const address, const uint32_t data)
{
  uint64union_t phrase; /*!< Stores the hi and lo components of the phrase */
  uint32_t theAddress = (uint32_t)address; /*!< Stores the starting address to store data */

  if ((theAddress/4) % 2 == 0) // Setting up a phrase
  {
    phrase.s.Lo = data; // Combine data in address+4 with current word
    phrase.s.Hi = _FW(theAddress+4);
    return WritePhrase(theAddress,phrase.l); // Return 64 bit phrase
  }
  else
  {
    phrase.s.Lo = _FW(theAddress-4); // Combine data in address-4 with current word
    phrase.s.Hi = data;
    return WritePhrase(theAddress-4,phrase.l); // Return 64 bit phrase
  }
}


BOOL Flash_Write16(volatile uint16_t* const address, const uint16_t data)
{
  uint32union_t word; /*!< Stores the hi and lo components of the word */
  uint32_t theAddress = (uint32_t)address; /*!< Stores the starting address to store data */

  if (theAddress % 4 == 0) // Setting up a word
  {
    word.s.Lo = data; // Combine data in address+2 with current half word
    word.s.Hi = _FH(theAddress+2);
    return Flash_Write32(&(_FW(theAddress)),word.l); // Return 32 bit word
  }
  else
  {
    word.s.Lo = _FH(theAddress-2); // Combine data in address-2 with current half word
    word.s.Hi = data;
    return Flash_Write32(&(_FW(theAddress-2)),word.l); // Return 32 bit word
  }
}


BOOL Flash_Write8(volatile uint8_t* const address, const uint8_t data)
{
  uint16union_t halfWord; /*!< Stores the hi and lo components of the half word */
  uint32_t theAddress = (uint32_t)address; /*!< Stores the starting address to store data */

  if (theAddress % 2 == 0) // Setting up a half word
  {
    halfWord.s.Lo = data; // Combine data in address+1 with current byte
    halfWord.s.Hi = _FB(theAddress+1);
    return Flash_Write16(&(_FH(theAddress)),halfWord.l); // Return 16 bit half word
  }
  else
  {
    halfWord.s.Lo = _FB(theAddress-1); // Combine data in address-1 with current byte
    halfWord.s.Hi = data;
    return Flash_Write16(&(_FH(theAddress-1)),halfWord.l); // Return 16 bit half word
  }
}


BOOL Flash_Erase(void)
{
  return EraseSector(FLASH_DATA_START); // Erase block 2's sector 0 in flash
}


/*! @brief Erases the entire Flash sector
 *
 *  @return BOOL - TRUE if the Flash "data" sector was erased successfully
 *  @param address is the starting address of the sector
 */
static BOOL EraseSector(const uint32_t address)
{
  Fccob.Command = FTFE_FCCOB0_CCOBn(0x09); // Command to erase sector
  Fccob.FlashAddress1 = address >> 16; // Bits [23:16] of starting address
  Fccob.FlashAddress2 = address >> 8; // Bits [15:8] of starting address
  Fccob.FlashAddress3 = address; // Bits [7:0] of starting address
  return LaunchCommand(&Fccob);
}


/*! @brief Updates FCCOB registers to write to flash
 *
 *  @return BOOL - TRUE if the the writing was completed successfully
 *  @param commonCommandObject is the structure which contains the stored values
 */
static BOOL LaunchCommand(TFCCOB* commonCommandObject)
{
  if(ACCERR_FPVIOL_ERROR) // Check for ACCERR flag and FPVIOL flag
    FTFE_FSTAT = FTFE_FSTAT_ACCERR_MASK | FTFE_FSTAT_FPVIOL_MASK; // Clear past errors (0x30)

  FTFE_FCCOB0 = commonCommandObject->Command; // Place structure content into FCCOB registers
  FTFE_FCCOB1 = commonCommandObject->FlashAddress1;
  FTFE_FCCOB2 = commonCommandObject->FlashAddress2;
  FTFE_FCCOB3 = commonCommandObject->FlashAddress3;

  FTFE_FCCOB8 = commonCommandObject->DataByte0;
  FTFE_FCCOB9 = commonCommandObject->DataByte1;
  FTFE_FCCOBA = commonCommandObject->DataByte2;
  FTFE_FCCOBB = commonCommandObject->DataByte3;
  FTFE_FCCOB4 = commonCommandObject->DataByte4;
  FTFE_FCCOB5 = commonCommandObject->DataByte5;
  FTFE_FCCOB6 = commonCommandObject->DataByte6;
  FTFE_FCCOB7 = commonCommandObject->DataByte7;

  FTFE_FSTAT = FTFE_FSTAT_CCIF_MASK; // Launch command sequence
  while(!(FTFE_FSTAT & FTFE_FSTAT_CCIF_MASK)) {} // Wait for command completion
  return bTRUE;
}

/*!
 * @}
*/
