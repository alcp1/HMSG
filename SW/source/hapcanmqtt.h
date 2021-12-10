//----------------------------------------------------------------------------//
//                               OBJECT HISTORY                               //
//----------------------------------------------------------------------------//
//  REVISION |    DATE     |                               |      AUTHOR      //
//----------------------------------------------------------------------------//
//  1.00     | 28/Nov/2021 |                               | ALCP             //
// - Creation of file                                                         //
//----------------------------------------------------------------------------//

#ifndef HAPCANMQTT_H
#define HAPCANMQTT_H

#ifdef __cplusplus
extern "C" {
#endif

//----------------------------------------------------------------------------//
// EXTERNAL DEFINITIONS
//----------------------------------------------------------------------------//
    
//----------------------------------------------------------------------------//
// EXTERNAL TYPES
//----------------------------------------------------------------------------//
/**
 * Define a HAPCAN response to a raw message received by the MQTT Client
 * 
 * \param   topic           MQTT Topic (INPUT)
 * \param   payload         MQTT payload (INPUT)
 * \param   payloadlen      MQTT payload length (INPUT)
 * \param   hapcanData      HAPCAN array to be filled (OUTPUT)
 *  
 * \return  HAPCAN_NO_RESPONSE          No response needed
 *          HAPCAN_CAN_RESPONSE         CAN response filled to be send
 *          HAPCAN_RESPONSE_ERROR       No defined answer found - error
 **/
int hm_setRawResponseFromMQTT(char* topic, void* payload, int payloadlen, 
        hapcanCANData* hCD_ptr);

/**
 * Define a MQTT Generic response to a HAPCAN message received on CAN Socket
 * 
 * \param   hapcanData      HAPCAN Data revceived (INPUT)
 * \param   topic           MQTT Topic (OUTPUT)
 * \param   payload         MQTT payload (OUTPUT) 
 * \param   payloadlen      MQTT payload length (OUTPUT)
 *  
 * \return  HAPCAN_NO_RESPONSE          No response needed
 *          HAPCAN_MQTT_RESPONSE        Response filled to be send
 *          HAPCAN_RESPONSE_ERROR       No defined answer found - error
 **/
int hm_setRawResponseFromCAN(hapcanCANData* hapcanData, 
        char** topic, void** payload, int *payloadlen);

#ifdef __cplusplus
}
#endif

#endif /* HAPCAN_H */

