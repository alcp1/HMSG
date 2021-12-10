//----------------------------------------------------------------------------//
//                               OBJECT HISTORY                               //
//----------------------------------------------------------------------------//
//  REVISION |    DATE     |                               |      AUTHOR      //
//----------------------------------------------------------------------------//
//  1.00     | 28/Nov/2021 |                               | ALCP             //
// - Creation of file                                                         //
//----------------------------------------------------------------------------//

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include "hapcan.h"
#include "hapcansocket.h"
#include "debug.h"

//----------------------------------------------------------------------------//
// INTERNAL DEFINITIONS
//----------------------------------------------------------------------------//

//----------------------------------------------------------------------------//
// INTERNAL GLOBAL VARIABLES
//----------------------------------------------------------------------------//

//----------------------------------------------------------------------------//
// INTERNAL FUNCTIONS
//----------------------------------------------------------------------------//

//----------------------------------------------------------------------------//
// EXTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
/**
 * Debug Initialization
 */
int debug_init(void)
{
    //return  EXIT_SUCCESS / EXIT_FAILURE
    return  EXIT_SUCCESS;
}

/**
 * Debug End
 */
int debug_end(void)
{
    //return  EXIT_SUCCESS / EXIT_FAILURE
    return  EXIT_SUCCESS;
}

/**
 * Print Debug Message
 */
void debug_print(const char * format, ...)
{
    char buff[20];
    struct tm *sTm;
    
    #ifdef DEBUG_ON    
    // Get Timestamp
    time_t now = time(0);
    sTm = gmtime (&now);
    strftime (buff, sizeof(buff), "%Y-%m-%d %H:%M:%S", sTm);    
    
    // Print
    va_list args;
    va_start (args, format);
    printf ("%s: ", buff);
    vprintf (format, args);
    va_end (args);
    
    // Will now print everything in the stdout buffer
    fflush(stdout);
    #endif
}

/**
 * Print CAN Message
 */
void debug_printCAN(const char * text, struct can_frame* const pcf_Frame)
{
    int li_Temp;
    hapcanCANData hapcanData;    
    
    #ifdef DEBUG_CAN_STANDARD    
    debug_print(text);
    debug_print("- CAN ID: 0x%08X\n", pcf_Frame->can_id);
    debug_print("- CAN Data:");
    for(li_Temp = 0; li_Temp < CAN_MAX_DLEN; li_Temp++)
    {
        printf("0x%02X ", pcf_Frame->data[li_Temp]);
    }
    printf("\n");
    #endif
    
    #ifdef DEBUG_CAN_HAPCAN 
    hapcan_getHAPCANDataFromCAN(pcf_Frame, &hapcanData);
    debug_print(text);
    debug_print("- HAPCAN Frame Type: 0x%03X\n", hapcanData.frametype);
    debug_print("- HAPCAN Flags: 0x%X\n", hapcanData.flags);
    debug_print("- HAPCAN Module: 0x%02X (%d in decimal)\n", hapcanData.module, hapcanData.module);
    debug_print("- HAPCAN Group: 0x%02X (%d in decimal)\n", hapcanData.group, hapcanData.group);
    debug_print("- HAPCAN Data D0 to D7: ");
    for(li_Temp = 0; li_Temp < HAPCAN_DATA_LEN; li_Temp++)
    {
        printf("0x%02X ", hapcanData.data[li_Temp]);
    }
    printf("\n");
    #endif
}

/**
 * Print CAN Message
 */
void debug_printHAPCAN(const char * text, hapcanCANData *hd)
{
    int li_Temp;    
    debug_print(text);
    debug_print("- HAPCAN Frame Type: 0x%03X\n", hd->frametype);
    debug_print("- HAPCAN Flags: 0x%X\n", hd->flags);
    debug_print("- HAPCAN Module: 0x%02X (%d in decimal)\n", hd->module, hd->module);
    debug_print("- HAPCAN Group: 0x%02X (%d in decimal)\n", hd->group, hd->group);
    debug_print("- HAPCAN Data D0 to D7: ");
    for(li_Temp = 0; li_Temp < HAPCAN_DATA_LEN; li_Temp++)
    {
        printf("0x%02X ", hd->data[li_Temp]);
    }
    printf("\n");
}

void debug_printSocket(const char * text, uint8_t* data, int dataLen)
{
    int li_Temp;
    hapcanCANData hapcanData;
    if(dataLen == HAPCAN_SOCKET_DATA_LEN)
    {
        hs_getHAPCANFromSocketArray(data, &hapcanData);
        debug_print(text);
        debug_print("- HAPCAN SOCKET DATA: \n");
        debug_print("- HAPCAN Frame Type: 0x%03X\n", hapcanData.frametype);
        debug_print("- HAPCAN Flags: 0x%X\n", hapcanData.flags);
        debug_print("- HAPCAN Module: 0x%02X (%d in decimal)\n", hapcanData.module, hapcanData.module);
        debug_print("- HAPCAN Group: 0x%02X (%d in decimal)\n", hapcanData.group, hapcanData.group);
        debug_print("- HAPCAN Data D0 to D7: ");
        for(li_Temp = 0; li_Temp < HAPCAN_DATA_LEN; li_Temp++)
        {
            printf("0x%02X ", hapcanData.data[li_Temp]);
        }
        printf("\n");
    }
    else
    {
        // Data not formatted as CAN Frame
        debug_print(text);
        debug_print("- HAPCAN SOCKET DATA: \n");
        debug_print(" (Data not formatted as CAN - only %d bytes) \n", dataLen);
        for(li_Temp = 0; li_Temp < dataLen; li_Temp++)
        {
            printf("0x%02X ", data[li_Temp]);
        }
        printf("\n");
    }
}