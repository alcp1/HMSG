//----------------------------------------------------------------------------//
//                               OBJECT HISTORY                               //
//----------------------------------------------------------------------------//
//  REVISION |    DATE     |                               |      AUTHOR      //
//----------------------------------------------------------------------------//
//  1.00     | 10/Dec/2021 |                               | ALCP             //
// - First version                                                            //
//----------------------------------------------------------------------------//
//  1.01     | 18/Jun/2023 |                               | ALCP             //
// - Add new frame types (HTIM and HRGBW)                                     //
//----------------------------------------------------------------------------//
//  1.02     | 27/Jun/2023 |                               | ALCP             //
// - Add Build Version Debug                                                  //
//----------------------------------------------------------------------------//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include "auxiliary.h"
#include "buffer.h"
#include "canbuf.h"
#include "config.h"
#include "debug.h"
#include "errorhandler.h"
#include "gateway.h"
#include "hapcan.h"
#include "hapcanconfig.h"
#include "hapcanmqtt.h"
#include "hapcanrgb.h"
#include "hrgbw.h"
#include "hapcansocket.h"
#include "hapcansystem.h"
#include "manager.h"
#include "mqttbuf.h"
#include "socketserverbuf.h"

//----------------------------------------------------------------------------//
// INTERNAL DEFINITIONS
//----------------------------------------------------------------------------//
#define NUMBER_OF_THREADS   14
#define NUMBER_OF_BUFFERS   MQTT_NUMBER_OF_BUFFERS + SOCKETSERVER_NUMBER_OF_BUFFERS + CAN_NUMBER_OF_BUFFERS // Use SOCKETCAN_CHANNELS*CAN_NUMBER_OF_BUFFERS if more than one CAN channel is used
#define INIT_RETRIES    5

//----------------------------------------------------------------------------//
// INTERNAL TYPES
//----------------------------------------------------------------------------//
typedef void* (*vp_thread_t)( void *arg );

//----------------------------------------------------------------------------//
// INTERNAL FUNCTIONS - THREADS
//----------------------------------------------------------------------------//
void* managerHandleMQTTConn(void *arg);
void* managerHandleMQTTSub(void *arg);
void* managerHandleMQTTPub(void *arg);
void* managerHandleCAN0Conn(void *arg);
void* managerHandleCAN0Read(void *arg);
void* managerHandleCAN0Write(void *arg);
void* managerHandleCAN0Buffers(void *arg);
void* managerHandleSocketServerConn(void *arg);
void* managerHandleSocketServerRead(void *arg);
void* managerHandleSocketServerWrite(void *arg);
void* managerHandleSocketServerBuffers(void *arg);
void* managerHandleHAPCANRTCEvents(void *arg);
void* managerHandleHAPCANPeriodic(void *arg);
void* managerHandleConfigFile(void *arg);

//----------------------------------------------------------------------------//
// INTERNAL GLOBAL VARIABLES
//----------------------------------------------------------------------------//
pthread_t pt_threadID[NUMBER_OF_THREADS];
vp_thread_t vp_thread[NUMBER_OF_THREADS] = 
{   managerHandleMQTTConn, 
    managerHandleMQTTSub, 
    managerHandleMQTTPub, 
    managerHandleCAN0Conn,              // Manage Socket connection for CAN0
    managerHandleCAN0Read,              // Fill the CAN0 Buffer IN (Read)
    managerHandleCAN0Write,             // Fill the CAN0 Buffer OUT (Write)
    managerHandleCAN0Buffers,           // Manage CAN0 Buffers
    managerHandleSocketServerConn,      // Manage Socket Client Connections
    managerHandleSocketServerRead,      // Fill the Socket Server Read Buffer
    managerHandleSocketServerWrite,     // Send from Socket Server Write Buffer
    managerHandleSocketServerBuffers,   // Manage Socket Server Buffers
    managerHandleHAPCANRTCEvents,       // Manage RTC messages
    managerHandleHAPCANPeriodic,        // Manage Periodic events (System)
    managerHandleConfigFile};           // Handle Config File Updates

//----------------------------------------------------------------------------//
// INTERNAL FUNCTIONS - AUXILIARY
//----------------------------------------------------------------------------//


//----------------------------------------------------------------------------//
// INTERNAL FUNCTIONS - THREADS
//----------------------------------------------------------------------------//
/* THREAD - Handle MQTT Connection */
void* managerHandleMQTTConn(void *arg)
{
    int check;
    bool enable;
    mqttState_t state;
    while(1)
    {
        // Check if this feature is enabled
        check = config_getBool(CONFIG_GENERAL_SETTINGS_LEVEL, 0, 
                "enableMQTT", 0, NULL, &enable);
        if(check != EXIT_SUCCESS)
        {
            enable = false;
        }
        // Check current state
        state = mqttbuf_getState();
        // Define connection / disconnection attempt based on enable and status
        if(enable)
        {
            /* When the feature is enabled, but there is no connection, 
             * try to connect */
            if(state != MQTT_CONNECTED)
            {
                mqttbuf_connect(); 
            }
        }        
        // 1 second loop to check MQTT connection
        sleep(1);
    }
}

/* THREAD - Handle MQTT SUB MESSAGES */
void* managerHandleMQTTSub(void *arg)
{
    int check;
    int payloadlen;
    char* topic = NULL;
    void* payload = NULL;
    unsigned long long timestamp;
    bool b_retry = false;
    while(1)
    {
        // Check for Errors when receiving MQTT message (set inside MQTT Sub 
        // callback)
        check = mqttbuf_getSubError();
        // Handle the error set when receiving MQTT message
        errorh_isError(ERROR_MODULE_MQTT_SUB, check);        
        /* STATE CHECK
         * Check MQTT State 
         */
        b_retry = true;
        if(mqttbuf_getState() == MQTT_CONNECTED)
        {
            b_retry = true; 
            while(b_retry)
            {                    
                // Check if a message was receive (buffer not empty)
                check = mqttbuf_getSubMsgFromBuffer(&topic, &payload, 
                        &payloadlen, &timestamp);
                // Check and handle the error
                b_retry = !errorh_isError(ERROR_MODULE_MQTT_SUB, check);
                // Stay in loop if a message was just read successfully
                b_retry = b_retry && (check == MQTT_SUB_OK);
                if(b_retry)
                {                    
                    //-------------------------------------------------
                    // Process MQTT Message and send CAN response
                    //-------------------------------------------------
                    // Error is handled within the function
                    hapcan_handleMQTT2CAN(topic, payload, payloadlen, 
                            timestamp);
                }
                // FREE DATA
                free(topic);
                topic = NULL;
                free(payload);
                payload = NULL;                
            }            
            // 2ms delay after sending all messages while connected
            usleep(2000);
        }
        else
        {
            // 5ms loop to check for new messages to be sent 
            // when not connected
            usleep(5000);
        }
    }
}

/* THREAD - Handle MQTT PUB MESSAGES */
void* managerHandleMQTTPub(void *arg)
{
    int check;
    bool b_retry;    
    while(1)
    {
        /* STATE CHECK
         * Check MQTT State 
         */        
        if( mqttbuf_getState() == MQTT_CONNECTED)
        {
            b_retry = true;
            while(b_retry)
            {
                // 200 retries, 1ms each - for wired connections, the latency 
                // should be between 0ms and 2ms
                check = mqttbuf_pubMsgFromBuffer(200, 1000);
                // Check and handle the error
                b_retry = !errorh_isError(ERROR_MODULE_MQTT_PUB, check);
                // Stay in loop if a message was just sent successfully
                b_retry = b_retry && (check == MQTT_PUB_OK);
            }
            // 2ms delay after sending all messages while connected
            usleep(2000);
        }
        else
        {
            // 5ms loop to check for new messages to be sent 
            // when not connected
            usleep(5000);
        }
    }
}

/* THREAD - Handle CAN0 Connection */
void* managerHandleCAN0Conn(void *arg)
{
    const int channel = 0;
    int check;
    stateCAN_t sc_state;
    while(1)
    {
        /* STATE CHECK AND RE-INIT */
        check = canbuf_getState(channel, &sc_state);
        if( check == EXIT_SUCCESS )
        {
            if(sc_state != CAN_CONNECTED)
            {
                canbuf_connect(channel);
            }
        }
        // 1 second loop to check CAN Bus 
        sleep(1);        
    }
}

/* THREAD - Handle CAN0 Reads */
void* managerHandleCAN0Read(void *arg)
{
    const int channel = 0;
    int check;
    stateCAN_t sc_state;
    bool b_retry;
    while(1)
    {
        /* STATE CHECK AND RE-INIT */
        check = canbuf_getState(channel, &sc_state);
        if( check == EXIT_SUCCESS )
        {
            if(sc_state == CAN_CONNECTED)
            {
                b_retry = true; 
                while(b_retry)
                {                    
                    // Check with 5ms timeout
                    check = canbuf_receive(channel, 5000);
                    // Check and handle the error
                    b_retry = !errorh_isError(ERROR_MODULE_CAN_RECEIVE, check);
                    // Stay in loop if a message was just read successfully
                    b_retry = b_retry && (check == CAN_RECEIVE_OK);                    
                }
                // 2ms delay after reading all messages while connected
                usleep(2000);
            }
            else
            {
                // 5ms loop to check for new messages to be read 
                // when not connected
                usleep(5000);
            }            
        }        
    }
}

/* THREAD - Handle CAN0 Writes */
void* managerHandleCAN0Write(void *arg)
{
    const int channel = 0;
    int check;
    stateCAN_t sc_state;
    bool b_retry;
    while(1)
    {
        /* STATE CHECK AND RE-INIT
         */
        check = canbuf_getState(channel, &sc_state);
        if( check == EXIT_SUCCESS )
        {
            if(sc_state == CAN_CONNECTED)
            {
                b_retry = true;
                while(b_retry)
                {
                    // Send CAN Write Buffer message
                    check = canbuf_send(channel);
                    // Check and handle the error
                    b_retry = !errorh_isError(ERROR_MODULE_CAN_SEND, check);
                    // Stay in loop if a message was just sent successfully
                    b_retry = b_retry && (check == CAN_SEND_OK);
                }
                // 2ms delay after sending all messages while connected
                usleep(2000);
            }
            else
            {
                // 5ms loop to check for new messages to be sent 
                // when not connected
                usleep(5000);
            }            
        }        
    }
}

/* THREAD - Handle CAN0 Buffers */
void* managerHandleCAN0Buffers(void *arg)
{
    const int channel = 0;
    int check;
    stateCAN_t sc_state;
    struct can_frame cf_Frame;
    hapcanCANData hapcanData;
    int dataLen;
    unsigned long long timestamp;
    uint8_t data[HAPCAN_SOCKET_DATA_LEN];
    bool b_retry;
    while(1)
    {
        // STATE CHECK AND RE-INIT
        check = canbuf_getState(channel, &sc_state);
        if( check == EXIT_SUCCESS )
        {
            if(sc_state == CAN_CONNECTED)
            {
                b_retry = true;
                while(b_retry)
                {
                    // Get data from CAN IN (Read) Buffer
                    check = canbuf_getReadMsgFromBuffer(channel, &cf_Frame, 
                            &timestamp);
                    // Check and handle the error
                    b_retry = !errorh_isError(ERROR_MODULE_CAN_RECEIVE, check);
                    // Stay in loop if a message was just read successfully
                    b_retry = b_retry && (check == CAN_RECEIVE_OK);                    
                    if(b_retry)
                    {
                        //------------------------------------------
                        // Message is OK to be sent to the Socket
                        //------------------------------------------
                        hapcan_getHAPCANDataFromCAN(&cf_Frame, &hapcanData);
                        hs_getSocketArrayFromHAPCAN(&hapcanData, data);
                        dataLen = HAPCAN_SOCKET_DATA_LEN;
                        check = socketserverbuf_setWriteMsgToBuffer(data, 
                                dataLen, timestamp);
                        // Handle the error
                        errorh_isError(ERROR_MODULE_SOCKETSERVER_SEND, check);
                        //-------------------------------------------------
                        // Process CAN Message and send MQTT response    
                        //-------------------------------------------------
                        // Error in handled within the function
                        hapcan_handleCAN2MQTT(&hapcanData, timestamp);                        
                    }
                }
                // 2ms loop after empty buffer
                usleep(2000); 
            }
            else
            {
                // 5ms Loop to check for buffers
                usleep(5000);
            }            
        }        
    }
}

/* THREAD - Handle Socket Server Connection */
void* managerHandleSocketServerConn(void *arg)
{
    int check;
    bool enable;
    stateSocketServer_t ss_state;
    while(1)
    {
        // Check if this feature is enabled
        check = config_getBool(CONFIG_GENERAL_SETTINGS_LEVEL, 0, 
                "enableSocketServer", 0, NULL, &enable);
        if(check != EXIT_SUCCESS)
        {
            enable = false;
        }
        // Check current state
        check = socketserverbuf_getState(&ss_state);
        if( check == EXIT_SUCCESS )
        {
            if(enable)
            {
                /* When the feature is enabled, but there is no connection, 
                 * try to connect */
                if(ss_state != SOCKETSERVER_CONNECTED)
                {
                    // Wait 5 seconds
                    socketserverbuf_connect(5000); 
                }
            }            
        }
        // 1 second loop to check Socket client connection again
        sleep(1); 
    }
}

/* THREAD - Handle Socket Server Reads */
void* managerHandleSocketServerRead(void *arg)
{
    int check;
    stateSocketServer_t ss_state;
    bool b_retry;
    while(1)
    {
        /* STATE CHECK AND RE-INIT */
        check = socketserverbuf_getState(&ss_state);
        if( check == EXIT_SUCCESS )
        {
            if(ss_state == SOCKETSERVER_CONNECTED)
            {
                b_retry = true;
                while(b_retry)
                {
                    // Check with 5ms timeout
                    check = socketserverbuf_receive(5000);
                    // Check and handle the error
                    b_retry = !errorh_isError(ERROR_MODULE_SOCKETSERVER_RECEIVE, 
                            check);
                    // Stay in loop if a message was just sent successfully
                    b_retry = b_retry && (check == SOCKETSERVER_RECEIVE_OK);
                }
                // 2ms delay after reading all messages while connected
                usleep(2000);
            }
            else
            {
                // 5ms loop to check for new messages to be read 
                // when not connected
                usleep(5000);
            }            
        }        
    }    
}

/* THREAD - Handle Socket Server Writes */
void* managerHandleSocketServerWrite(void *arg)
{
    int check;
    stateSocketServer_t ss_state;
    bool b_retry;
    while(1)
    {
        /* STATE CHECK AND RE-INIT */
        check = socketserverbuf_getState(&ss_state);
        if( check == EXIT_SUCCESS )
        {
            if(ss_state == SOCKETSERVER_CONNECTED)
            {
                b_retry = true;
                while(b_retry)
                {
                    check = socketserverbuf_send();
                    b_retry = !errorh_isError(ERROR_MODULE_SOCKETSERVER_SEND, 
                            check);
                    // Stay in loop if a message was just sent successfully
                    b_retry = b_retry && (check == SOCKETSERVER_SEND_OK);                    
                }
                // 2ms delay after sending all messages while connected
                usleep(2000);
            }
            else
            {
                // 5ms loop to check for new messages to be sent 
                // when not connected
                usleep(5000);
            }            
        }        
    }
}

void* managerHandleSocketServerBuffers(void *arg)
{
    int check;
    stateSocketServer_t ss_state;
    uint8_t data[HAPCAN_SOCKET_DATA_LEN];   // Data from Socket Read Buffer
    int dataLen;    // Data from Socket Read Buffer
    unsigned long long timestamp;
    bool b_retry;
    while(1)
    {
        /* STATE CHECK AND RE-INIT */
        check = socketserverbuf_getState(&ss_state);
        if( check == EXIT_SUCCESS )
        {
            if(ss_state == SOCKETSERVER_CONNECTED)
            {
                b_retry = true;
                while(b_retry)
                {
                    // Get data from Socket Server Buffer
                    check = socketserverbuf_getReadMsgFromBuffer(data, &dataLen, 
                            &timestamp);
                    // Check and handle the error
                    b_retry = !errorh_isError(ERROR_MODULE_SOCKETSERVER_RECEIVE, 
                            check);
                    // Stay in loop if a message was read from buffer
                    b_retry = b_retry && (check == SOCKETSERVER_RECEIVE_OK);                    
                    if(b_retry)
                    {                                
                        // Handle Messages - Send response to client 
                        // (PROGRAMMER) or a CAN Frame to Bus
                        // Error is handled within the function
                        hs_handleMsgFromSocket(data, dataLen, timestamp);
                    }
                }
                // 2ms delay after processing all messages while connected
                usleep(2000);
            }
            else
            {
                // 5ms loop to check for new messages to be processed 
                // when not connected
                usleep(5000);
            }            
        }        
    }    
}

void* managerHandleHAPCANRTCEvents(void *arg)
{
    int check;
    bool enable;
    stateCAN_t sc_state;
    hapcanCANData hapcanData;
    unsigned long long timestamp;
    int temp;
    
    // First sync the thread to run when seconds are zero.
    temp = aux_getTimeUntilZeroSeconds();
    sleep(temp + 1);
    while(1)
    {
        // Check if this feature is enabled:
        check = config_getBool(CONFIG_GENERAL_SETTINGS_LEVEL, 0, 
                "enableRTCFrame", 0, NULL, &enable);
        if(check != EXIT_SUCCESS)
        {
            enable = false;
        }
        if(enable)
        {
            // check for valid Local time
            temp = aux_getLocalYear();
            if(temp > 100) // After year 2000
            {
                /* STATE CHECK AND RE-INIT */
                check = canbuf_getState(0, &sc_state);
                if( check == EXIT_SUCCESS )
                {
                    if(sc_state == CAN_CONNECTED)
                    {
                        // Get Timestamp
                        timestamp = aux_getmsSinceEpoch();
                        // FILL RTC MESSAGE
                        hapcan_setHAPCANRTCMessage(&hapcanData);  
                        // Send CAN FRame - Error is handled within the function
                        hapcan_addToCANWriteBuffer(&hapcanData, timestamp, 
                                true);                        
                    }                    
                }
            }
        }
        temp = aux_getTimeUntilZeroSeconds();
        sleep(temp + 1);        
    }
}

void* managerHandleHAPCANPeriodic(void *arg)
{
    int check;
    bool enable;
    stateCAN_t sc_state;
    while(1)
    {
        // Check if this feature is enabled:
        check = hconfig_getConfigBool(HAPCAN_CONFIG_ENABLE_STATUS, &enable);
        if(check != EXIT_SUCCESS)
        {
            enable = false;
        }
        if(enable)
        {
            /* STATE CHECK AND RE-INIT */
            check = canbuf_getState(0, &sc_state);
            if( check == EXIT_SUCCESS )
            {
                if(sc_state == CAN_CONNECTED)
                {
                    //----------------------------------------------------------
                    // Check for messages to be sent to CAN Bus or 
                    // MQTT responses to be sent when updating the module 
                    // information, or getting its status
                    //----------------------------------------------------------
                    // Error is handled within the functions
                    hsystem_periodic();
                    hrgb_periodic();
                    hrgbw_periodic();
                }                
            }
        }
        // 50ms Loop - Give time for the modules to respond and to not increase
        // Bus load too much (some modules send 17 CAN messages for its status)
        usleep(50000);
    }
}

void* managerHandleConfigFile(void *arg)
{    
    bool reloadMQTT;
    bool reload_socket_server;
    while(1)
    {
        if(config_isNewConfigAvailable())
        {
            #ifdef DEBUG_MANAGER_CONFIG_EVENTS
            debug_print("managerHandleConfigFile - New config available!\n");
            #endif
            config_reload(&reloadMQTT, &reload_socket_server);
            // If configuration changed, close connections
            if(reloadMQTT)
            {
                mqttbuf_close(1, 1);
            }
            if(reload_socket_server)
            {
                socketserverbuf_close(1);
            }
            // For every new configuration file, reload the gateway
            gateway_init();
            hapcan_initGateway();
            hsystem_init();
            // Print Gateway
            #ifdef DEBUG_GATEWAY_LISTS
            gateway_printList(GATEWAY_MQTT2CAN_LIST);
            gateway_printList(GATEWAY_CAN2MQTT_LIST);
            #endif
        }
        // Only check every 10 seconds
        sleep(10);        
    }
}

//----------------------------------------------------------------------------//
// EXTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
/* Init */
void managerInit(void)
{    
    int li_index;
    int li_check;

    /**************************************************************************
     * Build date
     *************************************************************************/
    #ifdef DEBUG_VERSION
    debug_print("HMSG Start! Build date/time = %s - %s\n", __DATE__, __TIME__);
    #endif
    /**************************************************************************
     * INIT CONFIG AND GATEWAY
     *************************************************************************/
    config_init();
    gateway_init();
    hapcan_initGateway();
    hsystem_init();
    // Print Gateway
    #ifdef DEBUG_GATEWAY_LISTS
    gateway_printList(GATEWAY_MQTT2CAN_LIST);
    gateway_printList(GATEWAY_CAN2MQTT_LIST);
    #endif
    
    /**************************************************************************
     * INIT BUFFERS
     *************************************************************************/        
    for(li_index = 0; li_index < INIT_RETRIES; li_index++)
    {
        li_check = canbuf_init(0);
        if(li_check == EXIT_SUCCESS)
        {
            break;
        }
    }
    for(li_index = 0; li_index < INIT_RETRIES; li_index++)
    {
        li_check = mqttbuf_init();
        if(li_check == EXIT_SUCCESS)
        {
            break;
        }
    } 
    for(li_index = 0; li_index < INIT_RETRIES; li_index++)
    {
        li_check = socketserverbuf_init();
        if(li_check == EXIT_SUCCESS)
        {
            break;
        }
    }
    
    /**************************************************************************
     * INIT THREADS - CREATE AND JOIN
     *************************************************************************/    
    // Create Threads
    for(li_index = 0; li_index < NUMBER_OF_THREADS; li_index++)
    {
        li_check = pthread_create(&pt_threadID[li_index], 
                NULL, vp_thread[li_index], NULL);
        if(li_check)
        {
            /***************/
            /* FATAL ERROR */
            /***************/
            #ifdef DEBUG_MANAGER_ERRORS
            debug_print("MANAGER: THREAD CREATE ERROR!\n");
            debug_print("- Thread Index = %d\n", li_index);
            #endif
        }
    }    
    // Join Threads
    for(li_index = 0; li_index < NUMBER_OF_THREADS; li_index++)
    {
        li_check = pthread_join(pt_threadID[li_index], NULL);
        if(li_check)
        {
            /***************/
            /* FATAL ERROR */
            /***************/
            #ifdef DEBUG_MANAGER_ERRORS
            debug_print("MANAGER: THREAD JOIN ERROR!\n");
            debug_print("- Thread Index = %d\n", li_index);
            #endif
        }
    }
}
