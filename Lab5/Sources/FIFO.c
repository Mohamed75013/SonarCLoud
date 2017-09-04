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
#include "OS.h"
#include "FIFO.h"

void FIFO_Init(TFIFO * const FIFO)
{
  FIFO->NbBytes = 0; // Initialize The number of bytes stored in the FIFO to 0
  FIFO->Start = 0; // Initialize the head pointer to position 0
  FIFO->End = 0; // Initialize the tail pointer to position 0
  FIFO->NotFullSemaphore = OS_SemaphoreCreate(0); // FIFO not full semaphore initialized to 0
  FIFO->NotEmptySemaphore = OS_SemaphoreCreate(0); // FIFO not empty semaphore initialized to 0
}


BOOL FIFO_Put(TFIFO * const FIFO, const uint8_t data)
{
  if (FIFO->NbBytes == FIFO_SIZE) // Check if FIFO buffer is full
    OS_SemaphoreWait(FIFO->NotFullSemaphore,0); // Wait for signal saying FIFO is not full
  else
  {
    FIFO->Buffer[FIFO->End] = data; // Put data into FIFO buffer

    if (FIFO->End == FIFO_SIZE-1) // Reset the tail pointer position to 0 if it reaches the end of the buffer
      FIFO->End = 0;
    else
      FIFO->End++; // Move tail pointer to next position

    FIFO->NbBytes++; // Increment value of number of bytes in FIFO
    OS_SemaphoreSignal(FIFO->NotEmptySemaphore); // Signal saying FIFO is not empty
  }
}


BOOL FIFO_Get(TFIFO * const FIFO, uint8_t * const dataPtr)
{
  if (FIFO->NbBytes == 0) // Checking if FIFO buffer is empty
    OS_SemaphoreWait(FIFO->NotEmptySemaphore,0); // Wait for signal that FIFO is not empty
  else
  {
    *dataPtr = FIFO->Buffer[FIFO->Start]; // Get oldest data in FIFO and place it in dataPtr

    if (FIFO->Start == FIFO_SIZE-1) // Reset the head pointer position to 0 if it reaches the end of the buffer
      FIFO->Start = 0;
    else
      FIFO->Start++; // Move head pointer to next position

    FIFO->NbBytes--; // Decrement value of number of bytes in FIFO
    OS_SemaphoreSignal(FIFO->NotFullSemaphore); // Signal saying FIFO is not full

    return bTRUE; // Byte extracted from FIFO successfully
  }
}


/*!
 * @}
*/

