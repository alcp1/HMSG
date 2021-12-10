//----------------------------------------------------------------------------//
//                               OBJECT HISTORY                               //
//----------------------------------------------------------------------------//
//  REVISION |    DATE     |                               |      AUTHOR      //
//----------------------------------------------------------------------------//
//  1.00     | 10/Dec/2021 |                               | ALCP             //
// - First version                                                            //
//----------------------------------------------------------------------------//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <limits.h>
#include <time.h>
#include <sys/time.h>
#include <linux/kernel.h>       /* for struct sysinfo */
#include <sys/sysinfo.h>
#include <errno.h>
#include "auxiliary.h"
#include "debug.h"
#include "mqtt.h"


//----------------------------------------------------------------------------//
// INTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
static uint8_t decToBcd(uint8_t val);

static uint8_t decToBcd(uint8_t val)
{
  return( (val/10*16) + (val%10) );
}

//----------------------------------------------------------------------------//
// EXTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
/* Get Timestamp in milliseconds since epoch. */
unsigned long long aux_getmsSinceEpoch(void)
{
    struct timeval tv;
    unsigned long long millisecondsSinceEpoch;  
    
    gettimeofday(&tv, NULL);
    millisecondsSinceEpoch = (unsigned long long)(tv.tv_sec) * 1000 + (unsigned long long)(tv.tv_usec) / 1000;
    
    return millisecondsSinceEpoch;
}
/** Try to convert string to number, and returns if it was ok */
bool aux_parseLong(const char *str, long *val, int base)
{
    char *temp;
    bool rc = true;
    errno = 0;
    if(str != NULL)
    {
        *val = strtol(str, &temp, base);
        // Check for conversion error
        if(temp == str || *temp != '\0' || ((*val == LONG_MIN || *val == LONG_MAX) && errno == ERANGE))
        {
            // conversion error
            rc = false;
        }
    }
    else
    {
        rc = false;
    }        
    return rc;
}

/** Try to convert string to number, validate it, and returns if it was ok */
bool aux_parseValidateLong(const char *str, long *val, int base, long min, long max)
{
    bool rc;
    long temp;
    
    // Check for conversion error
    rc = aux_parseLong(str, &temp, base);
    
    // Check boundaries
    if(rc)
    {
        if( (temp < min) || (temp > max) )
        {
            // boundary error
            rc = false;
        }
        else
        {
            // Set value
            *val = temp;
        }
    }    
    
    // Return
    return rc;
}

/**
 * Try to convert string to number, and returns if it was ok.
 */
bool aux_parseDouble(const char *str, double *val)
{
    char *temp;
    bool rc = true;
    errno = 0;
    *val = strtod(str, &temp);
    
    // Check for conversion error
    if(temp == str || *temp != '\0' || (*val == 0 && errno == EINVAL) || 
            (errno == ERANGE))
    {
        // conversion error
        rc = false;
    }
    return rc;
}

/**
 * Try to convert string to number, and returns if it was ok based on the 
 * conversion and on the limits check
 */
bool aux_parseValidateDouble(const char *str, double *val, double min, 
        double max)
{
    bool rc;
    double temp;
    
    // Check for conversion error
    rc = aux_parseDouble(str, &temp);
    
    // Check boundaries
    if(rc)
    {
        if( (temp < min) || (temp > max) )
        {
            // boundary error
            rc = false;
        }
        else
        {
            // Set value
            *val = temp;
        }
    }    
    
    // Return
    return rc;
}

/**
 * Try to convert string with delimeters to number, and returns if it was ok 
 * based on the conversion and on the limits check
 */
bool aux_parseValidateIntArray(int *value, const char *inputstr, 
        const char *inputdelimeter, int n_values, int base, int min, int max)
{
    bool ret = false;
    char *token;
    char *str = NULL;
    char *pString = NULL;
    unsigned int len;
    long temp;
    bool stay;
    int i;
    // Initial check    
    if(inputstr == NULL || inputdelimeter == NULL || n_values <= 0)
    {
        ret = false;
    }
    else
    {
        ret = true;
    }
    if(ret)
    {
        // Copy input string before manipulations
        len = strlen(inputstr);
        str = malloc(len + 1);
        strcpy(str, inputstr);
        // Store the pointer to free it before manipulations with strsep
        pString = str;    
        // Try to get n values
        stay = true;
        i = 0;
        while(stay)
        {
            // Last value does not have delimeter
            if(i == n_values - 1)
            {
                if(str == NULL)
                {
                    ret = false;
                }
                else
                {
                    ret = aux_parseValidateLong(str, &temp, base, min, max);
                    if(ret)
                    {
                        value[i] = (int)(temp);
                    }
                }
            }
            else
            {
                // Try to get a token
                token = strsep(&str, inputdelimeter);
                if(token == NULL)
                {
                    ret = false;
                }
                else
                {
                    // With delimeter found / not found - try to get group
                    ret = aux_parseValidateLong(token, &temp, base, min, max);
                    if(ret)
                    {
                        value[i] = (int)(temp);
                    }
                }
            }            
            i++;
            if((i >= n_values) || (!ret))
            {
                stay = false;
            }
        }
    }
    // Free
    free(pString);
    // Return
    return ret;
}

/** Convert Byte array into CAN Frame */
bool aux_getCANFromBytes(uint8_t* data, uint8_t dataLen, struct can_frame* pcf_Frame)
{
    canid_t canID;
    int li_index;
    
    // Check data len
    if( (dataLen < 0) || (dataLen > CAN_MAX_DLEN) )
    {
        return false;
    }
    // ID
    canID = 0;
    canID = (__u32)(data[0]) << 24;
    canID = canID + ((__u32)(data[1]) << 16);
    canID = canID + ((__u32)(data[2]) << 8);
    canID = canID + (__u32)(data[3]);
    pcf_Frame->can_id = canID;
    // Data Len
    pcf_Frame->can_dlc = dataLen;
    // Data
    for(li_index = 0; li_index < dataLen; li_index++)
    {
        pcf_Frame->data[li_index] = data[li_index + 4];
    }
    // return
    return true;
    
}

/** Init a CAN Frame structure to 0 */
void aux_clearCANFrame(struct can_frame* pcf_Frame)
{
    int li_index;
    memset(pcf_Frame, 0, sizeof(struct can_frame));
    pcf_Frame->can_id = 0;
    pcf_Frame->can_dlc = 0;
    for(li_index = 0; li_index < CAN_MAX_DLEN; li_index++)
    {
        pcf_Frame->data[li_index] = 0;
    }
}

/** Init a HAPCAN Frame structure to 0 */
void aux_clearHAPCANFrame(hapcanCANData *p_hd)
{
    int i;
    memset(p_hd, 0, sizeof(hapcanCANData));
    p_hd->frametype = 0;
    p_hd->flags = 0;
    p_hd->group = 0;
    p_hd->module = 0;
    for(i = 0; i < CAN_MAX_DLEN; i++)
    {
        p_hd->data[i] = 0;
    }
}

/** Get the current Time on a byte array, according to the HAPCAN Spec */
void aux_getHAPCANTime(uint8_t* data)
{
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    int i_temp;
    int i_index;
    uint8_t ub_temp;
    
    i_index = 0;
    // YEAR: - year number (0-99) in BCD format, <7:4>-tens digit, <3:0>-unit digit (0x00-0x99)
    i_temp = tm.tm_year; // years since 1900
    i_temp += 100; // years since 2000
    ub_temp = (uint8_t)(i_temp % 100);
    data[i_index] = decToBcd(ub_temp); 
    i_index++;
    
    // MONTH - month number (1-12) in BCD format, <7:4>-tens digit, <3:0>-unit digit (0x01-0x12)
    i_temp = tm.tm_mon; // month of year [0,11]
    i_temp += 1;
    ub_temp = (uint8_t)(i_temp);
    data[i_index] = decToBcd(ub_temp);
    i_index++;
    
    // DATE - day of month number (1-31) in BCD format, <7:4>-tens digit, <3:0>-unit digit (0x01-0x31)    
    i_temp = tm.tm_mday; // day of month [1,31]
    ub_temp = (uint8_t)(i_temp);
    data[i_index] = decToBcd(ub_temp);
    i_index++;
    
    // DAY - day of week number (0x01-Mo, 0x02-Tu, 0x03-We, 0x04-Th, 0x05-Fr, 0x06-Sa, 0x07-Su)
    i_temp = tm.tm_wday; // day of week [0,6] (Sunday = 0)
    if(i_temp == 0)
    {
        i_temp = 7;
    }
    ub_temp = (uint8_t)(i_temp);
    data[i_index] = decToBcd(ub_temp);
    i_index++;
    
    // HOUR - hour number (0-23) in BCD format, <7:4>-tens digit, <3:0>-unit digit (0x00-0x23)
    i_temp = tm.tm_hour;    // hour [0,23]
    ub_temp = (uint8_t)(i_temp);
    data[i_index] = decToBcd(ub_temp);
    i_index++;
    
    // MIN - minute number (0-59) in BCD format, <7:4>-tens digit, <3:0>-unit digit (0x00-0x59)
    i_temp = tm.tm_min; // minutes [0,59]
    ub_temp = (uint8_t)(i_temp);
    data[i_index] = decToBcd(ub_temp);
    i_index++;
    
    // SEC - second number (0-59) in BCD format, <7:4>-tens digit, <3:0>-unit digit (0x00-0x59)
    i_temp = tm.tm_sec; // seconds [0,61]
    ub_temp = (uint8_t)(i_temp);
    data[i_index] = decToBcd(ub_temp);
    i_index++;    
}


/** Get the uptime on a byte array, according to the HAPCAN Spec */
void aux_getHAPCANUptime(uint8_t* data)
{
    struct sysinfo s_info;
    int error = sysinfo(&s_info);
    if(error != 0)
    {
        data[0] = 0;
        data[1] = 0;
        data[2] = 0;
        data[3] = 0;
    }
    else
    {        
        data[3] = (uint8_t)(s_info.uptime % 256);
        data[2] = (uint8_t)((s_info.uptime >> 8) % 256);
        data[1] = (uint8_t)((s_info.uptime >> 16) % 256);
        data[0] = (uint8_t)((s_info.uptime >> 24) % 256);
    }    
}

int aux_getTimeUntilZeroSeconds(void)
{
    int i_temp;
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    i_temp = tm.tm_sec; // seconds [0,61]
    if(i_temp >= 60)
    {
        i_temp = 1;
    }
    else
    {
        i_temp = 60 - i_temp;
    }
    // Paranoid check
    if(i_temp <=0)
    {
        i_temp = 1;
    }
    return i_temp;
}

/** Get current local year */
int aux_getLocalYear(void)
{
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    int i_temp;
    
    // YEAR: - year number (0-99) in BCD format, <7:4>-tens digit, <3:0>-unit digit (0x00-0x99)
    i_temp = tm.tm_year; // years since 1900
    return i_temp;
}

/** Get String Between Initial and Final Delimeter **/
bool aux_getStringFromDelimeters(char* dest, char* origin, char* initialD, char* finalD)
{
    bool b_return = false;
    char *initial;
    char *final;
    int len;
    // First get the initial substring pointer
    if(initialD == NULL)
    {
        initial = origin;
    }
    else
    {
        initial = strstr(origin, initialD);
    }
    if(initial != NULL)
    {
        // Get the final substring beginning from the initial substring position
        if(finalD == NULL)
        {
            final = origin + strlen(origin);
        }
        else
        {
            final = strstr(initial, finalD);
        }
        if(final != NULL)
        {
            // Get the length
            len = final - (initial + strlen(initialD));
            // Check the length
            if( (len < strlen(origin)) && (len > 0) )
            {                
                memset(dest,0,strlen(dest));
                memcpy(dest, initial + strlen(initialD), len);
                dest[len + 1] = '\0';
                b_return = true;
            }            
        }                
    }    
    return b_return;
}

/* Compare two strings and returns true if equal */
bool aux_compareStrings(char *str1, char *str2)
{
    bool ret = false;
    if(str1 == NULL)
    {
        if(str2 == NULL)
        {
            ret = true;
        }
        else
        {
            ret = false;
        }
    }
    else
    {
        if(str2 == NULL)
        {
            ret = false;
        }
        else
        {
            ret = !strcmp(str1, str2);
        }
    }
    return ret;
}

bool aux_compareStringsN(char *str1, char *str2, int len)
{
    bool ret = false;
    if(str1 == NULL)
    {
        if(str2 == NULL)
        {
            ret = true;
        }
        else
        {
            ret = false;
        }
    }
    else
    {
        if(str2 == NULL)
        {
            ret = false;
        }
        else
        {
            ret = !strncmp(str1, str2, len);
        }
    }
    return ret;
}

/**
 * Check if a received HAPCAN Frame matches the gateway filters
 */
bool aux_checkCAN2MQTTMatch(hapcanCANData *phd_received, 
        hapcanCANData *phd_mask, hapcanCANData *phd_check)
{
    int i;
    bool ret;
    hapcanCANData hd_result;
    // Fill the result and check it to leave as soon as possible
    hd_result.frametype = phd_received->frametype & phd_mask->frametype;
    hd_result.frametype = hd_result.frametype ^ phd_check->frametype;
    ret = (hd_result.frametype == 0);
    if(!ret)
    {
        return ret;
    }
    hd_result.module = phd_received->module & phd_mask->module;
    hd_result.module = hd_result.module ^ phd_check->module;
    ret = (hd_result.module == 0);
    if(!ret)
    {
        return ret;
    }
    hd_result.group = phd_received->group & phd_mask->group;
    hd_result.group = hd_result.group ^ phd_check->group;
    ret = (hd_result.group == 0);
    if(!ret)
    {
        return ret;
    }
    for(i = 0; i < CAN_MAX_DLEN; i++)
    {
        hd_result.data[i] = phd_received->data[i] & phd_mask->data[i];
        hd_result.data[i] = hd_result.data[i] ^ phd_check->data[i];
        ret = (hd_result.data[i] == 0);
        if(!ret)
        {
            return ret;
        }
    }
    // return - here it is true
    return ret;
}