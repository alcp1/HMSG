//----------------------------------------------------------------------------//
//                               OBJECT HISTORY                               //
//----------------------------------------------------------------------------//
//  REVISION |    DATE     |                               |      AUTHOR      //
//----------------------------------------------------------------------------//
//  1.00     | 10/Dec/2021 |                               | ALCP             //
// - First version                                                            //
//----------------------------------------------------------------------------//

#ifndef HAPCANSYSTEM_H
#define HAPCANSYSTEM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <linux/can.h>
#include <linux/can/raw.h>

//----------------------------------------------------------------------------//
// EXTERNAL DEFINITIONS
//----------------------------------------------------------------------------//    

    
//-------------------------------------------------------------------------//
// EXTERNAL TYPES
//-------------------------------------------------------------------------//

    
//----------------------------------------------------------------------------//
// EXTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
/*
 * Init - to be called every time the configuration changes
 */
void hsystem_init(void);

/**
 * Check if a received MQTT message is related to a system message / update 
 * request.
 * \param   topic           (INPUT) received topic
 *          payload         (INPUT) received payload
 *          payloadlen      (INPUT) received payload len
 *          timestamp       (INPUT) Received message timestamp
 *                      
 * \return  HAPCAN_GENERIC_OK_RESPONSE: System Message to be processed
 *          HAPCAN_NO_RESPONSE: Not a system message to be processed
 *          HAPCAN_RESPONSE_ERROR: Error
 *          
 */
int hsystem_checkMQTT(char* topic, void* payload, int payloadlen, 
        unsigned long long timestamp);

/**
 * Check if a received HAPCAN Frame is related to a system message.
 * \param   hd_received     (INPUT) Hapcan frame received
 *          timestamp       (INPUT) Received message timestamp
 *                      
 * \return  HAPCAN_GENERIC_OK_RESPONSE: System Message to be processed
 *          HAPCAN_NO_RESPONSE: Not a system message to be processed
 *          HAPCAN_RESPONSE_ERROR: Error
 *          
 */
int hsystem_checkCAN(hapcanCANData *hd_received, unsigned long long timestamp);

/**
 * To be called periodically to return the status of the system update.
 *                      
 * \return  HAPCAN_GENERIC_OK_RESPONSE: Update ongoing / finished
 *          HAPCAN_CAN_RESPONSE_ERROR: Error adding to CAN Write Buffer
 *          HAPCAN_MQTT_RESPONSE_ERROR: Error adding to MQTT Pub Buffer
 *          HAPCAN_RESPONSE_ERROR: Other error         
 */
int hsystem_periodic(void);

#ifdef __cplusplus
}
#endif

#endif /* HAPCAN_H */

