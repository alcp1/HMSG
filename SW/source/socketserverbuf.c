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
#include <unistd.h>
#include <pthread.h>
#include "debug.h"
#include "buffer.h"
#include "auxiliary.h"
#include "hapcan.h"
#include "socketserver.h"
#include "socketserverbuf.h"

//----------------------------------------------------------------------------//
// INTERNAL DEFINITIONS
//----------------------------------------------------------------------------//
/* Buffer Offset */
#define NUMBER_OF_SOCKETSERVER_WRITE_BUFFERS (SOCKETSERVER_WRITE_STAMP_BUFFER - SOCKETSERVER_WRITE_DATA_BUFFER + 1)
#define NUMBER_OF_SOCKETSERVER_READ_BUFFERS  (SOCKETSERVER_READ_STAMP_BUFFER - SOCKETSERVER_READ_DATA_BUFFER + 1)

//----------------------------------------------------------------------------//
// INTERNAL GLOBAL VARIABLES
//----------------------------------------------------------------------------//
volatile stateSocketServer_t socketserverbufState = SOCKETSERVER_DISCONNECTED;
static int socketserverbufID[SOCKETSERVER_NUMBER_OF_BUFFERS] = {-1, -1, -1, -1};
static pthread_mutex_t ssb_state_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t ssb_read_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t ssb_write_mutex = PTHREAD_MUTEX_INITIALIZER;

//----------------------------------------------------------------------------//
// INTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
static stateSocketServer_t getSSBStateLocked(void);
static void setSSBStateLocked(stateSocketServer_t sState);

static stateSocketServer_t getSSBStateLocked(void)
{
    stateSocketServer_t lss_socketState;
    // LOCK STATE: Protect in case more threads try to read/set state 
    // at the same time
    pthread_mutex_lock(&ssb_state_mutex);
    // Read state
    lss_socketState = socketserverbufState;
    // UNLOCK STATE:
    pthread_mutex_unlock(&ssb_state_mutex);
    // Return
    return lss_socketState;
}

static void setSSBStateLocked(stateSocketServer_t sState)
{
    // LOCK STATE: Protect in case more threads try to read/set state 
    // at the same time
    pthread_mutex_lock(&ssb_state_mutex);
    // Set state
    socketserverbufState = sState;
    // UNLOCK STATE:
    pthread_mutex_unlock(&ssb_state_mutex);
}

//----------------------------------------------------------------------------//
// EXTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
/* Socket Server Initialization - Buffer Initialization */
int socketserverbuf_init(void)
{
    int count;
    int check;        
    
    // Init buffers
    for(count = 0; count < SOCKETSERVER_NUMBER_OF_BUFFERS; count++)
    {
        if(socketserverbufID[count] < 0)
        {
            socketserverbufID[count] = buffer_init(SOCKETSERVER_BUFFER_SIZE);
        }
    }
    // Check buffers - All should have ID
    check = 0;
    for(count = 0; count < SOCKETSERVER_NUMBER_OF_BUFFERS; count++)
    {
        if(socketserverbufID[count] < 0)
        {
            #ifdef DEBUG_SOCKETSERVERBUF_ERRORS
            debug_print("SOCKET SERVER: socketserverbuf_init Buffer Error!\n");
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

/* Socket Server - Socket Connection */
int socketserverbuf_connect(int timeout)
{
    int i_Check;
    int i_Return;
    int li_index;
    
    // Init Socket
    i_Return = EXIT_SUCCESS;
    i_Check = socketserver_open(timeout);
    if (i_Check < 0)
    {
        i_Return = EXIT_FAILURE;
    }
    
    // Set state
    if(i_Return == EXIT_SUCCESS)
    {
        // Check previous socket state
        if(getSSBStateLocked() != SOCKETSERVER_CONNECTED)
        {
            //-----------------------------------------------------------------
            // JUST CONNECTED - CLEAR BUFFERS
            //-----------------------------------------------------------------
            // LOCK BUFFERS: Protect data and timestamp buffers from being 
            // read/written at different times
            pthread_mutex_lock(&ssb_read_mutex);
            pthread_mutex_lock(&ssb_write_mutex);
            for(li_index = SOCKETSERVER_READ_DATA_BUFFER; 
                    li_index < SOCKETSERVER_NUMBER_OF_BUFFERS; li_index++)
            {
                buffer_clean(socketserverbufID[li_index]);
            }
            // UNLOCK BUFFERS
            pthread_mutex_unlock(&ssb_read_mutex);
            pthread_mutex_unlock(&ssb_write_mutex);
        }
        /* Set State */
        setSSBStateLocked(SOCKETSERVER_CONNECTED);
    }
    
    /* Return */
    return i_Return;
}

/* Socket Server Close connection: Close socket, free mem, 
 * re-inits buffers if needed */
int socketserverbuf_close(int cleanBuffers)
{
    int li_index; 

    // Set Connection State
    setSSBStateLocked(SOCKETSERVER_DISCONNECTED);

    // Close Sockets and Set File Descriptors
    socketserver_close();
    
    // Check if clean buffers
    if(cleanBuffers > 0)
    {
        // LOCK BUFFERS: Protect data and timestamp buffers from being 
        // read/written at different times
        pthread_mutex_lock(&ssb_read_mutex);
        pthread_mutex_lock(&ssb_write_mutex);
        for(li_index = SOCKETSERVER_READ_DATA_BUFFER; 
                li_index < SOCKETSERVER_NUMBER_OF_BUFFERS; li_index++)
        {
            buffer_clean(socketserverbufID[li_index]);
        }
        // UNLOCK BUFFERS:
        pthread_mutex_unlock(&ssb_read_mutex);
        pthread_mutex_unlock(&ssb_write_mutex);
    }
    
    // Return
    return EXIT_SUCCESS;
}

/* Socket Server Get State (Socket State) */
int socketserverbuf_getState(stateSocketServer_t* s_state)
{
    *s_state = getSSBStateLocked();
    // Return
    return EXIT_SUCCESS;        
}

/** Set Write buffer with data from parameters */
int socketserverbuf_setWriteMsgToBuffer(uint8_t* data, int dataLen, 
        unsigned long long millisecondsSinceEpoch)
{    
    int li_index;
    int check[NUMBER_OF_SOCKETSERVER_WRITE_BUFFERS];    
    // Only add data if connected and length is positive
    if(getSSBStateLocked() == SOCKETSERVER_DISCONNECTED || dataLen <= 0)
    {
        return SOCKETSERVER_SEND_NO_DATA;
    }    
    //-------------------------------------------------------------------------
    // Simply add data to Publish buffers
    //-------------------------------------------------------------------------
    li_index = 0;
    // LOCK WRITE BUFFERS: Protect data and timestamp buffers from being 
    // read/written at different times
    pthread_mutex_lock(&ssb_write_mutex);
    check[li_index] = 
            buffer_push(socketserverbufID[SOCKETSERVER_WRITE_DATA_BUFFER], 
            data, dataLen);
    li_index++;
    check[li_index] = 
            buffer_push(socketserverbufID[SOCKETSERVER_WRITE_STAMP_BUFFER], 
            &millisecondsSinceEpoch, sizeof(millisecondsSinceEpoch));
    // UNLOCK BUFFERS:
    pthread_mutex_unlock(&ssb_write_mutex);
    /* Check for critical errors */
    for(li_index = 0; li_index < NUMBER_OF_SOCKETSERVER_WRITE_BUFFERS; li_index++)
    {
        if( check[li_index] != BUFFER_OK )
        {
            /***************/
            /* FATAL ERROR */
            /***************/
            #ifdef DEBUG_SOCKETSERVERBUF_ERRORS
            debug_print("SOCKET SERVER: socketserverbuf_setWriteMsgToBuffer "
                    "- Buffer Error!\n");
            #endif
            return SOCKETSERVER_SEND_BUFFER_ERROR;
        }
    }
    // Here all is good
    return SOCKETSERVER_SEND_OK;
}

/* Socket Server Send Data from Write Buffer */
int socketserverbuf_send(void)
{
    unsigned long long millisecondsSinceEpoch;
    uint8_t data[HAPCAN_SOCKET_DATA_LEN];
    int dataLen;
    int li_position;
    int li_index;
    int li_temp;
    int li_return;
    unsigned int lui_size;
    unsigned int bufferSize[NUMBER_OF_SOCKETSERVER_WRITE_BUFFERS];
    
    /**************************************************************************
    * CONSISTENCY CHECK
    *************************************************************************/    
    // LOCK WRITE BUFFERS: Protect data and timestamp buffers from being 
    // read/written at different times
    pthread_mutex_lock(&ssb_write_mutex);
    // Get every write buffer count
    for(li_index = 0; li_index < NUMBER_OF_SOCKETSERVER_WRITE_BUFFERS; 
            li_index++)
    {        
        // Get the number of elements in the buffer
        li_position = SOCKETSERVER_WRITE_DATA_BUFFER + li_index;
        bufferSize[li_index] = buffer_dataCount(socketserverbufID[li_position]);
    }		
    // Check if every write buffer is not empty
    li_temp = 0;
    for(li_index = 0; li_index < NUMBER_OF_SOCKETSERVER_WRITE_BUFFERS; 
            li_index++)
    {        
        if( bufferSize[li_index] != 0 )
        {
            li_temp = 1;
        }
    }
    if(li_temp == 0)
    {
        // No data to be sent - Unlock Buffers and Return now
        // UNLOCK WRITE BUFFERS
        pthread_mutex_unlock(&ssb_write_mutex);
        return SOCKETSERVER_SEND_NO_DATA;
    }
    
    // Check if every write buffer has the same count of elements
    for(li_index = 0; li_index < NUMBER_OF_SOCKETSERVER_WRITE_BUFFERS - 1; 
            li_index++)
    {        
        if( bufferSize[li_index] != bufferSize[li_index + 1] )
        {            
            /***************/
            /* FATAL ERROR */
            /***************/
            #ifdef DEBUG_SOCKETSERVERBUF_ERRORS
            debug_print("socketserverbuf_send: Write Buffer ERROR "
                    "(pre-check)!\n");
            #endif
            // Buffers out of sync - unlock buffers and return now
            // UNLOCK WRITE BUFFERS
            pthread_mutex_unlock(&ssb_write_mutex);
            return SOCKETSERVER_SEND_BUFFER_ERROR;
        }
    }    
    /* SEND DATA: At this point, the buffers are in sync, and there is data to 
     * be sent
     */        
    /*******************************************************************
    * FILL DATA - SOCKET SERVER frame, millisecondsSinceEpoch
    *******************************************************************/
    li_return = SOCKETSERVER_SEND_OK;
    li_position = SOCKETSERVER_WRITE_DATA_BUFFER;    
    lui_size = buffer_popSize(socketserverbufID[li_position]);
    if((lui_size > 0) && (lui_size <= HAPCAN_SOCKET_DATA_LEN))
    {
        dataLen = lui_size;
        li_temp = buffer_pop(socketserverbufID[li_position], data, lui_size);
        if( li_temp != BUFFER_OK )
        {
            /***************/
            /* FATAL ERROR */
            /***************/
            #ifdef DEBUG_SOCKETSERVERBUF_ERRORS
            debug_print("socketserverbuf_send: Write Buffer ERROR (pop)!\n");
            debug_print("- Buffer ID: %d\n", li_position);
            debug_print("- Data Size: %d\n", lui_size);
            #endif
            li_return = SOCKETSERVER_SEND_BUFFER_ERROR;
        }
    }
    else
    {
        /***************/
        /* FATAL ERROR */
        /***************/
        #ifdef DEBUG_SOCKETSERVERBUF_ERRORS
        debug_print("socketserverbuf_send: Write Buffer ERROR - "
                "Data Size is incorrect!\n");
        debug_print("- Buffer ID: %d\n", li_position);
        debug_print("- Data Size: %d\n", lui_size);
        #endif
        li_return = SOCKETSERVER_SEND_BUFFER_ERROR;
    }
    // Pop timestamp to keep buffers sync
    li_position = SOCKETSERVER_WRITE_STAMP_BUFFER;
    lui_size = buffer_popSize(socketserverbufID[li_position]);
    if(lui_size > 0)
    {
        li_temp = buffer_pop(socketserverbufID[li_position], 
                &millisecondsSinceEpoch, sizeof(millisecondsSinceEpoch));
        if((li_temp != BUFFER_OK) || 
                (lui_size != sizeof(millisecondsSinceEpoch)))
        {
            #ifdef DEBUG_SOCKETSERVERBUF_ERRORS
            debug_print("socketserverbuf_send: Write Buffer ERROR "
                    "(pop timestamp)!\n");
            debug_print("- Buffer ID: %d\n", li_position);
            debug_print("- Data Size: %d\n", lui_size);
            #endif
            li_return = SOCKETSERVER_SEND_BUFFER_ERROR;
        }
    }
    else
    {
        /***************/
        /* FATAL ERROR */
        /***************/
        #ifdef DEBUG_SOCKETSERVERBUF_ERRORS
        debug_print("socketserverbuf_send: Write Buffer ERROR - "
                "Data Size 0!\n");
        debug_print("- Buffer ID: %d\n", li_position);
        debug_print("- Data Size: %d\n", lui_size);
        #endif
        li_return = SOCKETSERVER_SEND_BUFFER_ERROR;
    }
    // UNLOCK WRITE BUFFERS
    pthread_mutex_unlock(&ssb_write_mutex);
    if(li_return != SOCKETSERVER_SEND_OK)
    {
        return li_return;
    }
    /*******************************************************************
    * SEND DATA
    *******************************************************************/
    // Send Data - At this point all buffers and data sizes are validated
    #ifdef DEBUG_SOCKETSERVERBUF_SEND
    debug_printSocket("socketserverbuf_send: There is data to be sent:\n", 
            data, dataLen);
    #endif
    li_temp = socketserver_write(data, dataLen);
    if(li_temp < 0)
    {
        #ifdef DEBUG_SOCKETSERVERBUF_ERRORS
        debug_print("socketserverbuf_send: Socket Write ERROR!\n");
        debug_print("- Error: %d\n", li_temp);
        #endif
        return SOCKETSERVER_SEND_SOCKET_ERROR;
    }
    else
    {
        #ifdef DEBUG_SOCKETSERVERBUF_SEND
        debug_print("socketserverbuf_send: Data sent!\n");
        #endif
        return SOCKETSERVER_SEND_OK;
    }
}

/** Get data from Read buffer and set data from parameters */
int socketserverbuf_getReadMsgFromBuffer(uint8_t* data, int* dataLen, 
        unsigned long long* millisecondsSinceEpoch)
{
    int li_index;
    unsigned int bufferSize[NUMBER_OF_SOCKETSERVER_WRITE_BUFFERS];
    int li_position;
    int li_temp;
    unsigned int lui_size;
    int li_return;
    
    /*************************************************************************
    * CONSISTENCY CHECK
    *************************************************************************/
    // LOCK READ BUFFERS: Protect data and timestamp buffers from being 
    // read/written at different times
    pthread_mutex_lock(&ssb_read_mutex);
    // Get every read buffer count
    for(li_index = 0; li_index < NUMBER_OF_SOCKETSERVER_READ_BUFFERS; 
            li_index++)
    {        
        // Get the number of elements in the buffer
        li_position = SOCKETSERVER_READ_DATA_BUFFER + li_index;
        bufferSize[li_index] = buffer_dataCount(socketserverbufID[li_position]);
    }    
    // Check if every buffer is not empty
    li_temp = 0;
    for(li_index = 0; li_index < NUMBER_OF_SOCKETSERVER_READ_BUFFERS; 
            li_index++)
    {        
        if( bufferSize[li_index] != 0 )
        {
            li_temp = 1;
        }
    }
    if(li_temp == 0)
    {
        // No data on buffers - Unlock buffers and Return now
        // UNLOCK READ BUFFERS
        pthread_mutex_unlock(&ssb_read_mutex);
        return SOCKETSERVER_RECEIVE_NO_DATA;
    }    
    // Check if every read buffer has the same count of elements
    for(li_index = 0; li_index < NUMBER_OF_SOCKETSERVER_READ_BUFFERS - 1; 
            li_index++)
    {        
        if( bufferSize[li_index] != bufferSize[li_index + 1] )
        {            
            /***************/
            /* FATAL ERROR */
            /***************/
            #ifdef DEBUG_SOCKETSERVERBUF_ERRORS
            debug_print("SOCKET SERVER: Read Buffer ERROR!");
            #endif
            // Buffers out of sync: Unlock buffers and Return now
            // UNLOCK READ BUFFERS
            pthread_mutex_unlock(&ssb_read_mutex);
            return SOCKETSERVER_RECEIVE_BUFFER_ERROR;
        }
    }            
    /* READ DATA: At this point, the buffers are in sync, and there is data to 
     * be read
     */        
    /*******************************************************************
    * FILL DATA - SOCKET SERVER frame, millisecondsSinceEpoch
    *******************************************************************/
    li_return = SOCKETSERVER_RECEIVE_OK;
    li_position = SOCKETSERVER_READ_DATA_BUFFER;
    lui_size = buffer_popSize(socketserverbufID[li_position]);
    if((lui_size > 0) && (lui_size <= HAPCAN_SOCKET_DATA_LEN))
    {
        *dataLen = lui_size;
        li_temp = buffer_pop(socketserverbufID[li_position], data, lui_size);
        if( li_temp != BUFFER_OK )
        {
            /***************/
            /* FATAL ERROR */
            /***************/
            #ifdef DEBUG_SOCKETSERVERBUF_ERRORS
            debug_print("SOCKET SERVER: Read Buffer ERROR!\n");
            debug_print("- Buffer ID: %d\n", li_position);
            debug_print("- Data Size: %d\n", lui_size);
            #endif
            li_return = SOCKETSERVER_RECEIVE_BUFFER_ERROR;
        }
    }
    else
    {
        /***************/
        /* FATAL ERROR */
        /***************/
        #ifdef DEBUG_SOCKETSERVERBUF_ERRORS
        debug_print("SOCKET SERVER: Read Buffer ERROR - Data Size is 0!\n");
        debug_print("- Buffer ID: %d\n", li_position);
        debug_print("- Data Size: %d\n", lui_size);
        #endif
        li_return = SOCKETSERVER_RECEIVE_BUFFER_ERROR;
    }
    // Pop timestamp to keep buffers sync
    li_position = SOCKETSERVER_READ_STAMP_BUFFER;
    lui_size = buffer_popSize(socketserverbufID[li_position]);
    if(lui_size > 0)
    {
        li_temp = buffer_pop(socketserverbufID[li_position], 
                millisecondsSinceEpoch, sizeof(*millisecondsSinceEpoch));
        if( (li_temp != BUFFER_OK) || 
                (lui_size != sizeof(*millisecondsSinceEpoch)) )
        {
            #ifdef DEBUG_SOCKETSERVERBUF_ERRORS
            debug_print("SOCKET SERVER: Read Buffer ERROR!\n");
            debug_print("- Buffer ID: %d\n", li_position);
            debug_print("- Data Size: %d\n", lui_size);
            #endif
            li_return = SOCKETSERVER_RECEIVE_BUFFER_ERROR;
        }
    }
    else
    {
        /***************/
        /* FATAL ERROR */
        /***************/
        #ifdef DEBUG_SOCKETSERVERBUF_ERRORS
        debug_print("SOCKET SERVER: Read Buffer ERROR - Data Size is 0!\n");
        debug_print("- Buffer ID: %d\n", li_position);
        debug_print("- Data Size: %d\n", lui_size);
        #endif
        li_return = SOCKETSERVER_RECEIVE_BUFFER_ERROR;
    }
    // UNLOCK BUFFERS:
    pthread_mutex_unlock(&ssb_read_mutex);    
    // Return
    return li_return;
}

/* Socket Server read Data and fill Read Buffer */
int socketserverbuf_receive(int timeout)
{
    unsigned long long millisecondsSinceEpoch;
    int socketReturn;
    uint8_t data[HAPCAN_SOCKET_DATA_LEN];
    int dataLen;
    int check[NUMBER_OF_SOCKETSERVER_READ_BUFFERS];
    int li_index;
    int li_position;
    // Check for new data
    socketReturn = socketserver_read(data, &dataLen, timeout);
    // Evaluate socket return
    switch(socketReturn) 
    {
        case SOCKETSERVER_OK:
            // Get Timestamp
            millisecondsSinceEpoch = aux_getmsSinceEpoch();
            break;

        case SOCKETSERVER_TIMEOUT:
            return SOCKETSERVER_RECEIVE_NO_DATA;
            break;
            
        case SOCKETSERVER_ERROR:
            /***************/
            /* FATAL ERROR */
            /***************/
            #ifdef DEBUG_SOCKETSERVERBUF_ERRORS
            debug_print("SOCKET SERVER: Socket Read - SOCKETSERVER_ERROR!\n");
            #endif
            return SOCKETSERVER_RECEIVE_SOCKET_ERROR;
            break;
            
        case SOCKETSERVER_OTHER_ERROR:
            /***************/
            /* FATAL ERROR */
            /***************/
            #ifdef DEBUG_SOCKETSERVERBUF_ERRORS
            debug_print("SOCKET SERVER: Socket Read - "
                    "SOCKETSERVER_OTHER_ERROR!\n");
            #endif
            return SOCKETSERVER_RECEIVE_SOCKET_ERROR;
            break;
            
        case SOCKETSERVER_CLOSED:
            /***************/
            /* FATAL ERROR */
            /***************/
            #ifdef DEBUG_SOCKETSERVERBUF_ERRORS
            debug_print("SOCKET SERVER: Socket Read - SOCKETSERVER_CLOSED!\n");
            #endif
            return SOCKETSERVER_RECEIVE_CLOSED_ERROR;
            break;
            
        case SOCKETSERVER_OVERFLOW:
            /***************/
            /*    ERROR    */
            /***************/
            #ifdef DEBUG_SOCKETSERVERBUF_ERRORS
            debug_print("SOCKET SERVER: Socket Read - "
                    "SOCKETSERVER_OVERFLOW!\n");
            #endif
            return SOCKETSERVER_RECEIVE_OVERFLOW;
            break;

        default:
            /***************/
            /* FATAL ERROR */
            /***************/
            #ifdef DEBUG_SOCKETSERVERBUF_ERRORS
            debug_print("SOCKET SERVER: Socket Read - NON-STANDARD ERROR!\n");
            #endif
            return SOCKETSERVER_RECEIVE_SOCKET_ERROR;
            break;
    }
    // Check data length
    if(dataLen <= 0)
    {
        return SOCKETSERVER_RECEIVE_NO_DATA;
    }
    // Set the position
    li_position = SOCKETSERVER_READ_DATA_BUFFER;
    li_index = 0;    
    //-------------------------------------------------------------------------
    // Add data to buffer and check results
    //-------------------------------------------------------------------------
    // LOCK BUFFERS: Protect data and timestamp buffers from being 
    // read/written at different times
    pthread_mutex_lock(&ssb_read_mutex);
    check[li_index] = buffer_push(socketserverbufID[li_position], data, 
            dataLen);
    li_position++;
    li_index++;
    check[li_index] = buffer_push(socketserverbufID[li_position], 
            &millisecondsSinceEpoch, sizeof(millisecondsSinceEpoch));
    // UNLOCK BUFFERS:
    pthread_mutex_unlock(&ssb_read_mutex);    
    /* Check for critical errors */
    for(li_index = 0; li_index < NUMBER_OF_SOCKETSERVER_READ_BUFFERS; 
            li_index++)
    {
        if( check[li_index] != BUFFER_OK )
        {
            /***************/
            /* FATAL ERROR */
            /***************/
            #ifdef DEBUG_SOCKETSERVERBUF_ERRORS
            debug_print("SOCKET SERVER: Socket Read ERROR - Buffer ERROR!\n");
            #endif
            return SOCKETSERVER_RECEIVE_BUFFER_ERROR;
        }
    }    
    
    // Here - Return OK
    return SOCKETSERVER_RECEIVE_OK;
}
