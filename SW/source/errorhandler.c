//----------------------------------------------------------------------------//
//                               OBJECT HISTORY                               //
//----------------------------------------------------------------------------//
//  REVISION |    DATE     |                               |      AUTHOR      //
//----------------------------------------------------------------------------//
//  1.00     | 28/Nov/2021 |                               | ALCP             //
// - Creation of file                                                         //
//----------------------------------------------------------------------------//

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "canbuf.h"
#include "debug.h"
#include "errorhandler.h"
#include "mqttbuf.h"
#include "socketserverbuf.h"

//----------------------------------------------------------------------------//
// INTERNAL DEFINITIONS
//----------------------------------------------------------------------------//

//----------------------------------------------------------------------------//
// INTERNAL GLOBAL VARIABLES
//----------------------------------------------------------------------------//

//----------------------------------------------------------------------------//
// INTERNAL FUNCTIONS
//----------------------------------------------------------------------------//

//----------------------------------------------------------------------------//
// EXTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
/*
 * Check the error response set by a module.
 */
bool errorh_isError(errorh_module_t module, int error)
{
    bool ret = false;
    switch(module)
    {
        case ERROR_MODULE_CAN_SEND:
            switch(error)
            {
                case CAN_SEND_OK:
                case CAN_SEND_NO_DATA:
                    ret = false;
                    break;                
                case CAN_SEND_BUFFER_ERROR:
                case CAN_SEND_SOCKET_ERROR:
                case CAN_SEND_PARAMETER_ERROR:    
                default:
                    // Re-Init and clean Buffers                    
                    canbuf_close(0, 1);
                    ret = true;                
                    break;
            }
            break;        
        case ERROR_MODULE_CAN_RECEIVE:
            switch(error)
            {
                case CAN_RECEIVE_OK:                    
                case CAN_RECEIVE_NO_DATA:
                    ret = false;
                    break;
                case CAN_RECEIVE_BUFFER_ERROR:
                case CAN_RECEIVE_SOCKET_ERROR:
                case CAN_RECEIVE_PARAMETER_ERROR:
                default:
                    // Re-Init and clean Buffers                    
                    canbuf_close(0, 1);
                    ret = true;
                    break;
            }
            break;        
        case ERROR_MODULE_SOCKETSERVER_SEND:
            switch(error)
            {
                case SOCKETSERVER_SEND_OK:
                case SOCKETSERVER_SEND_NO_DATA:
                    ret = false;
                    break;
                case SOCKETSERVER_SEND_BUFFER_ERROR:
                case SOCKETSERVER_SEND_SOCKET_ERROR:
                default:
                    // Re-Init and clean Buffers
                    socketserverbuf_close(1);
                    ret = true;
                    break;
            }
            break;        
        case ERROR_MODULE_SOCKETSERVER_RECEIVE:
            switch(error)
            {
                case SOCKETSERVER_RECEIVE_OK:
                case SOCKETSERVER_RECEIVE_NO_DATA:
                    ret = false;
                    break;
                case SOCKETSERVER_RECEIVE_BUFFER_ERROR:
                case SOCKETSERVER_RECEIVE_SOCKET_ERROR:
                case SOCKETSERVER_RECEIVE_CLOSED_ERROR:
                case SOCKETSERVER_RECEIVE_OVERFLOW:
                default:
                    ret = true;
                    socketserverbuf_close(1);
                    break;
            }
            break;        
        case ERROR_MODULE_MQTT_PUB:
            switch(error)
            {
                case MQTT_PUB_OK:
                case MQTT_PUB_NO_DATA:
                    ret = false;
                    break;
                case MQTT_PUB_TIMEOUT_ERROR:
                    #ifdef DEBUG_MQTT_PUBLISH_TIMEOUT
                    debug_print("ERROR: MQTT Publish Timeout - Message may be "
                            "lost!\n");
                    #endif
                    ret = false;
                    break;
                case MQTT_PUB_BUFFER_ERROR:
                case MQTT_PUB_OTHER_ERROR:                    
                default:
                    ret = true;
                    mqttbuf_close(1, 1);
                    break;
            }
            break;        
        case ERROR_MODULE_MQTT_SUB:
            switch(error)
            {
                case MQTT_SUB_OK:
                case MQTT_SUB_NO_DATA:
                    ret = false;
                    break;
                case MQTT_SUB_BUFFER_ERROR:
                case MQTT_SUB_OTHER_ERROR:
                default:
                    ret = true;
                    mqttbuf_close(1, 1);
                    break;
            }
            break;        
        default:
            ret = false;
            break;
    }
    // Return
    return ret;
}