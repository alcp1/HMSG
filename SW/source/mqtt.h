//----------------------------------------------------------------------------//
//                               OBJECT HISTORY                               //
//----------------------------------------------------------------------------//
//  REVISION |    DATE     |                               |      AUTHOR      //
//----------------------------------------------------------------------------//
//  1.00     | 10/Dec/2021 |                               | ALCP             //
// - First version                                                            //
//----------------------------------------------------------------------------//
//  1.02     | 26/Oct/2024 |                               | ALCP             //
// - Updtates to handle connecion lost events without destroying the client   //
//----------------------------------------------------------------------------//

#ifndef MQTT_H
#define MQTT_H

#ifdef __cplusplus
extern "C" {
#endif

/*
* Includes
*/

//----------------------------------------------------------------------------//
// EXTERNAL DEFINITIONS
//----------------------------------------------------------------------------//
/* Other Definitions */
    
//-------------------------------------------------------------------------//
// EXTERNAL TYPES
//-------------------------------------------------------------------------//
#define MQTT_SEND_NO_DATA   -1
#define MQTT_SEND_WAITING   -2
#define MQTT_SEND_OK        1

#define MQTT_STATE_ON           1
#define MQTT_STATE_DISCONNECTED 0
#define MQTT_STATE_OFF          -1
    
//----------------------------------------------------------------------------//
// EXTERNAL FUNCTIONS
//----------------------------------------------------------------------------//

 /**
 * MQTT Get State
 * 
 * \return  MQTT_STATE_ON / MQTT_STATE_OFF
 */
int mqtt_getState(void);
    
    
/**
 * MQTT Initialization - Connection / Topic Subscription.
 * 
 * \return  EXIT_SUCCESS / EXIT_FAILURE
 */
int mqtt_init(void);

/**
 * MQTT Close connection: Disconnect from Server, and free memory.
 * The function mqtt_init has to be called again after this function in 
 * order to send / receive MQTT messages
 * 
 * \return  Nothing
 */
void mqtt_close(void);

/**
 * MQTT Send Data
 * \param   topic
 * \param   payload
 * \param   payloadlen
 * \return  nothing
 */
void mqtt_publish(char* topic, void* payload, int payloadlen);

/**
 * Check if last sent message was received by the broker: 
 * Only usable if QOS is set to > 0
 * 
 * \return  MQTT_SEND_OK if data was sent
 *          MQTT_SEND_WAITING if no data available to be sent
 */
int mqtt_wasReceivedByBroker(void);

#ifdef __cplusplus
}
#endif

#endif /* MQTT_H */

