//----------------------------------------------------------------------------//
//                               OBJECT HISTORY                               //
//----------------------------------------------------------------------------//
//  REVISION |    DATE     |                               |      AUTHOR      //
//----------------------------------------------------------------------------//
//  1.00     | 10/Dec/2021 |                               | ALCP             //
// - First version                                                            //
//----------------------------------------------------------------------------//

#ifndef MQTTBUF_H
#define MQTTBUF_H

#ifdef __cplusplus
extern "C" {
#endif

#include <MQTTClient.h>
    
//----------------------------------------------------------------------------//
// EXTERNAL DEFINITIONS
//----------------------------------------------------------------------------//
#define MQTT_BUFFER_SIZE    600
enum
{
    MQTT_SUB_TOPIC_BUFFER = 0,
    MQTT_SUB_PAYLOAD_BUFFER,
    MQTT_SUB_STAMP_BUFFER,
    MQTT_PUB_TOPIC_BUFFER,
    MQTT_PUB_PAYLOAD_BUFFER,
    MQTT_PUB_STAMP_BUFFER,
    MQTT_NUMBER_OF_BUFFERS
};
#define MQTT_NUMBER_OF_SUB_BUFFERS (MQTT_SUB_STAMP_BUFFER - MQTT_SUB_TOPIC_BUFFER + 1)
#define MQTT_NUMBER_OF_PUB_BUFFERS (MQTT_PUB_STAMP_BUFFER - MQTT_PUB_TOPIC_BUFFER + 1)

// Send
#define MQTT_PUB_OK                 1
#define MQTT_PUB_NO_DATA            0
#define MQTT_PUB_BUFFER_ERROR       -1  // Re-Init and clean Buffers: mqttbuf_close(1)
#define MQTT_PUB_OTHER_ERROR        -2  // Re-Init and keep Buffers: mqttbuf_close(0)
#define MQTT_PUB_TIMEOUT_ERROR      -3  // Re-Init and keep Buffers: mqttbuf_close(0)
// Receive
#define MQTT_SUB_OK             1
#define MQTT_SUB_NO_DATA        0
#define MQTT_SUB_BUFFER_ERROR   -1  // Re-Init and clean Buffers: mqttbuf_close(1)
#define MQTT_SUB_OTHER_ERROR    -2  // Re-Init and keep Buffers: mqttbuf_close(0)

//----------------------------------------------------------------------------//
// EXTERNAL TYPES
//----------------------------------------------------------------------------//
typedef enum
{
  MQTT_DISCONNECTED,
  MQTT_CONNECTED
}mqttState_t;

//----------------------------------------------------------------------------//
// EXTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
/**
 * Buffers Initialization - Buffer Initialization.
 * \return  EXIT_SUCCESS
 *          EXIT_FAILURE (Reinit Buffers)
 */
int mqttbuf_init(void);

/**
 * MQTT and Buffers Initialization - Connection / Buffer Initialization.
 * \return  EXIT_SUCCESS
 *          EXIT_FAILURE (Close and Reinit Buffers)
 */
int mqttbuf_connect(void);

/**
 * MQTT Get State
 * 
 * \return  MQTT_CONNECTED / MQTT_DISCONNECTED
 */
mqttState_t mqttbuf_getState(void);

/**
 * MQTT Close connection: Close mqtt connection and/or re-inits buffers if needed.
 * \param   cleanBuffers    if buffers shall be reinitialized (>0 = Yes)
 * \param   close           if connection with broker shall be closed (>0 = Yes)
 * \return  EXIT_SUCCESS / EXIT_FAILURE
 */
int mqttbuf_close(int close, int cleanBuffers);

/**
 * Set Publish buffer with data from parameters.
 * 
 * \param   topic
 * \param   payload
 * \param   payloadlen
 * \param   millisecondsSinceEpoch
 * 
 * \return  MQTT_PUB_OK             if data was set to buffer
 *          MQTT_PUB_BUFFER_ERROR   if no data was set due to buffer error
 */
int mqttbuf_setPubMsgToBuffer(char* topic, void* payload, int payloadlen, unsigned long long millisecondsSinceEpoch);

/**
 * MQTT Publish Data from Buffer.
 * QOS = 1 is used, and the publish waits for a confirmation from the broker.
 * When the confirmation is receved, it return OK.
 * It will try for n retries. Each attempt will take up to timeout ms.
 * 
 * \param   retries number of retries to send message
 * \param   timout  timeout in ms for each attempt to send the message
 * 
 * \return  MQTT_PUB_OK             if data was sent
 *          MQTT_PUB_NO_DATA        if no data available to be sent
 *          MQTT_PUB_BUFFER_ERROR   if no data was sent due to buffer error
 *          MQTT_PUB_OTHER_ERROR    if no data was sent due to low layer error
 */
int mqttbuf_pubMsgFromBuffer(unsigned int retries, unsigned long timeout);

/**
 * Get Subscribed data from buffer.
 * 
 * \param   topic                   Has to be freed after used: free(topic)
 * \param   payload                 Has to be freed after used: free(payload)
 * \param   payloadlen
 * \param   millisecondsSinceEpoch
 * 
 * \return  MQTT_SUB_OK             if data was read from buffer
 *          MQTT_SUB_NO_DATA        if no data was read (no data available)
 *          MQTT_SUB_BUFFER_ERROR   if no data was read due to buffer error
 *          MQTT_SUB_OTHER_ERROR    if no data was read due to other error
 */
int mqttbuf_getSubMsgFromBuffer(char** topic, void** payload, int* payloadlen, unsigned long long* millisecondsSinceEpoch);

/**
 * Returns last error during mqtt subscription read. 
 * \return  MQTT_SUB_OK             if data was received
 *          MQTT_SUB_BUFFER_ERROR   if no data was received due to buffer error
 *          MQTT_SUB_OTHER_ERROR    if no data was received due to low layer error
 */
int mqttbuf_getSubError(void);

/**
 * MQTT callback to be called when new MQTT subscription data arrives. 
 * Data will be added to the Buffer
 * \param   topicName
 * \param   topicLen
 * \param   message
 * \return  nothing, but sets last error that can be read with mqttbuf_read
 */
void mqttbuf_subCallback(char *topicName, int topicLen, MQTTClient_message *message);

#ifdef __cplusplus
}
#endif

#endif /* MQTTBUF_H */

