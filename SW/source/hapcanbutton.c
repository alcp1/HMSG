//----------------------------------------------------------------------------//
//                               OBJECT HISTORY                               //
//----------------------------------------------------------------------------//
//  REVISION |    DATE     |                               |      AUTHOR      //
//----------------------------------------------------------------------------//
//  1.00     | 10/Dec/2021 |                               | ALCP             //
// - First version                                                            //
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
#include "gateway.h"
#include "hapcan.h"
#include "hapcanbutton.h"
#include "hapcanconfig.h"
#include "jsonhandler.h"
#include "mqtt.h"
#include "mqttbuf.h"

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
static void addButtonChannelToGateway(int node, int group, int channel, 
        char *state_str, char *command_str);
static int getButtonPayloads(hapcanCANData *hd_received, char*** a_payload, 
        int **a_payloadlen, int* n_payloads);
static int getButtonHAPCANFrame(void *payload, int payloadlen, 
        hapcanCANData *hd_result);

/*
 * Add a single button to the HAPCAN <--> MQTT gateway based on the data read 
 * from JSON file
 * 
 * \param   node            (INPUT) module / node
 * \param   group           (INPUT) group
 * \param   channel         (INPUT) button channel
 * \param   state_str       (INPUT) MQTT state topic
 * \param   command_str     (INPUT) MQTT command topic
 *  
 */
static void addButtonChannelToGateway(int node, int group, int channel, 
        char *state_str, char *command_str)
{
    int check;
    bool valid;
    int c_ID1;
    int c_ID2;
    hapcanCANData hd_mask;
    hapcanCANData hd_check;
    hapcanCANData hd_result;
    int temp;
    // Clear data
    aux_clearHAPCANFrame(&hd_mask);
    aux_clearHAPCANFrame(&hd_check);
    aux_clearHAPCANFrame(&hd_result);
    //---------------------------
    // Validate input data
    //---------------------------
    valid = true;
    valid = valid && (node >= 0) && (node <= 255);
    valid = valid && (group >= 0) && (group <= 255);
    valid = valid && (channel >= 1) && (channel <= 14);
    if(!valid)
    {
        #ifdef DEBUG_HAPCAN_BUTTON_ERRORS
        debug_print("addButtonChannelToGateway: parameter error!\n");
        #endif
    }
    else
    {
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
        //---------------------------
        // Set data for CAN2MQTT
        //---------------------------
        // For the button frame, the following fields should be checked for 
        // a match:
        // - frame type
        // - node (module)
        // - group
        // - channel (D2)
        if(state_str != NULL)
        {
            hd_mask.frametype = 0xFFF;
            hd_mask.module = 0xFF;
            hd_mask.group = 0xFF;
            hd_mask.data[2] = 0xFF;
            hd_check.frametype = HAPCAN_BUTTON_FRAME_TYPE;
            hd_check.module = node;
            hd_check.group = group;
            hd_check.data[2] = channel;
            // Add to list CAN2MQTT: hd_mask, hd_check, state_topic
            check = gateway_AddElementToList(GATEWAY_CAN2MQTT_LIST, &hd_mask, 
                &hd_check, state_str, NULL, &hd_result);
            if(check != EXIT_SUCCESS)
            {
                #ifdef DEBUG_HAPCAN_BUTTON_ERRORS
                debug_print("addButtonChannelToGateway: Error "
                        "adding to CAN2MQTT!\n");
                #endif
            }
        }
        //---------------------------
        // Set data for MQTT2CAN
        //---------------------------
        // For a button frame, the following fields should be sent to the CAN Bus 
        // to control a button channel
        // - frame type is 0x10A
        // - flag is 0
        // - Computer ID is set
        // - D1 is INSTR2 - channel bit
        // - D2 is node
        // - D3 is group
        if(command_str != NULL)
        {
            aux_clearHAPCANFrame(&hd_mask);
            aux_clearHAPCANFrame(&hd_check);
            hd_result.frametype = HAPCAN_BUTTON_FRAME_TYPE;
            hd_result.flags = 0;
            hd_result.module = c_ID1;
            hd_result.group = c_ID2;
            hd_result.data[2] = node;
            hd_result.data[3] = group;
            temp = (1 << (channel - 1));
            hd_result.data[1] = (uint8_t)(temp & 0xFF); // INSTR2
            hd_result.data[4] = (uint8_t)((temp & 0xFF00) >> 8); // INSTR3        
            // Add to list CAN2MQTT: command_str, hd_result
            check = gateway_AddElementToList(GATEWAY_MQTT2CAN_LIST, &hd_mask, 
                &hd_check, NULL, command_str, &hd_result);
            if(check != EXIT_SUCCESS)
            {
                #ifdef DEBUG_HAPCAN_BUTTON_ERRORS
                debug_print("addButtonChannelToGateway: Error "
                        "adding to MQTT2CAN!\n");
                #endif
            }
        }
    }
}

/**
 * Set a payload based on the data received
 * 
 * \param   hd_received     (INPUT) pointer to HAPCAN message received
 * \param   a_payload       (OUTPUT) array of payloads to be filled
 * \param   a_payloadlen    (OUTPUT) array of payloadlens to be filled
 * \param   n_payloads      (OUTPUT) number of payloads to be filled
 *                      
 * \return  HAPCAN_MQTT_RESPONSE: Payload set (OK)
 *          HAPCAN_RESPONSE_ERROR: Error - Payload not set
 *          HAPCAN_NO_RESPONSE: No response
 *          
 */
static int getButtonPayloads(hapcanCANData *hd_received, char*** a_payload, 
        int **a_payloadlen, int* n_payloads)
{
    // D3 is BUTTON:
    //    - 0x00 – open
    //    - 0x01 – disabled
    //    - 0xFF – closed
    //    - 0xFE – closed and held for 400ms
    //    - 0xFD – closed and held for 4s
    //    - 0xFC – closed and open within 400ms
    //    - 0xFB – closed and open between 400ms and 4s
    //    - 0xFA – closed and open after 4s
    // D4 is LED
    //    - 0x00 – off
    //    - 0xFF – on
    //    - 0x01 - disabled
    // REMARK: If BUTTON and LED are both enabled on the configuration, the 
    // status will be sent based on the BUTTON status.
    int ret = HAPCAN_MQTT_RESPONSE;
    unsigned int len;
    // Check if button is disabled
    if(hd_received->data[3] == 0x01)
    {
        // Check if LED is disabled
        if(hd_received->data[4] == 0x01)
        {
            // Button and LED disabled
            ret = HAPCAN_NO_RESPONSE;
        }        
        else if(hd_received->data[4] == 0x00)
        {
            // LED is OFF
            *n_payloads = 1;
            (*a_payload) = malloc(sizeof(void *)); 
            *(a_payloadlen) = malloc(sizeof(int));              
            len = strlen("OFF");
            (*a_payloadlen)[0] = len;
            (*a_payload)[0] = malloc(len + 1);
            strcpy((*a_payload)[0], "OFF");
        }
        else if(hd_received->data[4] == 0xFF)
        {
            // LED is ON
            *n_payloads = 1;
            (*a_payload) = malloc(sizeof(void *)); 
            *(a_payloadlen) = malloc(sizeof(int));              
            len = strlen("ON");
            (*a_payloadlen)[0] = len;
            (*a_payload)[0] = malloc(len + 1);
            strcpy((*a_payload)[0], "ON");
        }
        else
        {
            #if defined(DEBUG_HAPCAN_CAN2MQTT) || defined(DEBUG_HAPCAN_ERRORS)
            debug_print("getButtonPayloads - HAPCAN LED Frame Error. "
                    "D4 = %d\n", hd_received->data[4]);
            #endif
            ret = HAPCAN_RESPONSE_ERROR;
        }
    }
    else if(hd_received->data[3] == 0x00)
    {
        // Button is OFF
        *n_payloads = 1;
        (*a_payload) = malloc(sizeof(void *)); 
        *(a_payloadlen) = malloc(sizeof(int));              
        len = strlen("OFF");
        (*a_payloadlen)[0] = len;
        (*a_payload)[0] = malloc(len + 1);
        strcpy((*a_payload)[0], "OFF");
    }
    else if(hd_received->data[3] >= 0xFD)
    {
        // Button is Closed
        *n_payloads = 1;
        (*a_payload) = malloc(sizeof(void *)); 
        *(a_payloadlen) = malloc(sizeof(int));              
        len = strlen("ON");
        (*a_payloadlen)[0] = len;
        (*a_payload)[0] = malloc(len + 1);
        strcpy((*a_payload)[0], "ON");
    }
    else if(hd_received->data[3] >= 0xFA)
    {
        // Button was closed and then was opened
        *n_payloads = 2;
        (*a_payload) = malloc(2*sizeof(void *)); 
        *(a_payloadlen) = malloc(2*sizeof(int));              
        len = strlen("ON");
        (*a_payloadlen)[0] = len;
        (*a_payload)[0] = malloc(len + 1);
        strcpy((*a_payload)[0], "ON");
        len = strlen("OFF");
        (*a_payloadlen)[1] = len;
        (*a_payload)[1] = malloc(len + 1);
        strcpy((*a_payload)[1], "OFF");
    }
    else
    {
        #if defined(DEBUG_HAPCAN_CAN2MQTT) || defined(DEBUG_HAPCAN_ERRORS)
        debug_print("getButtonPayloads - HAPCAN Button Frame Error. D3 "
                "= %d\n", hd_received->data[3]);
        #endif
        ret = HAPCAN_RESPONSE_ERROR;
    }    
    return ret;
}

/**
 * Fill the HAPCAN Frame to be sent to the CAN Bus as a response to a received 
 * MQTT Message that was matched to a HAPCAN Module
 * 
 * \param   payload         (INPUT) payload to be filled
 * \param   payloadlen      (INPUT) payloadlen to be filled
 * \param   hd_result       (INPUT / OUTPUT) pointer to HAPCAN message matched
 *                      
 * \return  HAPCAN_NO_RESPONSE: HAPCAN Frame not set
 *          HAPCAN_CAN_RESPONSE: HAPCAN Frame set (OK)
 *          HAPCAN_RESPONSE_ERROR: Other error
 *       
 */
static int getButtonHAPCANFrame(void *payload, int payloadlen, hapcanCANData *hd_result)
{
    int ret = HAPCAN_NO_RESPONSE;
    int check;
    char *str = NULL;
    bool valid = true;
    long val;
    int temp;
    json_object *obj = NULL;
    // Check NULL or size 0
    if((payload == NULL) || (payloadlen <= 0))
    {
        valid = false;
    }
    else
    {
        // set to false unless there is a payload match 
        valid = false;
        // payload does not end with 0 for a string - copy it here
        str = malloc(payloadlen + 1);
        memcpy(str, payload, payloadlen);
        str[payloadlen] = 0;
        //----------------------------------------------------------------------
        // COMMAND TOPIC ACCEPTED PAYLOADS:
        // 1. command strings: "ON", "OFF", "TOGGLE"
        // 2. String as number: "0xFF", "0x00", "255", "0" (ON is 255, Off is 0)
        // 3. JSON: {"INSTR1":Integer, "INSTR4":Integer, 
        //          "INSTR5":Integer, "INSTR6":Integer}
        //----------------------------------------------------------------------
        if( aux_compareStrings(str, "ON") )
        {
            hd_result->data[0] = 0x01; // INSTR1. 0x01 = Turn ON
            hd_result->data[5] = 0xFF; // INSTR4 = 0xXX
            hd_result->data[6] = 0xFF; // INSTR5 = 0xXX   
            hd_result->data[7] = 0xFF; // INSTR6 = 0xXX
            valid = true;
        }
        else if( aux_compareStrings(str, "OFF") )
        {
            hd_result->data[0] = 0x00; // INSTR1. 0x01 = Turn OFF
            hd_result->data[5] = 0xFF; // INSTR4 = 0xXX
            hd_result->data[6] = 0xFF; // INSTR5 = 0xXX   
            hd_result->data[7] = 0xFF; // INSTR6 = 0xXX
            valid = true;
        }
        else if( aux_compareStrings(str, "TOGGLE") )
        {
            hd_result->data[0] = 0x02; // INSTR1. 0x01 = Toggle
            hd_result->data[5] = 0xFF; // INSTR4 = 0xXX
            hd_result->data[6] = 0xFF; // INSTR5 = 0xXX   
            hd_result->data[7] = 0xFF; // INSTR6 = 0xXX
            valid = true;
        }
        else if(aux_parseValidateLong(str, &val, 0, 0, 255))
        {
            // 0x00 or 0 are set to OFF
            if(val == 0)
            {
                hd_result->data[0] = 0x00; // INSTR1. 0x00 = Turn OFF
                hd_result->data[5] = 0xFF; // INSTR4 = 0xXX
                hd_result->data[6] = 0xFF; // INSTR5 = 0xXX   
                hd_result->data[7] = 0xFF; // INSTR6 = 0xXX
                valid = true;
            }
            // 0xFF or 255 are set to ON
            else if(val == 255)
            {
                hd_result->data[0] = 0x01; // INSTR1. 0x01 = Turn ON
                hd_result->data[5] = 0xFF; // INSTR4 = 0xXX
                hd_result->data[6] = 0xFF; // INSTR5 = 0xXX   
                hd_result->data[7] = 0xFF; // INSTR6 = 0xXX
                valid = true;
            }
        }
        else
        {
            // Check for JSON - Get the JSON Object
            jh_getObject(str, &obj);
            // Check if error parsing
            if(obj != NULL)
            {
                valid = true;
                // INSTR1
                check = jh_getObjectFieldAsInt(obj, "INSTR1", &temp);
                valid = valid && (check == JSON_OK) && 
                        (temp >= 0) && (temp <= 255);                
                if( valid )
                {
                    hd_result->data[0] = (uint8_t)temp;
                }
                // INSTR4
                check = jh_getObjectFieldAsInt(obj, "INSTR4", &temp);
                valid = valid && (check == JSON_OK) && 
                        (temp >= 0) && (temp <= 255);
                if( valid )
                {
                    hd_result->data[5] = (uint8_t)temp;
                }
                // INSTR5
                check = jh_getObjectFieldAsInt(obj, "INSTR5", &temp);
                valid = valid && (check == JSON_OK) && 
                        (temp >= 0) && (temp <= 255);
                if( valid )
                {
                    hd_result->data[6] = (uint8_t)temp;
                }
                // INSTR6
                check = jh_getObjectFieldAsInt(obj, "INSTR6", &temp);
                valid = valid && (check == JSON_OK) && 
                        (temp >= 0) && (temp <= 255);
                if( valid )
                {
                    hd_result->data[7] = (uint8_t)temp;
                }
            } 
        }        
    }    
    // Set return and Clean
    if( !valid )
    {
        ret = HAPCAN_RESPONSE_ERROR;
    }
    else
    {
        hd_result->frametype = HAPCAN_DIRECT_CONTROL_FRAME_TYPE;
        ret = HAPCAN_CAN_RESPONSE;
    }
    // Free
    jh_freeObject(obj);
    free(str);
    // Leave
    return ret; 
}

//----------------------------------------------------------------------------//
// EXTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
/*
 * Add buttons configured in the configuration JSON file to the HAPCAN <--> MQTT
 * gateway
 */
void hbutton_addToGateway(void)
{
    int i_button;
    int i_channel;
    int n_buttons;
    int n_channels;
    int node;
    int group;
    int channel;
    char *state_str = NULL;
    char *command_str = NULL;
    int check;
    bool valid;
    //------------------------------------------------
    // Get the number of configured button modules
    //------------------------------------------------
    check = jh_getJArrayElements("HAPCANButtons", 0, NULL, JSON_DEPTH_LEVEL, 
            &n_buttons);
    if(check == JSON_OK)
    {
        for(i_button = 0; i_button < n_buttons; i_button++)
        {
            //------------------------------------------------
            // Get the module information
            //------------------------------------------------
            valid = true;
            // Node (Module)
            check = jh_getJFieldInt("HAPCANButtons", i_button, 
                    "node", 0, NULL, &node);
            valid = valid && (check == JSON_OK);
            // Group
            check = jh_getJFieldInt("HAPCANButtons", i_button, 
                    "group", 0, NULL, &group);
            valid = valid && (check == JSON_OK);
            // Number of channels
            check = jh_getJArrayElements("HAPCANButtons", i_button, 
                    "buttons", JSON_DEPTH_FIELD, &n_channels);
            valid = valid && (check == JSON_OK);
            #ifdef DEBUG_HAPCAN_BUTTON_ERRORS
            if(!valid)
            {
                debug_print("hbutton_addToGateway: Module Information Error!\n");
            }
            #endif
            if(valid)
            {
                //------------------------------------------------
                // Get each channel configuration
                //------------------------------------------------
                for(i_channel = 0; i_channel < n_channels; i_channel++)
                {
                    check = jh_getJFieldInt("HAPCANButtons", i_button,
                        "buttons", i_channel, "channel", &channel);
                    valid = valid && (check == JSON_OK);
                    // State is mandatory - if not needed do not add the 
                    // channel to the JSON config file
                    check = jh_getJFieldStringCopy("HAPCANButtons", i_button,
                        "buttons", i_channel, "state", &state_str);                    
                    valid = valid && (check == JSON_OK);
                    // Command is optional
                    check = jh_getJFieldStringCopy("HAPCANButtons", i_button,
                        "buttons", i_channel, "command", &command_str);
                    if((check != JSON_OK) || (command_str == NULL))
                    {
                        command_str = NULL;
                    }
                    // Check if mandatory fields are valid
                    if(valid)
                    {
                        // Add to the gateway
                        addButtonChannelToGateway(node, group, channel, 
                                state_str, command_str);
                    }
                    #ifdef DEBUG_HAPCAN_BUTTON_ERRORS
                    if(!valid)
                    {
                        debug_print("hbutton_addToGateway: channel Information "
                                "Error!\n");
                    }
                    #endif
                    // Free
                    free(state_str);
                    state_str = NULL;
                    free(command_str);
                    command_str = NULL;
                }
            }
        }
    }
}

/**
 * Set a payload based on the data received, and add it to the MQTT Pub Buffer.       
 */
int hbutton_setCAN2MQTTResponse(char *state_str, hapcanCANData *hd_received, 
        unsigned long long timestamp)
{
    int ret = HAPCAN_NO_RESPONSE;
    int check;
    char **a_payload = NULL;
    int *a_payloadlen = NULL;
    int n_payloads = 0;
    int i;
    // Set the payloads
    check = getButtonPayloads(hd_received, &a_payload, &a_payloadlen, 
            &n_payloads);
    if(check == HAPCAN_MQTT_RESPONSE)
    {
        for(i = 0; i < n_payloads; i++)
        {
            // Set MQTT Pub buffer
            if(state_str != NULL)
            {
                ret = hapcan_addToMQTTPubBuffer(state_str, a_payload[i], 
                        a_payloadlen[i], timestamp);
            }
        }
    }
    // Free
    for(i = 0; i < n_payloads; i++)
    {
        free(a_payload[i]);
    }
    free(a_payloadlen);
    free(a_payload);
    // Return
    return ret;
}

/**
 * Set a HAPCAN message based on the payload received, and add it to the CAN 
 * write Buffer.    
 */
int hbutton_setMQTT2CANResponse(hapcanCANData *hd_result, void *payload, 
        int payloadlen, unsigned long long timestamp)
{
    int ret = HAPCAN_NO_RESPONSE;
    int check;
    // Set the payload
    check = getButtonHAPCANFrame(payload, payloadlen, hd_result);
    if(check == HAPCAN_CAN_RESPONSE)
    {        
        ret = hapcan_addToCANWriteBuffer(hd_result, timestamp, true);
    }
    return ret;
}
