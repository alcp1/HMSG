//----------------------------------------------------------------------------//
//                               OBJECT HISTORY                               //
//----------------------------------------------------------------------------//
//  REVISION |    DATE     |                               |      AUTHOR      //
//----------------------------------------------------------------------------//
//  1.00     | 28/Nov/2021 |                               | ALCP             //
// - Creation of file                                                         //
//----------------------------------------------------------------------------//

#ifndef HAPCANBUTTON_H
#define HAPCANBUTTON_H

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
 * Add buttons configured in the configuration JSON file to the HAPCAN <--> MQTT
 * gateway
 */
void hbutton_addToGateway(void);

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
int hbutton_setCAN2MQTTResponse(char *state_str, hapcanCANData *hd_received, 
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
int hbutton_setMQTT2CANResponse(hapcanCANData *hd_result, void *payload, 
        int payloadlen, unsigned long long timestamp);


#ifdef __cplusplus
}
#endif

#endif /* HAPCAN_H */

