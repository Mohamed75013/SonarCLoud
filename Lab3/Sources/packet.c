/*! @file
 *
 *  @brief Routines to implement packet encoding and decoding for the serial port.
 *
 *  This contains the functions for implementing the "Tower to PC Protocol" 5-byte packets.
 *
 *  @author Manujaya Kankanige & Smit Patel
 *  @date 201-03-26
 */

/*!
 *  @addtogroup packet_module packet module documentation
 *  @{
 */

// Included header files
#include "packet.h"
#include "UART.h"


BOOL Packet_Init(const uint32_t baudRate, const uint32_t moduleClk)
{
  return UART_Init(baudRate, moduleClk); // Initialize UART2
}


BOOL Packet_Get(void)
{
  uint8_t PacketChecksum = 0; // Initialize any values present already
  static uint8_t state = 0;  /*!< Current state of the state machine */

  while (1)
  {
    switch (state) // State machine
    {
    case 0:  if (UART_InChar(&Packet_Command)) // Get command byte from RxFIFO
	       state = 1; // If successful move to next state
	     else
	       return bFALSE; // Failed to get command byte from RxFIFO
	     break;

    case 1:  if (UART_InChar(&Packet_Parameter1)) // Get parameter 1 byte from RxFIFO
      	       state = 2; // If successful move to next state
       	     else
      	       return bFALSE; // Failed to get parameter 1 from RxFIFO
       	     break;

    case 2:  if (UART_InChar(&Packet_Parameter2)) // Get parameter 2 from RxFIFO
      	       state = 3; // If successful move to next state
      	     else
      	       return bFALSE; // Failed to get parameter 2 from RxFIFO
      	     break;

    case 3:  if (UART_InChar(&Packet_Parameter3)) // Get parameter 3 from RxFIFO
      	       state = 4; // If successful move to next state
             else
      	       return bFALSE; // Failed to get parameter 3 from RxFIFO
      	     break;

    case 4:  if (UART_InChar(&PacketChecksum)) // Get the checksum from RxFIFO
      	       state = 5; // If successful move to next state
      	     else
      	       return bFALSE; // Failed to get the checksum from RxFIFO
      	     break;

    case 5:  if ((Packet_Command ^ Packet_Parameter1 ^ Packet_Parameter2 ^ Packet_Parameter3) == PacketChecksum)
             {
	       state = 0; // Valid packet received, return to initial state
	       PacketChecksum = 0; // Initialize checksum
	       return bTRUE; // Exit state machine
	     }
	     else
	     {
	       Packet_Command = Packet_Parameter1; // Checksum does not add up, right shift bytes
	       Packet_Parameter1 = Packet_Parameter2;
	       Packet_Parameter2 = Packet_Parameter3;
	       Packet_Parameter3 = PacketChecksum;
	       state = 4; // Go back to previous state to re-check
	     }
	     break;
    }
  }
}


BOOL Packet_Put(const uint8_t command, const uint8_t parameter1, const uint8_t parameter2, const uint8_t parameter3)
{
  if (UART_OutChar(command)    &&  // Put the packet, byte by byte into TxFIFO
      UART_OutChar(parameter1) &&
      UART_OutChar(parameter2) &&
      UART_OutChar(parameter3) &&
      UART_OutChar(command^parameter1^parameter2^parameter3))
      return bTRUE; // Packet successfully placed in TxFIFO
  else
    return bFALSE; // Packet placement into TxFIFO was unsuccessful
}


/*!
 * @}
*/
