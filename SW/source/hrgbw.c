//----------------------------------------------------------------------------//
//                               OBJECT HISTORY                               //
//----------------------------------------------------------------------------//
//  REVISION |    DATE     |                               |      AUTHOR      //
//----------------------------------------------------------------------------//
//  1.00     | 19/Dec/2022 |                               | ALCP             //
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
#include <pthread.h>
#include "auxiliary.h"
#include "canbuf.h"
#include "config.h"
#include "debug.h"
#include "gateway.h"
#include "hapcan.h"
#include "hapcanconfig.h"
#include "hrgbw.h"
#include "jsonhandler.h"
#include "mqtt.h"
#include "mqttbuf.h"

//----------------------------------------------------------------------------//
// INTERNAL DEFINITIONS
//----------------------------------------------------------------------------//
enum RGBWColors
{
    RGBW_COLOUR_R = 0,
    RGBW_COLOUR_G,
    RGBW_COLOUR_B,
    RGBW_COLOUR_W,
    RGBW_MASTER,
    RGBW_N_COLOURS
};
#define RGBW_RGB_COMMAND    RGBW_N_COLOURS + 1
#define RGBW_RGBW_COMMAND   RGBW_N_COLOURS + 2

//----------------------------------------------------------------------------//
// INTERNAL TYPES
//----------------------------------------------------------------------------//
typedef struct rgbwList_t 
{
    //------------------------
    // General Identification
    //------------------------
    uint8_t node;
    uint8_t group;
    //------------------------
    // Type Fields
    //------------------------
    bool isRGB;
    bool isRGBW;
    //------------------------
    // Outputs
    //------------------------
    uint8_t colour[RGBW_N_COLOURS];
    //------------------------
    // Update Control
    //------------------------
    bool isColourUpdated[RGBW_N_COLOURS];
    bool ignore;
    //------------------------
    // RGB State Topic
    //------------------------
    char *rgb_state_str;
    //------------------------
    // Independent Channels State Topics
    //------------------------
    char *channel1_state_str;
    char *channel2_state_str;
    char *channel3_state_str;
    char *channel4_state_str;
    //------------------------
    // Linked List Control
    //------------------------
    struct rgbwList_t *next;
} rgbwList_t;

//----------------------------------------------------------------------------//
// INTERNAL GLOBAL VARIABLES
//----------------------------------------------------------------------------//
static pthread_mutex_t g_rgbw_mutex = PTHREAD_MUTEX_INITIALIZER;
static rgbwList_t* g_hrgbw_head = NULL;
static int g_lastSentNode;
static int g_lastSentGroup;
static int g_lastSentCount;

//----------------------------------------------------------------------------//
// INTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
// Linked List functions
static void rgbwl_clearElementData(rgbwList_t* element);
static void rgbwl_freeElementData(rgbwList_t* element);
static void rgbwl_addToList(rgbwList_t* element);
static rgbwList_t* rgbwl_getFromOffset(int offset);    // NULL means error
static void rgbwl_deleteList(void);
static void rgbwl_addElementToList(int node, int group, bool isRGBW, 
        bool isRGB, char *rgb_state_str, char *channel1_state_str, 
        char *channel2_state_str, char *channel3_state_str, 
        char *channel4_state_str);
#ifdef DEBUG_RGBW_FULL
static void rgbwl_printElementData(rgbwList_t* element);
#endif
// HAPCAN Module functions
static void rgbw_addRGBWChannelToGateway(int node, int group, bool isRGBW, 
        bool isRGB, int channel, char *state_str, char *command_str);
static int rgbw_getRGBWPayload(char *state_str, hapcanCANData *hd_received, 
        void** payload, int *payloadlen);
static int rgbw_checkAndSendCAN(void);

//------------------------------------------------------------------------------
// LINKED LIST FUNCTIONS
//------------------------------------------------------------------------------
// Clear all fields from rgbwList_t //
static void rgbwl_clearElementData(rgbwList_t* element)
{
    // No pointers to be freed
    memset(element, 0, sizeof(*element));
    element->rgb_state_str = NULL;
    element->channel1_state_str = NULL;
    element->channel2_state_str = NULL;
    element->channel3_state_str = NULL;
    element->channel4_state_str = NULL;
}

// Free allocated fields from rgbwList_t //
static void rgbwl_freeElementData(rgbwList_t* element)
{
    // Free pointers
    free(element->rgb_state_str);    
    free(element->channel1_state_str);    
    free(element->channel2_state_str);    
    free(element->channel3_state_str);    
    free(element->channel4_state_str);
    element->rgb_state_str = NULL;
    element->channel1_state_str = NULL;
    element->channel2_state_str = NULL;
    element->channel3_state_str = NULL;
    element->channel4_state_str = NULL;
}

// Add element to the list
static void rgbwl_addToList(rgbwList_t* element)
{
    rgbwList_t *link;
    int i;
    unsigned int len;
    // Create a new element to the list
    link = (rgbwList_t*)malloc(sizeof(*link));	
    // Copy structure data
    link->node = element->node;
    link->group = element->group;
    link->isRGB = element->isRGB;
    link->isRGBW = element->isRGBW;
    link->ignore = element->ignore;
    for(i = 0; i < RGBW_N_COLOURS; i++)
    {
        link->colour[i] = element->colour[i];
        link->isColourUpdated[i] = element->isColourUpdated[i];
    }
    //---------------------
    // Copy strings
    //---------------------
    // RGB State
    if(element->rgb_state_str != NULL)
    {
        len = strlen(element->rgb_state_str);
        link->rgb_state_str = malloc(len + 1);
        strcpy(link->rgb_state_str, element->rgb_state_str);
    }
    else
    {
        link->rgb_state_str = NULL;
    }
    // Channel 1 State
    if(element->channel1_state_str != NULL)
    {
        len = strlen(element->channel1_state_str);
        link->channel1_state_str = malloc(len + 1);
        strcpy(link->channel1_state_str, element->channel1_state_str);
    }
    else
    {
        link->channel1_state_str = NULL;
    }
    // Channel 2 State
    if(element->channel2_state_str != NULL)
    {
        len = strlen(element->channel2_state_str);
        link->channel2_state_str = malloc(len + 1);
        strcpy(link->channel2_state_str, element->channel2_state_str);
    }
    else
    {
        link->channel2_state_str = NULL;
    }
    // Channel 3 State
    if(element->channel3_state_str != NULL)
    {
        len = strlen(element->channel3_state_str);
        link->channel3_state_str = malloc(len + 1);
        strcpy(link->channel3_state_str, element->channel3_state_str);
    }
    else
    {
        link->channel3_state_str = NULL;
    }
    // Channel 4 State
    if(element->channel4_state_str != NULL)
    {
        len = strlen(element->channel4_state_str);
        link->channel4_state_str = malloc(len + 1);
        strcpy(link->channel4_state_str, element->channel4_state_str);
    }
    else
    {
        link->channel4_state_str = NULL;
    }
    // Set next in list to previous g_hrgbw_header (previous first node)
    link->next = g_hrgbw_head;	
    // Point g_hrgbw_header (first node) to current element (new first node)
    g_hrgbw_head = link;
}

// get element from g_hrgbw_header (after offset positions)
static rgbwList_t* rgbwl_getFromOffset(int offset)
{
    int len;
    rgbwList_t* current = NULL;
    // Check offset parameter
    if( offset < 0 )
    {
        return NULL;
    }
    // Check all
    len = 0;
    for(current = g_hrgbw_head; current != NULL; current = current->next) 
    {
        if(len >= offset)
        {
            break;
        }
        len++;        
    }
    // Return
    return current;
}

// Delete list
static void rgbwl_deleteList(void)
{
    rgbwList_t* current;
    rgbwList_t* next;
    // Check all
    current = g_hrgbw_head;
    while(current != NULL) 
    {
        //*******************************//
        // Get next structure address    //
        //*******************************//
        next = current->next;
        //*******************************//
        // Free fields that are pointers //
        //*******************************//
        rgbwl_freeElementData(current);
        //*******************************//
        // Free structure itself         //
        //*******************************//
        free(current);
        //*******************************//
        // Get new current address       //
        //*******************************//
        current = next;
        g_hrgbw_head = current;
    }    
}

// Add elements to the List
static void rgbwl_addElementToList(int node, int group, bool isRGBW, 
        bool isRGB, char *rgb_state_str, char *channel1_state_str, 
        char *channel2_state_str, char *channel3_state_str, 
        char *channel4_state_str)
{
    rgbwList_t element;
    int i;
    unsigned int len;
    rgbwl_clearElementData(&element);
    element.group = group;
    element.node = node;
    element.isRGBW = isRGBW;
    element.isRGB = isRGB;
    // Set flags
    for(i = 0; i < RGBW_N_COLOURS; i++)
    {
        element.isColourUpdated[i] = false;            
    }
    element.ignore = false;
    //-----------------------------
    // Copy strings
    //-----------------------------
    // RGB State
    if(rgb_state_str != NULL)
    {
        len = strlen(rgb_state_str);
        element.rgb_state_str = malloc(len + 1);
        strcpy(element.rgb_state_str, rgb_state_str);
    }
    // Channel 1 State
    if(channel1_state_str != NULL)
    {
        len = strlen(channel1_state_str);
        element.channel1_state_str = malloc(len + 1);
        strcpy(element.channel1_state_str, channel1_state_str);
    }
    // Channel 2 State
    if(channel2_state_str != NULL)
    {
        len = strlen(channel2_state_str);
        element.channel2_state_str = malloc(len + 1);
        strcpy(element.channel2_state_str, channel2_state_str);
    }
    // Channel 3 State
    if(channel3_state_str != NULL)
    {
        len = strlen(channel3_state_str);
        element.channel3_state_str = malloc(len + 1);
        strcpy(element.channel3_state_str, channel3_state_str);
    }
    // Channel 4 State
    if(channel4_state_str != NULL)
    {
        len = strlen(channel4_state_str);
        element.channel4_state_str = malloc(len + 1);
        strcpy(element.channel4_state_str, channel4_state_str);
    }
    // Add
    rgbwl_addToList(&element);
    // Free temp data and then return
    rgbwl_freeElementData(&element);
}

// Debug elements
#ifdef DEBUG_RGBW_FULL
static void rgbwl_printElementData(rgbwList_t* element)
{    
    int i;
    debug_print("rgbwl_printElementData:\n");
    debug_print(" - element->node: %d\n", element->node);
    debug_print(" - element->group: %d\n", element->group);
    debug_print(" - element->isRGB: %d\n", element->isRGB);
    debug_print(" - element->isRGBW: %d\n", element->isRGBW);
    if(element->rgb_state_str != NULL)
    {
        debug_print(" - element->rgb_state_str: %s\n", element->rgb_state_str);
    }
    else
    {
        debug_print(" - element->rgb_state_str: NULL\n");
    }
    if(element->channel1_state_str != NULL)
    {
        debug_print(" - element->channel1_state_str: %s\n", element->channel1_state_str);
    }
    else
    {
        debug_print(" - element->channel1_state_str: NULL\n");
    }
    if(element->channel2_state_str != NULL)
    {
        debug_print(" - element->channel2_state_str: %s\n", element->channel2_state_str);
    }
    else
    {
        debug_print(" - element->channel2_state_str: NULL\n");
    }
    if(element->channel3_state_str != NULL)
    {
        debug_print(" - element->channel3_state_str: %s\n", element->channel3_state_str);
    }
    else
    {
        debug_print(" - element->channel3_state_str: NULL\n");
    }
    if(element->channel4_state_str != NULL)
    {
        debug_print(" - element->channel4_state_str: %s\n", element->channel4_state_str);
    }
    else
    {
        debug_print(" - element->channel4_state_str: NULL\n");
    }
    for(i = 0; i < RGBW_N_COLOURS; i++)
    {
        debug_print(" - element->isColourUpdated[%d]: %d\n", i, 
                element->isColourUpdated[i]);
    }
    for(i = 0; i < RGBW_N_COLOURS; i++)
    {
        debug_print(" - element->colour[%d]: %d\n", i, element->colour[i]);
    }    
}
#endif

//------------------------------------------------------------------------------
// HAPCAN MODULE FUNCTIONS
//------------------------------------------------------------------------------
/*
 * Add a single button to the HAPCAN <--> MQTT gateway based on the data read 
 * from JSON file
 * 
 * \param   node            (INPUT) module / node
 * \param   group           (INPUT) group
 * \param   isRGBW          (INPUT) If the module has the outputs linked as RGBW
 * \param   isRGB           (INPUT) If the module has the outputs linked as RGB
 * \param   channel         (INPUT) button channel
 * \param   state_str       (INPUT) MQTT state topic
 * \param   command_str     (INPUT) MQTT command topic
 *  
 */
static void rgbw_addRGBWChannelToGateway(int node, int group, bool isRGBW, 
        bool isRGB, int channel, char *state_str, char *command_str)
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
    if(!isRGBW && !isRGB)
    {
       valid = valid && (channel >= 1) && (channel <= RGBW_N_COLOURS); 
    }
    else
    {
        valid = valid && (channel >= 1) && (channel <= RGBW_RGBW_COMMAND); 
    }
    if(!valid)
    {
        #ifdef DEBUG_RGBW_ERRORS
        debug_print("rgbw_addRGBWChannelToGateway - parameter error!\n");
        debug_print("rgbw_addRGBWChannelToGateway - node:%d!\n", node);
        debug_print("rgbw_addRGBWChannelToGateway - group:%d!\n", group);
        debug_print("rgbw_addRGBWChannelToGateway - channel:%d!\n", channel);
        debug_print("rgbw_addRGBWChannelToGateway - isRGBW:%d!\n", isRGBW);
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
        // For the RGBW frame, the following fields should be checked for 
        // a match:
        // - frame type
        // - node (module)
        // - group
        // - channel (D2)        
        hd_mask.frametype = 0xFFF;
        hd_mask.module = 0xFF;
        hd_mask.group = 0xFF;
        hd_mask.data[2] = 0xFF;
        hd_check.frametype = HAPCAN_RGBW_FRAME_TYPE;
        hd_check.module = node;
        hd_check.group = group;
        hd_check.data[2] = channel;        
        // Add to list CAN2MQTT: hd_mask, hd_check, state_topic
        check = gateway_AddElementToList(GATEWAY_CAN2MQTT_LIST, &hd_mask, 
            &hd_check, state_str, NULL, &hd_result);
        if(check != EXIT_SUCCESS)
        {
            #ifdef DEBUG_RGBW_ERRORS
            debug_print("rgbw_addRGBWChannelToGateway: Error "
                    "adding to CAN2MQTT!\n");
            #endif
        }
        //---------------------------
        // Set data for MQTT2CAN
        //---------------------------
        // When command is NULL, do not add to gateway
        if(command_str != NULL)
        {
            // For a RGBW frame, the following fields should be sent to the CAN Bus 
            // to control a RGBW channel
            // - frame type is 0x10A
            // - flag is 0
            // - Computer ID is set
            // - D1 is channel (0 if RGBW)
            // - D2 is node
            // - D3 is group        
            aux_clearHAPCANFrame(&hd_mask);
            aux_clearHAPCANFrame(&hd_check);
            hd_result.frametype = HAPCAN_RGBW_FRAME_TYPE;
            hd_result.flags = 0;
            hd_result.module = c_ID1;
            hd_result.group = c_ID2;
            hd_result.data[1] = channel;
            hd_result.data[2] = node;
            hd_result.data[3] = group;        
            // Add to list CAN2MQTT: command_str, hd_result
            check = gateway_AddElementToList(GATEWAY_MQTT2CAN_LIST, &hd_mask, 
                &hd_check, NULL, command_str, &hd_result);
            if(check != EXIT_SUCCESS)
            {
                #ifdef DEBUG_RGBW_ERRORS
                debug_print("rgbw_addRGBWChannelToGateway: Error adding "
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
 * \param   hd_received     (INPUT) pointer to HAPCAN message received
 * \param   payload         (OUTPUT) payload to be filled
 * \param   payloadlen      (OUTPUT) payloadlen to be filled
 *                      
 * \return  HAPCAN_MQTT_RESPONSE: Payload set (OK)
 *          HAPCAN_RESPONSE_ERROR: Error - Payload not set
 *          HAPCAN_NO_RESPONSE: No Payload to be set
 *          
 */
static int rgbw_getRGBWPayload(char *state_str, hapcanCANData *hd_received, 
        void** payload, int *payloadlen)
{
    int ret = HAPCAN_RESPONSE_ERROR;
    int node;
    int group;
    int channel;
    bool match;
    bool isRGBState;
    bool isChannel1State;
    bool isChannel2State;
    bool isChannel3State;
    bool isChannel4State;
    rgbwList_t* current = NULL;
    rgbwList_t element;
    int i;
    int temp[RGBW_MASTER];
    bool test;
    int len;
    char str[20]; // 255,255,255,255/0
    int n_colors;
    // Get node, group and channel from received message
    node = hd_received->module;
    group = hd_received->group;
    channel = hd_received->data[2];
    if(channel < 1 || channel > RGBW_N_COLOURS)
    {
        // Channel error
        ret = HAPCAN_RESPONSE_ERROR;
    }
    else
    {
        //----------------------------------------------------------------------
        // Search for node and group and update element with received CAN Data 
        //----------------------------------------------------------------------
        // LOCK LIST
        pthread_mutex_lock(&g_rgbw_mutex);
        // Check all elements - start from 0
        current = rgbwl_getFromOffset(0);
        match = false;
        while(current != NULL) 
        {
            // Check node and group
            match = (current->node == node);
            match = match && (current->group == group);
            if(match)
            {
                // Update data from received channel
                current->isColourUpdated[channel - 1] = true;
                current->colour[channel - 1] = hd_received->data[3];
                // If a status was received, ignore has to be updated
                current->ignore = false;            
                // Copy Data
                memcpy(&element, current, sizeof(element));
                #ifdef DEBUG_RGBW_FULL
                rgbwl_printElementData(&element);
                #endif                                               
                break;
            }
            // Get new current element
            current = current->next;
        }
        // UNLOCK LIST
        pthread_mutex_unlock(&g_rgbw_mutex);    
        if(!match)
        {
            // No match
            ret = HAPCAN_RESPONSE_ERROR;
        }
        else if(match && !element.isColourUpdated[RGBW_MASTER])
        {
            // Master not updated - nothing to send yet
            ret = HAPCAN_NO_RESPONSE;
        }
        else
        {
            // Check if state topic is for RGB status
            isRGBState = aux_compareStrings(element.rgb_state_str, state_str);
            isChannel1State = aux_compareStrings(element.channel1_state_str, 
                state_str);
            isChannel2State = aux_compareStrings(element.channel2_state_str, 
                state_str);
            isChannel3State = aux_compareStrings(element.channel3_state_str, 
                state_str);
            isChannel4State = aux_compareStrings(element.channel4_state_str, 
                state_str);
            // Check if single channel
            if( (!element.isRGBW && !element.isRGB) || 
                (!element.isRGBW && element.isRGB && !isRGBState) )
            {
                if(element.isRGB)
                {
                    // If isRGB, at this point, it is a single channel (4).
                    // In this case, when the master channel gets updated, the 
                    // independent channel (4) has to be updated.
                    channel = RGBW_COLOUR_W + 1;
                }
                else if( (!element.isRGBW) && (channel == RGBW_MASTER + 1) )
                {
                    if(isChannel1State)
                    {
                        channel = RGBW_COLOUR_R + 1;
                    }
                    else if(isChannel2State)
                    {
                        channel = RGBW_COLOUR_G + 1;
                    }
                    else if(isChannel3State)
                    {
                        channel = RGBW_COLOUR_B + 1;
                    }
                    else if(isChannel4State)
                    {
                        channel = RGBW_COLOUR_W + 1;
                    }
                }
                if(element.isColourUpdated[channel - 1] && 
                        element.isColourUpdated[RGBW_MASTER])
                {                    
                    //---------------------------------------------------------
                    // Single Channel - Return a string as a single number
                    //---------------------------------------------------------
                    // Get numeric result
                    // If Master is 255, the channel value is returned
                    temp[0] = element.colour[channel - 1];
                    if(element.colour[RGBW_MASTER] == 0)
                    {
                        temp[0] = 0;
                    }
                    else if(element.colour[RGBW_MASTER] != 255)
                    {
                        // If Master is not 255, the calculation is performed
                        temp[0] = temp[0] * (element.colour[RGBW_MASTER] + 1);
                        temp[0] = temp[0] >> 8;
                    }                    
                    // Add to temp string
                    snprintf(str, 4, "%d", temp[0]);
                    len = strlen(str);
                    // Set payload and payloadlen
                    *payload = malloc(len + 1);
                    strcpy(*payload, str);
                    *payloadlen = len;
                    ret = HAPCAN_MQTT_RESPONSE;
                }
                else
                {
                    // Data is not updated yet - nothing to send
                    ret = HAPCAN_NO_RESPONSE;
                }
            }
            else
            {
                // RGBW: Check if all channels are updated and set response
                test = element.isColourUpdated[RGBW_MASTER];
                // Set number of colors to send
                n_colors = 0;
                if(element.isRGBW)
                {          
                    // RGBW
                    n_colors = RGBW_N_COLOURS - 1;
                }
                else if(element.isRGB)
                {
                    // RGB
                    n_colors = RGBW_N_COLOURS - 2;
                }                
                for(i = 0; i < n_colors; i++)
                {
                    if(!element.isColourUpdated[i])
                    {
                        test = false;
                        break;
                    }
                    // Get numeric result
                    temp[i] = element.colour[i];
                    if(element.colour[RGBW_MASTER] == 0)
                    {
                        temp[i] = 0;
                    }
                    else if(element.colour[RGBW_MASTER] != 255)
                    {
                        // If Master is not 255, the calculation is performed
                        temp[i] = temp[i] * (element.colour[RGBW_MASTER] + 1);
                        temp[i] = temp[i] >> 8;
                    }
                }
                if(test)
                {
                    //---------------------------------------------------------
                    // RGBW Channel - Return a string as comma-separated values
                    //---------------------------------------------------------
                    if(n_colors == 0)
                    {
                        ret = HAPCAN_NO_RESPONSE;
                    }
                    else if( n_colors == (RGBW_N_COLOURS - 1) )
                    {
                        // 255,255,255,255/0                        
                        snprintf(str, 16, "%d,%d,%d,%d", temp[0], temp[1], 
                                temp[2], temp[3]);
                        len = strlen(str); 
                        // Set payload and payloadlen
                        *payload = malloc(len + 1);
                        strcpy(*payload, str);
                        *payloadlen = len;
                        ret = HAPCAN_MQTT_RESPONSE;
                    }
                    else if( n_colors == (RGBW_N_COLOURS - 2) )
                    {
                        // 255,255,255/0
                        snprintf(str, 16, "%d,%d,%d", temp[0], temp[1], 
                                temp[2]);
                        len = strlen(str); 
                        // Set payload and payloadlen
                        *payload = malloc(len + 1);
                        strcpy(*payload, str);
                        *payloadlen = len;
                        ret = HAPCAN_MQTT_RESPONSE;
                    }                    
                }
                else
                {
                    // Data is not updated yet - nothing to send
                    ret = HAPCAN_NO_RESPONSE;
                }
            }
        }
    }
    // Return
    return ret;
}

/**
 * Check if there is CAN messages to be sent to update RGBW Status
 * \return  HAPCAN_NO_RESPONSE: No response was added to CAN Write Buffer
 *          HAPCAN_CAN_RESPONSE: Response added to CAN Write Buffer (OK)
 *          HAPCAN_CAN_RESPONSE_ERROR: Error adding to CAN Write Buffer (FAIL)
 *          HAPCAN_RESPONSE_ERROR: Other error
 */
static int rgbw_checkAndSendCAN(void)
{
    int ret;
    bool match;
    rgbwList_t* current = NULL;
    int node;
    int group;
    int i;
    hapcanCANData hd_result;
    unsigned long long timestamp;
    // Limit messages sent to modules not responding
    int lastSentNode = g_lastSentNode;
    int lastSentGroup = g_lastSentGroup;
    //-----------------------------------------------------------------
    // Search all configured RGBW modules for fields not updated
    //-----------------------------------------------------------------
    // LOCK LIST
    pthread_mutex_lock(&g_rgbw_mutex);
    // Check all elements - start from 0
    current = rgbwl_getFromOffset(0);
    match = false;
    while(current != NULL) 
    {
        for(i = 0; i < RGBW_N_COLOURS; i++)
        {
            if((!current->isColourUpdated[i]) && !(current->ignore))
            {
                match = true;
                node = current->node;
                group = current->group;
                g_lastSentNode = node;
                g_lastSentGroup = group;
                if((g_lastSentNode == lastSentNode) && 
                        (g_lastSentGroup == lastSentGroup))
                {
                    g_lastSentCount++;
                    if(g_lastSentCount >= HAPCAN_CAN_STATUS_SEND_RETRIES)
                    {                                        
                        #ifdef DEBUG_RGBW_ERRORS
                        debug_print("INFO: rgbw_checkAndSendCAN: Module is not "
                                "responding - Node = %d, Group = %d!\n", 
                                node, group);
                        #endif
                        match = false;
                        current->ignore = true;
                        g_lastSentCount = 0;
                    }
                }
                break;
            }
        }
        if(match)
        {
            break;
        }
        // Get new current element
        current = current->next;
    }
    // UNLOCK LIST
    pthread_mutex_unlock(&g_rgbw_mutex);    
    if(!match)
    {
        ret = HAPCAN_NO_RESPONSE;
    }
    else
    {
        //-------------------------------------------------------------
        // Send Status request to the module without needed updates
        //-------------------------------------------------------------
        // Request STATUS update for the given module
        hapcan_getSystemFrame(&hd_result, 
                HAPCAN_STATUS_REQUEST_NODE_FRAME_TYPE, node, group);
        // Get Timestamp
        timestamp = aux_getmsSinceEpoch();
        // Send - error handled inside function
        ret = hapcan_addToCANWriteBuffer(&hd_result, timestamp, true);
    }
    return ret;
}

//----------------------------------------------------------------------------//
// EXTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
/*
 * Add buttons configured in the configuration JSON file to the HAPCAN <--> MQTT
 * gateway
 */
void hrgbw_addToGateway(void)
{
    int i_module;
    int i_channel;
    int n_modules;
    int i_RGBchannel;
    int n_RGBchannels;
    int i_RGBWchannel;
    int n_RGBWchannels;
    int node;
    int group;
    bool isRGB;
    bool isRGBW;
    int channel;
    char *rgb_state_str = NULL;
    char *channel1_state_str = NULL;
    char *channel2_state_str = NULL;
    char *channel3_state_str = NULL;
    char *channel4_state_str = NULL;
    char *state_str = NULL;
    char *command_str = NULL;
    unsigned int len;
    int check;
    bool valid;
    bool configured[RGBW_N_COLOURS];
    //---------------------------------------------
    // Delete and Add to list: PROTECTED
    //---------------------------------------------
    // LOCK LIST
    pthread_mutex_lock(&g_rgbw_mutex);
    // Delete
    rgbwl_deleteList();
    // UNLOCK LIST
    pthread_mutex_unlock(&g_rgbw_mutex);
    // Add to list
    //------------------------------------------------
    // Get the number of configured RGBW Modules
    //------------------------------------------------
    check = jh_getJArrayElements("RGBWs", 0, NULL, JSON_DEPTH_LEVEL, 
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
            check = jh_getJFieldInt("RGBWs", i_module, 
                    "node", 0, NULL, &node);
            valid = valid && (check == JSON_OK);
            // Group
            check = jh_getJFieldInt("RGBWs", i_module, 
                    "group", 0, NULL, &group);
            valid = valid && (check == JSON_OK);
            // RGBW or RGB or single channel
            check = jh_getJFieldBool("RGBWs", i_module, 
                    "isRGBW", 0, NULL, &isRGBW);
            valid = valid && (check == JSON_OK);
            check = jh_getJFieldBool("RGBWs", i_module, 
                    "isRGB", 0, NULL, &isRGB);
            valid = valid && (check == JSON_OK);
            // Number of channels - RGB (optional)
            check = jh_getJArrayElements("RGBWs", i_module, 
                    "rgb", JSON_DEPTH_FIELD, &n_RGBchannels);
            if(check != JSON_OK)
            {
                n_RGBchannels = 0;
            }            
            // Number of channels - RGB (optional)
            check = jh_getJArrayElements("RGBWs", i_module, 
                    "rgbw", JSON_DEPTH_FIELD, &n_RGBWchannels);
            if(check != JSON_OK)
            {
                n_RGBWchannels = 0;
            }
            // Further validation
            if(isRGBW)
            {
                // All four channels (R, G, B, W) are considered as one entity
                valid = valid && (n_RGBWchannels == 1) && (n_RGBchannels == 0);
                #ifdef DEBUG_RGBW_ERRORS
                if(!valid)
                {
                    debug_print("INFO: hrgbw_addToGateway: isRGBW check "
                            "Error - module %d - n_RGBWchannels %d - "
                            "n_RGBchannels %d!\n", i_module, n_RGBWchannels, 
                            n_RGBchannels);
                }
                #endif
            }
            else if(isRGB)
            {
                // The first three channels (R, G, B) are considered as one 
                // single entity. A fourth channel may be setup
                valid = valid && (n_RGBchannels == 1) && (n_RGBWchannels >= 0) 
                        && (n_RGBWchannels <= 1);
                #ifdef DEBUG_RGBW_ERRORS
                if(!valid)
                {
                    debug_print("INFO: hrgbw_addToGateway: isRGB check "
                            "Error - module %d!\n", i_module);
                }
                #endif
                if(valid)
                {
                    // Get RGB Status - it is needed because when the Master 
                    // channel is updated, it is necessary to know if the status 
                    // topic is for a RGB set (Payload is x,x,x) of for a single 
                    // channel (Payload is x).
                    check = jh_getJFieldStringCopy("RGBWs", i_module,
                        "rgb", 0, "state", &rgb_state_str);
                    if(check != JSON_OK)
                    {
                        rgb_state_str = NULL; 
                    }
                    // REMARK: rgb_state_str is freed after 
                    // rgbwl_addElementToList
                }
            }
            else
            {
                // When isRGBW is false and isRGB is false, each channel can be 
                // configured independently
                valid = valid && (n_RGBchannels == 0) && (n_RGBWchannels >= 1) 
                        && (n_RGBWchannels <= (RGBW_N_COLOURS - 1) );
                #ifdef DEBUG_RGBW_ERRORS
                if(!valid)
                {
                    debug_print("INFO: hrgbw_addToGateway: single channels "
                            "check Error - module %d - n_RGBWchannels %d - "
                            "n_RGBchannels %d!\n", i_module, n_RGBWchannels, 
                            n_RGBchannels);
                }
                #endif
                if(valid)
                {
                    // Get Channel Status - it is needed because when the Master 
                    // channel is updated, it is necessary to know the channel 
                    // that the payload will be set.
                    for(i_RGBWchannel = 0; i_RGBWchannel < n_RGBWchannels; 
                    i_RGBWchannel++)
                    {                        
                        // - Channel
                        check = jh_getJFieldInt("RGBWs", i_module,
                            "rgbw", i_RGBWchannel, "channel", &channel);
                        check = jh_getJFieldStringCopy("RGBWs", 
                                    i_module,"rgbw", i_RGBWchannel, "state", 
                                    &state_str);
                        if(check != JSON_OK)
                        {
                            state_str = NULL; 
                        }
                        // Check channel
                        switch(channel)
                        {
                            case RGBW_COLOUR_R + 1:
                                if(channel1_state_str == NULL)
                                {
                                    len = strlen(state_str);
                                    channel1_state_str = malloc(len + 1);
                                    strcpy(channel1_state_str, state_str);
                                }
                                else
                                {
                                    valid = false;
                                }
                                break;
                            case RGBW_COLOUR_G + 1:
                                if(channel2_state_str == NULL)
                                {
                                    len = strlen(state_str);
                                    channel2_state_str = malloc(len + 1);
                                    strcpy(channel2_state_str, state_str);
                                }
                                else
                                {
                                    valid = false;
                                }
                                break;
                            case RGBW_COLOUR_B + 1:
                                if(channel3_state_str == NULL)
                                {
                                    len = strlen(state_str);
                                    channel3_state_str = malloc(len + 1);
                                    strcpy(channel3_state_str, state_str);
                                }
                                else
                                {
                                    valid = false;
                                }
                                break;
                            case RGBW_COLOUR_W + 1:
                                if(channel4_state_str == NULL)
                                {
                                    len = strlen(state_str);
                                    channel4_state_str = malloc(len + 1);
                                    strcpy(channel4_state_str, state_str);
                                }
                                else
                                {
                                    valid = false;
                                }
                                break;
                            default:
                                    valid = false;
                                break;
                        }
                        // Free
                        free(state_str);
                        state_str = NULL;
                    }
                    // REMARK: channel1_state_str, channel2_state_str,
                    // channel3_state_str and channel4_state_str are freed after 
                    // rgbwl_addElementToList
                    #ifdef DEBUG_RGBW_ERRORS
                    if(!valid)
                    {
                        debug_print("INFO: hrgbw_addToGateway: single channels "
                                "duplication / wrong colour Error - module %d "
                                "- n_RGBWchannels %d - n_RGBchannels %d!\n", 
                                i_module, n_RGBWchannels, n_RGBchannels);
                    }
                    #endif
                }
            }            
            if(valid)
            {
                //---------------------------
                // Add data to list
                //---------------------------
                // LOCK LIST
                pthread_mutex_lock(&g_rgbw_mutex);
                // Add this module to the RGBW List
                rgbwl_addElementToList(node, group, isRGBW, isRGB, 
                    rgb_state_str, channel1_state_str, channel2_state_str,
                    channel3_state_str, channel4_state_str);
                free(rgb_state_str);
                rgb_state_str = NULL;
                free(channel1_state_str);
                channel1_state_str = NULL;
                free(channel2_state_str);
                channel2_state_str = NULL;
                free(channel3_state_str);
                channel3_state_str = NULL;
                free(channel4_state_str);
                channel4_state_str = NULL;
                // UNLOCK LIST
                pthread_mutex_unlock(&g_rgbw_mutex);
                //------------------------------------------------------------
                // Get configuration for each channel, even if not configured
                //------------------------------------------------------------
                // Set RGBW channels as not configured
                for(i_channel = 0; i_channel < RGBW_N_COLOURS; i_channel++)
                {
                    configured[i_channel] = false;
                }
                //------------------------------------------------------------
                // Check RGBW channels
                //------------------------------------------------------------
                for(i_RGBWchannel = 0; i_RGBWchannel < n_RGBWchannels; 
                    i_RGBWchannel++)
                {
                    //---------------------------------------------------------
                    // Get the fields: Channel / State and Command
                    //---------------------------------------------------------
                    // - Channel
                    check = jh_getJFieldInt("RGBWs", i_module,
                        "rgbw", i_RGBWchannel, "channel", &channel);
                    // - State (optional)
                    check = jh_getJFieldStringCopy("RGBWs", i_module,
                        "rgbw", i_RGBWchannel, "state", &state_str);
                    if(check != JSON_OK)
                    {
                       state_str = NULL; 
                    }
                    // - Command (optional)
                    check = jh_getJFieldStringCopy("RGBWs", i_module,
                        "rgbw", i_RGBWchannel, "command", &command_str);
                    if(check != JSON_OK)
                    {
                       command_str = NULL; 
                    }
                    //---------------------------------------------------------
                    // Field Validation
                    //---------------------------------------------------------
                    if(isRGBW)
                    {
                        // No channel is needed, state and command are optional
                        valid = true;
                    }
                    else
                    {
                        // Check channel
                        valid = valid && (check == JSON_OK) && (channel >= 1) 
                                && (channel <= RGBW_MASTER);                        
                    }
                    #ifdef DEBUG_RGBW_ERRORS
                    if(!valid)
                    {
                        debug_print("INFO: hrgb_addToGateway: RGBW channel "
                                "check Error - module %d!\n", i_module);
                    }
                    #endif
                    //---------------------------------------------------------
                    // Set channel as configured and add to gateway
                    //---------------------------------------------------------
                    if(valid)
                    {
                        if(isRGBW)
                        {
                            //-------------------------------------------------
                            // Add States to Gateway - RGBW channels
                            //-------------------------------------------------
                            for(i_channel = 0; i_channel < RGBW_MASTER;
                                i_channel++)
                            {                            
                                // Add to the gateway                        
                                rgbw_addRGBWChannelToGateway(node, group, 
                                    isRGBW, isRGB, i_channel + 1, state_str, 
                                    NULL);
                                // Set as configured
                                configured[i_channel] = true;
                            }
                            //-------------------------------------------------
                            // Add States to Gateway - MASTER Channels
                            // - When it changes, the status has to be updated.
                            //------------------------------------------------- 
                            i_channel = RGBW_MASTER;
                            // Add to the gateway                        
                            rgbw_addRGBWChannelToGateway(node, group,
                                isRGBW, isRGB, i_channel + 1, state_str, NULL);
                            // Set as configured
                            configured[i_channel] = true;
                            //-------------------------------------------------
                            // Add Command to Gateway
                            //-------------------------------------------------
                            rgbw_addRGBWChannelToGateway(node, group, 
                                    isRGBW, isRGB, RGBW_RGBW_COMMAND, NULL, 
                                    command_str);                            
                        }
                        else
                        {
                            //-------------------------------------------------
                            // Add States and Commands to Gateway 
                            // - RGBW channels
                            //-------------------------------------------------
                            // Add the channels
                            rgbw_addRGBWChannelToGateway(node, group, isRGBW, 
                                isRGB, channel, state_str, command_str);
                            // Set as configured
                            configured[channel - 1] = true;
                            //-------------------------------------------------
                            // Add States to Gateway - MASTER Channels
                            // - When it changes, the status has to be updated.
                            //------------------------------------------------- 
                            i_channel = RGBW_MASTER;
                            // Add to the gateway                        
                            rgbw_addRGBWChannelToGateway(node, group,
                                isRGBW, isRGB, i_channel + 1, state_str, NULL);
                            // Set as configured
                            configured[i_channel] = true;
                        }
                    }
                    // Free
                    free(state_str);
                    state_str = NULL;
                    free(command_str);
                    command_str = NULL;                    
                }
                //------------------------------------------------------------
                // Check RGB channels
                //------------------------------------------------------------
                // Only check if RGB was configured
                if(!isRGB || isRGBW)
                {
                    n_RGBchannels = 0;
                }
                // Check the RGB channels (maximum is 1)
                for(i_RGBchannel = 0; i_RGBchannel < n_RGBchannels; 
                    i_RGBchannel++)
                {
                    //---------------------------------------------------------
                    // Get the fields: Channel / State and Command
                    //---------------------------------------------------------
                    // - Channel does not need to be checked
                    // - State (optional)
                    check = jh_getJFieldStringCopy("RGBWs", i_module,
                        "rgb", i_RGBchannel, "state", &state_str);
                    if(check != JSON_OK)
                    {
                       state_str = NULL; 
                    }
                    // - Command (optional)
                    check = jh_getJFieldStringCopy("RGBWs", i_module,
                        "rgb", i_RGBchannel, "command", &command_str);
                    if(check != JSON_OK)
                    {
                       command_str = NULL; 
                    }
                    //---------------------------------------------------------
                    // Field Validation
                    //---------------------------------------------------------
                    // No channel is needed, state and command are optional
                    valid = true;                                        
                    //---------------------------------------------------------
                    // Set channel as configured and add to gateway
                    //---------------------------------------------------------
                    if(valid)
                    {                        
                        //-------------------------------------------------
                        // Add States to Gateway - RGB Channels
                        //-------------------------------------------------
                        for(i_channel = 0; i_channel < RGBW_COLOUR_W;
                                i_channel++)
                        {                            
                            // Add to the gateway                        
                            rgbw_addRGBWChannelToGateway(node, group, 
                                isRGBW, isRGB, i_channel + 1, state_str, 
                                NULL);
                            // Set as configured
                            configured[i_channel] = true;
                        }
                        //-------------------------------------------------
                        // Add States to Gateway - MASTER Channels
                        // - When it changes, the status has to be updated.
                        //-------------------------------------------------                        
                        i_channel = RGBW_MASTER;
                        // Add to the gateway                        
                        rgbw_addRGBWChannelToGateway(node, group,
                            isRGBW, isRGB, i_channel + 1, state_str, NULL);
                        // Set as configured
                        configured[i_channel] = true;
                        //-------------------------------------------------
                        // Add Command to Gateway
                        //-------------------------------------------------
                        rgbw_addRGBWChannelToGateway(node, group, 
                                isRGBW, isRGB, RGBW_RGB_COMMAND, NULL, 
                                command_str);
                    }
                    // Free
                    free(state_str);
                    state_str = NULL;
                    free(command_str);
                    command_str = NULL;                   
                }                
                // Check for unconfigured channels - they must be added to 
                // gateway as well in order to process status updates
                for(i_channel = 0; i_channel < RGBW_N_COLOURS; i_channel++)
                {
                    if(!configured[i_channel])
                    {
                        // Add to the gateway
                        rgbw_addRGBWChannelToGateway(node, group, isRGBW,
                                isRGB, i_channel + 1, NULL, NULL);
                    }
                }
            }
            // Free
            free(state_str);
            state_str = NULL;
            free(command_str);
            command_str = NULL;
        }
    }    
}

/**
 * Set a payload based on the data received, and add it to the MQTT Pub Buffer.       
 */
int hrgbw_setCAN2MQTTResponse(char *state_str, hapcanCANData *hd_received, 
        unsigned long long timestamp)
{
    int ret = HAPCAN_NO_RESPONSE;
    int check;
    void* payload = NULL;
    int payloadlen;
    // Set the payload
    check = rgbw_getRGBWPayload(state_str, hd_received, &payload, &payloadlen);
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
int hrgbw_setMQTT2CANResponse(hapcanCANData *hd_result, void *payload, 
        int payloadlen, unsigned long long timestamp)
{
    int ret = HAPCAN_RESPONSE_ERROR;
    int channel;    
    long val;
    char *str = NULL;    
    json_object *obj = NULL;
    bool valid = true;
    int temp;
    int colors[RGBW_N_COLOURS];
    int check;
    bool isRGB;
    bool isRGBW;
    int n_channels;
    //-------------------------------------------------------------------------
    // Init data
    //-------------------------------------------------------------------------
    // Get channel
    channel = hd_result->data[1];
    // Set isRGB and isRGBW
    isRGB = false;
    isRGBW = false;    
    if(channel == RGBW_RGBW_COMMAND)
    {
        isRGBW = true;
    }
    if(channel == RGBW_RGB_COMMAND)
    {
        isRGB = true;
    }    
    // Check NULL or size 0
    if((payload == NULL) || (payloadlen <= 0) || (channel <= 0) || 
            (channel > RGBW_RGBW_COMMAND))
    {
        valid = false;
    }
    else
    {
        // Copy payload to str
        str = malloc(payloadlen + 1);
        memcpy(str, payload, payloadlen);
        str[payloadlen] = 0;
        // Set the frame as direct control
        hd_result->frametype = HAPCAN_DIRECT_CONTROL_FRAME_TYPE;
        if(!isRGBW && !isRGB)
        {
            //------------------------------------------------------------------
            // Handle Single Channel Requests
            //------------------------------------------------------------------
            // COMMAND TOPIC ACCEPTED PAYLOADS:
            // 1. Command Strings: "ON", "OFF", "TOGGLE"
            // 2. String as number
            //------------------------------------------------------------------
            if( aux_compareStrings(str, "ON") )
            {
                valid = true;
                // SET the CHANNEL - INSTR1 from 0x14 to 0x18 = SET SOFTLY
                channel = channel + 0x14;
                hd_result->data[0] = channel - 1; // INSTR1.
                hd_result->data[1] = 0xFF; // INSTR2 = State.
                hd_result->data[4] = 0x00; // INSTR3 = Timer.
                hd_result->data[5] = 0xFF; // INSTR4 = 0xXX
                hd_result->data[6] = 0xFF; // INSTR5 = 0xXX   
                hd_result->data[7] = 0xFF; // INSTR6 = 0xXX
                ret = hapcan_addToCANWriteBuffer(hd_result, timestamp, true);
                if(ret != HAPCAN_CAN_RESPONSE_ERROR)                    
                {
                    // Set MASTER to 0xFF immediately;
                    hd_result->data[0] = 0x04; // INSTR1 (SET MASTER TO)
                    hd_result->data[1] = 0xFF; // INSTR2 = State (255)
                    // hd_result->data[4 to 7] already set                    
                    ret = hapcan_addToCANWriteBuffer(hd_result, timestamp, 
                            true);
                }               
            }
            else if( aux_compareStrings(str, "OFF") )
            {
                valid = true;
                // SET the CHANNEL - INSTR1 from 0x14 to 0x18 = SET SOFTLY
                channel = channel + 0x14;
                hd_result->data[0] = channel - 1; // INSTR1.
                hd_result->data[1] = 0x00; // INSTR2 = State.
                hd_result->data[4] = 0x00; // INSTR3 = Timer.
                hd_result->data[5] = 0xFF; // INSTR4 = 0xXX
                hd_result->data[6] = 0xFF; // INSTR5 = 0xXX   
                hd_result->data[7] = 0xFF; // INSTR6 = 0xXX
                ret = hapcan_addToCANWriteBuffer(hd_result, timestamp, true);                
            }
            else if( aux_compareStrings(str, "TOGGLE") )
            {
                valid = true;
                // TOGGLE the CHANNEL - INSTR1 from 0x05 to 0x09 = TOGGLE
                channel = channel + 0x05;
                hd_result->data[0] = channel - 1; // INSTR1.
                hd_result->data[1] = 0xFF; // INSTR2 = 0xXX.
                hd_result->data[4] = 0x00; // INSTR3 = Timer.
                hd_result->data[5] = 0xFF; // INSTR4 = 0xXX
                hd_result->data[6] = 0xFF; // INSTR5 = 0xXX   
                hd_result->data[7] = 0xFF; // INSTR6 = 0xXX
                ret = hapcan_addToCANWriteBuffer(hd_result, timestamp, true);
                if(ret != HAPCAN_CAN_RESPONSE_ERROR)                    
                {
                    // Set MASTER to 0xFF immediately;
                    hd_result->data[0] = 0x04; // INSTR1 (SET MASTER TO)
                    hd_result->data[1] = 0xFF; // INSTR2 = State (255)
                    // hd_result->data[4 to 7] already set                    
                    ret = hapcan_addToCANWriteBuffer(hd_result, timestamp, 
                            true);
                }                
            }
            else if(aux_parseValidateLong(str, &val, 0, 0, 255))
            {
                // Set the output to the specified level
                valid = true;
                // SET the CHANNEL - INSTR1 from 0x14 to 0x18 = SET SOFTLY
                channel = channel + 0x14;
                hd_result->data[0] = channel - 1; // INSTR1.
                hd_result->data[1] = (uint8_t)(val); // INSTR2 = State.
                hd_result->data[4] = 0x00; // INSTR3 = Timer.
                hd_result->data[5] = 0xFF; // INSTR4 = 0xXX
                hd_result->data[6] = 0xFF; // INSTR5 = 0xXX   
                hd_result->data[7] = 0xFF; // INSTR6 = 0xXX
                ret = hapcan_addToCANWriteBuffer(hd_result, timestamp, true);
                if(ret != HAPCAN_CAN_RESPONSE_ERROR)                    
                {
                    // Set MASTER to 0xFF immediately;
                    hd_result->data[0] = 0x04; // INSTR1 (SET MASTER TO)
                    hd_result->data[1] = 0xFF; // INSTR2 = State (255)
                    // hd_result->data[4 to 7] already set                    
                    ret = hapcan_addToCANWriteBuffer(hd_result, timestamp, 
                            true);
                }
            }
        }
        else if(isRGBW || isRGB)
        {
            //------------------------------------------------------------------
            // Handle RGB and RGBW Requests
            //------------------------------------------------------------------
            // COMMAND TOPIC ACCEPTED PAYLOADS:
            // 1. Command Strings: "ON", "OFF", "TOGGLE"
            // 2. String as number (comma-separated values)
            // 3. JSON: {"INSTR1":Integer, "INSTR2":Integer, "INSTR3":Integer, 
            //          "INSTR4":Integer, "INSTR5":Integer, "INSTR6":Integer}
            //------------------------------------------------------------------
            if( aux_compareStrings(str, "ON") )
            {
                valid = true;
                if(isRGBW)
                {
                    // Set RGBW channels to 127 softly and immediately.
                    hd_result->data[0] = 0x2E; // INSTR1 (SET SOFTLY RGBW TO)
                    hd_result->data[1] = 0x7F; // INSTR2 = State 1
                    hd_result->data[4] = 0x7F; // INSTR3 = State 2
                    hd_result->data[5] = 0x7F; // INSTR4 = State 3
                    hd_result->data[6] = 0x7F; // INSTR5 = State 4  
                    hd_result->data[7] = 0x00; // INSTR6 = Timer.
                }
                else if(isRGB)
                {
                    // Set RGB channels to 127 softly and immediately.
                    hd_result->data[0] = 0x29; // INSTR1 (SET SOFTLY RGB TO)
                    hd_result->data[1] = 0x7F; // INSTR2 = State 1
                    hd_result->data[4] = 0x7F; // INSTR3 = State 2
                    hd_result->data[5] = 0x7F; // INSTR4 = State 3
                    hd_result->data[6] = 0x00; // INSTR5 = Timer
                    hd_result->data[7] = 0xFF; // INSTR6 = 0xXX
                }
                ret = hapcan_addToCANWriteBuffer(hd_result, timestamp, true);
                if(ret != HAPCAN_CAN_RESPONSE_ERROR)                    
                {
                    // Set MASTER to 0xFF immediately;
                    hd_result->data[0] = 0x04; // INSTR1 (SET MASTER TO)
                    hd_result->data[1] = 0xFF; // INSTR2 = State (255)
                    hd_result->data[4] = 0x00; // INSTR3 = Timer
                    hd_result->data[5] = 0xFF; // INSTR4 = 0xXX
                    hd_result->data[6] = 0xFF; // INSTR5 = 0xXX
                    hd_result->data[7] = 0xFF; // INSTR6 = 0xXX                    
                    ret = hapcan_addToCANWriteBuffer(hd_result, timestamp, 
                            true);
                }
            }
            else if( aux_compareStrings(str, "OFF") )
            {
                valid = true;
                if(isRGBW)
                {
                    // Set RGBW channels to 0 softly and immediately.
                    hd_result->data[0] = 0x2E; // INSTR1 (SET SOFTLY RGBW TO)
                    hd_result->data[1] = 0x00; // INSTR2 = State 1
                    hd_result->data[4] = 0x00; // INSTR3 = State 2
                    hd_result->data[5] = 0x00; // INSTR4 = State 3
                    hd_result->data[6] = 0x00; // INSTR5 = State 4  
                    hd_result->data[7] = 0x00; // INSTR6 = Timer.
                }
                else if(isRGB)
                {
                    // Set RGB channels to 0 softly and immediately.
                    hd_result->data[0] = 0x29; // INSTR1 (SET SOFTLY RGB TO)
                    hd_result->data[1] = 0x00; // INSTR2 = State 1
                    hd_result->data[4] = 0x00; // INSTR3 = State 2
                    hd_result->data[5] = 0x00; // INSTR4 = State 3
                    hd_result->data[6] = 0x00; // INSTR5 = Timer
                    hd_result->data[7] = 0xFF; // INSTR6 = 0xXX
                }
                ret = hapcan_addToCANWriteBuffer(hd_result, timestamp, true);
            }
            else if( aux_compareStrings(str, "TOGGLE") )
            {
                valid = true;
                // Check how many channels to toggle
                if(isRGBW)
                {
                    n_channels = RGBW_MASTER;
                }
                else if(isRGB)
                {
                    n_channels = RGBW_COLOUR_W;
                }
                // Set the instructions (INSTR1 from 0x05 to 0x09 = TOGGLE)
                hd_result->data[1] = 0xFF; // INSTR2 = 0xXX.
                hd_result->data[4] = 0x00; // INSTR3 = Timer.
                hd_result->data[5] = 0xFF; // INSTR4 = 0xXX
                hd_result->data[6] = 0xFF; // INSTR5 = 0xXX   
                hd_result->data[7] = 0xFF; // INSTR6 = 0xXX
                for(channel = 0; channel < n_channels; channel++)
                {
                    hd_result->data[0] = 0x05 + channel; // INSTR1 (Channel)
                    ret = hapcan_addToCANWriteBuffer(hd_result, timestamp, 
                            true);
                    if(ret == HAPCAN_CAN_RESPONSE_ERROR)
                    {
                        // Leave loop
                        break;
                    }
                }                
                if(ret != HAPCAN_CAN_RESPONSE_ERROR)                    
                {
                    // Set MASTER to 0xFF immediately;
                    hd_result->data[0] = 0x04; // INSTR1 (SET MASTER TO)
                    hd_result->data[1] = 0xFF; // INSTR2 = State (255)
                    hd_result->data[4] = 0x00; // INSTR3 = Timer
                    hd_result->data[5] = 0xFF; // INSTR4 = 0xXX
                    hd_result->data[6] = 0xFF; // INSTR5 = 0xXX
                    hd_result->data[7] = 0xFF; // INSTR6 = 0xXX                    
                    ret = hapcan_addToCANWriteBuffer(hd_result, timestamp, 
                            true);
                }

            }
            else if( (isRGBW) && aux_parseValidateIntArray(colors, str,",", 
                    RGBW_N_COLOURS - 1, 0, 0, 255))
            {
                // Set the output to the specified level                                
                valid = true;
                // Set all RGBW softly to the specified level
                hd_result->data[0] = 0x2E; // INSTR1 (SET SOFTLY RGBW TO)
                hd_result->data[1] = colors[RGBW_COLOUR_R]; // INSTR2=State1
                hd_result->data[4] = colors[RGBW_COLOUR_G]; // INSTR3=State2
                hd_result->data[5] = colors[RGBW_COLOUR_B]; // INSTR4=State3
                hd_result->data[6] = colors[RGBW_COLOUR_W]; // INSTR5=State4
                hd_result->data[7] = 0x00; // INSTR6 = TIMER
                ret = hapcan_addToCANWriteBuffer(hd_result, timestamp, true);
                if(ret != HAPCAN_CAN_RESPONSE_ERROR)                    
                {
                    // Set MASTER to 0xFF immediately;
                    hd_result->data[0] = 0x04; // INSTR1 (SET MASTER TO)
                    hd_result->data[1] = 0xFF; // INSTR2 = State (255)
                    hd_result->data[4] = 0x00; // INSTR3 = Timer
                    hd_result->data[5] = 0xFF; // INSTR4 = 0xXX
                    hd_result->data[6] = 0xFF; // INSTR5 = 0xXX
                    hd_result->data[7] = 0xFF; // INSTR6 = 0xXX                    
                    ret = hapcan_addToCANWriteBuffer(hd_result, timestamp, 
                            true);
                }
            }
            else if( (isRGB) && aux_parseValidateIntArray(colors, str,",", 
                    RGBW_MASTER - 1, 0, 0, 255))
            {
                // Set the output to the specified level                                
                valid = true;
                // Set all RGBW softly to the specified level
                hd_result->data[0] = 0x29; // INSTR1 (SET SOFTLY RGB TO)
                hd_result->data[1] = colors[RGBW_COLOUR_R]; // INSTR2=State1
                hd_result->data[4] = colors[RGBW_COLOUR_G]; // INSTR3=State2
                hd_result->data[5] = colors[RGBW_COLOUR_B]; // INSTR4=State3
                hd_result->data[6] = 0x00; // INSTR5 = Timer
                hd_result->data[7] = 0xFF; // INSTR6 = 0xXX
                ret = hapcan_addToCANWriteBuffer(hd_result, timestamp, true);
                if(ret != HAPCAN_CAN_RESPONSE_ERROR)                    
                {
                    // Set MASTER to 0xFF immediately;
                    hd_result->data[0] = 0x04; // INSTR1 (SET MASTER TO)
                    hd_result->data[1] = 0xFF; // INSTR2 = State (255)
                    hd_result->data[4] = 0x00; // INSTR3 = Timer
                    hd_result->data[5] = 0xFF; // INSTR4 = 0xXX
                    hd_result->data[6] = 0xFF; // INSTR5 = 0xXX
                    hd_result->data[7] = 0xFF; // INSTR6 = 0xXX                    
                    ret = hapcan_addToCANWriteBuffer(hd_result, timestamp, 
                            true);
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
                        ret = hapcan_addToCANWriteBuffer(hd_result, timestamp, 
                            true);
                    }                    
                } 
            }
        }
    }
    // Set return and Clean
    if( !valid )
    {
        ret = HAPCAN_RESPONSE_ERROR;
    }    
    // Free
    jh_freeObject(obj);
    free(str);
    // Leave
    return ret;
}

/**
 * To be called periodically to check and update the RGBW modules status.         
 */
int hrgbw_periodic(void)
{
    int ret;    
    // Check and send CAN
    ret = rgbw_checkAndSendCAN();
    return ret;
}
