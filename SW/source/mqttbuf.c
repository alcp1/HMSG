//----------------------------------------------------------------------------//
//                               OBJECT HISTORY                               //
//----------------------------------------------------------------------------//
//  REVISION |    DATE     |                               |      AUTHOR      //
//----------------------------------------------------------------------------//
//  1.00     | 28/Nov/2021 |                               | ALCP             //
// - Creation of file                                                         //
//----------------------------------------------------------------------------//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include "auxiliary.h"
#include "buffer.h"
#include "mqtt.h"
#include "mqttbuf.h"
#include "debug.h"

//----------------------------------------------------------------------------//
// INTERNAL DEFINITIONS
//----------------------------------------------------------------------------//
/* Buffer Offset */
#define MQTT_PUB_BUFFER_OFFSET MQTT_PUB_TOPIC_BUFFER
#define MQTT_SUB_BUFFER_OFFSET MQTT_SUB_TOPIC_BUFFER

//----------------------------------------------------------------------------//
// INTERNAL GLOBAL VARIABLES
//----------------------------------------------------------------------------//
static int mqttbufID[MQTT_NUMBER_OF_BUFFERS] = {-1, -1, -1, -1, -1, -1};
volatile int lastSubError = MQTT_SUB_OK;
static pthread_mutex_t sub_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t pub_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t error_mutex = PTHREAD_MUTEX_INITIALIZER;

//----------------------------------------------------------------------------//
// INTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
static void mqttbuf_setLastError(int error);
static int mqttbuf_getLastError(void);
static int mqttbuf_publish(void);

/* Set Last Error set during subscription receive callback */
static void mqttbuf_setLastError(int error)
{
    // LOCK ERROR: Protect in case more threads try to read/set error at the same time
    pthread_mutex_lock(&error_mutex);
    lastSubError = error;
    // UNLOCK ERROR:
    pthread_mutex_unlock(&error_mutex);
}

/* Get Last Error set during subscription receive callback */
static int mqttbuf_getLastError(void)
{
    int ret;
    // LOCK ERROR: Protect in case more threads try to read/set 
    // error at the same time
    pthread_mutex_lock(&error_mutex);
    // Set return variable, and reset it to OK
    ret = lastSubError;
    lastSubError = MQTT_SUB_OK;
    // UNLOCK ERROR:
    pthread_mutex_unlock(&error_mutex);
    return ret;
}

/** MQTT Publish Data from Buffer */
static int mqttbuf_publish(void)
{
    char* topic = NULL;
    void* payload = NULL;
    int payloadlen;
    int li_index;
    int li_check;
    int li_return;
    int li_temp;    
    unsigned int lui_size;
    unsigned long long millisecondsSinceEpoch;    
    unsigned int bufferSize[MQTT_NUMBER_OF_PUB_BUFFERS];
    
    /**************************************************************************
     * CONSISTENCY CHECK
     *************************************************************************/
    // LOCK PUB BUFFERS: Protect data and timestamp buffers from being 
    // read/written at different times
    pthread_mutex_lock(&pub_mutex);
    // Get every Publish buffer count
    for(li_index = 0; li_index < MQTT_NUMBER_OF_PUB_BUFFERS; li_index++)
    {        
        bufferSize[li_index] = buffer_dataCount(mqttbufID[li_index + 
                MQTT_PUB_BUFFER_OFFSET]);
    }        
    // Check if every write buffer is not empty
    li_temp = 0;
    for(li_index = 0; li_index < MQTT_NUMBER_OF_PUB_BUFFERS; li_index++)
    {        
        if( bufferSize[li_index] != 0 )
        {
            li_temp = 1;
        }
    }
    if(li_temp == 0)
    {
        // No data to be sent - Unlock buffers and return now
        // UNLOCK PUB BUFFERS
        pthread_mutex_unlock(&pub_mutex);
        return MQTT_PUB_NO_DATA;
    }        
    // Check if every Publish buffer has the same count of elements
    for(li_index = 0; li_index < MQTT_NUMBER_OF_PUB_BUFFERS - 1; li_index++)
    {        
        if( bufferSize[li_index] != bufferSize[li_index + 1] )
        {            
            /***************/
            /* FATAL ERROR */
            /***************/
            #if defined(DEBUG_MQTT_SENT) || defined(DEBUG_MQTT_ERRORS)
            debug_print("MQTT: SEND Buffer ERROR!\n");
            #endif
            // Buffers out of sync - Unlock buffers and return now
            // UNLOCK PUB BUFFERS
            pthread_mutex_unlock(&pub_mutex);
            return MQTT_PUB_BUFFER_ERROR;
        }
    }    
    /**************************************************************************
     * FILL DATA - topic, payload, payloadlen, millisecondsSinceEpoch
     *************************************************************************/
    li_return = MQTT_PUB_OK;
    lui_size = buffer_popSize(mqttbufID[MQTT_PUB_TOPIC_BUFFER]);
    if(lui_size > 0)
    {
        topic = malloc(lui_size);
        li_check = buffer_pop(mqttbufID[MQTT_PUB_TOPIC_BUFFER], topic, lui_size);
        if(li_check != BUFFER_OK)
        {
            /***************/
            /* FATAL ERROR */
            /***************/
            #if defined(DEBUG_MQTT_SENT) || defined(DEBUG_MQTT_ERRORS)
            debug_print("MQTT: SEND POP Buffer ERROR!\n");
            debug_print("- Buffer ID: %d\n", mqttbufID[MQTT_PUB_TOPIC_BUFFER]);
            #endif
            // Free data allocated
            free(topic);
            topic = NULL;
            // Return
            li_return = MQTT_PUB_BUFFER_ERROR;
        }
    }
    else
    {
        /***************/
        /* FATAL ERROR */
        /***************/
        #if defined(DEBUG_MQTT_SENT) || defined(DEBUG_MQTT_ERRORS)
        debug_print("MQTT: SEND POP Buffer ERROR - Data size 0!\n");
        debug_print("- Buffer ID: %d\n", mqttbufID[MQTT_PUB_TOPIC_BUFFER]);
        #endif
        // No allocated data to be Freed
        // Return
        li_return = MQTT_PUB_BUFFER_ERROR;
    }
    payloadlen = buffer_popSize(mqttbufID[MQTT_PUB_PAYLOAD_BUFFER]);
    if(payloadlen > 0)
    {
        payload = malloc(payloadlen);
        li_check = buffer_pop(mqttbufID[MQTT_PUB_PAYLOAD_BUFFER], payload, 
                payloadlen);
        if(li_check != BUFFER_OK)
        {
            /***************/
            /* FATAL ERROR */
            /***************/
            #if defined(DEBUG_MQTT_SENT) || defined(DEBUG_MQTT_ERRORS)
            debug_print("MQTT: SEND POP Buffer ERROR!\n");
            debug_print("- Buffer ID: %d\n", 
                    mqttbufID[MQTT_PUB_PAYLOAD_BUFFER]);
            #endif
            // Free data allocated
            free(topic);
            free(payload);
            topic = NULL;
            payload = NULL;
            // Return
            li_return = MQTT_PUB_BUFFER_ERROR;
        }
    }
    else
    {
        /***************/
        /* FATAL ERROR */
        /***************/
        #if defined(DEBUG_MQTT_SENT) || defined(DEBUG_MQTT_ERRORS)
        debug_print("MQTT: SEND POP Buffer ERROR - Data size 0!\n");
        debug_print("- Buffer ID: %d\n", mqttbufID[MQTT_PUB_PAYLOAD_BUFFER]);
        #endif
        // Free data allocated
        free(topic);
        topic = NULL;
        // Return
        li_return = MQTT_PUB_BUFFER_ERROR;
    }
    // Pop timestamp to keep buffers sync
    lui_size = buffer_popSize(mqttbufID[MQTT_PUB_STAMP_BUFFER]);
    if(lui_size > 0)
    {
        li_check = buffer_pop(mqttbufID[MQTT_PUB_STAMP_BUFFER], 
                &millisecondsSinceEpoch, sizeof(millisecondsSinceEpoch));
        if((li_check != BUFFER_OK) || 
                (lui_size != sizeof(millisecondsSinceEpoch)))
        {
            /***************/
            /* FATAL ERROR */
            /***************/
            #if defined(DEBUG_MQTT_SENT) || defined(DEBUG_MQTT_ERRORS)
            debug_print("MQTT: SEND POP Buffer ERROR!\n");
            debug_print("- Buffer ID: %d\n", mqttbufID[MQTT_PUB_STAMP_BUFFER]);
            #endif
            // Free data allocated
            free(topic);
            free(payload);
            // Return
            li_return = MQTT_PUB_BUFFER_ERROR;
        }
    }
    else
    {
        /***************/
        /* FATAL ERROR */
        /***************/
        #if defined(DEBUG_MQTT_SENT) || defined(DEBUG_MQTT_ERRORS)
        debug_print("MQTT: SEND POP Buffer ERROR - Data size 0!\n");
        debug_print("- Buffer ID: %d\n", mqttbufID[MQTT_PUB_STAMP_BUFFER]);
        #endif
        // Free data allocated
        free(topic);
        free(payload);
        topic = NULL;
        payload = NULL;
        // Return
        li_return = MQTT_PUB_BUFFER_ERROR;
    }
    // UNLOCK BUFFERS:
    pthread_mutex_unlock(&pub_mutex);    
    // Check errors
    if(li_return != MQTT_PUB_OK)
    {
        return li_return;
    }    
    /**************************************************************************
     * PUBLISH
     *************************************************************************/
    mqtt_publish(topic, payload, payloadlen);
    
    /**************************************************************************
     * FREE AND RETURN
     *************************************************************************/
    // Free allocated memory
    free(topic);
    free(payload);    
    // Return
    return MQTT_PUB_OK; 
}

//----------------------------------------------------------------------------//
// EXTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
/* MQTT Get State */
mqttState_t mqttbuf_getState(void)
{
    int li_State;
    mqttState_t lms_state;
    li_State = mqtt_getState();  // This call is protected with mutex
    if(li_State == MQTT_STATE_ON)
    {
        lms_state = MQTT_CONNECTED;
    }
    else
    {
        lms_state = MQTT_DISCONNECTED;
    }
    return lms_state;
}

/** MQTT Init Buffers */
int mqttbuf_init(void)
{
    int count;
    int check;
    
    // Init buffers
    // LOCK BUFFERS: Protect data and timestamp buffers from being read/written at different times
    pthread_mutex_lock(&sub_mutex);
    pthread_mutex_lock(&pub_mutex);
    for(count = 0; count < MQTT_NUMBER_OF_BUFFERS; count++)
    {
        if(mqttbufID[count] < 0)
        {
            mqttbufID[count] = buffer_init(MQTT_BUFFER_SIZE);
        }
    }
    // UNLOCK BUFFERS:
    pthread_mutex_unlock(&sub_mutex);
    pthread_mutex_unlock(&pub_mutex);
    // Check buffers - All should have ID
    check = 0;
    for(count = 0; count < MQTT_NUMBER_OF_BUFFERS; count++)
    {
        if(mqttbufID[count] < 0)
        {
            #ifdef DEBUG_MQTT_ERRORS
            debug_print("MQTT Init: - Buffer ERROR!\n");
            debug_print("- Buffer: %d\n", count);
            #endif
            check = 1;
        }
    }
    if(check > 0)
    {
        // return error
        return EXIT_FAILURE;
    }
    else
    {
        // return OK
        return EXIT_SUCCESS;
    }
}
/** MQTT Connection */
int mqttbuf_connect(void)
{    
    int check;            
    // Connect to Broker
    check = mqtt_init();    
    // Return
    return check;
}

/** MQTT Close connection: Close mqtt connection and/or re-inits buffers 
 * if needed */
int mqttbuf_close(int close, int cleanBuffers)
{
    int li_index;
    
    // Check if close connection
    if(close > 0)
    {
        // close connection with Broker
        mqtt_close();
    }
    
    // Check if clean buffers
    if(cleanBuffers > 0)
    {
        // clean all buffers
        // LOCK BUFFERS: Protect data and timestamp buffers from being read/written at different times
        pthread_mutex_lock(&sub_mutex);
        pthread_mutex_lock(&pub_mutex);
        for(li_index = MQTT_SUB_TOPIC_BUFFER; li_index < MQTT_NUMBER_OF_BUFFERS; li_index++)
        {
            buffer_clean(mqttbufID[li_index]);
        }
        // UNLOCK BUFFERS:
        pthread_mutex_unlock(&sub_mutex);
        pthread_mutex_unlock(&pub_mutex);
    }
    
    // Return
    return EXIT_SUCCESS;
}

/** Set Publish buffer with data from parameters */
int mqttbuf_setPubMsgToBuffer(char* topic, void* payload, int payloadlen, 
        unsigned long long millisecondsSinceEpoch)
{
    int li_index;
    int li_size;
    int check[MQTT_NUMBER_OF_PUB_BUFFERS];
    // Simply add data to Publish buffers
    if(topic != NULL)
    {
        li_size = strlen(topic);
    }
    else
    {
        li_size = 0;
    }
    // Only add to buffer if connected and sizes are not 0
    if( mqttbuf_getState() == MQTT_DISCONNECTED || 
            li_size == 0 || payloadlen == 0)
    {
        return MQTT_PUB_NO_DATA;
    }
    li_index = 0;
    // LOCK PUB BUFFERS: Protect data and timestamp buffers from being 
    // read/written at different times
    pthread_mutex_lock(&pub_mutex);
    check[li_index] = buffer_push(mqttbufID[MQTT_PUB_TOPIC_BUFFER], 
            topic, li_size + 1); // +1 to get the final '\0'
    li_index++;
    check[li_index] = buffer_push(mqttbufID[MQTT_PUB_PAYLOAD_BUFFER], 
            payload, payloadlen);
    li_index++;
    check[li_index] = buffer_push(mqttbufID[MQTT_PUB_STAMP_BUFFER], 
            &millisecondsSinceEpoch, sizeof(millisecondsSinceEpoch));    
    // UNLOCK BUFFERS:
    pthread_mutex_unlock(&pub_mutex);
    /* Check for critical errors */
    for(li_index = 0; li_index < MQTT_NUMBER_OF_PUB_BUFFERS; li_index++)
    {
        if( check[li_index] != BUFFER_OK )
        {
            /***************/
            /* FATAL ERROR */
            /***************/
            #ifdef DEBUG_MQTT_ERRORS
            debug_print("MQTT: PUB Buffer ERROR!\n");
            debug_print("- Buffer ID: %d - Error = %d\n", li_index + 
                    MQTT_PUB_TOPIC_BUFFER, check[li_index]);
            #endif
            return MQTT_PUB_BUFFER_ERROR;
        }
    }
    // Here all is good
    return MQTT_PUB_OK;
}


/* PUB messages from buffer */
int mqttbuf_pubMsgFromBuffer(unsigned int retries, unsigned long timeout)
{
    int li_check;
    unsigned int lui_count;
    int m_State;
    bool keepInLoop;
    
    // Try to publish data from buffer    
    li_check = mqttbuf_publish();
    if(li_check == MQTT_PUB_OK)
    {
        li_check = MQTT_SEND_WAITING;
        lui_count = 0;
        do
        {
            // Check if QOS = 1 message was received
            li_check = mqtt_wasReceivedByBroker();             
            // Check If there is connection to broker
            m_State = mqtt_getState();
            // Increment attempts
            lui_count++;                
            // Update Loop Conditions
            keepInLoop = (li_check != MQTT_SEND_OK); // Keep while Publish is not OK
            keepInLoop = keepInLoop && (m_State == MQTT_STATE_ON);  // Keep only if connected
            keepInLoop = keepInLoop && (lui_count < retries);
            // Wait to check again
            if( keepInLoop )
            {
                // Sleep timeout
                usleep(timeout);
            }
        }
        while( keepInLoop );
        // Check if message was sent
        if(li_check != MQTT_SEND_OK)
        {
            /***************/
            /* FATAL ERROR */
            /***************/
            #ifdef DEBUG_MQTT_ERRORS
            debug_print("mqttbuf_pubMsgFromBuffer: PUBLISH ERROR (TIMEOUT) - Tried %d times!\n", lui_count);
            #endif
            return MQTT_PUB_TIMEOUT_ERROR;
        }
        else
        {
            return MQTT_PUB_OK;
        }
    }
    else
    {
        return li_check;
    }
}

/** Get Subscribed data from buffer */
int mqttbuf_getSubMsgFromBuffer(char** topic, void** payload, int* payloadlen, 
        unsigned long long* millisecondsSinceEpoch)
{
    *topic = NULL;
    *payload = NULL;
    *payloadlen = 0;
    *millisecondsSinceEpoch = 0;
    int li_index;
    int li_temp;
    int li_return;
    int li_check;
    unsigned int lui_size;
    unsigned int bufferSize[MQTT_NUMBER_OF_SUB_BUFFERS];
    
    /**************************************************************************
     * CONSISTENCY CHECK
     *************************************************************************/
    // Get every Sub buffer count
    // LOCK SUB BUFFERS: Protect data and timestamp buffers from being 
    // read/written at different times
    pthread_mutex_lock(&sub_mutex);
    for(li_index = 0; li_index < MQTT_NUMBER_OF_SUB_BUFFERS; li_index++)
    {        
        bufferSize[li_index] = buffer_dataCount(mqttbufID[li_index + 
                MQTT_SUB_BUFFER_OFFSET]);
    }    
    // Check if every write buffer is not empty
    li_temp = 0;
    for(li_index = 0; li_index < MQTT_NUMBER_OF_SUB_BUFFERS; li_index++)
    {        
        if( bufferSize[li_index] != 0 )
        {
            li_temp = 1;
        }
    }
    if(li_temp == 0)
    {
        // No data to be sent - Unlock Buffers and return now
        // UNLOCK SUB BUFFERS:
        pthread_mutex_unlock(&sub_mutex);
        return MQTT_SUB_NO_DATA;
    }        
    // Check if every buffer has the same count of elements
    for(li_index = 0; li_index < MQTT_NUMBER_OF_SUB_BUFFERS - 1; li_index++)
    {        
        if( bufferSize[li_index] != bufferSize[li_index + 1] )
        {            
            /***************/
            /* FATAL ERROR */
            /***************/
            #ifdef DEBUG_MQTT_ERRORS
            debug_print("MQTT: SUB Buffer ERROR!\n");
            #endif
            // Buffers out of sync - Unlock Buffers and return now
            // UNLOCK SUB BUFFERS:
            pthread_mutex_unlock(&sub_mutex);
            return MQTT_SUB_BUFFER_ERROR;
        }
    }    
    
    /**************************************************************************
     * FILL DATA - topic, payload, payloadlen, millisecondsSinceEpoch
     *************************************************************************/
    li_return = MQTT_SUB_OK;
    lui_size = buffer_popSize(mqttbufID[MQTT_SUB_TOPIC_BUFFER]);
    if(lui_size > 0)
    {
        *topic = malloc(lui_size);
        li_check = buffer_pop(mqttbufID[MQTT_SUB_TOPIC_BUFFER], *topic, 
                lui_size);
        if(li_check != BUFFER_OK)
        {
            /***************/
            /* FATAL ERROR */
            /***************/
            #ifdef DEBUG_MQTT_ERRORS
            debug_print("MQTT: Get SUB Buffer POP ERROR!\n");
            debug_print("- Buffer ID: %d\n", mqttbufID[MQTT_SUB_TOPIC_BUFFER]);
            #endif
            // Free data allocated
            free(*topic);
            *topic = NULL;
            // Return
            li_return = MQTT_SUB_BUFFER_ERROR;
        }
    }
    else
    {
        /***************/
        /* FATAL ERROR */
        /***************/
        #ifdef DEBUG_MQTT_ERRORS
        debug_print("MQTT: Get SUB Buffer POP ERROR!\n");
        debug_print("- Buffer ID: %d\n", mqttbufID[MQTT_SUB_TOPIC_BUFFER]);
        #endif
        // No allocated data to be Freed
        li_return = MQTT_SUB_BUFFER_ERROR;
    }
    *payloadlen = buffer_popSize(mqttbufID[MQTT_SUB_PAYLOAD_BUFFER]);
    if(*payloadlen > 0)
    {
        *payload = malloc(*payloadlen);
        li_check = buffer_pop(mqttbufID[MQTT_SUB_PAYLOAD_BUFFER], *payload, 
                *payloadlen);
        if(li_check != BUFFER_OK)
        {
            /***************/
            /* FATAL ERROR */
            /***************/
            #ifdef DEBUG_MQTT_ERRORS
            debug_print("MQTT: SEND POP Buffer ERROR!\n");
            debug_print("- Buffer ID: %d\n", 
                    mqttbufID[MQTT_SUB_PAYLOAD_BUFFER]);
            #endif
            // Free data allocated
            free(*topic);
            free(*payload);
            *topic = NULL;
            *payload = NULL;
            // Return
            li_return = MQTT_SUB_BUFFER_ERROR;
        }
    }
    else
    {
        /***************/
        /* FATAL ERROR */
        /***************/
        #ifdef DEBUG_MQTT_ERRORS
        debug_print("MQTT: SEND POP Buffer ERROR - Data size 0!\n");
        debug_print("- Buffer ID: %d\n", mqttbufID[MQTT_SUB_PAYLOAD_BUFFER]);
        #endif
        // Free data allocated
        free(*topic);
        *topic = NULL;
        // Return
        li_return = MQTT_SUB_BUFFER_ERROR;
    }
    // Pop timestamp to keep buffers sync
    lui_size = buffer_popSize(mqttbufID[MQTT_SUB_STAMP_BUFFER]);
    if(lui_size > 0)
    {
        li_check = buffer_pop(mqttbufID[MQTT_SUB_STAMP_BUFFER], 
                millisecondsSinceEpoch, sizeof(*millisecondsSinceEpoch));
        if((li_check != BUFFER_OK) || 
                (lui_size != sizeof(*millisecondsSinceEpoch)))
        {
            /***************/
            /* FATAL ERROR */
            /***************/
            #ifdef DEBUG_MQTT_ERRORS
            debug_print("MQTT: SEND POP Buffer ERROR!\n");
            debug_print("- Buffer ID: %d\n", mqttbufID[MQTT_SUB_STAMP_BUFFER]);
            #endif
            // Free data allocated
            free(*topic);
            free(*payload);
            *topic = NULL;
            *payload = NULL;
            // Return
            li_return = MQTT_SUB_BUFFER_ERROR;
        }
    }
    else
    {
        /***************/
        /* FATAL ERROR */
        /***************/
        #ifdef DEBUG_MQTT_ERRORS
        debug_print("MQTT: SEND POP Buffer ERROR - Data size 0!\n");
        debug_print("- Buffer ID: %d\n", mqttbufID[MQTT_SUB_STAMP_BUFFER]);
        #endif
        // Free data allocated
        free(*topic);
        free(*payload);
        *topic = NULL;
        *payload = NULL;
        // Return
        li_return = MQTT_SUB_BUFFER_ERROR;
    }
    // UNLOCK BUFFERS:
    pthread_mutex_unlock(&sub_mutex);    
    // return
    return li_return;
}

/** Returns last error during MQTT subscription received message */
int mqttbuf_getSubError(void)
{
    int ret;
    ret = mqttbuf_getLastError();
    return ret;
}

/** MQTT callback to be called when new MQTT subscription data arrives */
void mqttbuf_subCallback(char *topicName, int topicLen, 
        MQTTClient_message *message)
{
    unsigned long long millisecondsSinceEpoch;
    int check[MQTT_NUMBER_OF_SUB_BUFFERS];
    int li_index;
    int li_length;
    
    // Get Timestamp
    millisecondsSinceEpoch = aux_getmsSinceEpoch();

    #ifdef DEBUG_MQTT_RECEIVED
    debug_print("Message Received! \n");
    debug_print("- Topic: %s\n", topicName);
    debug_print("- Message: %s\n", message->payload);
    debug_print("- Topic Length: %d\n", topicLen);
    debug_print("- Message Length: %d\n", message->payloadlen);
    #endif

    /* Copy Message to Buffers */
    if(topicLen <= 0)
    {
        if(topicName != NULL)
        {
            li_length = strlen(topicName);
        }
        else
        {
            li_length = 0;
        }
    }
    else
    {
        li_length = topicLen;
    }
    
    // Check if data is OK. If not, Ignore the message
    if((li_length > 0) && (message->payloadlen > 0))
    {
        // LOCK BUFFERS: Protect data and timestamp buffers from being read/written at different times
        pthread_mutex_lock(&sub_mutex);
        check[MQTT_SUB_TOPIC_BUFFER] = buffer_push(mqttbufID[MQTT_SUB_TOPIC_BUFFER], topicName, li_length + 1); // +1 to get the final '\0'
        check[MQTT_SUB_PAYLOAD_BUFFER] = buffer_push(mqttbufID[MQTT_SUB_PAYLOAD_BUFFER], message->payload, message->payloadlen);
        check[MQTT_SUB_STAMP_BUFFER] = buffer_push(mqttbufID[MQTT_SUB_STAMP_BUFFER], &millisecondsSinceEpoch, sizeof(millisecondsSinceEpoch));
        // UNLOCK BUFFERS:
        pthread_mutex_unlock(&sub_mutex);
        /* Check for critical errors */
        for(li_index = 0; li_index < MQTT_NUMBER_OF_SUB_BUFFERS; li_index++)
        {
            if( check[li_index] != BUFFER_OK )
            {
                /***************/
                /* FATAL ERROR */
                /***************/
                #ifdef DEBUG_MQTT_ERRORS
                debug_print("MQTT: RECEIVE Buffer ERROR!\n");
                debug_print("- Buffer ID: %d\n", li_index);
                #endif
                // Set error and return
                mqttbuf_setLastError(MQTT_SUB_BUFFER_ERROR);
                return;
            }
        }
    }
    else
    {
        #ifdef DEBUG_MQTT_ERRORS
        debug_print("MQTT: RECEIVE Message ERROR!\n");
        debug_print("- Topic Length: %d\n", li_length);
        debug_print("- Message Length: %d\n", message->payloadlen);
        #endif
        // Set error and return
        mqttbuf_setLastError(MQTT_SUB_OTHER_ERROR);
        return;
    }
    
    // Here everything is OK
    mqttbuf_setLastError(MQTT_SUB_OK);
}
