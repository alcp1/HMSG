//----------------------------------------------------------------------------//
//                               OBJECT HISTORY                               //
//----------------------------------------------------------------------------//
//  REVISION |    DATE     |                               |      AUTHOR      //
//----------------------------------------------------------------------------//
//  1.00     | 10/Dec/2021 |                               | ALCP             //
// - First version                                                            //
//----------------------------------------------------------------------------//

#ifndef ERRORHANDLER_H
#define ERRORHANDLER_H

#ifdef __cplusplus
extern "C" {
#endif
    
#include <stdbool.h>

//----------------------------------------------------------------------------//
// EXTERNAL TYPES
//----------------------------------------------------------------------------//
// Error Type
typedef enum  
{
    ERROR_MODULE_CAN_SEND = 0,
    ERROR_MODULE_CAN_RECEIVE,
    ERROR_MODULE_SOCKETSERVER_SEND,
    ERROR_MODULE_SOCKETSERVER_RECEIVE,
    ERROR_MODULE_MQTT_PUB,
    ERROR_MODULE_MQTT_SUB    
} errorh_module_t;
    
//----------------------------------------------------------------------------//
// EXTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
/*
 * Check the error response set by a module.
 * \param   module  (INPUT) module that set the response
 * \param   error   (INPUT) error - the error response to be checked 
 * \return  TRUE: ERROR
 *          FALSE: NOT AN ERROR
 */
bool errorh_isError(errorh_module_t module, int error);

#ifdef __cplusplus
}
#endif

#endif

