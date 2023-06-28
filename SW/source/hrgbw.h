//----------------------------------------------------------------------------//
//                               OBJECT HISTORY                               //
//----------------------------------------------------------------------------//
//  REVISION |    DATE     |                               |      AUTHOR      //
//----------------------------------------------------------------------------//
//  1.00     | 19/Dec/2022 |                               | ALCP             //
// - First version                                                            //
//----------------------------------------------------------------------------//

#ifndef HRGBW_H
#define HRGBW_H

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

    
//----------------------------------------------------------------------------//
// EXTERNAL TYPES
//----------------------------------------------------------------------------//

    
//----------------------------------------------------------------------------//
// EXTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
/*
 * Add buttons configured in the configuration JSON file to the HAPCAN <--> MQTT
 * gateway
 */
void hrgbw_addToGateway(void);

/**
 * Set a payload based on the data received, and add it to the MQTT Pub Buffer.
 * \param   state_str       (INPUT) string with the MQTT State Topic
 * \param   hd_received     (INPUT) pointer to HAPCAN message received
 * \param   timestamp       (INPUT) timestamp of the CAN message
 *                      
 * \return  HAPCAN_NO_RESPONSE: No response was added to MQTT Pub Buffer
 *          HAPCAN_MQTT_RESPONSE: Response added to MQTT Buffer (OK)
 *          HAPCAN_MQTT_RESPONSE_ERROR: Error adding to MQTT Pub Buffer
 *          HAPCAN_RESPONSE_ERROR: Other error
 *          
 */
int hrgbw_setCAN2MQTTResponse(char *state_str, hapcanCANData *hd_received, 
        unsigned long long timestamp);

/**
 * Set a HAPCAN message based on the payload received, and add it to the CAN 
 * write Buffer.
 * \param   hd_result   (INPUT) matched results from the gateway
 * \param   payload     (INPUT) received MQTT Payload
 * \param   payloadlen  (INPUT) received MQTT Payload Length
 * \param   timestamp   (INPUT) timestamp of the CAN message
 *                      
 * \return  HAPCAN_NO_RESPONSE: No response was added to CAN Write Buffer
 *          HAPCAN_CAN_RESPONSE: Response added to CAN Write Buffer (OK)
 *          HAPCAN_CAN_RESPONSE_ERROR: Error adding to MQTT Pub Buffer
 *          HAPCAN_RESPONSE_ERROR: Other error
 *          
 */
int hrgbw_setMQTT2CANResponse(hapcanCANData *hd_result, void *payload, 
        int payloadlen, unsigned long long timestamp);

/**
 * To be called periodically to check and update the RGB modules status.
 * (Errors handled internally)
 *                      
 * \return  HAPCAN_GENERIC_OK_RESPONSE: Update ongoing / finished
 *          HAPCAN_CAN_RESPONSE_ERROR: Error adding to CAN Write Buffer
 *          HAPCAN_MQTT_RESPONSE_ERROR: Error adding to MQTT Pub Buffer
 *          HAPCAN_RESPONSE_ERROR: Other error         
 */
int hrgbw_periodic(void);

#ifdef __cplusplus
}
#endif

#endif