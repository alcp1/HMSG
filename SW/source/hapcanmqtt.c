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
#include "config.h"
#include "debug.h"
#include "hapcan.h"
#include "hapcanconfig.h"
#include "hapcanmqtt.h"
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
static int getHAPCANFromRawMQTT(void* payload, int payloadlen, 
        hapcanCANData* hCD_ptr);
static int checkRawSubTopic(char *str_command_topic);

/* From MQTT GENERIC to HAPCAN 
 * - returns HAPCAN_NO_RESPONSE / HAPCAN_CAN_RESPONSE / HAPCAN_RESPONSE_ERROR 
 */
static int getHAPCANFromRawMQTT(void* payload, int payloadlen, 
        hapcanCANData* hCD_ptr)
{
    // GENERIC FRAME:
    /* {"Frame":Frame,"Flags":Flags,"Module":Module,"Group":Group,
     * "D0":D0,"D1":D1,"D2":D2,"D3":D3,"D4":D4,"D5":D5,"D6":D6,"D7":D7} */
    json_object *obj = NULL;
    int ret;
    int check;
    int value;
    int i;
    char *str = NULL;
    bool valid;
    // Check NULL or size 0
    if((payload == NULL) || (payloadlen <= 0))
    {
        valid = false;
    }
    else
    {
        // Set valid and check if all is OK at the end
        valid = true;
        // payload does not end with 0 for a string - copy it here
        str = malloc(payloadlen + 1);
        memcpy(str, payload, payloadlen);
        str[payloadlen] = 0;
        // Get the JSON Object
        jh_getObject(str, &obj);
        free(str);
        str = NULL;
        // Check if error parsing
        if(obj == NULL)
        {
            valid = false;
        }
    }
    // Parse fields
    if(valid)
    {
        // Frame
        check = jh_getObjectFieldAsInt(obj, "Frame", &value);
        valid = valid && (check == JSON_OK) && (value >= 0) && (value <= 0xFFF);
        if( valid )
        {
            hCD_ptr->frametype = (uint16_t)value;
        }
        // Flags
        check = jh_getObjectFieldAsInt(obj, "Flags", &value);
        valid = valid && (check == JSON_OK) && (value >= 0) && (value <= 1);
        if( valid )
        {
            hCD_ptr->flags = (uint8_t)value;
        }
        // Module
        check = jh_getObjectFieldAsInt(obj, "Module", &value);
        valid = valid && (check == JSON_OK) && (value >= 0) && (value <= 255);
        if( valid )
        {
            hCD_ptr->module = (uint8_t)value;
        }
        // Group
        check = jh_getObjectFieldAsInt(obj, "Group", &value);
        valid = valid && (check == JSON_OK) && (value >= 0) && (value <= 255);
        if( valid )
        {
            hCD_ptr->group = (uint8_t)value;
        }
        str = strdup("D0");    
        for(i = 0; i < 8; i++)
        {
            check = jh_getObjectFieldAsInt(obj, str, &value);
            valid = valid && (check == JSON_OK) && (value >= 0) && 
                    (value <= 255);
            if( valid )
            {
                hCD_ptr->data[i] = (uint8_t)value;
            }
            // increment delimeter ("D0" to "D1" and so on)
            str[1]++;            
        }
    }    
    // Set return and Clean
    if( !valid )
    {
        ret = HAPCAN_RESPONSE_ERROR;
        // Init the data
        aux_clearHAPCANFrame(hCD_ptr);
    }
    else
    {
        ret = HAPCAN_CAN_RESPONSE;
    }
    // Free
    jh_freeObject(obj);
    free(str);
    // Leave
    return ret;    
}

/**
 * Check if the MQTT topic matches the configured MQTT Raw Sub Topic
 * 
 * \param   str_command_topic   MQTT Topic (INPUT)
 *  
 * \return  HAPCAN_NO_RESPONSE          Not a match / Configuration Error
 *          HAPCAN_CAN_RESPONSE         It is a match
 */
static int checkRawSubTopic(char *str_command_topic)
{
    int ret = HAPCAN_NO_RESPONSE; // Set to NO_RESPONSE unless OK
    char *str = NULL;
    // GET TOPIC
    ret = hconfig_getConfigStr(HAPCAN_CONFIG_RAW_SUB, &str);
    if((ret == EXIT_SUCCESS) && (str != NULL))
    {            
        if(aux_compareStrings(str_command_topic, str))
        {
            ret = HAPCAN_CAN_RESPONSE;
        }
    }
    free(str);
    return ret;
}

//----------------------------------------------------------------------------//
// EXTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
/**
 * Define a HAPCAN response to a raw message received by the MQTT Client
 **/
int hm_setRawResponseFromMQTT(char* topic, void* payload, int payloadlen, 
        hapcanCANData* hCD_ptr)
{
    int ret;
    ret = checkRawSubTopic(topic);
    if(ret == HAPCAN_CAN_RESPONSE)
    {
        ret = getHAPCANFromRawMQTT(payload, payloadlen, hCD_ptr);
    }
    return ret;
}

/**
 * Define a MQTT Raw response to a HAPCAN message received on CAN Socket
 **/
int hm_setRawResponseFromCAN(hapcanCANData* hapcanData, char** topic, 
        void** payload, int *payloadlen)
{        
    unsigned int size;
    int check;
    int ret;
    int field;
    char *str = NULL;
    jsonFieldData j_arr[12];
    // Init returns
    *topic = NULL;
    *payload = NULL;
    *payloadlen = 0;    
    // Only for application messages
    if(hapcanData->frametype > HAPCAN_START_NORMAL_MESSAGES)
    {
        // GET TOPIC
        check = hconfig_getConfigStr(HAPCAN_CONFIG_RAW_PUB, &str);      
        if(check != EXIT_SUCCESS)
        {            
            ret = HAPCAN_RESPONSE_ERROR;
        }
        else if(str == NULL)
        {
            ret = HAPCAN_NO_RESPONSE;
        }
        else
        {
            // GET TOPIC
            size = strlen(str);
            *topic = malloc(size + 1);
            strcpy(*topic, str);
            free(str);
            // SET PAYLOAD
            field = 0;
            j_arr[field].field = "Frame";
            j_arr[field].value_type = JSON_TYPE_INT;
            j_arr[field].int_value = hapcanData->frametype;
            field++;
            j_arr[field].field = "Flags";
            j_arr[field].value_type = JSON_TYPE_INT;
            j_arr[field].int_value = hapcanData->flags;
            field++;
            j_arr[field].field = "Module";
            j_arr[field].value_type = JSON_TYPE_INT;
            j_arr[field].int_value = hapcanData->module;
            field++;
            j_arr[field].field = "Group";
            j_arr[field].value_type = JSON_TYPE_INT;
            j_arr[field].int_value = hapcanData->group;
            field++;
            j_arr[field].field = "D0";
            j_arr[field].value_type = JSON_TYPE_INT;
            j_arr[field].int_value = hapcanData->data[0];
            field++;
            j_arr[field].field = "D1";
            j_arr[field].value_type = JSON_TYPE_INT;
            j_arr[field].int_value = hapcanData->data[1];
            field++;
            j_arr[field].field = "D2";
            j_arr[field].value_type = JSON_TYPE_INT;
            j_arr[field].int_value = hapcanData->data[2];
            field++;
            j_arr[field].field = "D3";
            j_arr[field].value_type = JSON_TYPE_INT;
            j_arr[field].int_value = hapcanData->data[3];
            field++;
            j_arr[field].field = "D4";
            j_arr[field].value_type = JSON_TYPE_INT;
            j_arr[field].int_value = hapcanData->data[4];
            field++;
            j_arr[field].field = "D5";
            j_arr[field].value_type = JSON_TYPE_INT;
            j_arr[field].int_value = hapcanData->data[5];
            field++;
            j_arr[field].field = "D6";
            j_arr[field].value_type = JSON_TYPE_INT;
            j_arr[field].int_value = hapcanData->data[6];
            field++;
            j_arr[field].field = "D7";
            j_arr[field].value_type = JSON_TYPE_INT;
            j_arr[field].int_value = hapcanData->data[7];
            field++;
            jh_getStringFromFieldValuePairs(j_arr, field, &str);            
            size = strlen(str);
            *payload = malloc(size + 1);
            strcpy(*payload, str);
            ret = HAPCAN_MQTT_RESPONSE;
            // SET PAYLOAD LEN
            *payloadlen = size; 
            // free
            free(str);
        }               
    }
    else
    {
        ret = HAPCAN_NO_RESPONSE;
    }    
    // return
    return ret;
}