//----------------------------------------------------------------------------//
//                               OBJECT HISTORY                               //
//----------------------------------------------------------------------------//
//  REVISION |    DATE     |                               |      AUTHOR      //
//----------------------------------------------------------------------------//
//  1.00     | 18/Jun/2023 |                               | ALCP             //
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
#include "htim.h"
#include "jsonhandler.h"
#include "mqtt.h"
#include "mqttbuf.h"

//----------------------------------------------------------------------------//
// INTERNAL DEFINITIONS
//----------------------------------------------------------------------------//
enum TemperatureSensors
{
    TIM_SENSOR_1 = 0,
    TIM_SENSOR_2,
    TIM_SENSOR_3,
    TIM_SENSOR_4,    
    TIM_N_SENSORS
};

//----------------------------------------------------------------------------//
// INTERNAL TYPES
//----------------------------------------------------------------------------//

//----------------------------------------------------------------------------//
// INTERNAL GLOBAL VARIABLES
//----------------------------------------------------------------------------//

//----------------------------------------------------------------------------//
// INTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
static void htiml_addTemperatureModuleToGateway(int node, int group, 
                int channel, char *state_str);
static void htiml_addThermostatModuleToGateway(int node, int group, 
                int channel, char *state_str, char *command_str);
static void htiml_addTErrorModuleToGateway(int node, int group, 
                int channel, char *state_str);
static void htiml_addIRModuleToGateway(int node, int group, char *command_str);
static int htiml_getTempPayload(hapcanCANData *hd_received, void** payload, 
        int *payloadlen);
static int htiml_getTempHAPCANFrame(void *payload, int payloadlen, 
        hapcanCANData *hd_result);

/*
 * Add a single temperature reading module to the HAPCAN <--> MQTT gateway 
 * based on the data read from the configuration JSON file
 * 
 * \param   node            (INPUT) module / node
 * \param   group           (INPUT) group
 * \param   channel         (INPUT) sensor (1 to 4)
 * \param   state_str       (INPUT) MQTT state topic
 *  
 */
static void htiml_addTemperatureModuleToGateway(int node, int group, 
                int channel, char *state_str)
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
    valid = valid && (channel > TIM_SENSOR_1) && (channel <= TIM_N_SENSORS);
    if(!valid)
    {
        #ifdef DEBUG_TIM_ERRORS
        debug_print("htiml_addTemperatureModuleToGateway: parameter error!\n");
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
        // For the current temperature frame, the following fields should be 
        // checked for a match:
        // - frame type
        // - node (module)
        // - group
        // - D1 = Sensor
        // - D2 = 0x17
        if(state_str != NULL)
        {
            hd_mask.frametype = 0xFFF;
            hd_mask.module = 0xFF;
            hd_mask.group = 0xFF;
            hd_mask.data[1] = 0xFF;
            hd_mask.data[2] = 0xFF;
            hd_check.frametype = HAPCAN_MULTIPLE_TEMPERATURE_FRAME_TYPE;
            hd_check.module = node;
            hd_check.group = group;
            hd_check.data[1] = channel - 1;
            hd_check.data[2] = 0x17;
            // Add to list CAN2MQTT: hd_mask, hd_check, state_topic
            check = gateway_AddElementToList(GATEWAY_CAN2MQTT_LIST, 
                    &hd_mask, &hd_check, state_str, NULL, &hd_result);
            if(check != EXIT_SUCCESS)
            {
                #ifdef DEBUG_TIM_ERRORS
                debug_print("htiml_addTemperatureModuleToGateway: Error adding "
                        "to CAN2MQTT!\n");
                #endif
            }
        }
    }
}

/*
 * Add a single thermostat module to the HAPCAN <--> MQTT gateway 
 * based on the data read from the configuration JSON file
 * 
 * \param   node            (INPUT) module / node
 * \param   group           (INPUT) group
 * \param   channel         (INPUT) sensor (1 to 4)
 * \param   state_str       (INPUT) MQTT state topic
 * \param   command_str     (INPUT) MQTT command topic
 *  
 */
static void htiml_addThermostatModuleToGateway(int node, int group, int channel,
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
    valid = valid && (channel > TIM_SENSOR_1) && (channel <= TIM_N_SENSORS);
    if(!valid)
    {
        #ifdef DEBUG_TIM_ERRORS
        debug_print("htiml_addThermostatModuleToGateway: parameter error!\n");
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
        // For the thermostat frame, the following fields should be 
        // checked for a match:
        // - frame type
        // - node (module)
        // - group
        // - D1 = Sensor
        // - D2 = 0x18
        if(state_str != NULL)
        {
            hd_mask.frametype = 0xFFF;
            hd_mask.module = 0xFF;
            hd_mask.group = 0xFF;
            hd_mask.data[1] = 0xFF;
            hd_mask.data[2] = 0xFF;
            hd_check.frametype = HAPCAN_MULTIPLE_TEMPERATURE_FRAME_TYPE;
            hd_check.module = node;
            hd_check.group = group;
            hd_check.data[1] = channel - 1;
            hd_check.data[2] = 0x18;
            // Add to list CAN2MQTT: hd_mask, hd_check, state_topic
            check = gateway_AddElementToList(GATEWAY_CAN2MQTT_LIST, &hd_mask, 
                &hd_check, state_str, NULL, &hd_result);
            if(check != EXIT_SUCCESS)
            {
                #ifdef DEBUG_TIM_ERRORS
                debug_print("htiml_addThermostatModuleToGateway: Error adding "
                        "to CAN2MQTT!\n");
                #endif
            }
        }
        //---------------------------
        // Set data for MQTT2CAN
        //---------------------------
        // For a thermostat frame, the following fields should be sent to 
        // the CAN Bus to control a thermostat module
        // - frame type is 0x10A
        // - flag is 0
        // - Computer ID is set
        // - D0 is INSTR1 - command
        // - D1 is INSTR2 - THMSB or STEP or THERMOSTAT (0x01)
        // - D2 is node
        // - D3 is group
        // - D4 is INSTR3 - THLSB or 0xXX
        // - D5 is INSTR4 - SENSOR
        if(command_str != NULL)
        {
            aux_clearHAPCANFrame(&hd_mask);
            aux_clearHAPCANFrame(&hd_check);
            hd_result.frametype = HAPCAN_MULTIPLE_TEMPERATURE_FRAME_TYPE;
            hd_result.flags = 0;
            hd_result.module = c_ID1;
            hd_result.group = c_ID2;
            // Temporarily set D1 to 0x18 to differentiate from other frames
            hd_result.data[1] = 0x18;  
            hd_result.data[2] = node;
            hd_result.data[3] = group;
            hd_result.data[5] = channel - 1;
            // Add to list CAN2MQTT: command_str, hd_result
            check = gateway_AddElementToList(GATEWAY_MQTT2CAN_LIST, &hd_mask, 
                &hd_check, NULL, command_str, &hd_result);
            if(check != EXIT_SUCCESS)
            {
                #ifdef DEBUG_TIM_ERRORS
                debug_print("htiml_addThermostatModuleToGateway: Error adding "
                        "to MQTT2CAN!\n");
                #endif
            }
        }
    }
}

/*
 * Add a single temperature error module to the HAPCAN <--> MQTT gateway 
 * based on the data read from the configuration JSON file
 * 
 * \param   node            (INPUT) module / node
 * \param   group           (INPUT) group
 * \param   channel         (INPUT) sensor (1 to 4)
 * \param   state_str       (INPUT) MQTT state topic
 *  
 */
static void htiml_addTErrorModuleToGateway(int node, int group, int channel, 
        char *state_str)
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
    valid = valid && (channel > TIM_SENSOR_1) && (channel <= TIM_N_SENSORS);
    if(!valid)
    {
        #ifdef DEBUG_TIM_ERRORS
        debug_print("htiml_addTErrorModuleToGateway: parameter error!\n");
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
        // For the current temperature frame, the following fields should be 
        // checked for a match:
        // - frame type
        // - node (module)
        // - group
        // - D1 = Sensor
        // - D2 = 0x17
        if(state_str != NULL)
        {
            hd_mask.frametype = 0xFFF;
            hd_mask.module = 0xFF;
            hd_mask.group = 0xFF;
            hd_mask.data[1] = 0xFF;
            hd_mask.data[2] = 0xFF;
            hd_check.frametype = HAPCAN_MULTIPLE_TEMPERATURE_FRAME_TYPE;
            hd_check.module = node;
            hd_check.group = group;
            hd_check.data[1] = channel - 1;
            hd_check.data[2] = 0xF6;
            // Add to list CAN2MQTT: hd_mask, hd_check, state_topic
            check = gateway_AddElementToList(GATEWAY_CAN2MQTT_LIST, &hd_mask, 
                &hd_check, state_str, NULL, &hd_result);
            if(check != EXIT_SUCCESS)
            {
                #ifdef DEBUG_TIM_ERRORS
                debug_print("htiml_addTErrorModuleToGateway: Error adding to "
                        "CAN2MQTT!\n");
                #endif
            }
        }
    }
}

/*
 * Add a IR module to the HAPCAN <--> MQTT gateway based on the data read 
 * from the configuration JSON file
 * 
 * \param   node            (INPUT) module / node
 * \param   group           (INPUT) group
 * \param   command_str     (INPUT) MQTT command topic
 *  
 */
static void htiml_addIRModuleToGateway(int node, int group, char *command_str)
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
    if(!valid)
    {
        #ifdef DEBUG_TIM_ERRORS
        debug_print("htiml_addIRModuleToGateway: parameter error!\n");
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
        // Set data for MQTT2CAN
        //---------------------------
        // For a thermostat frame, the following fields should be sent to 
        // the CAN Bus to control a thermostat module
        // - frame type is 0x10A
        // - flag is 0
        // - Computer ID is set
        // - D0 is INSTR1 - command (0xC0 or >0xE0)
        // - D1 is INSTR2 - THMSB or STEP or THERMOSTAT (0x01)
        // - D2 is node
        // - D3 is group
        // - D4 is INSTR3 - THLSB or 0xXX
        // - D5 is INSTR4 - SENSOR
        if(command_str != NULL)
        {
            aux_clearHAPCANFrame(&hd_mask);
            aux_clearHAPCANFrame(&hd_check);
            hd_result.frametype = HAPCAN_MULTIPLE_TEMPERATURE_FRAME_TYPE;
            hd_result.flags = 0;
            hd_result.module = c_ID1;
            hd_result.group = c_ID2;
            // Temporarily set D1 to 0xC0 to differentiate from other frames
            hd_result.data[1] = 0xC0;  
            hd_result.data[2] = node;
            hd_result.data[3] = group;
            // Add to list CAN2MQTT: command_str, hd_result
            check = gateway_AddElementToList(GATEWAY_MQTT2CAN_LIST, &hd_mask, 
                &hd_check, NULL, command_str, &hd_result);
            if(check != EXIT_SUCCESS)
            {
                #ifdef DEBUG_TIM_ERRORS
                debug_print("htiml_addIRModuleToGateway: Error adding "
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
 *          HAPCAN_NO_RESPONSE: No response
 *          
 */
static int htiml_getTempPayload(hapcanCANData *hd_received, void** payload, 
        int *payloadlen)
{
    // D2 is Type of Temperature message:
    //    - 0x17 – current temperature
    //    - 0x18 – thermostat
    //    - 0xF6 – temperature error
    int ret = HAPCAN_MQTT_RESPONSE;
    int pair;
    char *str;
    jsonFieldData j_arr[5];    
    unsigned int len;
    int16_t i;
    double calc;
    switch(hd_received->data[2])
    {
        //-------------------------------------------
        // Current Temperature Frame
        //    - {"Temperature": Float, "Thermostat": Float, "Hysteresis": Float}
        //-------------------------------------------        
        case 0x17:
            // SET PAYLOAD AND PAYLOADLEN
            pair = 0;
            i = hd_received->data[4] | (hd_received->data[3] << 8);
            calc = i * 0.0625;
            j_arr[pair].field = "Temperature";
            j_arr[pair].value_type = JSON_TYPE_DOUBLE;
            j_arr[pair].double_value = calc;
            pair++;
            j_arr[pair].field = "Thermostat";
            i = hd_received->data[6] | (hd_received->data[5] << 8);
            calc = i * 0.0625;
            j_arr[pair].value_type = JSON_TYPE_DOUBLE;
            j_arr[pair].double_value = calc;
            pair++;
            j_arr[pair].field = "Hysteresis";
            i = hd_received->data[7] + 1;
            calc = i * 0.0625;
            j_arr[pair].value_type = JSON_TYPE_DOUBLE;
            j_arr[pair].double_value = calc;
            pair++;
            jh_getStringFromFieldValuePairs(j_arr, pair, &str);
            len = strlen(str);
            if(len > 0)
            {
                *payload = malloc(len + 1);
                strcpy(*payload, str);
                ret = HAPCAN_MQTT_RESPONSE;
                *payloadlen = len; 
            }
            else
            {
                *payload = NULL;
                *payloadlen = 0;
                ret = HAPCAN_RESPONSE_ERROR;
            }
            // free
            free(str);
            break;
        //-------------------------------------------
        // Thermostat Frame
        //    - {"Position": Integer, "State": Integer}   
        //-------------------------------------------
        case 0x18: 
            // SET PAYLOAD AND PAYLOADLEN
            pair = 0;            
            j_arr[pair].field = "Position";
            j_arr[pair].value_type = JSON_TYPE_INT;
            j_arr[pair].int_value = hd_received->data[3];
            pair++;
            if(hd_received->data[7] == 0x00)
            {
                j_arr[pair].field = "State";            
                j_arr[pair].value_type = JSON_TYPE_STRING;
                j_arr[pair].str_value = "OFF";
                pair++;
            }
            else if(hd_received->data[7] == 0xFF)
            {
                j_arr[pair].field = "State";            
                j_arr[pair].value_type = JSON_TYPE_STRING;
                j_arr[pair].str_value = "ON";
                pair++;
            }
            jh_getStringFromFieldValuePairs(j_arr, pair, &str);
            len = strlen(str);
            if(len > 0)
            {
                *payload = malloc(len + 1);
                strcpy(*payload, str);
                ret = HAPCAN_MQTT_RESPONSE;
                *payloadlen = len; 
            }
            else
            {
                *payload = NULL;
                *payloadlen = 0;
                ret = HAPCAN_RESPONSE_ERROR;
            }
            // free
            free(str);
            break;        
        //-------------------------------------------
        // Temperature Sensor Error Frame
        //    - Integer (error)
        //-------------------------------------------
        case 0xF6:
            len = snprintf(NULL, 0, "%d", hd_received->data[3]);
            if(len > 0)
            {
                *payload = malloc(len + 1);
                snprintf(*payload, len + 1, "%d", hd_received->data[3]);
                *payloadlen = len;
                ret = HAPCAN_MQTT_RESPONSE;
            }
            else
            {
                *payload = NULL;
                *payloadlen = 0;
                ret = HAPCAN_RESPONSE_ERROR;
            }
            break;
        //-------------------------------------------
        // OTHER - ERROR
        //-------------------------------------------
        default:
            #ifdef DEBUG_TIM_ERRORS
            debug_print("htiml_getTempPayload: Unknown Temperature "
                "Frame Type!\n");
            #endif
            break;
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
static int htiml_getTempHAPCANFrame(void *payload, int payloadlen, 
            hapcanCANData *hd_result)
{
    // D1 is Type of Temperature message:
    //    - 0x17 – current temperature
    //    - 0x18 – thermostat
    //    - 0xF6 – temperature error
    //    - 0xC0 or > 0xE0: IR Frame (Set to 0xC0 in htim_addToGateway)
    int ret = HAPCAN_NO_RESPONSE;
    int check;
    bool valid = false;
    int16_t i;
    double calc;
    int temp;
    char *str = NULL;
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
        // Check the result message D1 (previously set when added to gateway)
        switch(hd_result->data[1])
        {
            case 0x18:
                //--------------------------------------------------------------
                // COMMAND TOPIC ACCEPTED PAYLOADS:
                // 1. "ON", "OFF", "TOGGLE"
                // 2. Double (setpoint)
                // 3. JSON: {"Setpoint":Double}
                // 4. JSON: {"Increase": Double}
                // 5. JSON: {"Decrease": Double}
                //--------------------------------------------------------------
                if( aux_compareStrings(str, "ON") )
                {
                    hd_result->data[0] = 0x07; // INSTR1. 0x07 = Turn ON
                    hd_result->data[1] = 0x01; // INSTR2. 0x01 = Thermostat
                    hd_result->data[4] = 0xFF; // INSTR3. 0xXX
                    // INSTR4. Sensor (set an htim_addToGateway)
                    hd_result->data[6] = 0xFF; // INSTR5. 0xXX  
                    hd_result->data[7] = 0xFF; // INSTR6. 0xXX
                    valid = true;
                }
                else if( aux_compareStrings(str, "OFF") )
                {
                    hd_result->data[0] = 0x06; // INSTR1. 0x06 = Turn OFF
                    hd_result->data[1] = 0x01; // INSTR2. 0x01 = Thermostat
                    hd_result->data[4] = 0xFF; // INSTR3. 0xXX
                    // INSTR4. Sensor (set in htim_addToGateway)
                    hd_result->data[6] = 0xFF; // INSTR5. 0xXX  
                    hd_result->data[7] = 0xFF; // INSTR6. 0xXX
                    valid = true;
                }
                else if( aux_compareStrings(str, "TOGGLE") )
                {
                    hd_result->data[0] = 0x08; // INSTR1. 0x06 = Toggle
                    hd_result->data[1] = 0x01; // INSTR2. 0x01 = Thermostat
                    hd_result->data[4] = 0xFF; // INSTR3. 0xXX
                    // INSTR4. Sensor (set in htim_addToGateway)
                    hd_result->data[6] = 0xFF; // INSTR5. 0xXX  
                    hd_result->data[7] = 0xFF; // INSTR6. 0xXX
                    valid = true;
                }
                else if(aux_parseValidateDouble(str, &calc, -55, 125))
                {                                        
                    i = (int16_t)(calc / 0.0625); 
                    hd_result->data[0] = 0x03; // INSTR1. 0x03 = Set
                    hd_result->data[1] = (uint8_t)((i & 0xFF00) >> 8); 
                        // INSTR2. THMSB
                    hd_result->data[4] = (uint8_t)(i & 0x00FF);
                        // INSTR3. THLSB
                    // INSTR4. Sensor (set in htim_addToGateway)
                    hd_result->data[6] = 0xFF; // INSTR5. 0xXX  
                    hd_result->data[7] = 0xFF; // INSTR6. 0xXX 
                    valid = true;
                }
                else
                {
                    valid = false;
                    // Check for JSON - Get the JSON Object
                    jh_getObject(str, &obj);                    
                    // Check if error parsing
                    if(obj != NULL)
                    {                        
                        // Setpoint
                        check = jh_getObjectFieldAsDouble(obj, "Setpoint", 
                                &calc);                        
                        valid = (check == JSON_OK) && (calc >= -55) && 
                                (calc <= 125);                
                        if( valid )
                        {
                            i = (int16_t)(calc / 0.0625); 
                            hd_result->data[0] = 0x03; // INSTR1. 0x03 = Set
                            hd_result->data[1] = (uint8_t)((i & 0xFF00) >> 8); 
                                // INSTR2. THMSB
                            hd_result->data[4] = (uint8_t)(i & 0x00FF);
                                // INSTR3. THLSB
                            // INSTR4. Sensor (set in htim_addToGateway)
                            hd_result->data[6] = 0xFF; // INSTR5. 0xXX  
                            hd_result->data[7] = 0xFF; // INSTR6. 0xXX  
                        }
                        if(!valid)
                        {
                            // Increase
                            check = jh_getObjectFieldAsDouble(obj, "Increase", 
                                    &calc);
                            valid = !valid && (check == JSON_OK) && (calc > 0) 
                                    && (calc <= 16);
                            if( valid )
                            {
                                if(calc > 15.95)
                                {
                                    calc = 0;
                                }
                                i = (int16_t)(calc / 0.0625); 
                                hd_result->data[0] = 0x05; // INSTR1. 0x05: Incr
                                hd_result->data[1] = (uint8_t)(i & 0x00FF); 
                                    // INSTR2. STEP
                                hd_result->data[4] = 0xFF; // INSTR3. 0xXX
                                // INSTR4. Sensor (set in htim_addToGateway)
                                hd_result->data[6] = 0xFF; // INSTR5. 0xXX  
                                hd_result->data[7] = 0xFF; // INSTR6. 0xXX  
                            }
                        }
                        if(!valid)
                        {
                            // Decrease
                            check = jh_getObjectFieldAsDouble(obj, "Decrease", 
                                    &calc);
                            valid = !valid && (check == JSON_OK) && (calc > 0) 
                                    && (calc <= 16);
                            if( valid )
                            {
                                if(calc > 15.95)
                                {
                                    calc = 0;
                                }
                                i = (int16_t)(calc / 0.0625);  
                                hd_result->data[0] = 0x04; // INSTR1. 0x04: Decr
                                hd_result->data[1] = (uint8_t)(i & 0x00FF); 
                                    // INSTR2. STEP
                                hd_result->data[4] = 0xFF; // INSTR3. 0xXX
                                // INSTR4. Sensor (set in htim_addToGateway)
                                hd_result->data[6] = 0xFF; // INSTR5. 0xXX  
                                hd_result->data[7] = 0xFF; // INSTR6. 0xXX  
                            }
                        }
                    }
                }
                break;
            case 0xC0:
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
                    // INSTR2
                    check = jh_getObjectFieldAsInt(obj, "INSTR2", &temp);
                    valid = valid && (check == JSON_OK) && 
                            (temp >= 0) && (temp <= 255);
                    if( valid )
                    {
                        hd_result->data[1] = (uint8_t)temp;
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
                    valid = valid && (check == JSON_OK) && 
                            (temp >= 0) && (temp <= 255);                
                }
                break;            
            default:
                // 0x17 and 0xF6 are not used - they do not accept commands
                break;
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
void htim_addToGateway(void)
{
    int i_module;
    int n_modules;
    int node;
    int group;
    int i_channel;
    int n_channels;
    int channel;
    char *state_str = NULL;
    char *command_str = NULL;
    int check;
    bool valid;
    //------------------------------------------------
    // Get the number of configured button modules
    //------------------------------------------------
    check = jh_getJArrayElements("TIMs", 0, NULL, JSON_DEPTH_LEVEL, 
            &n_modules);
    if(check == JSON_OK)
    {
        for(i_module = 0; i_module < n_modules; i_module++)
        {
            //------------------------------------------------
            // Get the module information
            //------------------------------------------------
            valid = true;
            // Node (Module)
            check = jh_getJFieldInt("TIMs", i_module, 
                    "node", 0, NULL, &node);
            valid = valid && (check == JSON_OK);
            // Group
            check = jh_getJFieldInt("TIMs", i_module, 
                    "group", 0, NULL, &group);
            valid = valid && (check == JSON_OK);
            // Number of channels
            check = jh_getJArrayElements("TIMs", i_module, 
                    "temperature", JSON_DEPTH_FIELD, &n_channels);
            valid = valid && (check == JSON_OK);
            valid = valid && (n_channels <= TIM_N_SENSORS);
            #ifdef DEBUG_TIM_ERRORS
            if(!valid)
            {
                debug_print("htim_addToGateway: Basic Information Error!\n");
            }
            #endif
            if(valid)
            {                
                //-----------------------------------
                // Check temperature sensors
                //-----------------------------------
                for(i_channel = 0; i_channel < n_channels; i_channel++)
                {                    
                    //-----------------------------------
                    // Get Channel
                    //-----------------------------------            
                    check = jh_getJFieldInt("TIMs", i_module,
                        "temperature", i_channel, "channel", &channel);
                    valid = (check == JSON_OK) && (channel > TIM_SENSOR_1) && 
                            (channel <= TIM_N_SENSORS);
                    #ifdef DEBUG_TIM_ERRORS
                    if(!valid)
                    {
                        debug_print("htim_addToGateway: Channel Information "
                            "Error!\n");
                    }
                    #endif
                    if(!valid)
                    {
                        break;
                    }
                    //-----------------------------------
                    // Current Temperature Frame - Mandatory
                    //-----------------------------------            
                    check = jh_getJFieldStringCopy("TIMs", i_module, 
                            "temperature", i_channel, "temperatureState", 
                            &state_str);
                    valid = (check == JSON_OK) && (state_str != NULL);
                    if(valid)
                    {
                        htiml_addTemperatureModuleToGateway(node, group, 
                            channel, state_str);
                    }
                    free(state_str);
                    state_str = NULL;
                    //-----------------------------------
                    // Thermostat Frame (Optional)
                    //-----------------------------------
                    check = jh_getJFieldStringCopy("TIMs", i_module, 
                            "temperature", i_channel, "thermostatState", 
                            &state_str);
                    if(check != JSON_OK)
                    {
                        state_str = NULL;
                    }
                    check = jh_getJFieldStringCopy("TIMs", i_module, 
                            "temperature", i_channel, "thermostatCommand",
                             &command_str);
                    if(check != JSON_OK)
                    {
                        command_str = NULL;
                    }
                    valid = ((state_str != NULL) || (command_str != NULL));
                    if(valid)
                    {
                        htiml_addThermostatModuleToGateway(node, group, channel,
                                state_str, command_str);
                    }
                    free(state_str);
                    state_str = NULL;
                    free(command_str);
                    command_str = NULL;            
                    //-----------------------------------
                    // Temperature Error Frame 
                    //-----------------------------------            
                    check = jh_getJFieldStringCopy("TIMs", i_module, 
                            "temperature", i_channel, "errorState", 
                            &state_str);
                    valid = (check == JSON_OK) && (state_str != NULL);
                    if(valid)
                    {
                        htiml_addTErrorModuleToGateway(node, group, channel, 
                            state_str);
                    }
                    free(state_str);
                    state_str = NULL;
                }
                //-----------------------------------
                // Check IR Transmitter
                //-----------------------------------
                check = jh_getJFieldStringCopy("TIMs", i_module, 
                    "irCommand", 0, NULL, &command_str);
                if(check != JSON_OK)
                {
                    command_str = NULL;
                }
                valid = (command_str != NULL);
                if(valid)
                {
                    htiml_addIRModuleToGateway(node, group, command_str);
                }                
                free(command_str);
                command_str = NULL;                
            }            
        }
    }
}

/**
 * Set a payload based on the data received, and add it to the MQTT Pub Buffer.       
 */
int htim_setCAN2MQTTResponse(char *state_str, hapcanCANData *hd_received, 
        unsigned long long timestamp)
{
    int ret = HAPCAN_NO_RESPONSE;
    int check;
    void* payload = NULL;
    int payloadlen;
    // Set the payload
    check = htiml_getTempPayload(hd_received, &payload, &payloadlen);
    if(check == HAPCAN_MQTT_RESPONSE)
    {
        // Set MQTT Pub buffer
        if(state_str != NULL)
        {
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
int htim_setMQTT2CANResponse(hapcanCANData *hd_result, void *payload, 
        int payloadlen, unsigned long long timestamp)
{
    int ret = HAPCAN_NO_RESPONSE;
    int check;
    // Set the payload
    check = htiml_getTempHAPCANFrame(payload, payloadlen, hd_result);
    if(check == HAPCAN_CAN_RESPONSE)
    {
        ret = hapcan_addToCANWriteBuffer(hd_result, timestamp, true);
    }
    return ret;
}