//----------------------------------------------------------------------------//
//                               OBJECT HISTORY                               //
//----------------------------------------------------------------------------//
//  REVISION |    DATE     |                               |      AUTHOR      //
//----------------------------------------------------------------------------//
//  1.00     | 28/Nov/2021 |                               | ALCP             //
// - Creation of file                                                         //
//----------------------------------------------------------------------------//

/*
* Includes
*/
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <MQTTClient.h>
#include "auxiliary.h"
#include "config.h"
#include "mqtt.h"
#include "mqttbuf.h"
#include "debug.h"

//----------------------------------------------------------------------------//
// INTERNAL DEFINITIONS
//----------------------------------------------------------------------------//
/* QOS */
#define QOS 1

//----------------------------------------------------------------------------//
// INTERNAL GLOBAL VARIABLES
//----------------------------------------------------------------------------//
volatile MQTTClient_deliveryToken deliveredtoken = 0;
volatile int mqttState = MQTT_STATE_OFF;
static MQTTClient_deliveryToken lastdeliveredtoken = 0;
static MQTTClient client;
static pthread_mutex_t m_state_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t m_close_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t m_dt_mutex = PTHREAD_MUTEX_INITIALIZER;
#ifdef DEBUG_MQTT_SENT
static unsigned long long g_mqqt_latency;
#endif

//----------------------------------------------------------------------------//
// INTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
static int getMQTTStateLocked(void);
static void setMQTTStateLocked(int li_State);
int mqtt_onMsgArrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message);
void mqtt_onConnLost(void *context, char *cause);
void mqtt_onDelivered(void *context, MQTTClient_deliveryToken dt);

/* Protected get State */
static int getMQTTStateLocked(void)
{
    int li_state;
    // LOCK STATE: Protect in case more threads try to read/set state at the same time
    pthread_mutex_lock(&m_state_mutex);
    // Read state
    li_state = mqttState;
    // UNLOCK STATE:
    pthread_mutex_unlock(&m_state_mutex);
    // Return
    return li_state;
}

/* Protected set State */
static void setMQTTStateLocked(int li_State)
{
    // LOCK STATE: Protect in case more threads try to read/set state at the same time
    pthread_mutex_lock(&m_state_mutex);
    // Set state
    mqttState = li_State;
    // UNLOCK STATE:
    pthread_mutex_unlock(&m_state_mutex);
}

/* MQTT Callback - Message Received */
int mqtt_onMsgArrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message) 
{
    // Callback to add data to buffers
    mqttbuf_subCallback(topicName, topicLen, message);       
    // Free Data
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}

/* MQTT Callback - Connection Lost */
void mqtt_onConnLost(void *context, char *cause)
{
    #if defined(DEBUG_MQTT_CONNECT) || defined(DEBUG_MQTT_ERRORS)
    debug_print("Connection lost!\n");
    debug_print("- Cause: %s\n", cause);
    #endif
    // Do not Re-Init Buffers
    mqtt_close();    
}

/* MQTT Callbak - On Message Delivered (only for QOS > 0) */
void mqtt_onDelivered(void *context, MQTTClient_deliveryToken dt)
{
    #ifdef DEBUG_MQTT_SENT
    unsigned long long timestamp;
    int latency;
    timestamp = aux_getmsSinceEpoch();
    latency = timestamp - g_mqqt_latency;
    debug_print("MQTT Confirmation Received. Latency = %d\n", latency);
    debug_print("- Token: %d\n", dt);
    #endif
    // LOCK delivery token
    pthread_mutex_lock(&m_dt_mutex);
    // Protected section - delivery token
    deliveredtoken = dt;
    // UNLOCK delivery token
    pthread_mutex_unlock(&m_dt_mutex);
}

//----------------------------------------------------------------------------//
// EXTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
/* Get State */
int mqtt_getState(void)
{
    return getMQTTStateLocked();
}

/* MQTT Initialization */
int mqtt_init(void) 
{
    int check;
    int ret;
    char *server = NULL;
    char *clientID = NULL;
    char **sub_topics = NULL;
    int n_sub_topics = 0;
    int i;
    bool ok;

    // Set state to OFF
    setMQTTStateLocked(MQTT_STATE_OFF);
    //---------------------------------
    // READ CONFIGURATION
    //---------------------------------
    check = config_getString(CONFIG_GENERAL_SETTINGS_LEVEL, 0,
            "mqttBroker", 0, NULL, &server);
    if (check != EXIT_SUCCESS) 
    {
        server = NULL;
    }
    check = config_getString(CONFIG_GENERAL_SETTINGS_LEVEL, 0,
            "mqttClientID", 0, NULL, &clientID);
    if (check != EXIT_SUCCESS) 
    {
        clientID = NULL;
    }
    check = config_getStringArray(CONFIG_GENERAL_SETTINGS_LEVEL,
            "subscribeTopics", &n_sub_topics, &sub_topics);
    if (check != EXIT_SUCCESS) 
    {
        sub_topics = NULL;
        n_sub_topics = 0;
    }
    //---------------------------------
    // VALIDATE CONFIGURATION
    //---------------------------------
    if((server == NULL) || (clientID == NULL)) 
    {
        // Free Sub Topics
        for (i = 0; i < n_sub_topics; i++)
        {
            free(sub_topics[i]);
        }
        free(sub_topics);
        // Free server
        if (server != NULL) 
        {
            free(server);
        }
        // Free client ID
        if (clientID != NULL) 
        {
            free(clientID);
        }
        #if defined(DEBUG_MQTT_CONNECT)
        debug_print("mqtt_init: Wrong Configuraion\n");
        #endif
        return EXIT_FAILURE;
    }
    //---------------------------------
    // MQTT Initialization: options  
    //---------------------------------
    MQTTClient_create(&client, server, clientID, MQTTCLIENT_PERSISTENCE_NONE, 
            NULL);
    free(server);
    server = NULL;
    free(clientID);
    clientID = NULL;
    MQTTClient_setCallbacks(client, NULL, mqtt_onConnLost, mqtt_onMsgArrvd, 
            mqtt_onDelivered);  
    //---------------------------------
    // CONNECT to Broker and subscribe
    //---------------------------------
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    conn_opts.keepAliveInterval = 30;
    conn_opts.cleansession = 1;
    conn_opts.connectTimeout = 10;
    check = MQTTClient_connect(client, &conn_opts);
    if (check != MQTTCLIENT_SUCCESS) 
    {
        #if defined(DEBUG_MQTT_CONNECT)
        debug_print("Failed to connect to MQTT Broker. Error: %d\n", check);
        #endif
        ret = EXIT_FAILURE;
        // Free Sub Topics - server and client ID are already free
        for (i = 0; i < n_sub_topics; i++)
        {
            free(sub_topics[i]);
        }
        free(sub_topics);        
        // Close and set state
        mqtt_close();
    } 
    else 
    {        
        //---------------------------------
        // Subscribe to topics and free
        //---------------------------------
        ok = true;
        for (i = 0; i < n_sub_topics; i++)
        {
            check = MQTTClient_subscribe(client, sub_topics[i], 0);
            if(check != MQTTCLIENT_SUCCESS)
            {                
                #ifdef DEBUG_MQTT_CONNECT
                debug_print("Failed to connect subscribe to topic: %s\n", 
                        sub_topics[i]);
                #endif
                ok = false;
            }            
            free(sub_topics[i]);
        }
        free(sub_topics);
        if(!ok)
        {
            ret = EXIT_FAILURE;
            // Close and set state
            mqtt_close();
        }
        else
        {
            ret = EXIT_SUCCESS;
            // Set State
            setMQTTStateLocked(MQTT_STATE_ON);   
            #if defined(DEBUG_MQTT_CONNECT) || defined(DEBUG_MQTT_CONNECTED)
            debug_print("mqtt_init: Connected to Broker!\n");
            #endif        
        }        
    }
    /* Return */
    return ret;
}

/* MQTT Close */
void mqtt_close(void)
{
    #ifdef DEBUG_MQTT_CONNECT
    debug_print("MQTT Disconnect and Free!\n");
    #endif
    // LOCK: Protect in case more threads try to close client at the same time
    pthread_mutex_lock(&m_close_mutex);
    // Disconnect and Free Memory
    MQTTClient_disconnect(client, 100);
    MQTTClient_destroy(&client);
    // UNLOCK:
    pthread_mutex_unlock(&m_close_mutex);
    // Set state
    setMQTTStateLocked(MQTT_STATE_OFF);
}

/* MQTT Message Publish
*/
void mqtt_publish(char* topic, void* payload, int payloadlen) 
{
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    // LOCK delivery token
    pthread_mutex_lock(&m_dt_mutex);
    // Init deliveredtoken with 0 (to be different from lastdeliveredtoken)
    deliveredtoken = 0;
    // For each message, increment the expected token so when the message is 
    // received we know for sure it is from the sent message
    lastdeliveredtoken++;
    // The expected token cannot be 0
    if(lastdeliveredtoken == 0)
    {
        lastdeliveredtoken = 1;
    }
    // UNLOCK delivery token
    pthread_mutex_unlock(&m_dt_mutex);
    // Init Message to be sent
    pubmsg.payload = payload;
    pubmsg.payloadlen = payloadlen;
    pubmsg.qos = QOS;
    pubmsg.retained = 0;
    // Publish
    MQTTClient_publishMessage(client, topic, &pubmsg, &lastdeliveredtoken);    
    #ifdef DEBUG_MQTT_SENT
    g_mqqt_latency = aux_getmsSinceEpoch();
    debug_print("Message Sent!\n");
    debug_print("- Topic: %s\n", topic);
    debug_print("- Waiting for Token: %d\n", lastdeliveredtoken);
    #endif     
}

/**
 * Check if last sent message was received by the broker
 */
int mqtt_wasReceivedByBroker(void)
{
    // Blocking execution: MQTTClient_waitForCompletion(client, token, 1000L);
    // Non-Blocking: Wait until deliveredtoken == token
    int i_Return;
    // LOCK delivery token
    pthread_mutex_lock(&m_dt_mutex);
    // Protected section - delivery token
    if(deliveredtoken == lastdeliveredtoken)
    {
        i_Return = MQTT_SEND_OK;
    }
    else
    {
        i_Return = MQTT_SEND_WAITING;
    }
        // UNLOCK delivery token
    pthread_mutex_unlock(&m_dt_mutex);    
    // Return
    return i_Return;
}