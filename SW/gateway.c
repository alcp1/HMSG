//----------------------------------------------------------------------------//
//                               OBJECT HISTORY                               //
//----------------------------------------------------------------------------//
//  REVISION |    DATE     |                               |      AUTHOR      //
//----------------------------------------------------------------------------//
//  1.00     | 28/Nov/2021 |                               | ALCP             //
// - Creation of file                                                         //
//----------------------------------------------------------------------------//

//----------------------------------------------------------------------------//
// Includes
//----------------------------------------------------------------------------//
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>
#include <errno.h>
#include <pthread.h>
#include "gateway.h"
#include "hapcan.h"
#include "auxiliary.h"
#include "debug.h"

//----------------------------------------------------------------------------//
// INTERNAL DEFINITIONS
//----------------------------------------------------------------------------//
 
 
//----------------------------------------------------------------------------//
// INTERNAL TYPES
//----------------------------------------------------------------------------//
// List element (single structure for MQTT to CAN and CAN to MQTT)
typedef struct gatewayList 
{
    hapcanCANData hd_mask;      // CAN2MQTT (INPUT)
    hapcanCANData hd_check;     // CAN2MQTT (INPUT)
    char* state_topic;          // CAN2MQTT (OUTPUT)
    char* command_topic;        // MQTT2CAN (INPUT)
    hapcanCANData hd_result;    // MQTT2CAN (OUTPUT)
    struct gatewayList *next;
} gatewayList;

//----------------------------------------------------------------------------//
// INTERNAL GLOBAL VARIABLES
//----------------------------------------------------------------------------//
static pthread_mutex_t g_CAN2MQTT_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_MQTT2CAN_mutex = PTHREAD_MUTEX_INITIALIZER;
static gatewayList* head[NUMBER_OF_GATEWAY_LISTS] = {NULL, NULL};

//----------------------------------------------------------------------------//
// INTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
static void clearElementData(gatewayList* element);
static void freeElementData(gatewayList* element);
static void gateway_addToList(int list, gatewayList* element);
static gatewayList* gateway_getFromOffset(int list, int offset);    // NULL means error
static int gateway_deleteList(int list);       // EXIT_SUCCESS / EXIT_FAILURE
#ifdef DEBUG_GATEWAY_PRINT
static void gateway_printElement(gatewayList* current);
#endif

// Clear all fields from gatewayList //
static void clearElementData(gatewayList* element)
{
    aux_clearHAPCANFrame(&(element->hd_mask));
    aux_clearHAPCANFrame(&(element->hd_check));
    element->state_topic = NULL;
    element->command_topic = NULL;
    element->next = NULL;
}

// Free allocated fields from gatewayList //
static void freeElementData(gatewayList* element)
{
    // Topic
    if(element->state_topic != NULL)
    {
        free(element->state_topic);
        element->state_topic = NULL;
    }
    // Payload
    if(element->command_topic != NULL)
    {
        free(element->command_topic);
        element->command_topic = NULL;
    }
}

// Add element to a given list
static void gateway_addToList(int list, gatewayList* element)
{
    gatewayList *link;
            
    // Check list index parameter
    if((list < 0) || (list >= NUMBER_OF_GATEWAY_LISTS))
    {
        #ifdef DEBUG_GATEWAY_ERRORS
        debug_print("gateway_addToList ERROR: list ERROR!\n");
        #endif
        return;
    }    
    // Create a new element to the list
    link = (gatewayList*)malloc(sizeof(*link));	
    // Copy structure data ("shallow" copy)
    *link = *element;   
    // Create and copy field that are pointers
    // - STATE TOPIC
    if(element->state_topic != NULL)
    {
        link->state_topic = strdup(element->state_topic);
    }
    else
    {
        link->state_topic = NULL;
    }
    // - COMMAND TOPIC
    if(element->command_topic != NULL)
    {
        link->command_topic = strdup(element->command_topic);
    }
    else
    {
        link->command_topic = NULL;
    }	
    // Set next in list to previous header (previous first node)
    link->next = head[list];	
    // Point header (first node) to current element (new first node)
    head[list] = link;
}

// get element from header (after offset positions)
static gatewayList* gateway_getFromOffset(int list, int offset)
{
    int li_length;
    gatewayList* current = NULL;
    // Check list index parameter
    if((list < 0) || (list >= NUMBER_OF_GATEWAY_LISTS))
    {
        return NULL;
    }
    // Check offset parameter
    if( offset < 0 )
    {
        return NULL;
    }
    // Check all
    li_length = 0;
    for(current = head[list]; current != NULL; current = current->next) 
    {
        if(li_length >= offset)
        {
            break;
        }
        li_length++;        
    }
    // Return
    return current;
}

// Delete list
static int gateway_deleteList(int list)
{
    gatewayList* current;
    gatewayList* next;
    
    // Check list index parameter
    if((list < 0) || (list >= NUMBER_OF_GATEWAY_LISTS))
    {
        return EXIT_FAILURE;
    }
    // Check all
    current = head[list];
    while(current != NULL) 
    {
        //*******************************//
        // Get next structure address    //
        //*******************************//
        next = current->next;
        //*******************************//
        // Free fields that are pointers //
        //*******************************//
        freeElementData(current);
        //*******************************//
        // Free structure itself         //
        //*******************************//
        free(current);
        //*******************************//
        // Get new current address       //
        //*******************************//
        current = next;
        head[list] = current;
    }    
    // return
    return EXIT_SUCCESS;
}

// Print all fields from a given element
#ifdef DEBUG_GATEWAY_PRINT
static void gateway_printElement(gatewayList* current)
{
    debug_print("gateway_printElement\n");
    debug_print("CAN2MQTT fields:\n");
    debug_printHAPCAN("    - MASK (IN):\n", 
            &(current->hd_mask));
    debug_printHAPCAN("    - CHECK (IN):\n", 
            &(current->hd_check));
    debug_print("    - STATE TOPIC (OUT) = %s\n", 
            current->state_topic);
    debug_print("MQTT2CAN fields:\n");
    debug_print("    - COMMAND TOPIC (IN) = %s\n", 
            current->command_topic);
    debug_printHAPCAN("    - HAPCAN Frame (OUT):\n", 
            &(current->hd_result));
    debug_print("    *NEXT = %d\n----\n", (int)current->next);
}
#endif

//----------------------------------------------------------------------------//
// EXTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
// Init
void gateway_init(void)
{
    int list;
    int check;
    for(list = GATEWAY_MQTT2CAN_LIST; list < NUMBER_OF_GATEWAY_LISTS; list++)
    {        
        //---------------------------------------------
        // Delete list - PROTECTED
        //---------------------------------------------
        // LOCK GATEWAY: CAN2MQTT and MQTT2CAN data
        pthread_mutex_lock(&g_CAN2MQTT_mutex);
        pthread_mutex_lock(&g_MQTT2CAN_mutex);
        // Delete
        check = gateway_deleteList(list);
        // UNLOCK GATEWAY: CAN2MQTT and MQTT2CAN data
        pthread_mutex_unlock(&g_CAN2MQTT_mutex);
        pthread_mutex_unlock(&g_MQTT2CAN_mutex);
        if( check == EXIT_FAILURE )
        {
            #ifdef DEBUG_GATEWAY_ERRORS
            debug_print("gateway_init error - List = %d\n", list);    
            #endif
        }
    }
}

// Add elements to the List
int gateway_AddElementToList(int list, hapcanCANData *phd_mask, 
        hapcanCANData *phd_check, char *state_topic, char* command_topic,         
        hapcanCANData *phd_result)
{
    int ret = 0;
    gatewayList element;
    clearElementData(&element);
    if(phd_mask != NULL)
    {
        element.hd_mask = *phd_mask;
    }
    if(phd_check != NULL)
    {
        element.hd_check = *phd_check;
    }
    if(state_topic != NULL)
    {
        element.state_topic = strdup(state_topic);
    }
    if(command_topic != NULL)
    {
        element.command_topic = strdup(command_topic);
    }
    if(phd_result != NULL)
    {
        element.hd_result = *phd_result;
    }
    // Check list index parameter
    if((list < 0) || (list >= NUMBER_OF_GATEWAY_LISTS))
    {
        ret = EXIT_FAILURE;
    }
    else
    {
        ret = EXIT_SUCCESS;
        //---------------------------------------------
        // Add to list - PROTECTED
        //---------------------------------------------
        // LOCK GATEWAY: CAN2MQTT and MQTT2CAN data
        pthread_mutex_lock(&g_CAN2MQTT_mutex);
        pthread_mutex_lock(&g_MQTT2CAN_mutex);
        // Add
        gateway_addToList(list, &element);
        // UNLOCK GATEWAY: CAN2MQTT and MQTT2CAN data
        pthread_mutex_unlock(&g_CAN2MQTT_mutex);
        pthread_mutex_unlock(&g_MQTT2CAN_mutex);
    }
    // Free data and then return
    freeElementData(&element);
    return ret;
}

 /* Search if CAN to MQTT list has a match with a given HAPCAN Frame.
 *  returns -1 if not matched or fail */
 int gateway_searchMQTTFromCAN(hapcanCANData *phd_received, int offset)
 {
    const int list = GATEWAY_CAN2MQTT_LIST;
    gatewayList* current;    
    int position;
    bool match;    
    // LOCK GATEWAY: CAN2MQTT data
    pthread_mutex_lock(&g_CAN2MQTT_mutex);
    // Get element from offset position
    current = gateway_getFromOffset(list, offset);
    #ifdef DEBUG_GATEWAY_SEARCH
    debug_printHAPCAN("gateway_searchMQTTFromCAN - CAN Frame to be matched:\n", 
            phd_received);    
    #endif     
    // Check remaining from offset
    position = offset;
    match = false;
    while(current != NULL) 
    {
        //*******************************//
        // Check data                    //
        //*******************************//    
        match = aux_checkCAN2MQTTMatch(phd_received, &(current->hd_mask), 
                &(current->hd_check));
        if(match)
        {
            #ifdef DEBUG_GATEWAY_SEARCH
            debug_print("gateway_searchMQTTFromCAN - Frame Matched \n");
            #endif
            // Found - stop searching
            break;
        }                
        //*******************************//
        // Get new current address       //
        //*******************************//
        position++;
        current = current->next;        
    }
    if(!match)
    {
        position = -1;
    }
    // UNLOCK GATEWAY: CAN2MQTT data
    pthread_mutex_unlock(&g_CAN2MQTT_mutex);
    // return
    return position;    
 }

// Returns the MQTT data from a given position. EXIT_SUCCESS / EXIT_FAILURE
int gateway_getMQTTFromCAN(int offset, char** topic)
{
    int ret = EXIT_SUCCESS;
    const int list = GATEWAY_CAN2MQTT_LIST;
    gatewayList* current;    
    // LOCK GATEWAY: CAN2MQTT data
    pthread_mutex_lock(&g_CAN2MQTT_mutex);
    // Get element from offset position
    current = gateway_getFromOffset(list, offset);    
    // Check
    if(current == NULL)
    {
        #ifdef DEBUG_GATEWAY_ERRORS
        debug_print("gateway_getMQTTFromCAN ERROR: Nothing to get!\n");
        #endif
        ret = EXIT_FAILURE;
    }
    if(ret == EXIT_SUCCESS)
    {
        // Copy data
        // - TOPIC
        if(current->state_topic != NULL)
        {
            *topic = strdup(current->state_topic);
        }
        else
        {
            *topic = NULL;
        }
    }
    // UNLOCK GATEWAY: CAN2MQTT data
    pthread_mutex_unlock(&g_CAN2MQTT_mutex);
    // RETURN
    return ret;
}

// Search if MQTT to CAN list has matched a given MQTT message. -1 if fail 
 int gateway_searchCANFromMQTT(char* const topic, int offset)
 {
    const int list = GATEWAY_MQTT2CAN_LIST;
    gatewayList* current = NULL;    
    int position;
    bool match;
    // LOCK GATEWAY: MQTT2CAN data
    pthread_mutex_lock(&g_MQTT2CAN_mutex);
    // Get element from offset position
    current = gateway_getFromOffset(list, offset);
    // Debug
    #ifdef DEBUG_GATEWAY_SEARCH
    debug_print("gateway_searchCANFromMQTT - MQTT Frame to Match - TOPIC: %s\n", 
            topic);
    #endif    
    // Check remaining from offset
    position = offset;
    match = false;
    while(current != NULL) 
    {
        //*******************************//
        // Check data                    //
        //*******************************//    
        if( aux_compareStrings(topic, current->command_topic) && 
                (topic != NULL) )
        {
            #ifdef DEBUG_GATEWAY_SEARCH
            debug_print("gateway_searchMQTTFromCAN - Topic Matched \n");
            #endif 
            match = true;
            // Found - stop searching
            break;
        }
        //*******************************//
        // Get new current address       //
        //*******************************//
        position++;
        current = current->next;        
    }
    if(!match)
    {
        position = -1;
    }
    // UNLOCK GATEWAY: MQTT2CAN data
    pthread_mutex_unlock(&g_MQTT2CAN_mutex);
    // Return
    return position;    
 }

// Returns the CAN data from a given position. EXIT_SUCCESS / EXIT_FAILURE
int gateway_getCANFromMQTT(int offset, hapcanCANData *p_hd)
{
    int ret = EXIT_SUCCESS;
    const int list = GATEWAY_MQTT2CAN_LIST;
    gatewayList* current;
    // LOCK GATEWAY: MQTT2CAN data
    pthread_mutex_lock(&g_MQTT2CAN_mutex);
    // Get element from offset position
    current = gateway_getFromOffset(list, offset);    
    // Check
    if(current == NULL)
    {
        #ifdef DEBUG_GATEWAY_ERRORS
        debug_print("gateway_getCANFromMQTT ERROR: Nothing to get!\n");
        #endif
        ret = EXIT_FAILURE;
    }    
    if(ret == EXIT_SUCCESS)
    {
        // Copy data
        // - HAPCAN 
        *p_hd = current->hd_result;
    }      
    // UNLOCK GATEWAY: MQTT2CAN data
    pthread_mutex_unlock(&g_MQTT2CAN_mutex);
    // RETURN
    return ret;
}

// Used for debug only:
// Print all fields from each element of a list
void gateway_printList(int list)
{
    #ifdef DEBUG_GATEWAY_LISTS
    gatewayList* current;    
    // Print List number
    debug_print("gateway_printList: List = %d\n", list);    
    // Check list index parameter
    if((list < 0) || (list >= NUMBER_OF_GATEWAY_LISTS))
    {
        return;
    }
    for(current = head[list]; current != NULL; current = current->next) 
    {
        gateway_printElement(current);
    }
    #endif
}