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
#include <pthread.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "auxiliary.h"
#include "config.h"
#include "jsonhandler.h"
#include "debug.h"

//----------------------------------------------------------------------------//
// INTERNAL DEFINITIONS
//----------------------------------------------------------------------------//

//----------------------------------------------------------------------------//
// INTERNAL TYPES
//----------------------------------------------------------------------------//

//----------------------------------------------------------------------------//
// INTERNAL GLOBAL VARIABLES
//----------------------------------------------------------------------------//
static time_t g_last_date;

//----------------------------------------------------------------------------//
// INTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
static bool getConfigFileModifiedDate(time_t *date);
static bool isFileChanged(void);
static void updateConfigFromFile(void);

/**
 * Check if configuration file was changed
 * \param   date    date to be filled
 * \return   If check was OK
 */
static bool getConfigFileModifiedDate(time_t *date) 
{
    bool ret;
    struct stat file_details;
    if( stat(JSON_CONFIG_FILE_PATH, &file_details) != -1)
    {
        ret = true;
        *date = file_details.st_mtime;
    }
    else
    {
        ret = false;
    }
    return ret;
}

/**
 * Check if configuration file was changed
 * \return   TRUE or FALSE
 */
static bool isFileChanged(void) 
{
    time_t date;
    bool ret;
    
    if( getConfigFileModifiedDate(&date) )
    {
        ret = (difftime(date, g_last_date) != 0);
    }
    else
    {
        ret = false;
    }
    return ret;
}

/**
 * Update the local data structure with the data from the file.
 * In case of errors, default values are used.
 **/
static void updateConfigFromFile(void)
{
    // JSON: Read File
    jh_readConfigFile();
    // Update file date
    if( !getConfigFileModifiedDate(&g_last_date) )
    {
        g_last_date = 0;
    }
}

//----------------------------------------------------------------------------//
// EXTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
void config_init(void)
{
    updateConfigFromFile();
}

void config_end(void)
{
    // JSON: Free
    jh_freeConfigFile();
}

int config_isNewConfigAvailable(void)
{
    int ret;
    if( isFileChanged() )
    {
        ret = CONFIG_FILE_UNCHANGED;
    }
    else
    {
        ret = CONFIG_FILE_UPDATED;
    }
    return ret;
}

void config_reload(bool *reloadMQTT, bool *reload_socket_server)
{        
    int check;
    bool temp;
    char *old_mqtt_server = NULL;
    char *new_mqtt_server = NULL;
    char *old_mqtt_ID = NULL;
    char *new_mqtt_ID = NULL;
    bool old_enable_MQTT;
    bool new_enable_MQTT;
    char **old_sub_topics;
    char **new_sub_topics;
    int old_n_sub_topics = 0;
    int new_n_sub_topics = 0;
    char *old_port = NULL;
    char *new_port = NULL;
    bool old_enable_server;
    bool new_enable_server;
    int i;
    //--------------------------------------
    // Read last configurations for MQTT
    //--------------------------------------
    check = config_getBool(CONFIG_GENERAL_SETTINGS_LEVEL, 0, 
                "enableMQTT", 0, NULL, &old_enable_MQTT);
    if(check != EXIT_SUCCESS)
    {
        old_enable_MQTT = false;
    }
    check = config_getString(CONFIG_GENERAL_SETTINGS_LEVEL, 0, 
            "mqttBroker", 0, NULL, &old_mqtt_server);
    if(check != EXIT_SUCCESS)
    {
        old_mqtt_server = NULL;
    }
    check = config_getString(CONFIG_GENERAL_SETTINGS_LEVEL, 0, 
            "mqttClientID", 0, NULL, &old_mqtt_ID);
    if(check != EXIT_SUCCESS)
    {
        old_mqtt_ID = NULL;
    }
    check = config_getStringArray(CONFIG_GENERAL_SETTINGS_LEVEL,
            "subscribeTopics", &old_n_sub_topics,
            &old_sub_topics);
    if(check != EXIT_SUCCESS)
    {
        old_sub_topics = NULL;
        old_n_sub_topics = 0;
    }
    //--------------------------------------
    // Read last configurations for Socket Server
    //--------------------------------------
    check = config_getBool(CONFIG_GENERAL_SETTINGS_LEVEL, 0, 
                "enableSocketServer", 0, NULL, &old_enable_server);
    if(check != EXIT_SUCCESS)
    {
        old_enable_server = false;
    }
    check = config_getString(CONFIG_GENERAL_SETTINGS_LEVEL, 0, 
            "socketServerPort", 0, NULL, &old_port);
    if(check != EXIT_SUCCESS)
    {
        old_port = NULL;
    }
    //--------------------------------------
    // JSON: Free
    //--------------------------------------
    jh_freeConfigFile(); 
    //--------------------------------------
    // Read NEW configuration file
    //--------------------------------------
    updateConfigFromFile();
    //--------------------------------------
    // Read current configurations for MQTT
    //--------------------------------------
    check = config_getBool(CONFIG_GENERAL_SETTINGS_LEVEL, 0, 
                "enableMQTT", 0, NULL, &new_enable_MQTT);
    if(check != EXIT_SUCCESS)
    {
        new_enable_MQTT = false;
    }
    check = config_getString(CONFIG_GENERAL_SETTINGS_LEVEL, 0, 
            "mqttBroker", 0, NULL, &new_mqtt_server);
    if(check != EXIT_SUCCESS)
    {
        new_mqtt_server = NULL;
    }
    check = config_getString(CONFIG_GENERAL_SETTINGS_LEVEL, 0, 
            "mqttClientID", 0, NULL, &new_mqtt_ID);
    if(check != EXIT_SUCCESS)
    {
        new_mqtt_ID = NULL;
    }
    check = config_getStringArray(CONFIG_GENERAL_SETTINGS_LEVEL,
            "subscribeTopics", &new_n_sub_topics,
            &new_sub_topics);
    if(check != EXIT_SUCCESS)
    {
        new_sub_topics = NULL;
        new_n_sub_topics = 0;
    }
    //--------------------------------------
    // Read current configurations for Socket Server
    //--------------------------------------
    check = config_getBool(CONFIG_GENERAL_SETTINGS_LEVEL, 0, 
                "enableSocketServer", 0, NULL, &new_enable_server);
    if(check != EXIT_SUCCESS)
    {
        new_enable_server = false;
    }
    check = config_getString(CONFIG_GENERAL_SETTINGS_LEVEL, 0, 
            "socketServerPort", 0, NULL, &new_port);
    if(check != EXIT_SUCCESS)
    {
        new_port = NULL;
    }   
    //--------------------------------------
    // Compare MQTT configurations
    //--------------------------------------
    temp = true;
    temp = temp && aux_compareStrings(old_mqtt_server, new_mqtt_server);
    temp = temp && aux_compareStrings(old_mqtt_ID, new_mqtt_ID);
    temp = temp && (old_enable_MQTT == new_enable_MQTT);
    temp = temp && (old_n_sub_topics == new_n_sub_topics);
    if(temp && old_n_sub_topics > 0)
    {
        for (i = 0; i < old_n_sub_topics; i++)
        {
            temp = temp && aux_compareStrings(old_sub_topics[i], 
                    new_sub_topics[i]);
        }
    }
    // Free array items
    for (i = 0; i < old_n_sub_topics; i++)
    {
        free(old_sub_topics[i]);
    }
    for (i = 0; i < new_n_sub_topics; i++)
    {
        free(new_sub_topics[i]);
    }
    free(old_sub_topics);
    free(new_sub_topics);    
    if(!temp)
    {
        *reloadMQTT = true;
    }
    else
    {
        *reloadMQTT = false;
    }
    //--------------------------------------
    // Compare Socket Server configurations
    //--------------------------------------
    temp = true;
    temp = temp && aux_compareStrings(old_port, new_port);
    temp = temp && (old_enable_server == new_enable_server);
    if(!temp)
    {
        *reload_socket_server = true;
    }
    else
    {
        *reload_socket_server = false;
    }
    #ifdef DEBUG_CONFIG_RELOAD
    debug_print("config_reload: Using new file. Reload MQTT = %d, "
            "Reload Socket Server = %d\n", *reloadMQTT, *reload_socket_server);
    #endif
    //--------------------------------------
    // Finish: free
    //--------------------------------------
    free(old_mqtt_server);
    free(new_mqtt_server);
    free(old_mqtt_ID);
    free(new_mqtt_ID);
    free(old_port);
    free(new_port);
}

int config_getBool(const char *level, int levelIndex, const char *field, 
        int fieldIndex, const char *subField, bool *value)
{
    int check;
    check = jh_getJFieldBool(level, levelIndex, field, fieldIndex, 
            subField, value);
    if( check != JSON_OK )
    {
        check = EXIT_FAILURE;
    }
    else
    {
        check = EXIT_SUCCESS;
    }
    return check;
}

int config_getDouble(const char *level, int levelIndex, const char *field, 
        int fieldIndex, const char *subField, double *value)
{
    int check;
    check = jh_getJFieldDouble(level, levelIndex, field, fieldIndex, 
            subField, value);
    if( check != JSON_OK )
    {
        check = EXIT_FAILURE;
    }
    else
    {
        check = EXIT_SUCCESS;
    }
    return check;
}

int config_getInt(const char *level, int levelIndex, const char *field, 
        int fieldIndex, const char *subField, int *value)
{
    int check;
    check = jh_getJFieldInt(level, levelIndex, field, fieldIndex, 
            subField, value);
    if( check != JSON_OK )
    {
        check = EXIT_FAILURE;
    }
    else
    {
        check = EXIT_SUCCESS;
    }
    return check;
}

int config_getString(const char *level, int levelIndex, const char *field, 
        int fieldIndex, const char *subField, char **value)
{
    int check;
    check = jh_getJFieldStringCopy(level, levelIndex, field, fieldIndex, 
            subField, value);
    if( check != JSON_OK )
    {
        check = EXIT_FAILURE;
    }
    else
    {
        check = EXIT_SUCCESS;
    }
    return check;
}

int config_getStringArray(const char *level, const char *field, 
        int *elementslen, char ***value)
{
    int check;
    check = jh_getJFieldStringArrayCopy(level, field, elementslen, value);
    if( check != JSON_OK )
    {
        check = EXIT_FAILURE;
    }
    else
    {
        check = EXIT_SUCCESS;
    }
    return check;
}