//----------------------------------------------------------------------------//
//                               OBJECT HISTORY                               //
//----------------------------------------------------------------------------//
//  REVISION |    DATE     |                               |      AUTHOR      //
//----------------------------------------------------------------------------//
//  1.00     | 10/Dec/2021 |                               | ALCP             //
// - First version                                                            //
//----------------------------------------------------------------------------//
//  1.01     | 15/Jul/2023 |                               | ALCP             //
// - Add new frame types (HTIM and HRGBW)                                     //
//----------------------------------------------------------------------------//
//  1.02     | 19/Aug/2023 |                               | ALCP             //
// - Perform initial status update for all configured modules on init         //
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
#include "auxiliary.h"
#include "debug.h"
#include "hapcan.h"
#include "hapcanconfig.h"
#include "hapcansystem.h"
#include "jsonhandler.h"

//----------------------------------------------------------------------------//
// INTERNAL DEFINITIONS
//----------------------------------------------------------------------------//
typedef enum
{
    UPDATE_TYPE_STATIC = 0,
    UPDATE_TYPE_DYNAMIC,
    UPDATE_TYPE_STATUS,
    UPDATE_TYPE_ALL,
} update_t;
// Static Message Frames to be received for fully updating fields
enum
{
    HAPCAN_104_FRAME_UPDATE = 0,
    HAPCAN_106_FRAME_UPDATE,
    HAPCAN_10E_FRAME_P1_UPDATE,
    HAPCAN_10E_FRAME_P2_UPDATE,
    HAPCAN_111_FRAME_UPDATE,
    HAPCAN_STATIC_N_UPDATES
};
const int c_frametype_static[HAPCAN_STATIC_N_UPDATES] = 
{
  HAPCAN_HW_TYPE_REQUEST_NODE_FRAME_TYPE, 
  HAPCAN_FW_TYPE_REQUEST_NODE_FRAME_TYPE,  
  HAPCAN_DESCRIPTION_REQUEST_NODE_FRAME_TYPE,
  HAPCAN_DESCRIPTION_REQUEST_NODE_FRAME_TYPE,
  HAPCAN_DEV_ID_REQUEST_NODE_FRAME_TYPE
};

// Dynamic Message Frames to be received for fully updating fields
enum
{
    HAPCAN_10C_FRAME_UPDATE = 0,
    HAPCAN_113_FRAME_UPDATE,
    HAPCAN_115_FRAME_P1_UPDATE,
    HAPCAN_115_FRAME_P2_UPDATE,
    HAPCAN_DYNAMIC_N_UPDATES
};
const int c_frametype_dynamic[HAPCAN_DYNAMIC_N_UPDATES] = 
{
  HAPCAN_SUPPLY_REQUEST_NODE_FRAME_TYPE, 
  HAPCAN_UPTIME_REQUEST_NODE_FRAME_TYPE,  
  HAPCAN_HEALTH_CHECK_REQUEST_NODE_FRAME_TYPE,
  HAPCAN_HEALTH_CHECK_REQUEST_NODE_FRAME_TYPE
};

//----------------------------------------------------------------------------//
// INTERNAL TYPES
//----------------------------------------------------------------------------//
// List element
#define NODE_LIST_N_FIELDS 26
typedef struct nodeList_t 
{
    //------------------------
    // General Identification
    //------------------------
    uint8_t node;
    uint8_t group;
    //------------------------
    // STATIC Fields
    //------------------------
    //  - Frametypes 0x103, 0x104, 0x105, 0x106
    uint16_t hard;          
    uint8_t hver;
    //  - Frametypes 0x103, 0x104
    uint32_t id;            
    //  - Frametypes 0x105, 0x106
    uint8_t atype;          
    uint8_t avers;          
    uint8_t fvers;          
    uint16_t bver;          
    //  - Frametypes 0x10D, 0x10E
    char description[16];  
    //  - Frametypes 0x10F, 0x111
    uint16_t devId;         
    //------------------------
    // DYNAMIC Fields
    //------------------------
    //  - Frametypes 0x10B, 0x10C
    double volbus;          
    double volcpu;
    //  - Frametypes 0x112, 0x113
    uint32_t uptime;  
    //  - Frametypes 0x114, 0x115
    uint8_t rxcnt;          
    uint8_t txcnt;          
    uint8_t rxcntmx;        
    uint8_t txcntmx;        
    uint8_t canintcnt;      
    uint8_t rxerrcnt;       
    uint8_t txerrcnt;       
    uint8_t rxcntmxe;       
    uint8_t txcntmxe;       
    uint8_t canintcnte;     
    uint8_t rxerrcnte;      
    uint8_t txerrcnte;
    //------------------------
    // Update Control - CAN Received
    //------------------------
    bool isStaticUpdated[HAPCAN_STATIC_N_UPDATES];
    bool isDynamicUpdated[HAPCAN_DYNAMIC_N_UPDATES];
    bool isRequestHandled;
    //------------------------
    // Sent Control - MQTT sent
    //------------------------
    bool isStaticSent;
    bool isDynamicSent;
    bool isStatusSent;
    //------------------------
    // Linked List Control
    //------------------------
    struct nodeList_t *next;
} nodeList_t;

// Control the status update
typedef struct 
{
    uint8_t initialNode;
    uint8_t initialGroup;
    uint8_t lastNode;
    uint8_t lastGroup;
    bool isFinished;
}statusUpdate_t;

//----------------------------------------------------------------------------//
// INTERNAL GLOBAL VARIABLES
//----------------------------------------------------------------------------//
static pthread_mutex_t g_hsystem_list_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_hsystem_control_mutex = PTHREAD_MUTEX_INITIALIZER;
static nodeList_t* g_hsystem_head = NULL;
static statusUpdate_t g_status_control;
static int g_lastSentNode;
static int g_lastSentGroup;
static int g_lastSentCount;
static int g_lastSentFrame;

//----------------------------------------------------------------------------//
// INTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
// Linked List functions
static void hsystem_clearElementData(nodeList_t* element);
static void hsystem_freeElementData(nodeList_t* element);
static void hsystem_addToList(nodeList_t* element);
static nodeList_t* hsystem_getFromOffset(int offset);    // NULL means error
static void hsystem_deleteList(void);
static void hsystem_addElementToList(int node, int group);
// Other internal functions
static void hsystem_setUpdateNodes(int node, int group, int *ig, int *fg, 
        int *in, int *fn);
static void hsystem_setListFlags(nodeList_t* element, update_t type, bool req);
static void hsystem_setControlFlags(int ig, int fg, int in, int fn, 
        update_t type, bool req);
static bool hsystem_getGroupNodeFromTopic(char *received_topic, 
        char *configured_topic, int *node, int *group);
static void hsystem_addModulesToList(void);
static void hsystem_setUpdateFlags(update_t type, int node, int group);
static int hsystem_updateData(hapcanCANData *hd_received, nodeList_t* element);
static int hsystem_checkUpdateData(hapcanCANData *hd_received);
static void hsystem_getMQTTPayload(nodeList_t* element, char **topic, 
        void **payload, int *payloadlen);
static int hsystem_checkAndSendCAN(void);
static int hsystem_checkAndSendMQTT(void);
#ifdef DEBUG_HAPCAN_SYSTEM_PRINT 
static void hsystem_printElement(nodeList_t* current);
#endif

// Clear all fields from nodeList_t //
static void hsystem_clearElementData(nodeList_t* element)
{
    // No pointers to be freed
    memset(element, 0, sizeof(*element));
}

// Free allocated fields from nodeList_t //
static void hsystem_freeElementData(nodeList_t* element)
{
    // No pointers to be freed - do nothing
}

// Add element to the list
static void hsystem_addToList(nodeList_t* element)
{
    nodeList_t *link;        
    // Create a new element to the list
    link = (nodeList_t*)malloc(sizeof(*link));	
    // Copy structure data ("shallow" copy)
    *link = *element;   
    // No pointers - no need to copy memory    	
    // Set next in list to previous g_hsystem_header (previous first node)
    link->next = g_hsystem_head;	
    // Point g_hsystem_header (first node) to current element (new first node)
    g_hsystem_head = link;
}

// get element from g_hsystem_header (after offset positions)
static nodeList_t* hsystem_getFromOffset(int offset)
{
    int len;
    nodeList_t* current = NULL;
    // Check offset parameter
    if( offset < 0 )
    {
        return NULL;
    }
    // Check all
    len = 0;
    for(current = g_hsystem_head; current != NULL; current = current->next) 
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
static void hsystem_deleteList(void)
{
    nodeList_t* current;
    nodeList_t* next;
    // Check all
    current = g_hsystem_head;
    while(current != NULL) 
    {
        //*******************************//
        // Get next structure address    //
        //*******************************//
        next = current->next;
        //*******************************//
        // Free fields that are pointers //
        //*******************************//
        hsystem_freeElementData(current);
        //*******************************//
        // Free structure itself         //
        //*******************************//
        free(current);
        //*******************************//
        // Get new current address       //
        //*******************************//
        current = next;
        g_hsystem_head = current;
    }    
}

// Add elements to the List
static void hsystem_addElementToList(int node, int group)
{
    nodeList_t element;
    hsystem_clearElementData(&element);
    element.group = group;
    element.node = node;
    // Set flags to clear update request
    hsystem_setListFlags(&element, UPDATE_TYPE_ALL, true);
    // Add
    hsystem_addToList(&element);
    // Free temp data and then return
    hsystem_freeElementData(&element);
}

// Set the initial and Final Group / nodes for a given input node and group
static void hsystem_setUpdateNodes(int node, int group, int *ig, int *fg, 
        int *in, int *fn)
{
    if(group == 0)
    {
        *ig = 1;
        *fg = 255;
    }
    else
    {
        *ig = group;
        *fg = group;
    }
    if(node == 0)
    {
        *in = 1;
        *fn = 255;
    }
    else
    {
        *in = node;
        *fn = node;
    }
}

// Set Flags for a given module
static void hsystem_setListFlags(nodeList_t* element, update_t type, bool req)
{
    int i;    
    if((type == UPDATE_TYPE_ALL) || (type == UPDATE_TYPE_STATIC))
    {
        for(i = 0; i < HAPCAN_STATIC_N_UPDATES; i++)
        {
            element->isStaticUpdated[i] = false;            
        }
        element->isStaticSent = req;            
    }
    if((type == UPDATE_TYPE_ALL) || (type == UPDATE_TYPE_DYNAMIC))
    {
        for(i = 0; i < HAPCAN_DYNAMIC_N_UPDATES; i++)
        {
            element->isDynamicUpdated[i] = false;                        
        }
        element->isDynamicSent = req;        
    }
    if((type == UPDATE_TYPE_ALL) || (type == UPDATE_TYPE_STATIC) || 
            (type == UPDATE_TYPE_DYNAMIC))
    {
        element->isRequestHandled = req;
    }    
    if((type == UPDATE_TYPE_ALL) || (type == UPDATE_TYPE_STATUS))
    {
        element->isStatusSent = req;        
    }
}

// Set the flags for controlling status messages
static void hsystem_setControlFlags(int ig, int fg, int in, int fn, 
        update_t type, bool req)
{
    if((type == UPDATE_TYPE_ALL) || (type == UPDATE_TYPE_STATUS))
    {
        g_status_control.isFinished = req;
        g_status_control.initialGroup = ig;
        g_status_control.lastGroup = fg;
        g_status_control.initialNode = in;
        g_status_control.lastNode = fn; 
    }
}

// Get the node and group from a received MQTT topic, after the configured topic
// MQTT Format is: 
//    - configured_topic/Group/Node
//    - configured_topic/Group
//    - configured_topic
static bool hsystem_getGroupNodeFromTopic(char *received_topic, 
        char *configured_topic, int *node, int *group)
{
    bool ret = false;
    char *token;
    char *str = NULL;
    char *pString = NULL;
    unsigned int len;
    long val;
    *node = 0;
    *group = 0;
    if(aux_compareStrings(received_topic, configured_topic))
    {
        *node = 0;
        *group = 0;
        ret = true;
    }
    if(!ret)
    {
        len = strlen(configured_topic);
        if(received_topic[len] == '/')
        {
            // Allocate for string (difference between received and configured
            if(strlen(received_topic) > strlen(configured_topic))
            {
                len = strlen(received_topic) - strlen(configured_topic);
            }
            if(len < 1)
            {
                // The first character after configured topic is / but there is
                // nothing after that
                ret = false;
            }
            else
            {
                str = malloc(len + 1);
                // Store the pointer to free it before manipulations with strsep
                pString = str; 
                // Copy to string the difference / Skip first "/"
                len = strlen(configured_topic);                
                strcpy(str, &(received_topic[len + 1]));
                // Try to get a first token
                token = strsep(&str,"/");
                // With delimeter found / not found - try to get group
                if( aux_parseValidateLong(token, &val, 0, 0, 255) )
                {
                    *group = (int)val;
                    *node = 0;
                    ret = true;
                }
                // With delimeter found, and group found try to get node
                if((str != NULL) && ret)
                {        
                    // Try to get node (no more delimenters - just number)
                    if( aux_parseValidateLong(str, &val, 0, 0, 255) )
                    {
                        *node = (int)val;
                    }
                    else
                    {
                        *group = 0;
                        *node = 0;
                        ret = false;
                    }
                }                
            }
        }
        else
        {
            // received is not equal to configured, and is received is not
            // followed by the first '/' delimeter
            ret = false;
        }
    }
    // Free
    free(pString);
    // Return
    return ret;
}

// Add a node / group to list
static void hsystem_addModulesToList(void)
{
    int check;
    const char* a_str[] = {"HAPCANRelays", "HAPCANButtons", "HAPCANRGBs", 
                            "RGBWs", "TIMs"};
    int n_types = sizeof(a_str)/sizeof(a_str[0]);
    int i_type;
    int i_module;
    int n_modules;
    int node;
    int group;
    bool valid;
    // Check all configured modules
    for(i_type = 0; i_type < n_types; i_type++)
    {
        //-------------------------------------------------------
        // Get the number of configured MODULES for a given TYPE
        //-------------------------------------------------------
        check = jh_getJArrayElements(a_str[i_type], 0, NULL, JSON_DEPTH_LEVEL, 
                &n_modules);
        if(check == JSON_OK)
        {
            for(i_module = 0; i_module < n_modules; i_module++)
            {
                valid = true;
                // Node (Module)
                check = jh_getJFieldInt(a_str[i_type], i_module, 
                        "node", 0, NULL, &node);
                valid = valid && (check == JSON_OK);
                // Group
                check = jh_getJFieldInt(a_str[i_type], i_module, 
                        "group", 0, NULL, &group);
                valid = valid && (check == JSON_OK);
                #ifdef DEBUG_HAPCAN_SYSTEM_ERRORS
                if(!valid)
                {
                    debug_print("hsystem_addModulesToList: Module Information "
                            "Error - Type = %s!\n", a_str[i_type]);
                }
                #endif
                if(valid)
                {
                    hsystem_addElementToList(node, group);
                }
            }
        }
    }
}

// Update the flags for updating the fields when messages are received
static void hsystem_setUpdateFlags(update_t type, int node, int group)
{
    nodeList_t* current;
    bool match;
    int initialGroup;
    int finalGroup;
    int initialNode;
    int finalNode;
    //-----------------------------------------
    // Define search parameters
    //-----------------------------------------
    hsystem_setUpdateNodes(node, group, &initialGroup, &finalGroup, 
        &initialNode, &finalNode);   
    //-----------------------------------------
    // Update STATUS
    //-----------------------------------------
    // LOCK CONTROL
    pthread_mutex_lock(&g_hsystem_control_mutex);
    hsystem_setControlFlags(initialGroup, finalGroup, initialNode, finalNode, 
        type, false);    
    // UNLOCK CONTROL
    pthread_mutex_unlock(&g_hsystem_control_mutex);
    //-----------------------------------------
    // Update DYNAMIC / STATIC / ALL
    //-----------------------------------------   
    // LOCK LIST
    pthread_mutex_lock(&g_hsystem_list_mutex);
    // Get all elements - start from 0
    current = hsystem_getFromOffset(0);    
    // Check remaining     
    while(current != NULL) 
    {
        match = false;
        // Check node and group
        match = current->group >= initialGroup;
        match = match && current->group <= finalGroup;
        match = match && current->node >= initialNode;
        match = match && current->node <= finalNode;
        if(match)
        {
            // Update Flags
            hsystem_setListFlags(current, type, false);
        }                            
        // Get new current element
        current = current->next;        
    }
    // UNLOCK LIST
    pthread_mutex_unlock(&g_hsystem_list_mutex);
}

/**
 * Update the element with the data from HAPCAN Frame
 * \param   hd_received     (INPUT) received HAPCAN Frame
 *          element         (INPUT / OUTPUT) element to be updated
 * 
 * \return  HAPCAN_GENERIC_OK_RESPONSE: Data will be updated internally
 *          HAPCAN_NO_RESPONSE: Not to a configured Node / No update needed
 *          HAPCAN_RESPONSE_ERROR: Error
 */
static int hsystem_updateData(hapcanCANData *hd_received, nodeList_t* element)
{
    int ret = HAPCAN_NO_RESPONSE;
    int i;
    // Update data
    switch(hd_received->frametype)
    {
        case HAPCAN_HEALTH_CHECK_REQUEST_NODE_FRAME_TYPE:
        case HAPCAN_HEALTH_CHECK_REQUEST_GROUP_FRAME_TYPE:            
            if(hd_received->data[0] == 0x01)
            {
                if(!element->isDynamicUpdated[HAPCAN_115_FRAME_P1_UPDATE])
                {
                    element->rxcnt = hd_received->data[1];
                    element->txcnt = hd_received->data[2];
                    element->rxcntmx = hd_received->data[3];
                    element->txcntmx = hd_received->data[4];
                    element->canintcnt = hd_received->data[5];
                    element->rxerrcnt = hd_received->data[6];
                    element->txerrcnt = hd_received->data[7];
                    element->isDynamicUpdated[HAPCAN_115_FRAME_P1_UPDATE] = 
                            true;
                    ret =  HAPCAN_GENERIC_OK_RESPONSE;
                }
            }
            else if(hd_received->data[0] == 0x02)
            {
                if(!element->isDynamicUpdated[HAPCAN_115_FRAME_P2_UPDATE])
                {
                    element->rxcntmxe = hd_received->data[3];
                    element->txcntmxe = hd_received->data[4];
                    element->canintcnte = hd_received->data[5];
                    element->rxerrcnte = hd_received->data[6];
                    element->txerrcnte = hd_received->data[7];
                    element->isDynamicUpdated[HAPCAN_115_FRAME_P2_UPDATE] = 
                            true;
                    ret =  HAPCAN_GENERIC_OK_RESPONSE;
                }
            }
            else
            {
                ret = HAPCAN_RESPONSE_ERROR;
            }
            break;
        case HAPCAN_UPTIME_REQUEST_NODE_FRAME_TYPE:
        case HAPCAN_UPTIME_REQUEST_GROUP_FRAME_TYPE:
            if(!element->isDynamicUpdated[HAPCAN_113_FRAME_UPDATE])
            {
                element->uptime = hd_received->data[4]*256*256*256;
                element->uptime += hd_received->data[5]*256*256;
                element->uptime += hd_received->data[6]*256;
                element->uptime += hd_received->data[7];
                element->isDynamicUpdated[HAPCAN_113_FRAME_UPDATE] = 
                            true;
                ret =  HAPCAN_GENERIC_OK_RESPONSE;
            }
            break;
        case HAPCAN_DESCRIPTION_REQUEST_NODE_FRAME_TYPE:
        case HAPCAN_DESCRIPTION_REQUEST_GROUP_FRAME_TYPE:
            if((!element->isStaticUpdated[HAPCAN_10E_FRAME_P1_UPDATE]) && 
                    (!element->isStaticUpdated[HAPCAN_10E_FRAME_P2_UPDATE]))
            {
                for(i = 0; i < HAPCAN_DATA_LEN; i++)
                {
                    element->description[i] = hd_received->data[i];
                }
                element->isStaticUpdated[HAPCAN_10E_FRAME_P1_UPDATE] = true;
                ret =  HAPCAN_GENERIC_OK_RESPONSE;
            }
            if((element->isStaticUpdated[HAPCAN_10E_FRAME_P1_UPDATE]) && 
                    (!element->isStaticUpdated[HAPCAN_10E_FRAME_P2_UPDATE]))
            {
                for(i = 0; i < HAPCAN_DATA_LEN; i++)
                {
                    element->description[i + HAPCAN_DATA_LEN] = 
                            hd_received->data[i];
                }
                element->isStaticUpdated[HAPCAN_10E_FRAME_P2_UPDATE] = true;
                ret =  HAPCAN_GENERIC_OK_RESPONSE;
            }
            break;
        case HAPCAN_SUPPLY_REQUEST_NODE_FRAME_TYPE:
        case HAPCAN_SUPPLY_REQUEST_GROUP_FRAME_TYPE:
            if(!element->isDynamicUpdated[HAPCAN_10C_FRAME_UPDATE])
            {
                element->volbus = hd_received->data[0]*256;
                element->volbus += hd_received->data[1];
                element->volbus = element->volbus / 2084;
                element->volcpu = hd_received->data[2]*256;
                element->volcpu += hd_received->data[3];
                element->volcpu = element->volcpu / 13100;
                element->isDynamicUpdated[HAPCAN_10C_FRAME_UPDATE] = true;
                ret =  HAPCAN_GENERIC_OK_RESPONSE;
            }
            break;
        case HAPCAN_FW_TYPE_REQUEST_NODE_FRAME_TYPE:
        case HAPCAN_FW_TYPE_REQUEST_GROUP_FRAME_TYPE:
            if(!element->isStaticUpdated[HAPCAN_106_FRAME_UPDATE])
            {
                element->hard = (uint16_t)(hd_received->data[0] << 8);
                element->hard += (uint16_t)(hd_received->data[1]);
                element->hver = hd_received->data[2];
                element->atype = hd_received->data[3];
                element->avers = hd_received->data[4];
                element->fvers = hd_received->data[5];
                element->bver = (uint16_t)(hd_received->data[6] << 8);
                element->bver += (uint16_t)(hd_received->data[7]);
                element->isStaticUpdated[HAPCAN_106_FRAME_UPDATE] = true;
                ret =  HAPCAN_GENERIC_OK_RESPONSE;
            }
            break;
        case HAPCAN_HW_TYPE_REQUEST_NODE_FRAME_TYPE:
        case HAPCAN_HW_TYPE_REQUEST_GROUP_FRAME_TYPE:
            if(!element->isStaticUpdated[HAPCAN_104_FRAME_UPDATE])
            {
                element->hard = (uint16_t)(hd_received->data[0] << 8);
                element->hard += (uint16_t)(hd_received->data[1]);
                element->hver = hd_received->data[2];
                element->id = (uint32_t)(hd_received->data[4] << 24);
                element->id += (uint32_t)(hd_received->data[5] << 16);
                element->id += (uint32_t)(hd_received->data[6] << 8);
                element->id += (uint32_t)(hd_received->data[7]);
                element->isStaticUpdated[HAPCAN_104_FRAME_UPDATE] = true;
                ret =  HAPCAN_GENERIC_OK_RESPONSE;
            }
            break;
        case HAPCAN_DEV_ID_REQUEST_NODE_FRAME_TYPE:
        case HAPCAN_DEV_ID_REQUEST_GROUP_FRAME_TYPE:            
            if(!element->isStaticUpdated[HAPCAN_111_FRAME_UPDATE])
            {
                element->devId = (uint16_t)(hd_received->data[0] << 8);
                element->devId += (uint16_t)(hd_received->data[1]);
                element->isStaticUpdated[HAPCAN_111_FRAME_UPDATE] = true;
                ret =  HAPCAN_GENERIC_OK_RESPONSE;
            }
            break;
        default:
            ret = HAPCAN_RESPONSE_ERROR;
            break;
    }
    return ret;
}

/**
 * Check if the message is from any configured node, and update its data
 * \param   hd_received     (INPUT) received HAPCAN Frame
 *                      
 * \return  HAPCAN_GENERIC_OK_RESPONSE: Data will be updated internally
 *          HAPCAN_NO_RESPONSE: Not to a configured Node / No update needed
 *          HAPCAN_RESPONSE_ERROR: Error
 */
static int hsystem_checkUpdateData(hapcanCANData *hd_received)
{
    int ret = HAPCAN_NO_RESPONSE;
    nodeList_t* current;
    bool match;
    // First check if the message shall be processed or not
    switch(hd_received->frametype)
    {
        case HAPCAN_HEALTH_CHECK_REQUEST_NODE_FRAME_TYPE:
        case HAPCAN_HEALTH_CHECK_REQUEST_GROUP_FRAME_TYPE:
        case HAPCAN_UPTIME_REQUEST_NODE_FRAME_TYPE:
        case HAPCAN_UPTIME_REQUEST_GROUP_FRAME_TYPE:
        case HAPCAN_DESCRIPTION_REQUEST_NODE_FRAME_TYPE:
        case HAPCAN_DESCRIPTION_REQUEST_GROUP_FRAME_TYPE:
        case HAPCAN_SUPPLY_REQUEST_NODE_FRAME_TYPE:
        case HAPCAN_SUPPLY_REQUEST_GROUP_FRAME_TYPE:
        case HAPCAN_FW_TYPE_REQUEST_NODE_FRAME_TYPE:
        case HAPCAN_FW_TYPE_REQUEST_GROUP_FRAME_TYPE:
        case HAPCAN_HW_TYPE_REQUEST_NODE_FRAME_TYPE:
        case HAPCAN_HW_TYPE_REQUEST_GROUP_FRAME_TYPE:
        case HAPCAN_DEV_ID_REQUEST_NODE_FRAME_TYPE:
        case HAPCAN_DEV_ID_REQUEST_GROUP_FRAME_TYPE:
            ret = HAPCAN_GENERIC_OK_RESPONSE;
            break;
        default:
            ret = HAPCAN_NO_RESPONSE;
            break;
    }
    // Check all configured nodes
    if(ret == HAPCAN_GENERIC_OK_RESPONSE)
    {        
        // LOCK LIST
        pthread_mutex_lock(&g_hsystem_list_mutex);
        // Get all elements - start from 0
        current = hsystem_getFromOffset(0);    
        // Check remaining     
        while(current != NULL) 
        {
            match = false;
            // Check node and group
            match = current->group == hd_received->group;
            match = match && current->node == hd_received->module;
            if(match)
            {
                // Update data
                ret = hsystem_updateData(hd_received, current);
            }                            
            // Get new current element
            current = current->next;        
        }
        // UNLOCK LIST
        pthread_mutex_unlock(&g_hsystem_list_mutex);         
    }
    return ret;
}

/**
 * Fill MQTT data with a system MQTT message based on element data
 * \param   element     (INPUT) date to be read
 *          topic       (OUTPUT) topic to be filled
 *          payload     (OUTPUT) payload to be filled
 *          payloadlen  (OUTPUT) payloadlen to be filled
 * 
 */
static void hsystem_getMQTTPayload(nodeList_t* element, char **topic, 
        void **payload, int *payloadlen)
{        
    int check;
    int field;
    char *str = NULL;
    char description[17];
    unsigned int len;
    bool valid = false;
    //-------------------------------------------------
    // Read configuration and validate data
    //-------------------------------------------------
    // GET TOPIC
    check = hconfig_getConfigStr(HAPCAN_CONFIG_STATUS_PUB, &str);
    valid = (check == EXIT_SUCCESS) && (str != NULL);
    valid = valid && (element->group >= 1) && (element->group <= 255);
    valid = valid && (element->node >= 1) && (element->node <= 255);
    if(!valid)
    {
        *topic = NULL;
        *payload = NULL;
        *payloadlen = 0;
    }    
    else
    {
        //-------------------------------------------------
        // Create topic
        //-------------------------------------------------
        len = strlen(str);
        *topic = malloc(len + 10); // additional bytes for For "/xxx/xxx" and \0
        // Copy PUB topic to topic
        strcpy(*topic, str);
        // Get rest of topic "/GROUP/NODE"
        free(str);
        str = malloc(9);
        snprintf(str, 9, "/%d/%d/", element->group, element->node);
        strncat(*topic, str, 9);
        free(str);
        //-------------------------------------------------
        // Create Payload
        //-------------------------------------------------
        // Copy description
        memcpy(description, &(element->description[0]), 16);
        description[16] = '\0';
        // Set all fields
        field = 0;
        jsonFieldData j_arr[NODE_LIST_N_FIELDS];
        j_arr[field].field = "NODE";
        j_arr[field].value_type = JSON_TYPE_INT;
        j_arr[field].int_value = element->node;
        field++;
        j_arr[field].field = "GROUP";
        j_arr[field].value_type = JSON_TYPE_INT;
        j_arr[field].int_value = element->group;
        field++;
        j_arr[field].field = "HARD";
        j_arr[field].value_type = JSON_TYPE_INT;
        j_arr[field].int_value = element->hard;
        field++;
        j_arr[field].field = "HVER";
        j_arr[field].value_type = JSON_TYPE_INT;
        j_arr[field].int_value = element->hver;
        field++;
        j_arr[field].field = "ID";
        j_arr[field].value_type = JSON_TYPE_INT;
        j_arr[field].int_value = element->id;
        field++;
        j_arr[field].field = "ATYPE";
        j_arr[field].value_type = JSON_TYPE_INT;
        j_arr[field].int_value = element->atype;
        field++;
        j_arr[field].field = "AVERS";
        j_arr[field].value_type = JSON_TYPE_INT;
        j_arr[field].int_value = element->avers;
        field++;
        j_arr[field].field = "FVERS";
        j_arr[field].value_type = JSON_TYPE_INT;
        j_arr[field].int_value = element->fvers;
        field++;
        j_arr[field].field = "BVER";
        j_arr[field].value_type = JSON_TYPE_INT;
        j_arr[field].int_value = element->bver;
        field++;    
        j_arr[field].field = "DESCRIPTION";
        j_arr[field].value_type = JSON_TYPE_STRING;
        j_arr[field].str_value = description;
        field++;
        j_arr[field].field = "DEVID";
        j_arr[field].value_type = JSON_TYPE_INT;
        j_arr[field].int_value = element->devId;
        field++;
        j_arr[field].field = "VOLBUS";
        j_arr[field].value_type = JSON_TYPE_DOUBLE;
        j_arr[field].double_value = element->volbus;
        field++;
        j_arr[field].field = "VOLCPU";
        j_arr[field].value_type = JSON_TYPE_DOUBLE;
        j_arr[field].double_value = element->volcpu;
        field++;
        j_arr[field].field = "UPTIME";
        j_arr[field].value_type = JSON_TYPE_INT;
        j_arr[field].int_value = element->uptime;
        field++;
        j_arr[field].field = "RXCNT";
        j_arr[field].value_type = JSON_TYPE_INT;
        j_arr[field].int_value = element->rxcnt;
        field++;
        j_arr[field].field = "TXCNT";
        j_arr[field].value_type = JSON_TYPE_INT;
        j_arr[field].int_value = element->txcnt;
        field++;
        j_arr[field].field = "RXCNTMX";
        j_arr[field].value_type = JSON_TYPE_INT;
        j_arr[field].int_value = element->rxcntmx;
        field++;
        j_arr[field].field = "TXCNTMX";
        j_arr[field].value_type = JSON_TYPE_INT;
        j_arr[field].int_value = element->txcntmx;
        field++;
        j_arr[field].field = "CANINTCNT";
        j_arr[field].value_type = JSON_TYPE_INT;
        j_arr[field].int_value = element->canintcnt;
        field++;
        j_arr[field].field = "RXERRCNT";
        j_arr[field].value_type = JSON_TYPE_INT;
        j_arr[field].int_value = element->rxerrcnt;
        field++;
        j_arr[field].field = "TXERRCNT";
        j_arr[field].value_type = JSON_TYPE_INT;
        j_arr[field].int_value = element->txerrcnt;
        field++;
        j_arr[field].field = "RXCNTMXE";
        j_arr[field].value_type = JSON_TYPE_INT;
        j_arr[field].int_value = element->rxcntmxe;
        field++;
        j_arr[field].field = "TXCNTMXE";
        j_arr[field].value_type = JSON_TYPE_INT;
        j_arr[field].int_value = element->txcntmxe;
        field++;
        j_arr[field].field = "CANINTCNTE";
        j_arr[field].value_type = JSON_TYPE_INT;
        j_arr[field].int_value = element->canintcnte;
        field++;
        j_arr[field].field = "RXERRCNTE";
        j_arr[field].value_type = JSON_TYPE_INT;
        j_arr[field].int_value = element->rxerrcnte;
        field++;
        j_arr[field].field = "TXERRCNTE";
        j_arr[field].value_type = JSON_TYPE_INT;
        j_arr[field].int_value = element->txerrcnte;
        field++;        
        // Get Payload string
        jh_getStringFromFieldValuePairs(j_arr, field, &str);          
        len = strlen(str);
        if(str != NULL && len > 0)
        {
            *payload = malloc(len + 1);
            strcpy(*payload, str);
        }
        else
        {
            *payload = NULL;
        }
        // SET PAYLOAD LEN
        *payloadlen = len;
    }
    // free
    free(str);
}

/**
 * Check if there is CAN messages to be sent
 * \return  HAPCAN_NO_RESPONSE: No response was added to CAN Write Buffer
 *          HAPCAN_CAN_RESPONSE: Response added to CAN Write Buffer (OK)
 *          HAPCAN_CAN_RESPONSE_ERROR: Error adding to CAN Write Buffer (FAIL)
 *          HAPCAN_RESPONSE_ERROR: Other error
 */
static int hsystem_checkAndSendCAN(void)
{
    int ret = HAPCAN_NO_RESPONSE;
    unsigned long long timestamp;
    hapcanCANData hd_result;
    int initialNode;
    int finalNode;
    int initialGroup;
    int finalGroup;
    int node;
    int group;
    bool isFinished;
    bool sendStatusRequest;
    bool sendStaticRequest;
    bool sendDynamicRequest;
    uint16_t frametype;
    nodeList_t* current;
    int i;
    // Limit messages sent to modules not responding
    int lastSentNode = g_lastSentNode;
    int lastSentGroup = g_lastSentGroup;
    int lastSentFrame = g_lastSentFrame;    
    //---------------------------
    // Get data for STATUS REQUEST message
    //---------------------------
    // LOCK CONTROL
    pthread_mutex_lock(&g_hsystem_control_mutex);
    // Check and send STATUS Request
    isFinished = g_status_control.isFinished;
    initialNode = g_status_control.initialNode;
    finalNode = g_status_control.lastNode;
    initialGroup = g_status_control.initialGroup;
    finalGroup = g_status_control.lastGroup;
    // UNLOCK CONTROL
    pthread_mutex_unlock(&g_hsystem_control_mutex);    
    //-------------------------------------------
    // Check every configured module
    //-------------------------------------------
    // LOCK LIST
    pthread_mutex_lock(&g_hsystem_list_mutex);
    // Get all elements - start from 0
    current = hsystem_getFromOffset(0);    
    // Check all
    sendStatusRequest = false;
    sendStaticRequest = false;
    sendDynamicRequest = false;
    while(current != NULL) 
    {
        // Get basic module info
        node = current->node;
        group = current->group; 
        // Check for STATUS REQUEST
        if(!isFinished)
        {
            if(!current->isStatusSent)
            {
                if((group >= initialGroup) && (group <= finalGroup) &&
                        (node >= initialNode) && (node <= finalNode))
                {
                    sendStatusRequest = true;
                    break;
                }
            }
        }
        else
        {
            // Check Dynamic fields
            for(i = 0; i < HAPCAN_DYNAMIC_N_UPDATES; i++)
            {
                if(!current->isDynamicUpdated[i])
                {
                    sendDynamicRequest = true;
                    // Set frametype
                    frametype = c_frametype_dynamic[i];
                    break;
                }
            }
            if(!sendDynamicRequest)
            {
                // Check Static fields 
                for(i = 0; i < HAPCAN_STATIC_N_UPDATES; i++)
                {
                    if(!current->isStaticUpdated[i])
                    {
                        sendStaticRequest = true;
                        // Set frametype
                        frametype = c_frametype_static[i];
                        break;
                    }
                }
            }
            // If there is nothing to be updated, the update request was handled
            if(!sendDynamicRequest && !sendStaticRequest)
            {
                current->isRequestHandled = true;
            }
            // If the request is already handled, no need to update data
            if(current->isRequestHandled)
            {
                sendStaticRequest = false;
                sendDynamicRequest = false;
            }
            else
            {
                // If the request was not handled, and there is a request
                // send it unless module not responding
                if(sendDynamicRequest || sendStaticRequest)
                {
                    g_lastSentNode = node;
                    g_lastSentGroup = group;
                    g_lastSentFrame = frametype;
                    if((g_lastSentNode == lastSentNode) &&
                            (g_lastSentGroup == lastSentGroup) &&
                            (g_lastSentFrame == lastSentFrame))
                    {
                        g_lastSentCount++;
                        if(g_lastSentCount >= HAPCAN_CAN_STATUS_SEND_RETRIES)
                        {
                            // Error - Module may not be responding - clear
                            sendStaticRequest = false;
                            sendDynamicRequest = false;
                            hsystem_setListFlags(current, UPDATE_TYPE_ALL, 
                                    true);
                            #ifdef DEBUG_HAPCAN_SYSTEM_ERRORS
                            debug_print("INFO: hsystem_checkAndSendCAN: Module "
                                    "is not responding - Node = %d, "
                                    "Group = %d!\n", node, group);                            
                            #endif
                        }
                    }                  
                    break;
                }
            }
        }        
        // Get new current element
        current = current->next;        
    }
    // UNLOCK LIST
    pthread_mutex_unlock(&g_hsystem_list_mutex);
    //------------------------------------------
    // Check and update data for STATUS REQUEST 
    //------------------------------------------
    if(sendStatusRequest)
    {
        // Request STATUS update for the given module
        hapcan_getSystemFrame(&hd_result, 
                HAPCAN_STATUS_REQUEST_NODE_FRAME_TYPE, node, group);
        // Get Timestamp
        timestamp = aux_getmsSinceEpoch();
        ret = hapcan_addToCANWriteBuffer(&hd_result, timestamp, true);
        if(ret == HAPCAN_CAN_RESPONSE)
        {
            // LOCK LIST
            pthread_mutex_lock(&g_hsystem_list_mutex);
            current->isStatusSent = true;
            // UNLOCK LIST
            pthread_mutex_unlock(&g_hsystem_list_mutex);
        }
    }
    else
    {
        // LOCK CONTROL
        pthread_mutex_lock(&g_hsystem_control_mutex);
        // No module to send STATUS REQUEST: finished
        g_status_control.isFinished = true;
        // UNLOCK CONTROL
        pthread_mutex_unlock(&g_hsystem_control_mutex);    
    }
    //---------------------------------------
    // Check STATIC and DYNAMIC Data
    //---------------------------------------
    if(sendStaticRequest || sendDynamicRequest)
    {        
        // send data and update field for current element
        hapcan_getSystemFrame(&hd_result, frametype, node, group);
        // Get Timestamp
        timestamp = aux_getmsSinceEpoch();
        ret = hapcan_addToCANWriteBuffer(&hd_result, timestamp, true);
    }
    else
    {
        // No request to be sent - all is fine
        g_lastSentNode = 0;
        g_lastSentGroup = 0;
        g_lastSentCount = 0;
        g_lastSentFrame = 0; 
    }
    return ret;
}

/**
 * Check if there is MQTT messages to be sent
 * \return  HAPCAN_NO_RESPONSE: No RAW response was added to MQTT Pub Buffer
 *          HAPCAN_MQTT_RESPONSE: RAW Response added to MQTT Pub Buffer (OK)
 *          HAPCAN_MQTT_RESPONSE_ERROR: Error adding to MQTT Pub Buffer (FAIL)
 *          HAPCAN_RESPONSE_ERROR: Other error
 */
static int hsystem_checkAndSendMQTT(void)
{
    int ret = HAPCAN_NO_RESPONSE;
    bool sendDynamic;    
    bool sendStatic;    
    bool dynamicReady;
    bool staticReady;
    char *topic = NULL;
    void *payload = NULL;
    int payloadlen = 0;
    unsigned long long timestamp;
    nodeList_t* current;
    int i;
    //-------------------------------------------
    // Check every configured module
    //-------------------------------------------
    sendDynamic = false;
    sendStatic = false;
    dynamicReady = false;
    staticReady = false;
    // LOCK LIST
    pthread_mutex_lock(&g_hsystem_list_mutex);
    // Get all elements - start from 0
    current = hsystem_getFromOffset(0);    
    // Check all    
    while(current != NULL) 
    {
        // Check if data is ready to be sent - Dynamic fields
        dynamicReady = true;
        for(i = 0; i < HAPCAN_DYNAMIC_N_UPDATES; i++)
        {
            if(!current->isDynamicUpdated[i])
            {                    
                dynamicReady = false;
                break;
            }
        }
        // Check if data is ready to be sent - Static fields
        staticReady = true;
        for(i = 0; i < HAPCAN_STATIC_N_UPDATES; i++)
        {
            if(!current->isStaticUpdated[i])
            {
                staticReady = false;
                break;
            }
        }
        // Check if data was already sent - Dynamic
        if(!current->isDynamicSent && dynamicReady && staticReady)
        {            
            sendDynamic = true;
        }        
        // Check if data was already sent - Static
        if(!current->isStaticSent && staticReady && dynamicReady)
        {
            sendStatic = true;
        }
        // Prepare data to be sent
        if(sendDynamic || sendStatic)
        {
            hsystem_getMQTTPayload(current, &topic, &payload, &payloadlen);
            if(topic == NULL || payload == NULL || payloadlen <= 0)
            {
                sendStatic = false;
                sendDynamic = false;
            }
            break;
        }
        // Get new current element
        current = current->next;        
    }
    // UNLOCK LIST
    pthread_mutex_unlock(&g_hsystem_list_mutex);
    // Check if message is available to be sent
    if(sendDynamic || sendStatic)
    {
        // Get Timestamp and Set MQTT Pub buffer
        timestamp = aux_getmsSinceEpoch();
        ret = hapcan_addToMQTTPubBuffer(topic, payload, payloadlen, timestamp);
        if(ret == HAPCAN_MQTT_RESPONSE)
        {
            // LOCK LIST
            pthread_mutex_lock(&g_hsystem_list_mutex);
            if(sendDynamic)
            {
                current->isDynamicSent = true;
            }
            if(sendStatic)
            {
                current->isStaticSent = true;
            }
            // UNLOCK LIST
            pthread_mutex_unlock(&g_hsystem_list_mutex);
        }
    }
    // Free
    free(topic);
    free(payload);
    // Return
    return ret;
}

//----------------------------------------------------------------------------//
// EXTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
/*
 * Init - to be called every time the configuration changes
 */
void hsystem_init(void)
{
    //---------------------------------------------
    // Delete and Add to list: PROTECTED
    //---------------------------------------------
    // LOCK LIST
    pthread_mutex_lock(&g_hsystem_list_mutex);
    // Delete
    hsystem_deleteList();
    // Add to list
    hsystem_addModulesToList();
    // UNLOCK LIST
    pthread_mutex_unlock(&g_hsystem_list_mutex); 
    // LOCK CONTROL
    pthread_mutex_lock(&g_hsystem_control_mutex);
    // Clear Update Request
    hsystem_setControlFlags(255, 255, 255, 255, UPDATE_TYPE_ALL, true);    
    // UNLOCK CONTROL
    pthread_mutex_unlock(&g_hsystem_control_mutex);
    // Initial Status Update
    hsystem_statusUpdate();
}

/*
 * Request Status Update for all configured modules
 */
void hsystem_statusUpdate(void)
{
    // INITIAL STATUS UPDATE: Set node to 0 and group to 0 to update status of 
    // all nodes
    hsystem_setUpdateFlags(UPDATE_TYPE_STATUS, 0, 0);
}

/**
 * Check if a received MQTT message is related to a system message / update 
 * request. 
 */
int hsystem_checkMQTT(char* topic, void* payload, int payloadlen, 
        unsigned long long timestamp)
{
    int ret = HAPCAN_NO_RESPONSE;
    int check;
    char *str = NULL;
    int len;
    int node;
    int group;
    update_t type;
    // GET TOPIC
    check = hconfig_getConfigStr(HAPCAN_CONFIG_STATUS_SUB, &str);
    if((check == EXIT_SUCCESS) && (str != NULL))
    {            
        // Only compare up to the configured status sub topic length
        len = strlen(str);
        if(aux_compareStringsN(str, topic, len))
        {
            // There is a Topic "Base" Match - get node and group
            if(hsystem_getGroupNodeFromTopic(topic, str, &node, &group))
            {
                // Read Payload
                free(str); // From Topic usage
                str = malloc(payloadlen + 1);
                memcpy(str, payload, payloadlen);
                str[payloadlen] = 0;
                // Check Payload
                if( aux_compareStrings(str, "STATIC") )
                {
                    type = UPDATE_TYPE_STATIC;
                    hsystem_setUpdateFlags(type, node, group);
                    ret = HAPCAN_GENERIC_OK_RESPONSE;
                }
                else if( aux_compareStrings(str, "DYNAMIC") )
                {
                    type = UPDATE_TYPE_DYNAMIC;
                    hsystem_setUpdateFlags(type, node, group);
                    ret = HAPCAN_GENERIC_OK_RESPONSE;
                }
                else if( aux_compareStrings(str, "STATUS") )
                {
                    type = UPDATE_TYPE_STATUS;
                    hsystem_setUpdateFlags(type, node, group);
                    ret = HAPCAN_GENERIC_OK_RESPONSE;
                }
                else if( aux_compareStrings(str, "ALL") )
                {
                    type = UPDATE_TYPE_ALL;
                    hsystem_setUpdateFlags(type, node, group);
                    ret = HAPCAN_GENERIC_OK_RESPONSE;
                }
                else
                {
                    ret = HAPCAN_RESPONSE_ERROR;
                }                
            }
            else
            {
                ret = HAPCAN_RESPONSE_ERROR;
            }
        }
    }
    // Free
    free(str);
    // Return;    
    return ret;
}

/**
 * Check if a received HAPCAN Frame is related to a system message.
 */
int hsystem_checkCAN(hapcanCANData *hd_received, unsigned long long timestamp)
{
    
    int ret;
    // Check for a system frame message and update the configured node
    ret = hsystem_checkUpdateData(hd_received);
    return ret;
}

/**
 * To be called periodically to return the status of the system update.         
 */
int hsystem_periodic(void)
{
    int ret;    
    // Check and send CAN
    ret = hsystem_checkAndSendCAN();
    // If it is an error, application must be informed
    if(ret != HAPCAN_CAN_RESPONSE_ERROR)
    {
        ret = hsystem_checkAndSendMQTT();
    }
    return ret;
}