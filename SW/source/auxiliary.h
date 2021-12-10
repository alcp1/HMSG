//----------------------------------------------------------------------------//
//                               OBJECT HISTORY                               //
//----------------------------------------------------------------------------//
//  REVISION |    DATE     |                               |      AUTHOR      //
//----------------------------------------------------------------------------//
//  1.00     | 10/Dec/2021 |                               | ALCP             //
// - First version                                                            //
//----------------------------------------------------------------------------//

#ifndef AUXILIARY_H
#define AUXILIARY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include "hapcan.h"
    
//----------------------------------------------------------------------------//
// EXTERNAL DEFINITIONS
//----------------------------------------------------------------------------//

    
//----------------------------------------------------------------------------//
// EXTERNAL TYPES
//----------------------------------------------------------------------------//

    
//----------------------------------------------------------------------------//
// EXTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
/**
 * Get Timestamp in milliseconds since epoch.
 * 
 * \param   None
 * \return  milliseconds since epoch.
 */
unsigned long long aux_getmsSinceEpoch(void);

/**
 * Try to convert string to number, and returns if it was ok.
 * 
 * \param   string  string to be converted
 *          val     value from string (if return is ok)
 *          base    base (10 = decimal, 16 = hexadecimal)
 * \return  true:   conversion ok
 *          false:  conversion failed
 */
bool aux_parseLong(const char *str, long *val, int base);

/**
 * Try to convert string to number, and returns if it was ok based on the 
 * conversion and on the limits check
 * 
 * \param   string  string to be converted
 *          val     value from string (if return is ok)
 *          base    base (10 = decimal, 16 = hexadecimal)
 *          min     minimum value that the result shall have
 *          max     maximum value that the result shall have
 * \return  true:   conversion and validation ok
 *          false:  conversion and validation failed
 */
bool aux_parseValidateLong(const char *str, long *val, int base, long min, 
        long max);

/**
 * Try to convert string to number, and returns if it was ok.
 * 
 * \param   string  string to be converted
 *          val     value from string (if return is ok)
 *          base    base (10 = decimal, 16 = hexadecimal)
 * \return  true:   conversion ok
 *          false:  conversion failed
 */
bool aux_parseDouble(const char *str, double *val);

/**
 * Try to convert string to number, and returns if it was ok based on the 
 * conversion and on the limits check
 * 
 * \param   string  string to be converted
 *          val     value from string (if return is ok)
 *          base    base (10 = decimal, 16 = hexadecimal)
 *          min     minimum value that the result shall have
 *          max     maximum value that the result shall have
 * \return  true:   conversion and validation ok
 *          false:  conversion and validation failed
 */
bool aux_parseValidateDouble(const char *str, double *val, double min, 
        double max);

/**
 * Try to convert string with delimeters to number, and returns if it was ok 
 * based on the conversion and on the limits check
 * 
 * \param   val             (OUTPUT) value array to be updated from string
 *          input_str       (INPUT) string to be converted
 *          input_delimeter (INPUT) delimeter within string
 *          n_values        (INPUT) number of delimeters
 *          base            (INPUT) base for parsing function
 *          min             (INPUT) minimum value that the result shall have
 *          max             (INPUT) maximum value that the result shall have
 * \return  true:   conversion and validation ok
 *          false:  conversion and validation failed
 */
bool aux_parseValidateIntArray(int *value, const char *inputstr, 
        const char *inputdelimeter, int n_values, int base, int min, int max);

/**
 * Convert Byte array into CAN Frame
 * 
 * \param   data        byte array to be converted to CAN Frame
 *          dataLen     Number of Bytes of Data
 *          pcf_Frame   CAN FRAME to be populated
 * \return  true:   conversion ok
 *          false:  conversion failed (wrong datalen)
 */
bool aux_getCANFromBytes(uint8_t* data, uint8_t dataLen, struct can_frame* pcf_Frame);

/**
 * Init a CAN Frame structure to 0
 * 
 * \param   pcf_Frame   CAN FRAME to be populated
 * \return  Nothing
 */
void aux_clearCANFrame(struct can_frame* pcf_Frame);

/**
 * Init a HAPCAN Frame structure to 0
 * 
 * \param   hd   HAPCAN FRAME to be populated
 * \return  Nothing
 */
void aux_clearHAPCANFrame(hapcanCANData *p_hd);

/**
 * Get the current Time on a byte array, according to the HAPCAN Spec
 * 
 * \param   data   byte array to be populated
 * \return  Nothing
 */
void aux_getHAPCANTime(uint8_t* data);

/**
 * Get the uptime on a byte array, according to the HAPCAN Spec
 * 
 * \param   data   byte array to be populated
 * \return  Nothing
 */
void aux_getHAPCANUptime(uint8_t* data);

/**
 * Get seconds to 0
 * 
 * \return  number of seconds until the system local time seconds is set to 0
 */
int aux_getTimeUntilZeroSeconds(void);

/**
 * Get current local year
 * 
 * \return  number of years since 1900
 */
int aux_getLocalYear(void);

/**
 * Get String Between Initial and Final Delimeter
 * 
 * \param   dest        destination string to be populated
 *          origin      Origin String to be checked
 *          initialD    Initial Delimeter (ignored if NULL)
 *          finalD      Final Delimeter (ignored if NULL)
 * 
 * \return  true:   return string OK
 *          false:  return string NOK
 */
bool aux_getStringFromDelimeters(char* dest, char* origin, char* initialD, char* finalD);

/**
 * Check if strings are equal
 * 
 * \param   str1      First String
 *          str2      Second String
 * 
 * \return  true:   same strings
 *          false:  different strings
 */
bool aux_compareStrings(char *str1, char *str2);

/**
 * Check if strings are equal up to N length
 * 
 * \param   str1    First String
 *          str2    Second String
 *          len     maximum length for the comparison
 * 
 * \return  true:   same strings
 *          false:  different strings
 */
bool aux_compareStringsN(char *str1, char *str2, int len);

/**
 * Check if a received HAPCAN Frame matches the gateway filters
 * 
 * \param   phd_received    The received HAPCAN Frame
 *          phd_mask        The gatway mask
 *          phd_check       The gateway check
 * 
 * \return  true:   match
 *          false:  not a match
 */
bool aux_checkCAN2MQTTMatch(hapcanCANData *phd_received, 
        hapcanCANData *phd_mask, hapcanCANData *phd_check);

#ifdef __cplusplus
}
#endif

#endif /* AUXILIARY_H */