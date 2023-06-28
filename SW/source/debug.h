//----------------------------------------------------------------------------//
//                               OBJECT HISTORY                               //
//----------------------------------------------------------------------------//
//  REVISION |    DATE     |                               |      AUTHOR      //
//----------------------------------------------------------------------------//
//  1.00     | 10/Dec/2021 |                               | ALCP             //
// - First version                                                            //
//----------------------------------------------------------------------------//
//  1.01     | 19/Dec/2022 |                               | ALCP             //
// - Add new errors (HTIM and HRGBW)                                          //
//----------------------------------------------------------------------------//
//  1.02     | 27/Jun/2023 |                               | ALCP             //
// - Add Build Version Debug                                                  //
//----------------------------------------------------------------------------//

#ifndef DEBUG_H
//#define DEBUG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include "hapcan.h"
    
//----------------------------------------------------------------------------//
// EXTERNAL DEFINITIONS
//----------------------------------------------------------------------------//
/* Debug */ 
#define DEBUG_ON
#define DEBUG_VERSION

/* MQTT - Only Events */
#define DEBUG_MQTT_ERRORS
#define DEBUG_MQTT_CONNECTED
#define DEBUG_MQTT_PUBLISH_TIMEOUT
//#define DEBUG_MQTT_CONNECT
//#define DEBUG_MQTT_RECEIVED // Disable for production
//#define DEBUG_MQTT_SENT // Disable for production
    
/* SocketCAN */
#define DEBUG_SOCKETCAN_ERROR
#define DEBUG_SOCKETCAN_OPENED
//#define DEBUG_SOCKETCAN_OPEN // Disable for production
//#define DEBUG_SOCKETCAN_READ_FULL // Disable for production
//#define DEBUG_SOCKETCAN_READ_EVENTS // Disable for production
//#define DEBUG_SOCKETCAN_WRITE // Disable for production

/* Buffer */
//#define DEBUG_BUFFER
    
/* Manager */
#define DEBUG_MANAGER_ERRORS
#define DEBUG_MANAGER_CONFIG_EVENTS
    
/* CAN Buffer */
#define DEBUG_CANBUF_ERRORS
//#define DEBUG_CANBUF_SEND // Disable for production


/* Socket Server Buffer */
#define DEBUG_SOCKETSERVERBUF_ERRORS
//#define DEBUG_SOCKETSERVERBUF_SEND // Disable for production
    
/* Socket Server */
#define DEBUG_SOCKETSERVER_ERROR
#define DEBUG_SOCKETSERVER_OPENED
//#define DEBUG_SOCKETSERVER_PROCESS_ERROR // Disable for production
//#define DEBUG_SOCKETSERVER_OPEN // Disable for production
//#define DEBUG_SOCKETSERVER_READ_FULL // Disable for production
//#define DEBUG_SOCKETSERVER_READ_EVENTS // Disable for production
//#define DEBUG_SOCKETSERVER_WRITE // Disable for production

/* CAN DEBUG */   
#define DEBUG_CAN_HAPCAN
//#define DEBUG_CAN_STANDARD // Disable for production
    
/* HAPCAN DEBUG */   
#define DEBUG_HAPCAN_ERRORS
#define DEBUG_HAPCAN_RELAY_ERRORS
#define DEBUG_HAPCAN_BUTTON_ERRORS
#define DEBUG_HAPCAN_TEMPERATURE_ERRORS
#define DEBUG_HAPCAN_RGB_ERRORS
#define DEBUG_HAPCAN_SYSTEM_ERRORS
#define DEBUG_RGBW_ERRORS
#define DEBUG_TIM_ERRORS
//#define DEBUG_RGBW_FULL // Disable for production
//#define DEBUG_HAPCAN_CAN2MQTT // Disable for production
//#define DEBUG_HAPCAN_MQTT2CAN // Disable for production
    
/* CONFIG DEBUG */
#define DEBUG_CONFIG_ERRORS
#define DEBUG_CONFIG_RELOAD
//#define DEBUG_CONFIG_FULL // Disable for production
    
/* JSON DEBUG */
//#define DEBUG_JSON_ERRORS // Disable for production
//#define DEBUG_JSON_FULL // Disable for production

/* GATEWAY DEBUG */
#define DEBUG_GATEWAY_ERRORS
//#define DEBUG_GATEWAY_PRINT // Disable for production
//#define DEBUG_GATEWAY_LISTS // Disable for production 
//#define DEBUG_GATEWAY_SEARCH // Disable for production
    
//----------------------------------------------------------------------------//
// EXTERNAL TYPES
//----------------------------------------------------------------------------//

//----------------------------------------------------------------------------//
// EXTERNAL CONSTANTS
//----------------------------------------------------------------------------//

//----------------------------------------------------------------------------//
// EXTERNAL GLOBAL VARIABLES
//----------------------------------------------------------------------------//

//----------------------------------------------------------------------------//
// EXTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
/**
 * Debug Initialization
 * 
 * \param   None
 * \return  EXIT_SUCCESS / EXIT_FAILURE
 */
int debug_init(void);

/**
 * Debug End
 * 
 * \param   None
 * \return  EXIT_SUCCESS / EXIT_FAILURE
 */
int debug_end(void);

/**
 * Debug Print
 * 
 * \param   same as printf
 * \return  none
 */
void debug_print(const char * format, ...);

/**
 * Debug Print CAN Frame
 * 
 * \param   same as printf
 * \return  none
 */
void debug_printCAN(const char * text, struct can_frame* const pcf_Frame);

/**
 * Debug Print HAPCAN Frame
 * 
 * \param   same as printf
 * \return  none
 */
void debug_printHAPCAN(const char * text, hapcanCANData *hd);

/**
 * Debug Print HAPCAN Socket Frame
 * 
 * \param   same as printf
 * \return  none
 */
void debug_printSocket(const char * text, uint8_t* data, int dataLen);

#ifdef __cplusplus
}
#endif

#endif /* DEBUG_H */

