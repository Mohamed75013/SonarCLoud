/*! @file
 *
 *  @brief Routines to implement a FIFO buffer.
 *
 *  This contains the structure and "methods" for accessing a byte-wide FIFO.
 *
 *  @author Manujaya Kankanige & Smit Patel
 *  @date 2016-03-26
 */

/*!
 *  @addtogroup fifo_module FIFO module documentation
 *  @{
 */

// Included header files
#include "Cpu.h"
#include "PE_Types.h"
#include "FIFO.h"

void FIFO_Init(TFIFO * const FIFO)
{
  FIFO->NbBytes = 0; // Initialize The number of bytes stored in the FIFO to 0
  FIFO->Start = 0; // Initialize the head pointer to position 0
  FIFO->End = 0; // Initialize the tail pointer to position 0
}


BOOL FIFO_Put(TFIFO * const FIFO, const uint8_t data)
{
  EnterCritical(); // Enter critical section
  if (FIFO->NbBytes == FIFO_SIZE)  // Check if FIFO buffer is full
  {
    ExitCritical(); // Exit critical section
    return bFALSE; // If buffer is full, return 0(error)
  }
  else
  {
    FIFO->Buffer[FIFO->End] = data; // Put data into FIFO buffer

    if (FIFO->End == FIFO_SIZE-1) // Reset the tail pointer position to 0 if it reaches the end of the buffer
      FIFO->End = 0;
    else
      FIFO->End++; // Move tail pointer to next position

    FIFO->NbBytes++; // Increment value of number of bytes in FIFO
    ExitCritical(); // Exit critical section
    return bTRUE; // Byte inserted to FIFO successfully
  }
}


BOOL FIFO_Get(TFIFO * const FIFO, uint8_t * const dataPtr)
{
  EnterCritical(); // Enter critical section
  if (FIFO->NbBytes == 0) // Checking if FIFO buffer is empty
  {
    ExitCritical(); // Exit critical section
    return bFALSE; // If buffer is empty, return 0(error)
  }
  else
  {
    *dataPtr = FIFO->Buffer[FIFO->Start]; // Get oldest data in FIFO and place it in dataPtr

    if (FIFO->Start == FIFO_SIZE-1) // Reset the head pointer position to 0 if it reaches the end of the buffer
      FIFO->Start = 0;
    else
      FIFO->Start++; // Move head pointer to next position

    FIFO->NbBytes--; // Decrement value of number of bytes in FIFO
    ExitCritical(); // Exit critical section
    return bTRUE; // Byte extracted from FIFO successfully
  }
}


/*!
 * @}
*/
