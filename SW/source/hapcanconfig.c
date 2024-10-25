//----------------------------------------------------------------------------//
//                               OBJECT HISTORY                               //
//----------------------------------------------------------------------------//
//  REVISION |    DATE     |                               |      AUTHOR      //
//----------------------------------------------------------------------------//
//  1.00     | 10/Dec/2021 |                               | ALCP             //
// - First version                                                            //
//----------------------------------------------------------------------------//
//  1.01     | 20/Oct/2024 |                               | ALCP             //
// - config.json: remove field rawHapcanSubTopic                              //
// - config.json: add fields rawHapcanSubTopics, rawHapcanPubAll,             //
// rawHapcanPubModules                                                        //
//----------------------------------------------------------------------------//

/*
 * ----------------------------------------------------------------------------
 * REMARKS:
 * - Mutex is not used for the configuration variables. They are only set when 
 * the configuration file changes, and they have boundary checks. NULL pointers,
 * for instance, would not be a problem.
 * ----------------------------------------------------------------------------
 */

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
#include "jsonhandler.h"

//----------------------------------------------------------------------------//
// INTERNAL DEFINITIONS
//----------------------------------------------------------------------------//

//----------------------------------------------------------------------------//
// INTERNAL TYPES
//----------------------------------------------------------------------------//

//----------------------------------------------------------------------------//
// INTERNAL GLOBAL VARIABLES
//----------------------------------------------------------------------------//
// ID for direct control frame
static int g_computerID1 = HAPCAN_DEFAULT_CIDx;
static int g_computerID2 = HAPCAN_DEFAULT_CIDx;
// MQTT <--> Hapcan Raw data config
static bool enableRawHapcan = false;
static bool rawHapcanPubAll = false;
static char *rawHapcanPubTopic = NULL;
static int n_rawSubtopics = 0;
static char **rawHapcanSubTopics = NULL;
static int n_rawPubModules = 0;
rawModuleID_t *rawModulesPubList = NULL;
// MQTT <--> Hapcan System Messages config
static bool enableHapcanStatus;
static char *statusPubTopic = NULL;
static char *statusSubTopic = NULL;
// MQTT <--> Hapcan gateway config
static bool enableGateway = false;

//----------------------------------------------------------------------------//
// INTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
// Config
static void getHAPCANConfiguration(void);

/*
 * Read items from configuration 
 */
static void getHAPCANConfiguration(void)
{
    int check;
    int c_ID1;
    int c_ID2;
    bool valid;
    int i;
    int n;
    int node;
    int group;
    //--------------------------------------------------------------------------
    // Init - Free all values to default in case it is a reload
    //     REMARK: temporary save n to prevent simultaneous read while config 
    //     is being loaded
    //-------------------------------------------------------------------------
    // RAW
    enableRawHapcan = false;
    rawHapcanPubAll = false;
    n = n_rawSubtopics;
    n_rawSubtopics = 0;
    for (i = 0; i < n; i++)
    {
        free(rawHapcanSubTopics[i]);
    }
    free(rawHapcanSubTopics);
    n_rawPubModules = 0;
    free(rawModulesPubList);
    free(rawHapcanPubTopic); 
    // STATUS   
    enableHapcanStatus = false;
    free(statusPubTopic);
    free(statusSubTopic);
    // GATEWAY
    enableGateway = false;

    //-----------------------------------
    // ID for direct control frame
    //-----------------------------------
    valid = true;
    check = config_getInt(CONFIG_GENERAL_SETTINGS_LEVEL, 0, 
            "computerID1", 0, NULL, &c_ID1);      
    if(check != EXIT_SUCCESS)
    {
        valid = false;
    }
    check = config_getInt(CONFIG_GENERAL_SETTINGS_LEVEL, 0, 
            "computerID2", 0, NULL, &c_ID2);      
    if(check != EXIT_SUCCESS)
    {
        valid = false;
    }
    valid = valid && (c_ID1 >= 0) && (c_ID1 <= 255);
    valid = valid && (c_ID2 >= 0) && (c_ID2 <= 255);
    if(!valid)
    {
        c_ID1 = HAPCAN_DEFAULT_CIDx;
        c_ID2 = HAPCAN_DEFAULT_CIDx;
    }
    g_computerID1 = c_ID1;
    g_computerID2 = c_ID2;
    //-----------------------------------
    // MQTT <--> Hapcan Raw data config
    //-----------------------------------
    check = config_getBool(CONFIG_GENERAL_SETTINGS_LEVEL, 0, 
            "enableRawHapcan", 0, NULL, &enableRawHapcan);      
    if(check != EXIT_SUCCESS)
    {            
        enableRawHapcan = false;
    }
    check = config_getBool(CONFIG_GENERAL_SETTINGS_LEVEL, 0, 
            "rawHapcanPubAll", 0, NULL, &rawHapcanPubAll);      
    if(check != EXIT_SUCCESS)
    {            
        rawHapcanPubAll = false;
    }    
    check = config_getString(CONFIG_GENERAL_SETTINGS_LEVEL, 0, 
        "rawHapcanPubTopic", 0, NULL, &rawHapcanPubTopic);      
    if(check != EXIT_SUCCESS)
    {            
        rawHapcanPubTopic = NULL;
    }
    check = config_getStringArray(CONFIG_GENERAL_SETTINGS_LEVEL,
            "rawHapcanSubTopics", &n_rawSubtopics, &rawHapcanSubTopics);
    if (check != EXIT_SUCCESS) 
    {
        rawHapcanSubTopics = NULL;
        n_rawSubtopics = 0;
    }
    //------------------------------------------------
    // Get the number of configured RAW Modules
    //------------------------------------------------
    check = jh_getJArrayElementsObj(CONFIG_GENERAL_SETTINGS_LEVEL, 
        "rawHapcanPubModules", 0, NULL, JSON_DEPTH_LEVEL, &n_rawPubModules);
    if(check == JSON_OK)
    {
        rawModulesPubList = (rawModuleID_t*)malloc(
            sizeof(*rawModulesPubList)*n_rawPubModules);
        valid = true;
        for(i = 0; i < n_rawPubModules; i++)
        {            
            // Node
            check = jh_getJFieldIntObj(CONFIG_GENERAL_SETTINGS_LEVEL, 
                "rawHapcanPubModules", i, "node", 0, NULL, &node);
            valid = valid && (check == JSON_OK);
            // Group
            check = jh_getJFieldIntObj(CONFIG_GENERAL_SETTINGS_LEVEL, 
                "rawHapcanPubModules", i, "group", 0, NULL, &group);
            valid = valid && (check == JSON_OK);
            valid = valid && (node >= 0) && (node <= 255);
            valid = valid && (group >= 0) && (group <= 255);
            if(valid)
            {
                rawModulesPubList[i].node = node;
                rawModulesPubList[i].group = group;
            }
            #ifdef DEBUG_HAPCAN_CONFIG_ERRORS
            if(!valid)
            {
                n_rawPubModules = 0;
                debug_print("getHAPCANConfiguration: Raw Pub List Error!\n");
            }
            #endif
        }
    }
    else
    {
        n_rawPubModules = 0;
        rawModulesPubList = NULL;
    }
    //-----------------------------------
    // MQTT <--> Hapcan System Messages config
    //-----------------------------------
    check = config_getBool(CONFIG_GENERAL_SETTINGS_LEVEL, 0, 
            "enableHapcanStatus", 0, NULL, &enableHapcanStatus);      
    if(check != EXIT_SUCCESS)
    {            
        enableHapcanStatus = false;
    }
    check = config_getString(CONFIG_GENERAL_SETTINGS_LEVEL, 0, 
            "statusPubTopic", 0, NULL, &statusPubTopic);      
    if(check != EXIT_SUCCESS)
    {
        statusPubTopic = NULL;
    }
    check = config_getString(CONFIG_GENERAL_SETTINGS_LEVEL, 0, 
            "statusSubTopic", 0, NULL, &statusSubTopic);      
    if(check != EXIT_SUCCESS)
    {
        statusSubTopic = NULL;
    }
    //-----------------------------------
    // MQTT <--> Hapcan gateway config
    //-----------------------------------
    check = config_getBool(CONFIG_GENERAL_SETTINGS_LEVEL, 0, 
            "enableGateway", 0, NULL, &enableGateway);      
    if(check != EXIT_SUCCESS)
    {            
        enableGateway = false;
    }
}

//----------------------------------------------------------------------------//
// EXTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
/**
 * Init the gateway - Setup the lists based on configuration file
 **/
void hconfig_init(void)
{    
    //--------------------------------------------
    // Get Configuration
    //--------------------------------------------
    getHAPCANConfiguration();
}

/**
 * Return the specified string configuration
 */
int hconfig_getConfigStr(hapcanConfigID config, char **str)
{
    int ret = EXIT_SUCCESS;
    unsigned int len;
    switch(config)
    {
        case HAPCAN_CONFIG_RAW_PUB:
            if(rawHapcanPubTopic == NULL)
            {            
                *str = NULL;
            }
            else
            {
                len = strlen(rawHapcanPubTopic);
                *str = malloc(len + 1);
                strcpy(*str, rawHapcanPubTopic);
            }
            break;        
        case HAPCAN_CONFIG_STATUS_PUB:
            if(statusPubTopic == NULL)
            {            
                *str = NULL;
            }
            else
            {
                len = strlen(statusPubTopic);
                *str = malloc(len + 1);
                strcpy(*str, statusPubTopic);
            }
            break;
        case HAPCAN_CONFIG_STATUS_SUB:
            if(statusSubTopic == NULL)
            {            
                *str = NULL;
            }
            else
            {
                len = strlen(statusSubTopic);
                *str = malloc(len + 1);
                strcpy(*str, statusSubTopic);
            }
            break;
        default:
            ret = EXIT_FAILURE;
            *str = NULL;
            break;
    }
    return ret;
}

/**
 * Return the specified string configuration with a specified index position
 */
int hconfig_getConfigStrN(hapcanConfigID config, uint16_t i_field, char **str)
{
    int ret = EXIT_SUCCESS;
    unsigned int len;
    switch(config)
    {
        case HAPCAN_CONFIG_RAW_SUBS:
            if(i_field >= 0 && i_field < n_rawSubtopics && n_rawSubtopics > 0)
            {                
                len = strlen(rawHapcanSubTopics[i_field]);
                *str = malloc(len + 1);
                strcpy(*str, rawHapcanSubTopics[i_field]);
            }
            else
            {
                ret = EXIT_FAILURE;
                *str = NULL;
            }
            break;
        default:
            ret = EXIT_FAILURE;
            *str = NULL;
            break;
    }
    return ret;
}

/**
 * Return the specified boolean configuration
 */
int hconfig_getConfigBool(hapcanConfigID config, bool *value)
{
    int ret = EXIT_SUCCESS;
    switch(config)
    {
        case HAPCAN_CONFIG_ENABLE_RAW:
            *value = enableRawHapcan;
            break;
        case HAPCAN_CONFIG_PUB_ALL:
            *value = rawHapcanPubAll;
            break;
        case HAPCAN_CONFIG_ENABLE_GATEWAY:
            *value = enableGateway;
            break;
        case HAPCAN_CONFIG_ENABLE_STATUS:
            *value = enableHapcanStatus;
            break;
        default:
            ret = EXIT_FAILURE;
            *value = false;
            break;
    }
    return ret;
}

/**
 * Return the specified integer configuration
 */
int hconfig_getConfigInt(hapcanConfigID config, int *value)
{
    int ret = EXIT_SUCCESS;
    switch(config)
    {
        case HAPCAN_CONFIG_COMPUTER_ID1:
            *value = g_computerID1;
            break;
        case HAPCAN_CONFIG_COMPUTER_ID2:
            *value = g_computerID2;
            break;
        case HAPCAN_CONFIG_N_RAW_SUBS:
            *value = n_rawSubtopics;
            break;
        case HAPCAN_CONFIG_N_PUB_MODULES:
            *value = n_rawPubModules;
            break;
        default:
            *value = 0;
            ret = EXIT_FAILURE;
            break;
    }
    return ret;
}

/**
 * Return the specified module ID configuration on the n-th index of the field          
 */
int hconfig_getConfigID(hapcanConfigID config, uint16_t i_field, 
    rawModuleID_t *id)
{
    int ret = EXIT_SUCCESS;
    switch(config)
    {
        case HAPCAN_CONFIG_PUB_MODULES:
            if(i_field >= 0 && i_field < n_rawPubModules && n_rawPubModules > 0)
            {
                id->group = rawModulesPubList[i_field].group;
                id->node = rawModulesPubList[i_field].node;
            }
            else
            {
                id->group = 0;
                id->node = 0;
                ret = EXIT_FAILURE;
            }
            break;
        default:
            id->group = 0;
            id->node = 0;
            ret = EXIT_FAILURE;
            break;
    }
    return ret;
}