/*! @file
 *
 *  @brief Median filter.
 *
 *  This contains the functions for performing a median filter on byte-sized data.
 *
 *  @author Manujaya Kankanige & Smit Patel
 *  @date 2016-05-07
 */

/*!
 *  @addtogroup median_module median module documentation
 *  @{
 */

// Included header files
#include "types.h"

uint8_t Median_Filter3(const uint8_t n1, const uint8_t n2, const uint8_t n3)
{
  uint8_t result; /*!< Variable to store middle value */

  if (n1 > n2)
  {
    if (n2 > n3)
      result = n2; // n1>n2,n2>n3 n1>n2>n3
    else
    {
      if (n1 > n3)
        result = n3; // n1>n2,n3>n2,n1>n3 n1>n3>n2
      else
        result = n1; // n1>n2,n3>n2,n3>n1 n3>n1>n2
    }
  }
  else
  {
    if (n3 > n2)
      result = n2; // n2>n1,n3>n2 n3>n2>n1
    else
    {
      if (n1 > n3)
        result = n1; // n2>n1,n2>n3,n1>n3 n2>n1>n3
      else
        result = n3; // n2>n1,n2>n3,n3>n1 n2>n3>n1
    }
  }
  return result; // Median is returned
}


/*!
 * @}
*/
