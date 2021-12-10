//----------------------------------------------------------------------------//
//                               OBJECT HISTORY                               //
//----------------------------------------------------------------------------//
//  REVISION |    DATE     |                               |      AUTHOR      //
//----------------------------------------------------------------------------//
//  1.00     | 28/Nov/2021 |                               | ALCP             //
// - Creation of file                                                         //
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
static char *rawHapcanPubTopic = NULL;
static char *rawHapcanSubTopic = NULL;
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
    // first free all values to default in case it is a reload
    free(rawHapcanPubTopic);
    free(rawHapcanSubTopic);
    free(statusPubTopic);
    free(statusSubTopic);
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
    check = config_getString(CONFIG_GENERAL_SETTINGS_LEVEL, 0, 
        "rawHapcanPubTopic", 0, NULL, &rawHapcanPubTopic);      
    if(check != EXIT_SUCCESS)
    {            
        rawHapcanPubTopic = NULL;
    }
    check = config_getString(CONFIG_GENERAL_SETTINGS_LEVEL, 0, 
            "rawHapcanSubTopic", 0, NULL, &rawHapcanSubTopic);      
    if(check != EXIT_SUCCESS)
    {            
        rawHapcanSubTopic = NULL;
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
        case HAPCAN_CONFIG_RAW_SUB:
            if(rawHapcanSubTopic == NULL)
            {            
                *str = NULL;
            }
            else
            {
                len = strlen(rawHapcanSubTopic);
                *str = malloc(len + 1);
                strcpy(*str, rawHapcanSubTopic);
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
        default:
            *value = 0;
            ret = EXIT_FAILURE;
            break;
    }
    return ret;
}