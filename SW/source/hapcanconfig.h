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

#ifndef HAPCANCONFIG_H
#define HAPCANCONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

//----------------------------------------------------------------------------//
// EXTERNAL DEFINITIONS
//----------------------------------------------------------------------------//    
// List of modules to have their frames published
typedef struct
{
    uint8_t node;
    uint8_t group;
} rawModuleID_t;

//----------------------------------------------------------------------------//
// EXTERNAL TYPES
//----------------------------------------------------------------------------//
// HAPCAN Config
typedef enum  
{
    HAPCAN_CONFIG_COMPUTER_ID1 = 0,
    HAPCAN_CONFIG_COMPUTER_ID2,
    HAPCAN_CONFIG_ENABLE_RAW,
    HAPCAN_CONFIG_PUB_ALL,
    HAPCAN_CONFIG_RAW_PUB,
    HAPCAN_CONFIG_N_RAW_SUBS,
    HAPCAN_CONFIG_RAW_SUBS,
    HAPCAN_CONFIG_N_PUB_MODULES,
    HAPCAN_CONFIG_PUB_MODULES,
    HAPCAN_CONFIG_ENABLE_STATUS,
    HAPCAN_CONFIG_STATUS_SUB,
    HAPCAN_CONFIG_STATUS_PUB,
    HAPCAN_CONFIG_ENABLE_GATEWAY,
} hapcanConfigID;

//----------------------------------------------------------------------------//
// EXTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
/**
 * Init the configuration variables
 *  
 **/
void hconfig_init(void);

/**
 * Return the specified string configuration
 * \param   config  (INPUT) Configuration Field ID
 *          str     (OUTPUT) Field to be filled
 *                      
 * \return  EXIT_SUCCESS: OK
 *          EXIT_FAILURE: Error
 *          
 */
int hconfig_getConfigStr(hapcanConfigID config, char **str);

/**
 * Return the specified string configuration on the n-th index of the field
 * \param   config  (INPUT) Configuration Field ID
 *          i_field (INPUT) Field Index
 *          str     (OUTPUT) Field to be filled
 *                      
 * \return  EXIT_SUCCESS: OK
 *          EXIT_FAILURE: Error
 *          
 */
int hconfig_getConfigStrN(hapcanConfigID config, uint16_t i_field, char **str);

/**
 * Return the specified boolean configuration
 * \param   config  (INPUT) Configuration Field ID
 *          value   (OUTPUT) Field to be filled
 *                      
 * \return  EXIT_SUCCESS: OK
 *          EXIT_FAILURE: Error
 *          
 */
int hconfig_getConfigBool(hapcanConfigID config, bool *value);

/**
 * Return the specified integer configuration
 * \param   config  (INPUT) Configuration Field ID
 *          value   (OUTPUT) Field to be filled
 *                      
 * \return  EXIT_SUCCESS: OK
 *          EXIT_FAILURE: Error
 *          
 */
int hconfig_getConfigInt(hapcanConfigID config, int *value);

/**
 * Return the specified module ID configuration on the n-th index of the field
 * \param   config  (INPUT) Configuration Field ID
 *          i_field (INPUT) Field Index
 *          id      (OUTPUT) Field to be filled
 *                      
 * \return  EXIT_SUCCESS: OK
 *          EXIT_FAILURE: Error
 *          
 */
int hconfig_getConfigID(hapcanConfigID config, uint16_t i_field, 
    rawModuleID_t *id);

#ifdef __cplusplus
}
#endif

#endif /* HAPCANCONFIG_H */

