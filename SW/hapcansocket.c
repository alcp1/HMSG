//----------------------------------------------------------------------------//
//                               OBJECT HISTORY                               //
//----------------------------------------------------------------------------//
//  REVISION |    DATE     |                               |      AUTHOR      //
//----------------------------------------------------------------------------//
//  1.00     | 28/Nov/2021 |                               | ALCP             //
// - Creation of file                                                         //
//----------------------------------------------------------------------------//

/*
* Includes
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <limits.h>
#include "auxiliary.h"
#include "debug.h"
#include "errorhandler.h"
#include "hapcan.h"
#include "hapcanconfig.h"
#include "hapcansocket.h"
#include "mqtt.h"
#include "socketserverbuf.h"

//----------------------------------------------------------------------------//
// INTERNAL DEFINITIONS
//----------------------------------------------------------------------------//
#define HAPCAN_FRAME_OK             0
#define HAPCAN_FRAME_HEADER_ERROR   -1
#define HAPCAN_FRAME_CHECKSUM_ERROR -2

//----------------------------------------------------------------------------//
// INTERNAL TYPES
//----------------------------------------------------------------------------//


//----------------------------------------------------------------------------//
// INTERNAL GLOBAL VARIABLES
//----------------------------------------------------------------------------//


//----------------------------------------------------------------------------//
// INTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
static uint8_t hs_getChecksumFromSocket(const uint8_t* data, int dataLen);
static int hs_checkHAPCANFrame(uint8_t* data);
static int hs_setHAPCANResponseFromSocket(uint8_t* data, int dataLen, 
        uint8_t* sResponse, int* responseLen, int* responses, 
        hapcanCANData* hapcanData);

/**
 * Get Checksum from Socket Data
 * \param   data     pointer to HAPCAN Socket Data
 *                      
 * \return  Checksum
 *          
 */
static uint8_t hs_getChecksumFromSocket(const uint8_t* data, int dataLen)
{
    uint16_t  ui_Temp;
    int i;
    
    ui_Temp = 0;
    for(i = 1; i < dataLen - 2; i++)
    {
        ui_Temp += data[i];
    }
    
    // Return
    return (uint8_t)(ui_Temp & 0xFF);
}

/**
 * Check if the HAPCAN byte array is valid.
 * 
 * \param   data    HAPCAN Socket Data
 *  
 * \return  HAPCAN_FRAME_OK                 if data was set to buffer
 *          HAPCAN_FRAME_HEADER_ERROR       if there is no data on the buffer
 *          HAPCAN_FRAME_CHECKSUM_ERROR     if no data was set due to buffer error
 */
static int hs_checkHAPCANFrame(uint8_t* data)
{
    hapcanCANData hapcanData;
    uint8_t checksum;
    // Check header
    if((data[0] != 0xAA) || (data[HAPCAN_SOCKET_DATA_LEN - 1] != 0xA5))
    {
        return HAPCAN_FRAME_HEADER_ERROR;
    }
    // Get HAPCAN data and check checksum
    hs_getHAPCANFromSocketArray(data, &hapcanData);
    checksum = hapcan_getChecksumFromCAN(&hapcanData);
    if(checksum != data[HAPCAN_SOCKET_DATA_LEN - 2])
    {
        return HAPCAN_FRAME_CHECKSUM_ERROR;
    }
    else
    {
        return HAPCAN_FRAME_OK;
    }
}

/**
 * Define a response to a HAPCAN Message message received by the Socket Server
 * 
 * \param   data            Socket data (INPUT)
 * \param   dataLen         Socket data length (INPUT)
 * \param   sResponse       Socket Response Buffer to be filled (OUTPUT)
 * \param   responseLen     Socket data length to be filled (OUTPUT)
 * \param   responses       Number of Socket data responses (OUTPUT)
 * \param   hapcanData      HAPCAN Socket Data to be filled (OUTPUT)
 *  
 * \return  HAPCAN_NO_RESPONSE              No response needed
 *          HAPCAN_SOCKET_RESPONSE          Socket response filled to be send
 *          HAPCAN_CAN_RESPONSE             CAN response filled to be send
 *          HAPCAN_RESPONSE_ERROR           No defined answer found - error
 **/
static int hs_setHAPCANResponseFromSocket(uint8_t* data, int dataLen, 
        uint8_t* sResponse, int* responseLen, int* responses, 
        hapcanCANData* hapcanData)
{
    uint16_t ui_Temp;
    uint8_t ub_checksum;
    int i_index;
    int i_check = HAPCAN_RESPONSE_ERROR;
    int c_ID1;
    int c_ID2;
    
    //-------------------------------------------------------------------------
    // Check for basic HEADER and STOP error 
    //-------------------------------------------------------------------------
    if((data[0] != 0xAA) || (data[dataLen - 1] != 0xA5))
    {
        return HAPCAN_RESPONSE_ERROR;
    }
    //-------------------------------------------------------------------------
    // Check for Checksum error
    //-------------------------------------------------------------------------
    ui_Temp = 0;
    ub_checksum = hs_getChecksumFromSocket(data, dataLen); 
    if(ub_checksum != data[dataLen - 2])
    {
        return HAPCAN_RESPONSE_ERROR;
    }
    //-------------------------------------------------------------------------
    // Check for UART system messages
    //-------------------------------------------------------------------------
    if(dataLen == 5)
    {
        ui_Temp = (data[1]) << 8;
        ui_Temp += data[2];
        switch(ui_Temp) 
        {
            case 0x1000: // x100 – enter into programming mode request to node
                responseLen[0] = 13;
                *responses = 1;
                i_index = 0;
                sResponse[i_index] = 0xAA; i_index++;
                sResponse[i_index] = 0x10; i_index++;
                sResponse[i_index] = 0x41; i_index++;
                sResponse[i_index] = 0xFF; i_index++;
                sResponse[i_index] = 0xFF; i_index++;
                sResponse[i_index] = HAPCAN_HW_BVER1; i_index++; // BVER1
                sResponse[i_index] = HAPCAN_HW_BVER2; i_index++; // BVER2
                sResponse[i_index] = 0xFF; i_index++;
                sResponse[i_index] = 0xFF; i_index++;
                sResponse[i_index] = 0xFF; i_index++;
                sResponse[i_index] = 0xFF; i_index++;
                sResponse[i_index + 1] = 0xA5;
                sResponse[i_index] = hs_getChecksumFromSocket(sResponse, responseLen[0]);
                i_check = HAPCAN_SOCKET_RESPONSE;
                break;
            case 0x1020: //0x102 – reboot request to node
                i_check = HAPCAN_NO_RESPONSE;
                break;
            case 0x1040: // 0x104 – hardware type request to node
                responseLen[0] = 13;
                *responses = 1;
                i_index = 0;
                sResponse[i_index] = 0xAA; i_index++;                
                sResponse[i_index] = 0x10; i_index++;
                sResponse[i_index] = 0x41; i_index++;
                sResponse[i_index] = (uint8_t)(HAPCAN_HW_HWTYPE >> 8); i_index++; // HARD1
                sResponse[i_index] = (uint8_t)(HAPCAN_HW_HWTYPE & 0xFF); i_index++; // HARD2
                sResponse[i_index] = HAPCAN_HW_HWVER; i_index++; // HVER
                sResponse[i_index] = 0xFF; i_index++; 
                sResponse[i_index] = HAPCAN_HW_ID0; i_index++; // ID0
                sResponse[i_index] = HAPCAN_HW_ID1; i_index++; // ID1
                sResponse[i_index] = HAPCAN_HW_ID2; i_index++; // ID2
                sResponse[i_index] = HAPCAN_HW_ID3; i_index++; // ID3
                sResponse[i_index + 1] = 0xA5;
                sResponse[i_index] = hs_getChecksumFromSocket(sResponse, responseLen[0]);
                i_check = HAPCAN_SOCKET_RESPONSE;
                break;
            case 0x1060: // 0x106 – firmware type request to node
                responseLen[0] = 13;
                *responses = 1;
                i_index = 0;
                sResponse[i_index] = 0xAA; i_index++;                
                sResponse[i_index] = 0x10; i_index++;
                sResponse[i_index] = 0x61; i_index++;
                sResponse[i_index] = (uint8_t)(HAPCAN_HW_HWTYPE >> 8); i_index++; // HARD1
                sResponse[i_index] = (uint8_t)(HAPCAN_HW_HWTYPE & 0xFF); i_index++; // HARD2
                sResponse[i_index] = HAPCAN_HW_HWVER; i_index++; // HVER
                sResponse[i_index] = HAPCAN_HW_ATYPE; i_index++; // ATYPE
                sResponse[i_index] = HAPCAN_HW_AVERS; i_index++; // AVERS
                sResponse[i_index] = HAPCAN_HW_FVERS; i_index++; // FVERS
                sResponse[i_index] = HAPCAN_HW_BVER1; i_index++; // BVER1
                sResponse[i_index] = HAPCAN_HW_BVER2; i_index++; // BREV2
                sResponse[i_index + 1] = 0xA5;
                sResponse[i_index] = hs_getChecksumFromSocket(sResponse, responseLen[0]);
                i_check = HAPCAN_SOCKET_RESPONSE;
                break;
            case 0x10C0: // 0x10C – supply voltage request to node
                responseLen[0] = 13;
                *responses = 1;
                i_index = 0;
                sResponse[i_index] = 0xAA; i_index++;                
                sResponse[i_index] = 0x10; i_index++;
                sResponse[i_index] = 0xC1; i_index++;
                sResponse[i_index] = HAPCAN_VOLBUS1; i_index++; // VOLBUS1
                sResponse[i_index] = HAPCAN_VOLBUS2; i_index++; // VOLBUS2
                sResponse[i_index] = HAPCAN_VOLCPU1; i_index++; // VOLCPU1
                sResponse[i_index] = HAPCAN_VOLCPU2; i_index++; // VOLCPU2
                sResponse[i_index] = 0xFF; i_index++;
                sResponse[i_index] = 0xFF; i_index++;
                sResponse[i_index] = 0xFF; i_index++;
                sResponse[i_index] = 0xFF; i_index++;
                sResponse[i_index + 1] = 0xA5;
                sResponse[i_index] = hs_getChecksumFromSocket(sResponse, responseLen[0]);
                i_check = HAPCAN_SOCKET_RESPONSE;
                break;
            case 0x10E0: // 0x10E – description request to node
                responseLen[0] = 13;
                responseLen[1] = 13;
                *responses = 2;
                i_index = 0;
                sResponse[i_index] = 0xAA; i_index++;
                sResponse[i_index] = 0x10; i_index++;
                sResponse[i_index] = 0xE1; i_index++;
                sResponse[i_index] = 'H'; i_index++;
                sResponse[i_index] = 'M'; i_index++;
                sResponse[i_index] = 'S'; i_index++; 
                sResponse[i_index] = 'G'; i_index++; 
                sResponse[i_index] = '-'; i_index++;
                sResponse[i_index] = 'r'; i_index++; 
                sResponse[i_index] = 'P'; i_index++; 
                sResponse[i_index] = 'i'; i_index++;
                sResponse[i_index + 1] = 0xA5;
                sResponse[i_index] = hs_getChecksumFromSocket(sResponse, responseLen[0]);
                i_index++;
                i_index++;
                sResponse[i_index] = 0xAA; i_index++;
                sResponse[i_index] = 0x10; i_index++;
                sResponse[i_index] = 0xE1; i_index++;                
                sResponse[i_index] = 'H'; i_index++;
                sResponse[i_index] = 'M'; i_index++;
                sResponse[i_index] = 'S'; i_index++; 
                sResponse[i_index] = 'G'; i_index++; 
                sResponse[i_index] = '-'; i_index++;
                sResponse[i_index] = 'r'; i_index++; 
                sResponse[i_index] = 'P'; i_index++; 
                sResponse[i_index] = 'i'; i_index++;
                sResponse[i_index + 1] = 0xA5;
                sResponse[i_index] = hs_getChecksumFromSocket(&(sResponse[responseLen[0]]), responseLen[1]); i_index++;
                i_check = HAPCAN_SOCKET_RESPONSE;
                break;
            case 0x1110: // 0x111 – DEV ID request to node
                responseLen[0] = 13;
                *responses = 1;
                i_index = 0;
                sResponse[i_index] = 0xAA; i_index++;                
                sResponse[i_index] = 0x11; i_index++;
                sResponse[i_index] = 0x11; i_index++;
                sResponse[i_index] = HAPCAN_DEVID1; i_index++; // DEV ID1
                sResponse[i_index] = HAPCAN_DEVID2; i_index++; // DEV ID2
                sResponse[i_index] = 0xFF; i_index++; 
                sResponse[i_index] = 0xFF; i_index++; 
                sResponse[i_index] = 0xFF; i_index++;
                sResponse[i_index] = 0xFF; i_index++;
                sResponse[i_index] = 0xFF; i_index++;
                sResponse[i_index] = 0xFF; i_index++;
                sResponse[i_index + 1] = 0xA5;
                sResponse[i_index] = hs_getChecksumFromSocket(sResponse, responseLen[0]);
                i_check = HAPCAN_SOCKET_RESPONSE;
                break;
            default:
                i_check = HAPCAN_RESPONSE_ERROR;
                break;
        }
    }
    if(i_check != HAPCAN_RESPONSE_ERROR)
    {
        return i_check;
    }
    //-------------------------------------------------------------------------
    // Check for Messages addressed to the Ethernet port
    //-------------------------------------------------------------------------
    if(dataLen == 13)
    {
        ui_Temp = (data[1]) << 8;
        ui_Temp += data[2];
        switch(ui_Temp) 
        {
            case 0x1090: // STATUS REQUEST (0x109) – Ethernet port
                if(hconfig_getConfigInt(HAPCAN_CONFIG_COMPUTER_ID1, &c_ID1)
                        != EXIT_SUCCESS)
                {
                    c_ID1 = HAPCAN_DEFAULT_CIDx;
                }
                if(hconfig_getConfigInt(HAPCAN_CONFIG_COMPUTER_ID2, &c_ID2)
                        != EXIT_SUCCESS)
                {
                    c_ID2 = HAPCAN_DEFAULT_CIDx;
                }
                responseLen[0] = 15;
                *responses = 1;
                i_index = 0;
                sResponse[i_index] = 0xAA; i_index++;
                sResponse[i_index] = 0x30; i_index++;
                sResponse[i_index] = 0x01; i_index++;
                sResponse[i_index] = c_ID1; i_index++; 
                sResponse[i_index] = c_ID2; i_index++;
                sResponse[i_index] = 0xFF; i_index++; 
                aux_getHAPCANTime(&(sResponse[i_index])); i_index+=7;
                sResponse[i_index + 1] = 0xA5;
                sResponse[i_index] = hs_getChecksumFromSocket(sResponse, responseLen[0]);
                i_check = HAPCAN_SOCKET_RESPONSE;
                break;
            case 0x1130: // UPTIME REQUEST (0x113) – Ethernet port
                if(hconfig_getConfigInt(HAPCAN_CONFIG_COMPUTER_ID1, &c_ID1)
                        != EXIT_SUCCESS)
                {
                    c_ID1 = HAPCAN_DEFAULT_CIDx;
                }
                if(hconfig_getConfigInt(HAPCAN_CONFIG_COMPUTER_ID2, &c_ID2)
                        != EXIT_SUCCESS)
                {
                    c_ID2 = HAPCAN_DEFAULT_CIDx;
                }
                responseLen[0] = 15;
                *responses = 1;
                i_index = 0;
                sResponse[i_index] = 0xAA; i_index++;
                sResponse[i_index] = 0x11; i_index++;
                sResponse[i_index] = 0x31; i_index++;
                sResponse[i_index] = c_ID1; i_index++;  // NODE
                sResponse[i_index] = c_ID2; i_index++;  // GROUP
                sResponse[i_index] = 0xFF; i_index++; 
                sResponse[i_index] = 0xFF; i_index++; 
                sResponse[i_index] = 0xFF; i_index++; 
                sResponse[i_index] = 0xFF; i_index++; 
                aux_getHAPCANUptime(&(sResponse[i_index])); i_index+=4;
                sResponse[i_index + 1] = 0xA5;
                sResponse[i_index] = hs_getChecksumFromSocket(sResponse, responseLen[0]);
                i_check = HAPCAN_SOCKET_RESPONSE;
                break;
            default:
                i_check = HAPCAN_RESPONSE_ERROR;
                break;
        }
    }
    if(i_check != HAPCAN_RESPONSE_ERROR)
    {
        return i_check;
    }
    //-------------------------------------------------------------------------
    // Check for Messages addressed to the CAN Bus
    //-------------------------------------------------------------------------
    if(dataLen == HAPCAN_SOCKET_DATA_LEN)
    {
        if(hs_checkHAPCANFrame(data) == HAPCAN_FRAME_OK)
        {
            hs_getHAPCANFromSocketArray(data, hapcanData);
            i_check = HAPCAN_CAN_RESPONSE;
        }
        else
        {
            i_check = HAPCAN_RESPONSE_ERROR;
        }
    }
    return i_check;
}

//----------------------------------------------------------------------------//
// EXTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
/* Socket byte array from HAPCAN Data */
void hs_getSocketArrayFromHAPCAN(hapcanCANData* hCD_ptr, uint8_t* hSD_ptr)
{
    int i;
    int li_dataIndex;
    i = 0;
    
    // START
    hSD_ptr[i] = 0xAA;
    i++;
    
    // Frame Type and Flags
    hSD_ptr[i] = (uint8_t)(hCD_ptr->frametype >> 4);
    i++;
    hSD_ptr[i] = (uint8_t)((hCD_ptr->frametype << 4)&(0xF0)); 
    hSD_ptr[i] += hCD_ptr->flags; 
    i++;
    
    // Module
    hSD_ptr[i] = hCD_ptr->module;
    i++;
    
    // Group
    hSD_ptr[i] = hCD_ptr->group;
    i++;
    
    // Data
    for(li_dataIndex = 0; li_dataIndex < HAPCAN_DATA_LEN; li_dataIndex++)
    {
        hSD_ptr[i] = hCD_ptr->data[li_dataIndex];
        i++;
    }
    
    // CHECKSUM
    hSD_ptr[i] = hapcan_getChecksumFromCAN(hCD_ptr);
    i++;
    
    // STOP
    hSD_ptr[i] = 0xA5;
}

/* HAPCAN Data from Socket byte array */
void hs_getHAPCANFromSocketArray(uint8_t* hSD_ptr, hapcanCANData* hCD_ptr)
{
    int i;
    int li_dataIndex;
    i = 1;
       
    // Frame Type and Flags
    hCD_ptr->frametype = (uint16_t)(hSD_ptr[i])<< 8;
    i++;
    hCD_ptr->frametype += (uint16_t)(hSD_ptr[i] & 0xF0);
    hCD_ptr->frametype = hCD_ptr->frametype >> 4;
    hCD_ptr->flags = (hSD_ptr[i] & 0x0F);
    i++;
    
    // Module
    hCD_ptr->module = hSD_ptr[i];
    i++;
    
    // Group
    hCD_ptr->group = hSD_ptr[i];
    i++;
    
    // Data
    for(li_dataIndex = 0; li_dataIndex < HAPCAN_DATA_LEN; li_dataIndex++)
    {
        hCD_ptr->data[li_dataIndex] = hSD_ptr[i];
        i++;
    }
}


/**
 * Handle a message received by the Socket Server (sent by HAPCAN PROGRAMMER)
 **/
void hs_handleMsgFromSocket(uint8_t* data, int dataLen, 
        unsigned long long timestamp)
{
    int check;
    uint8_t sResponse[HAPCAN_MAX_RESPONSES * HAPCAN_SOCKET_DATA_LEN];
    int responseLen[HAPCAN_MAX_RESPONSES];
    int offset[HAPCAN_MAX_RESPONSES];
    int responses;
    hapcanCANData hapcanData;
    int i;
    check = hs_setHAPCANResponseFromSocket(data, dataLen, sResponse, 
            responseLen, &responses, &hapcanData);
    switch(check) 
    {
        case HAPCAN_NO_RESPONSE:
            // Do nothing
            break;
        case HAPCAN_SOCKET_RESPONSE:
            // For the case we need to respond to the socket client (PROGRAMMER) 
            // instead of sending a CAN message
            offset[0] = 0;
            for(i = 1; i < responses; i++)
            {
               offset[i] = offset[i - 1];
               offset[i] += responseLen[i - 1];
            }
            for(i = 0; i < responses; i++)
            {
                check = socketserverbuf_setWriteMsgToBuffer(
                        &(sResponse[offset[i]]), responseLen[i], timestamp);
                // Handle the error
                errorh_isError(ERROR_MODULE_SOCKETSERVER_SEND, check);                
            }
            break;
        case HAPCAN_CAN_RESPONSE:
            // For the case we need to send a CAN Frame from client (PROGRAMMER)
            // (error is handled within the function)
            hapcan_addToCANWriteBuffer(&hapcanData, timestamp, false);                                                                            
            break;
        case HAPCAN_RESPONSE_ERROR:
            #ifdef DEBUG_SOCKETSERVER_PROCESS_ERROR            
            debug_printSocket("hs_handleMsgFromSocket - "
                    "HAPCAN_RESPONSE_ERROR - Socket Data:\n", data, dataLen);
            debug_printHAPCAN("hs_handleMsgFromSocket - "
                    "HAPCAN_RESPONSE_ERROR - HAPCAN DATA:\n", &hapcanData);
            #endif
            break;
        default: // HAPCAN_NO_RESPONSE
            break;
    }
}