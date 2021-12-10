//----------------------------------------------------------------------------//
//                               OBJECT HISTORY                               //
//----------------------------------------------------------------------------//
//  REVISION |    DATE     |                               |      AUTHOR      //
//----------------------------------------------------------------------------//
//  1.00     | 10/Dec/2021 |                               | ALCP             //
// - First version                                                            //
//----------------------------------------------------------------------------//

#ifndef CONFIG_H
#define CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif
    
/*
* Includes
*/
#include <stdbool.h>
    
//----------------------------------------------------------------------------//
// EXTERNAL DEFINITIONS
//----------------------------------------------------------------------------//
#define CONFIG_GENERAL_SETTINGS_LEVEL  "GeneralSettings"
/* Other Definitions */
#define JSON_CONFIG_FILE_PATH  "/home/pi/HMSG/SW/config.json"
    
//----------------------------------------------------------------------------//
// EXTERNAL TYPES
//----------------------------------------------------------------------------//
#define CONFIG_FILE_UPDATED     0
#define CONFIG_FILE_UNCHANGED   -1
    
//----------------------------------------------------------------------------//
// EXTERNAL FUNCTIONS
//----------------------------------------------------------------------------//

/**
 * Initial setup (only called during startup)
 **/
void config_init(void);

/**
 * For tests only: free allocated memory
 **/
void config_end(void);

/**
 * Inform if a new configuraton file is available
 * 
 * \return  true    new file available
 *          false   no new file available
 **/
int config_isNewConfigAvailable(void);

/**
 * Fill the entire configuration file with data from the available file
 * \param   reloadMQTT              (OUTPUT) Flag to signal if MQTT parameters 
 *                                  were changed, and a restart is needed.
 * \param   reload_socket_server   (OUTPUT) Flag to signal if socket parameters 
 *                                  were changed, and a restart is needed.
 * 
 **/
void config_reload(bool *reloadMQTT, bool *reload_socket_server);

/**
 * Get a boolean from a JSON object
 * 
 * \param   level       string for the level within the config file (INPUT)
 * \param   levelIndex  if the level returns an array, the element number 
 *                          "levelIndex" is considered for the Level. (INPUT)
 * \param   field       Field as a string to be searched (INPUT)
 * \param   fieldIndex  if the field returns an array, the element number 
 *                          "fieldIndex" is considered for the field. (INPUT)
 * \param   subField    if the field Index is an object, the SubField is used 
 *                          to get correct value. (INPUT)
 * \param   value   Value to be filled (OUTUT)
 *  
 * \return  EXIT_FAILURE     error / empty field
 *          EXIT_SUCCESS     OK
 **/
int config_getBool(const char *level, int levelIndex, const char *field, 
        int fieldIndex, const char *subField, bool *value);

/**
 * Get a double from a JSON object
 * 
 * \param   level       string for the level within the config file (INPUT)
 * \param   levelIndex  if the level returns an array, the element number 
 *                          "levelIndex" is considered for the Level. (INPUT)
 * \param   field       Field as a string to be searched (INPUT)
 * \param   fieldIndex  if the field returns an array, the element number 
 *                          "fieldIndex" is considered for the field. (INPUT)
 * \param   subField    if the field Index is an object, the SubField is used 
 *                          to get correct value. (INPUT)
 * \param   value   Value to be filled (OUTUT)
 * 
 * \return  EXIT_FAILURE     error / empty field
 *          EXIT_SUCCESS     OK
 **/
int config_getDouble(const char *level, int levelIndex, const char *field, 
        int fieldIndex, const char *subField, double *value);

/**
 * Get an integer from a JSON object
 * 
 * \param   level       string for the level within the config file (INPUT)
 * \param   levelIndex  if the level returns an array, the element number 
 *                          "levelIndex" is considered for the Level. (INPUT)
 * \param   field       Field as a string to be searched (INPUT)
 * \param   fieldIndex  if the field returns an array, the element number 
 *                          "fieldIndex" is considered for the field. (INPUT)
 * \param   subField    if the field Index is an object, the SubField is used 
 *                          to get correct value. (INPUT)
 * \param   value   Value to be filled (OUTUT)
 *  
 * \return  EXIT_FAILURE     error / empty field
 *          EXIT_SUCCESS     OK
 **/
int config_getInt(const char *level, int levelIndex, const char *field, 
        int fieldIndex, const char *subField, int *value);

/**
 * Get a string copy from a JSON object 
 * - STRING HAS TO BE FREED AFTERWARDS!
 * 
 * \param   level       string for the level within the config file (INPUT)
 * \param   levelIndex  if the level returns an array, the element number 
 *                          "levelIndex" is considered for the Level. (INPUT)
 * \param   field       Field as a string to be searched (INPUT)
 * \param   fieldIndex  if the field returns an array, the element number 
 *                          "fieldIndex" is considered for the field. (INPUT)
 * \param   subField    if the field Index is an object, the SubField is used 
 *                          to get correct value. (INPUT)
 * \param   value   Value to be filled (OUTUT)
 * 
 * \return  EXIT_FAILURE     error / empty field
 *          EXIT_SUCCESS     OK
 **/
int config_getString(const char *level, int levelIndex, const char *field, 
        int fieldIndex, const char *subField, char **value);

/**
 * Get a string array copy from a JSON object
 * - STRINGS HAVE TO BE FREED AFTERWARDS!
 * 
 * \param   level   string for the level within the configuraton file (INPUT)
 * \param   field   Field as a string to be searched (INPUT)
 * \param   value   Value to be filled (OUTUT)
 *  
 * \return  EXIT_FAILURE     error / empty field
 *          EXIT_SUCCESS     OK
 **/
int config_getStringArray(const char *level, const char *field, 
        int *elementslen, char ***value);


#ifdef __cplusplus
}
#endif

#endif /* CONFIG_H */

