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
#include "canbuf.h"
#include "config.h"
#include "debug.h"
#include "errorhandler.h"
#include "gateway.h"
#include "hapcan.h"
#include "hapcanconfig.h"
#include "hapcanmqtt.h"
#include "hapcanbutton.h"
#include "hapcanrelay.h"
#include "hapcanrgb.h"
#include "hapcansocket.h"
#include "hapcansystem.h"
#include "hapcantemperature.h"
#include "jsonhandler.h"
#include "mqtt.h"
#include "mqttbuf.h"
#include "socketserverbuf.h"

//----------------------------------------------------------------------------//
// INTERNAL DEFINITIONS
//----------------------------------------------------------------------------//

//----------------------------------------------------------------------------//
// INTERNAL TYPES
//----------------------------------------------------------------------------//

//----------------------------------------------------------------------------//
// INTERNAL GLOBAL VARIABLES
//----------------------------------------------------------------------------//

//----------------------------------------------------------------------------//
// INTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
// From CAN to MQTT: use CAN message to send MQTT response(s)
static int handleRawFromCAN(hapcanCANData* hapcanData, 
        unsigned long long timestamp);
static int handleConfiguredFromCAN(hapcanCANData* hapcanData, 
        unsigned long long timestamp);
static int getModuleResponseFromCAN(char *state_str, hapcanCANData *hd_received, 
        unsigned long long timestamp);
// From MQTT to CAN: use MQTT data to send CAN response(s)
static int handleRawFromMQTT(char* topic, void* payload, int payloadlen, 
        unsigned long long timestamp);
static int handleConfiguredFromMQTT(char* topic, void* payload, int payloadlen, 
        unsigned long long timestamp);
static int getModuleResponseFromMQTT(hapcanCANData *hd_result, char* topic, 
        void* payload, int payloadlen, unsigned long long timestamp);

/*
 * Send the RAW MQTT response for a HAPCAN message
 * \param   hapcanData      (INPUT) received HAPCAN Frame
 *          timestamp       (INPUT) Received message timestamp
 * 
 * \return  HAPCAN_NO_RESPONSE: No RAW response was added to MQTT Pub Buffer
 *          HAPCAN_MQTT_RESPONSE: RAW Response added to MQTT Buffer (OK)
 *          HAPCAN_MQTT_RESPONSE_ERROR: Error adding to MQTT Pub Buffer (FAIL)
 */
static int handleRawFromCAN(hapcanCANData* hapcanData, 
        unsigned long long timestamp)
{
    int check;
    int ret;
    char* topic = NULL;
    void* payload = NULL;
    int payloadlen;
    // Init with no response
    ret = HAPCAN_NO_RESPONSE;
    // Use CAN message to set MQTT Generic message response
    check = hm_setRawResponseFromCAN(hapcanData, &topic, 
            &payload, &payloadlen);
    #if defined(DEBUG_HAPCAN_CAN2MQTT)
    debug_print("handleRawFromCAN - Raw message check = %d\n", 
            check);
    #endif
    if(check == HAPCAN_RESPONSE_ERROR)
    {
        // APPLICATION ERROR: no need to set "ret" to error - simply there 
        // will be no MQTT response to be sent
        #if defined(DEBUG_HAPCAN_CAN2MQTT) || defined(DEBUG_HAPCAN_ERRORS)
        debug_print("handleRawFromCAN - "
                "ERROR: Check configuration - wrong topic!\n");
        #endif
    }
    if(check == HAPCAN_MQTT_RESPONSE)
    {
        ret = hapcan_addToMQTTPubBuffer(topic, payload, payloadlen, timestamp);
    }
    // FREE RAW MQTT DATA
    free(topic);
    topic = NULL;
    free(payload);
    payload = NULL;
    // Return
    return ret;
}

/*
 * Send the MQTT response for a HAPCAN message based on the configured modules
 * \param   hapcanData      (INPUT) received HAPCAN Frame
 *          timestamp       (INPUT) Received message timestamp
 * 
 * \return  HAPCAN_NO_RESPONSE: No RAW response was added to MQTT Pub Buffer
 *          HAPCAN_MQTT_RESPONSE: RAW Response added to MQTT Buffer (OK)
 *          HAPCAN_MQTT_RESPONSE_ERROR: Error adding to MQTT Pub Buffer (FAIL)
 */
static int handleConfiguredFromCAN(hapcanCANData* hapcanData, 
        unsigned long long timestamp)
{
    int check;
    int ret;
    char* topic = NULL;
    int offset;
    // Init with no response
    ret = HAPCAN_NO_RESPONSE;
    //------------------------------------------
    // Specific (configured) MQTT response
    //------------------------------------------
    // Search for a gateway match
    check = 0;
    offset = 0;
    while(check >=0)
    {
        check = gateway_searchMQTTFromCAN(hapcanData, offset);
        if(check >= 0)                            
        {
            // Match found
            offset = check;
            #ifdef DEBUG_HAPCAN_CAN2MQTT
            debug_print("handleConfiguredFromCAN - match found at "
                    "offset = %d\n", offset);
            #endif
            check = gateway_getMQTTFromCAN(offset, &topic);
            if(check == EXIT_SUCCESS)
            {
                // Match found - check the frame type to send the response
                ret = getModuleResponseFromCAN(topic, hapcanData, 
                        timestamp);
                #ifdef DEBUG_HAPCAN_CAN2MQTT
                debug_print("handleConfiguredFromCAN - Response = %d\n", ret);
                #endif
                if(ret != HAPCAN_MQTT_RESPONSE_ERROR)
                {
                    // Set check to offset to continue loop to check for other 
                    // matches
                    check = offset;
                }
                else
                {
                    // Free memory and leave loop
                    check = -1;
                }
            }
            else
            {
                //------------------
                // GATEWAY ERROR
                //------------------
                #if defined(DEBUG_HAPCAN_CAN2MQTT)||defined(DEBUG_HAPCAN_ERRORS)
                debug_print("handleConfiguredFromCAN - MQTT Data read ERROR\n");
                #endif
                // Free memory and leave loop
                check = -1;
            }
            // Skip to the next position - This one was valuated already.
            offset++;
            // FREE DATA
            free(topic);
            topic = NULL;
        }
        else
        {
            #ifdef DEBUG_HAPCAN_CAN2MQTT
            debug_print("handleConfiguredFromCAN - No match found since "
                    "offset = %d\n", offset);
            #endif
        }
    }
    return ret;
}

/*
 * Check which module shall process the incoming message to set the payload and 
 * send it to the defined topic
 * 
 * \param   state_str       (INPUT) state topic found by the gateway
 *          hd_received     (INPUT) Received HAPCAN Frame
 *          timestamp       (INPUT) Received message timestamp
 * 
 * \return  HAPCAN_NO_RESPONSE: No response was added to MQTT Pub Buffer
 *          HAPCAN_MQTT_RESPONSE: Response added to MQTT Buffer (OK)
 *          HAPCAN_MQTT_RESPONSE_ERROR: Error adding to MQTT Pub Buffer (FAIL)
 *          HAPCAN_RESPONSE_ERROR: Other error
 */
static int getModuleResponseFromCAN(char *state_str, hapcanCANData *hd_received, 
        unsigned long long timestamp)
{
    int ret = HAPCAN_NO_RESPONSE;    
    switch(hd_received->frametype)
    {
        case HAPCAN_BUTTON_FRAME_TYPE:
            ret = hbutton_setCAN2MQTTResponse(state_str, hd_received, 
                    timestamp);
            break;
        case HAPCAN_RELAY_FRAME_TYPE:
            ret = hrelay_setCAN2MQTTResponse(state_str, hd_received, 
                    timestamp);
            break;
        case HAPCAN_TEMPERATURE_FRAME_TYPE:
            ret = htemp_setCAN2MQTTResponse(state_str, hd_received, 
                    timestamp);
            break;
        case HAPCAN_RGB_FRAME_TYPE:
            ret = hrgb_setCAN2MQTTResponse(state_str, hd_received, 
                    timestamp);
            break;
        default:
            break;
    }
    return ret;
}

/*
 * Send the HAPCAN Frame based on the Raw MQTT message received
 * 
 * \param   topic           (INPUT) received topic
 *          payload         (INPUT) received payload
 *          payloadlen      (INPUT) received payload len
 *          timestamp       (INPUT) Received message timestamp
 * 
 * \return  HAPCAN_NO_RESPONSE: No response was added to CAN Write Buffer
 *          HAPCAN_CAN_RESPONSE: Response added to CAN Write Buffer (OK)
 *          HAPCAN_CAN_RESPONSE_ERROR: Error adding to MQTT Pub Buffer (FAIL)
 *          HAPCAN_RESPONSE_ERROR: Other error
 */
static int handleRawFromMQTT(char* topic, void* payload, int payloadlen, 
        unsigned long long timestamp)
{
    int check;
    int ret;
    hapcanCANData hapcanData;
    // Init with no response
    ret = HAPCAN_NO_RESPONSE;
    // Use CAN message to set MQTT Generic message response
    check = hm_setRawResponseFromMQTT(topic, payload, payloadlen, &hapcanData);
    #if defined(DEBUG_HAPCAN_MQTT2CAN)
    debug_print("handleRawFromMQTT - Raw message check = %d\n", check);
    #endif
    if(check == HAPCAN_CAN_RESPONSE)
    {
        check = hapcan_addToCANWriteBuffer(&hapcanData, timestamp, true);
        if(check == HAPCAN_CAN_RESPONSE)
        {
            ret = HAPCAN_MQTT_RESPONSE;
        }
    }
    // Return
    return ret;
}

/*
 * Send the HAPCAN response based on the MQTT message received (search gateway 
 * for a match and define appropriate response).
 * 
 * \param   topic           (INPUT) received topic
 *          payload         (INPUT) received payload
 *          payloadlen      (INPUT) received payload len
 *          timestamp       (INPUT) Received message timestamp
 * 
 * \return  HAPCAN_NO_RESPONSE: No response was added to CAN Write Buffer
 *          HAPCAN_CAN_RESPONSE: Response added to CAN Write Buffer (OK)
 *          HAPCAN_CAN_RESPONSE_ERROR: Error adding to CAN Write Buffer (FAIL)
 *          HAPCAN_RESPONSE_ERROR: Other error
 */
static int handleConfiguredFromMQTT(char* topic, void* payload, int payloadlen, 
        unsigned long long timestamp)
{
    int check;
    int ret;
    int offset;
    hapcanCANData hapcanResult;
    // Init with no response
    ret = HAPCAN_NO_RESPONSE;
    //------------------------------------------
    // Specific (configured) HAPCAN response
    //------------------------------------------
    // Search for a gateway match
    check = 0;
    offset = 0;
    while(check >=0)
    {
        check = gateway_searchCANFromMQTT(topic, offset);
        if(check >= 0)                            
        {
            // Match found
            offset = check;
            #ifdef DEBUG_HAPCAN_MQTT2CAN
            debug_print("handleConfiguredFromMQTT - match found at "
                    "offset = %d\n", offset);
            #endif
            check = gateway_getCANFromMQTT(offset, &hapcanResult);
            if(check == EXIT_SUCCESS)
            {
                // Match found - check the frame type to send the response
                ret = getModuleResponseFromMQTT(&hapcanResult, topic, payload, 
                        payloadlen, timestamp);
                #ifdef DEBUG_HAPCAN_MQTT2CAN
                debug_print("handleConfiguredFromMQTT: Response is %d\n", ret);
                #endif
                if(ret != HAPCAN_CAN_RESPONSE_ERROR)
                {
                    // Set check to offset to continue loop to check for other 
                    // matches
                    check = offset;
                }
                else
                {
                    // Free memory and leave loop
                    check = -1;
                }
            }
            else
            {
                //------------------
                // GATEWAY ERROR
                //------------------
                #if defined(DEBUG_HAPCAN_MQTT2CAN)||defined(DEBUG_HAPCAN_ERRORS)
                debug_print("handleConfiguredFromMQTT - CAN Data read ERROR\n");
                #endif
                // Free memory and leave loop
                check = -1;
            }
            // Skip to the next position - This one was valuated already.
            offset++;
        }
        else
        {
            #ifdef DEBUG_HAPCAN_MQTT2CAN
            debug_print("handleConfiguredFromMQTT - No match found since "
                    "offset = %d\n", offset);
            #endif
        }
    }
    return ret;
}

/*
 * Check which module shall process the incoming message to set the HAPCAN 
 * response
 * 
 * \param   hd_result       (INPUT) HAPCAN frame found by the gateway
 *          topic           (INPUT) received topic
 *          payload         (INPUT) received payload
 *          payloadlen      (INPUT) received payload len
 *          timestamp       (INPUT) Received message timestamp
 * 
 * \return  HAPCAN_NO_RESPONSE: No response was added to MQTT Pub Buffer
 *          HAPCAN_MQTT_RESPONSE: Response added to MQTT Buffer (OK)
 *          HAPCAN_MQTT_RESPONSE_ERROR: Error adding to MQTT Pub Buffer (FAIL)
 *          HAPCAN_RESPONSE_ERROR: Other error
 */
static int getModuleResponseFromMQTT(hapcanCANData *hd_result, char* topic, 
        void* payload, int payloadlen, unsigned long long timestamp)
{
    int ret = HAPCAN_RESPONSE_ERROR;
    switch(hd_result->frametype)
    {
        case HAPCAN_BUTTON_FRAME_TYPE:
            ret = hbutton_setMQTT2CANResponse(hd_result, payload, payloadlen, 
                    timestamp);
            break;
        case HAPCAN_RELAY_FRAME_TYPE:
            ret = hrelay_setMQTT2CANResponse(hd_result, payload, payloadlen, 
                    timestamp);            
            break;
        case HAPCAN_TEMPERATURE_FRAME_TYPE:
            ret = htemp_setMQTT2CANResponse(hd_result, payload, payloadlen, 
                    timestamp);
            break;
        case HAPCAN_RGB_FRAME_TYPE:
            ret = hrgb_setMQTT2CANResponse(hd_result, payload, payloadlen, 
                    timestamp);
            break;
        default:
            break;
    }
    return ret;
}

//----------------------------------------------------------------------------//
// EXTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
/* From CAN to HAPCAN */
void hapcan_getHAPCANDataFromCAN(struct can_frame* pcf_Frame, hapcanCANData* hCD_ptr)
{
    uint32_t ul_Id;
    uint16_t ui_Temp;
    uint8_t  ub_Temp;
    int li_index;
    
    // Frame Type
    ul_Id = pcf_Frame->can_id;
    ui_Temp = (uint16_t)(ul_Id >> 17);
    hCD_ptr->frametype = ui_Temp;
    
    // Flags
    ul_Id = pcf_Frame->can_id;
    ub_Temp = (uint8_t)(ul_Id >> 16);
    ub_Temp = ub_Temp & 0b00000001;
    hCD_ptr->flags = ub_Temp;
    
    // Module
    ul_Id = pcf_Frame->can_id;
    ub_Temp = (uint8_t)(ul_Id >> 8);
    hCD_ptr->module = ub_Temp;
    
    // Group
    ul_Id = pcf_Frame->can_id;
    ub_Temp = (uint8_t)(ul_Id);
    hCD_ptr->group = ub_Temp;
    
    // Data
    for(li_index = 0; li_index < HAPCAN_DATA_LEN; li_index++)
    {
        hCD_ptr->data[li_index] = pcf_Frame->data[li_index];
    }
}

/* From HAPCAN to CAN */
void hapcan_getCANDataFromHAPCAN(hapcanCANData* hCD_ptr, struct can_frame* pcf_Frame)
{
    uint32_t ul_Id;
    uint16_t ui_Temp;
    uint8_t  ub_Temp;
    int li_index;
    
    // Group
    pcf_Frame->can_id = hCD_ptr->group;
    
    // Module
    ub_Temp = hCD_ptr->module;
    ul_Id = (uint32_t)(ub_Temp << 8);
    pcf_Frame->can_id += ul_Id;
    
    // Flags
    ub_Temp = hCD_ptr->flags;
    ub_Temp = ub_Temp & 0b00000001;
    ul_Id = (uint32_t)(ub_Temp << 16);
    pcf_Frame->can_id += ul_Id;
    
    // Frame Type
    ui_Temp = hCD_ptr->frametype;
    ul_Id = (uint32_t)(ui_Temp << 17);
    pcf_Frame->can_id += ul_Id;
    
    // Data Length
    pcf_Frame->can_dlc = CAN_MAX_DLEN;
    
    // Data
    for(li_index = 0; li_index < HAPCAN_DATA_LEN; li_index++)
    {
        pcf_Frame->data[li_index] = hCD_ptr->data[li_index];
    }
}

/* Checksum from data */
uint8_t hapcan_getChecksumFromCAN(hapcanCANData* hCD_ptr)
{
    uint16_t  ui_Temp;
    int li_index;
    
    // Frame Type
    ui_Temp = (uint8_t)(hCD_ptr->frametype >> 4);
    ui_Temp += (uint8_t)((hCD_ptr->frametype << 4)&(0xF0));
    
    // Flags
    ui_Temp += hCD_ptr->flags;
    
    // Module
    ui_Temp += hCD_ptr->module;
    
    // Group
    ui_Temp += hCD_ptr->group;
        
    // Data
    for(li_index = 0; li_index < HAPCAN_DATA_LEN; li_index++)
    {
        ui_Temp += hCD_ptr->data[li_index];
    }
    
    // Return
    return (uint8_t)(ui_Temp & 0xFF);
}


/** Set the RTC message */
void hapcan_setHAPCANRTCMessage(hapcanCANData* hCD_ptr)
{
    int check;
    int c_ID1;
    int c_ID2;
    //---------------------------
    // Read configuration
    //---------------------------
    check = hconfig_getConfigInt(HAPCAN_CONFIG_COMPUTER_ID1, &c_ID1);
    if(check != EXIT_SUCCESS)
    {
        c_ID1 = HAPCAN_DEFAULT_CIDx;
    }
    check = hconfig_getConfigInt(HAPCAN_CONFIG_COMPUTER_ID2, &c_ID2);
    if(check != EXIT_SUCCESS)
    {
        c_ID2 = HAPCAN_DEFAULT_CIDx;
    }	
    hCD_ptr->frametype = HAPCAN_RTC_FRAME_TYPE;
    hCD_ptr->flags = 0x00;
    hCD_ptr->module = c_ID1;
    hCD_ptr->group = c_ID2;
    hCD_ptr->data[0] = 0xFF;
    aux_getHAPCANTime(&(hCD_ptr->data[1]));
    hCD_ptr->data[7] = 0x00;
}

/**
 * Fill HAPCAN data with a system message based on destination node and group, 
 * for a given frametype
 */
void hapcan_getSystemFrame(hapcanCANData *hd_result, uint16_t frametype, 
        int node, int group)
{
    int c_ID1;
    int c_ID2;
    int check;
    int i;
    //---------------------------
    // Read configuration
    //---------------------------
    check = hconfig_getConfigInt(HAPCAN_CONFIG_COMPUTER_ID1, &c_ID1);
    if(check != EXIT_SUCCESS)
    {
        c_ID1 = HAPCAN_DEFAULT_CIDx;
    }
    check = hconfig_getConfigInt(HAPCAN_CONFIG_COMPUTER_ID2, &c_ID2);
    if(check != EXIT_SUCCESS)
    {
        c_ID2 = HAPCAN_DEFAULT_CIDx;
    }
    aux_clearHAPCANFrame(hd_result);
    hd_result->frametype = frametype;
    hd_result->flags = 0x00;
    hd_result->module = c_ID1;
    hd_result->group = c_ID2;
    for(i = 0; i < HAPCAN_DATA_LEN; i++)
    {
        hd_result->data[i] = 0xFF;
    }
    hd_result->data[2] = node;
    hd_result->data[3] = group;
    if(frametype == HAPCAN_HEALTH_CHECK_REQUEST_NODE_FRAME_TYPE)
    {
        hd_result->data[0] = 0x01;
    }
}

/**
 * Init the gateway - Setup the lists based on configuration file
 **/
void hapcan_initGateway(void)
{    
    //--------------------------------------------
    // Get Configuration
    //--------------------------------------------
    hconfig_init();
    //--------------------------------------------
    // Add configured modules to the gateway
    //--------------------------------------------
    hrelay_addToGateway();   
    hbutton_addToGateway(); 
    htemp_addToGateway();
    hrgb_addToGateway();
}

/**
 * Check the CAN message received, and add to MQTT Pub buffer the needed 
 * response(s)        
 */
int hapcan_handleCAN2MQTT(hapcanCANData* hapcanData, 
        unsigned long long timestamp)
{
    int check; 
    int ret;
    bool enable;
    // Init return as no response
    ret = HAPCAN_NO_RESPONSE;
    //------------------------------------------
    // Raw (generic) MQTT response
    //------------------------------------------
    check = hconfig_getConfigBool(HAPCAN_CONFIG_ENABLE_RAW, &enable);
    if(check == EXIT_FAILURE)
    {
       enable = false; 
    }
    if(enable)
    {        
        ret = handleRawFromCAN(hapcanData, timestamp);
    }
    //------------------------------------------
    // Configured MQTT response
    //------------------------------------------
    if(ret != HAPCAN_MQTT_RESPONSE_ERROR)
    {
        check = hconfig_getConfigBool(HAPCAN_CONFIG_ENABLE_GATEWAY, &enable);
        if(check == EXIT_FAILURE)
        {
           enable = false; 
        }
        if(enable)
        {
            ret = handleConfiguredFromCAN(hapcanData, timestamp);
        }
    }
    //------------------------------------------
    // System MQTT response
    //------------------------------------------
    if((ret != HAPCAN_MQTT_RESPONSE_ERROR) && (ret != HAPCAN_MQTT_RESPONSE))
    {
        check = hconfig_getConfigBool(HAPCAN_CONFIG_ENABLE_STATUS, &enable);
        if(check == EXIT_FAILURE)
        {
           enable = false; 
        }
        if(enable)
        {
            ret = hsystem_checkCAN(hapcanData, timestamp);
        }
    }
    return ret;
}

/**
 * Check the MQTT message received, and add to CAN Write buffer the needed 
 * response(s)      
 */
int hapcan_handleMQTT2CAN(char* topic, void* payload, int payloadlen, 
        unsigned long long timestamp)
{
    int check; 
    int ret;
    bool enable;
    // Init return as no response
    ret = HAPCAN_NO_RESPONSE;
    //------------------------------------------
    // Raw (generic) response
    //------------------------------------------
    check = hconfig_getConfigBool(HAPCAN_CONFIG_ENABLE_RAW, &enable);
    if(check == EXIT_FAILURE)
    {
       enable = false; 
    }
    if(enable)
    {        
        ret = handleRawFromMQTT(topic, payload, payloadlen, timestamp);
    }
    //------------------------------------------
    // Configured CAN response
    //------------------------------------------
    if(ret != HAPCAN_CAN_RESPONSE_ERROR)
    {
        check = hconfig_getConfigBool(HAPCAN_CONFIG_ENABLE_GATEWAY, &enable);
        if(check == EXIT_FAILURE)
        {
           enable = false; 
        }
        if(enable)
        {
            ret = handleConfiguredFromMQTT(topic, payload, payloadlen, 
                    timestamp);
        }
    }
    //------------------------------------------
    // System CAN response
    //------------------------------------------
    if((ret != HAPCAN_CAN_RESPONSE_ERROR) && (ret != HAPCAN_CAN_RESPONSE))
    {
        check = hconfig_getConfigBool(HAPCAN_CONFIG_ENABLE_STATUS, &enable);
        if(check == EXIT_FAILURE)
        {
           enable = false; 
        }
        if(enable)
        {
            ret = hsystem_checkMQTT(topic, payload, payloadlen, 
                    timestamp);
        }
    }
    return ret;
}

/**
 * Add a HAPCAN Message to the CAN Write Buffer
 */
int hapcan_addToCANWriteBuffer(hapcanCANData* hapcanData, 
        unsigned long long timestamp, bool sendToSocket)
{
    int check;
    struct can_frame cf_Frame;
    int ret = HAPCAN_CAN_RESPONSE_ERROR;
    int dataLen;
    uint8_t data[HAPCAN_SOCKET_DATA_LEN];
    //---------------------------------
    // Add data to CAN Write Buffer
    //---------------------------------    
    aux_clearCANFrame(&cf_Frame);
    hapcan_getCANDataFromHAPCAN(hapcanData, &cf_Frame);
    check = canbuf_setWriteMsgToBuffer(0, &cf_Frame, timestamp);
    // Check if error occurred when adding to buffer
    errorh_isError(ERROR_MODULE_CAN_SEND, check);
    if(check != CAN_SEND_OK)
    {
        // Here we have to set to error to inform the application to 
        // restart CAN.
        ret = HAPCAN_CAN_RESPONSE_ERROR;        
    }        
    else
    {
        ret = HAPCAN_CAN_RESPONSE;
        if(sendToSocket)
        {
            //---------------------------------
            // Add data to Socket Write Buffer
            //---------------------------------
            hs_getSocketArrayFromHAPCAN(hapcanData, data);
            dataLen = HAPCAN_SOCKET_DATA_LEN;
            check = socketserverbuf_setWriteMsgToBuffer(data, dataLen, 
                    timestamp);
            // Handle possible Socket Server Errors
            errorh_isError(ERROR_MODULE_SOCKETSERVER_SEND, check);
        }
    }
    // return
    return ret;
}

/**
 * Add a MQTT Message to the MQTT Pub Buffer
 * \return  HAPCAN_NO_RESPONSE: No response was added to CAN Write Buffer
 *          HAPCAN_MQTT_RESPONSE: Response added to CAN Write Buffer (OK)
 *          HAPCAN_MQTT_RESPONSE_ERROR: Buffer Error
 */
int hapcan_addToMQTTPubBuffer(char* topic, void* payload, int payloadlen, 
        unsigned long long timestamp)
{
    int ret = HAPCAN_NO_RESPONSE;
    int check;
    if(topic != NULL && payload != NULL && payloadlen > 0)
    {        
        check = mqttbuf_setPubMsgToBuffer(topic, payload, payloadlen, 
                timestamp);
        // Check if error occurred when adding to buffer
        errorh_isError(ERROR_MODULE_MQTT_PUB, check);
        if(check == MQTT_PUB_BUFFER_ERROR)
        {
            // Here we have to set to error to inform the application to 
            // restart MQTT.
            ret = HAPCAN_MQTT_RESPONSE_ERROR;        
        }
        else if(check == MQTT_PUB_OK)
        {
            ret = HAPCAN_MQTT_RESPONSE;
        }
    }    
    return ret;
}