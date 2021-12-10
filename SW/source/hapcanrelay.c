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
#include "hapcanconfig.h"
#include "hapcanrelay.h"
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
static void addRelayChannelToGateway(int node, int group, int channel, 
        char *state_str, char *command_str);
static int getRelayPayload(hapcanCANData *hd_received, void** payload, 
        int *payloadlen);
static int getRelayHAPCANFrame(void *payload, int payloadlen, 
        hapcanCANData *hd_result);

/*
 * Add a single relay to the HAPCAN <--> MQTT gateway based on the data read 
 * from JSON file
 * 
 * \param   node            (INPUT) module / node
 * \param   group           (INPUT) group
 * \param   channel         (INPUT) relay channel
 * \param   state_str       (INPUT) MQTT state topic
 * \param   command_str     (INPUT) MQTT command topic
 *  
 */
static void addRelayChannelToGateway(int node, int group, int channel, 
        char *state_str, char *command_str)
{
    int check;
    bool valid;
    int c_ID1;
    int c_ID2;
    hapcanCANData hd_mask;
    hapcanCANData hd_check;
    hapcanCANData hd_result;
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
    valid = valid && (channel >= 1) && (channel <= 6);
    if(!valid)
    {
        #ifdef DEBUG_HAPCAN_RELAY_ERRORS
        debug_print("addRelayChannelToGateway: parameter error!\n");
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
        // For the relay frame, the following fields should be checked for 
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
            hd_check.frametype = HAPCAN_RELAY_FRAME_TYPE;
            hd_check.module = node;
            hd_check.group = group;
            hd_check.data[2] = channel;
            // Add to list CAN2MQTT: hd_mask, hd_check, state_topic
            check = gateway_AddElementToList(GATEWAY_CAN2MQTT_LIST, &hd_mask, 
                &hd_check, state_str, NULL, &hd_result);
            if(check != EXIT_SUCCESS)
            {
                #ifdef DEBUG_HAPCAN_RELAY_ERRORS
                debug_print("addRelayChannelToGateway: Error "
                        "adding to CAN2MQTT!\n");
                #endif
            }
        }
        //---------------------------
        // Set data for MQTT2CAN
        //---------------------------
        // For a relay frame, the following fields should be sent to the CAN Bus 
        // to control a relay channel
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
            hd_result.frametype = HAPCAN_RELAY_FRAME_TYPE;
            hd_result.flags = 0;
            hd_result.module = c_ID1;
            hd_result.group = c_ID2;
            hd_result.data[1] = (1 << (channel - 1));
            hd_result.data[2] = node;
            hd_result.data[3] = group;
            // Add to list CAN2MQTT: command_str, hd_result
            check = gateway_AddElementToList(GATEWAY_MQTT2CAN_LIST, &hd_mask, 
                &hd_check, NULL, command_str, &hd_result);
            if(check != EXIT_SUCCESS)
            {
                #ifdef DEBUG_HAPCAN_RELAY_ERRORS
                debug_print("addRelayChannelToGateway: Error adding "
                        "to MQTT2CAN!\n");
                #endif
            }
        }
    }
}

/**
 * Set a payload based on the data received
 * 
 * \param   hd_received     (INPUT) pointer to HAPCAN message received
 * \param   payload         (OUTPUT) payload to be filled
 * \param   payloadlen      (OUTPUT) payloadlen to be filled
 *                      
 * \return  HAPCAN_MQTT_RESPONSE: Payload set (OK)
 *          HAPCAN_RESPONSE_ERROR: Error - Payload not set
 *          
 */
static int getRelayPayload(hapcanCANData *hd_received, void** payload, 
        int *payloadlen)
{
    int ret = HAPCAN_MQTT_RESPONSE;
    unsigned int len;
    if(hd_received->data[3] == 0x00)
    {
        len = strlen("OFF");
        *payload = malloc(len + 1);
        strcpy(*payload, "OFF");
        *payloadlen = len;
    }
    else if(hd_received->data[3] == 0xFF)
    {
        len = strlen("ON");
        *payload = malloc(len + 1);
        strcpy(*payload, "ON");
        *payloadlen = len;
    }
    else
    {
        #if defined(DEBUG_HAPCAN_CAN2MQTT) || defined(DEBUG_HAPCAN_ERRORS)
        debug_print("getRelayPayload - HAPCAN Relay Frame Error. D3 = %d\n", 
                hd_received->data[3]);
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
static int getRelayHAPCANFrame(void *payload, int payloadlen, 
        hapcanCANData *hd_result)
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
        // 3. JSON: {"INSTR1":Integer, "INSTR3":Integer, "INSTR4":Integer, 
        //          "INSTR5":Integer, "INSTR6":Integer}
        //----------------------------------------------------------------------
        if( aux_compareStrings(str, "ON") )
        {
            hd_result->data[0] = 0x01; // INSTR1. 0x01 = Turn ON
            hd_result->data[4] = 0x00; // INSTR3 = Timer. 0 = set immediately.
            hd_result->data[5] = 0xFF; // INSTR4 = 0xXX
            hd_result->data[6] = 0xFF; // INSTR5 = 0xXX   
            hd_result->data[7] = 0xFF; // INSTR6 = 0xXX
            valid = true;
        }
        else if( aux_compareStrings(str, "OFF") )
        {
            hd_result->data[0] = 0x00; // INSTR1. 0x00 = Turn OFF
            hd_result->data[4] = 0x00; // INSTR3 = Timer. 0 = set immediately.
            hd_result->data[5] = 0xFF; // INSTR4 = 0xXX
            hd_result->data[6] = 0xFF; // INSTR5 = 0xXX   
            hd_result->data[7] = 0xFF; // INSTR6 = 0xXX
            valid = true;
        }
        else if( aux_compareStrings(str, "TOGGLE") )
        {
            hd_result->data[0] = 0x02; // INSTR1. 0x02 = Toggle
            hd_result->data[4] = 0x00; // INSTR3 = Timer. 0 = set immediately.
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
                hd_result->data[4] = 0x00; // INSTR3 = Timer.
                hd_result->data[5] = 0xFF; // INSTR4 = 0xXX
                hd_result->data[6] = 0xFF; // INSTR5 = 0xXX   
                hd_result->data[7] = 0xFF; // INSTR6 = 0xXX
                valid = true;
            }
            // 0xFF or 255 are set to ON
            else if(val == 255)
            {
                hd_result->data[0] = 0x01; // INSTR1. 0x01 = Turn ON
                hd_result->data[4] = 0x00; // INSTR3 = Timer.
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
                // INSTR3
                check = jh_getObjectFieldAsInt(obj, "INSTR3", &temp);
                valid = valid && (check == JSON_OK) && 
                        (temp >= 0) && (temp <= 255);
                if( valid )
                {
                    hd_result->data[4] = (uint8_t)temp;
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
 * Add relays configured in the configuration JSON file to the HAPCAN <--> MQTT
 * gateway
 */
void hrelay_addToGateway(void)
{
    int i_relay;
    int i_channel;
    int n_relays;
    int n_channels;
    int node;
    int group;
    int channel;
    char *state_str = NULL;
    char *command_str = NULL;
    int check;
    bool valid;
    //------------------------------------------------
    // Get the number of configured relays
    //------------------------------------------------
    check = jh_getJArrayElements("HAPCANRelays", 0, NULL, JSON_DEPTH_LEVEL, 
            &n_relays);
    if(check == JSON_OK)
    {
        for(i_relay = 0; i_relay < n_relays; i_relay++)
        {
            //------------------------------------------------
            // Get the module information
            //------------------------------------------------
            valid = true;
            // Node (Module)
            check = jh_getJFieldInt("HAPCANRelays", i_relay, 
                    "node", 0, NULL, &node);
            valid = valid && (check == JSON_OK);
            // Group
            check = jh_getJFieldInt("HAPCANRelays", i_relay, 
                    "group", 0, NULL, &group);
            valid = valid && (check == JSON_OK);
            // Number of channels
            check = jh_getJArrayElements("HAPCANRelays", i_relay, 
                    "relays", JSON_DEPTH_FIELD, &n_channels);
            valid = valid && (check == JSON_OK);
            #ifdef DEBUG_HAPCAN_RELAY_ERRORS
            if(!valid)
            {
                debug_print("hrelay_addToGateway: Module Information Error!\n");
            }
            #endif
            if(valid)
            {
                //------------------------------------------------
                // Get each channel configuration
                //------------------------------------------------
                for(i_channel = 0; i_channel < n_channels; i_channel++)
                {
                    check = jh_getJFieldInt("HAPCANRelays", i_relay,
                        "relays", i_channel, "channel", &channel);
                    valid = valid && (check == JSON_OK);
                    // State is optional
                    check = jh_getJFieldStringCopy("HAPCANRelays", i_relay,
                        "relays", i_channel, "state", &state_str);
                    if(check != JSON_OK)
                    {
                       state_str = NULL; 
                    }
                    // Command is optional
                    check = jh_getJFieldStringCopy("HAPCANRelays", i_relay,
                        "relays", i_channel, "command", &command_str);
                    if(check != JSON_OK)
                    {
                       command_str = NULL; 
                    }
                    if(valid)
                    {
                        // Add to the gateway
                        addRelayChannelToGateway(node, group, channel, 
                                state_str, command_str);
                    }
                    #ifdef DEBUG_HAPCAN_RELAY_ERRORS
                    if(!valid)
                    {
                        debug_print("hrelay_addToGateway: channel Information "
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
int hrelay_setCAN2MQTTResponse(char *state_str, hapcanCANData *hd_received, 
        unsigned long long timestamp)
{
    int ret = HAPCAN_NO_RESPONSE;
    int check;
    void* payload = NULL;
    int payloadlen;
    // Set the payload
    check = getRelayPayload(hd_received, &payload, &payloadlen);
    if(check == HAPCAN_MQTT_RESPONSE)
    {
        if(state_str != NULL)
        {
            // Set MQTT Pub buffer
            ret = hapcan_addToMQTTPubBuffer(state_str, payload, payloadlen, 
                    timestamp);
        }
    }
    // Free
    free(payload);
    // Return
    return ret;
}

/**
 * Set a HAPCAN message based on the payload received, and add it to the CAN 
 * write Buffer.    
 */
int hrelay_setMQTT2CANResponse(hapcanCANData *hd_result, void *payload, 
        int payloadlen, unsigned long long timestamp)
{
    int ret = HAPCAN_NO_RESPONSE;
    int check;
    // Set the payload
    check = getRelayHAPCANFrame(payload, payloadlen, hd_result);
    if(check == HAPCAN_CAN_RESPONSE)
    {
        ret = hapcan_addToCANWriteBuffer(hd_result, timestamp, true);
    }
    return ret;
}