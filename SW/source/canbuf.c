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
#include "socketcan.h"
#include "canbuf.h"

//----------------------------------------------------------------------------//
// INTERNAL DEFINITIONS
//----------------------------------------------------------------------------//
/* Buffer Offset */
#define NUMBER_OF_CAN_WRITE_BUFFERS (CAN_WRITE_STAMP_BUFFER - CAN_WRITE_DATA_BUFFER + 1)
#define NUMBER_OF_CAN_READ_BUFFERS  (CAN_READ_STAMP_BUFFER - CAN_READ_DATA_BUFFER + 1)

//----------------------------------------------------------------------------//
// INTERNAL GLOBAL VARIABLES
//----------------------------------------------------------------------------//
// State: For two CAN Channels: use {CAN_DISCONNECTED, CAN_DISCONNECTED};
volatile stateCAN_t canbufState[SOCKETCAN_CHANNELS] = {CAN_DISCONNECTED};
// ID: For two CAN channels
   //{-1, -1, -1, -1} ,   /*  initializers for row indexed by 0 */
   //{-1, -1, -1, -1}     /*  initializers for row indexed by 1 */
static int canbufID[SOCKETCAN_CHANNELS][CAN_NUMBER_OF_BUFFERS] = {
   {-1, -1, -1, -1}   /*  initializers for row indexed by 0 */
};
// File descriptor: // For two CAN Channels: {-1, -1};
static int fd[SOCKETCAN_CHANNELS] = {-1}; 

static pthread_mutex_t cb_state_mutex[SOCKETCAN_CHANNELS] = {
    PTHREAD_MUTEX_INITIALIZER};
static pthread_mutex_t cb_read_mutex[SOCKETCAN_CHANNELS] = {
    PTHREAD_MUTEX_INITIALIZER};
static pthread_mutex_t cb_write_mutex[SOCKETCAN_CHANNELS] = {
    PTHREAD_MUTEX_INITIALIZER};

//----------------------------------------------------------------------------//
// INTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
static int canbuf_validateChannel(int channel);
static stateCAN_t getCANBufState(int channel);
static void setCANBufState(int channel, stateCAN_t cState);

/**
 * CAN Validate channel
 * \param   channel     Channel 0 (SOCKETCAN_CHANNEL_0): can0 
 *                      Channel 1 (SOCKETCAN_CHANNEL_1): can1
 * \return  EXIT_SUCCESS / EXIT_FAILURE
 */
static int canbuf_validateChannel(int channel)
{
    if( (channel < SOCKETCAN_CHANNEL_0) || (channel >= SOCKETCAN_CHANNELS) )
    {
        return EXIT_FAILURE;
    }
    else
    {
        return EXIT_SUCCESS;
    }
}

static stateCAN_t getCANBufState(int channel)
{
    stateCAN_t lcs_State;
    // LOCK STATE: Protect in case more threads try to read/set state 
    // at the same time
    pthread_mutex_lock(&cb_state_mutex[channel]);
    // Read state
    lcs_State = canbufState[channel];
    // UNLOCK STATE:
    pthread_mutex_unlock(&cb_state_mutex[channel]);
    // Return
    return lcs_State;
}

static void setCANBufState(int channel, stateCAN_t cState)
{
    // LOCK STATE: Protect in case more threads try to read/set state 
    // at the same time
    pthread_mutex_lock(&cb_state_mutex[channel]);
    // Set state
    canbufState[channel] = cState;
    // UNLOCK STATE:
    pthread_mutex_unlock(&cb_state_mutex[channel]);
}


//----------------------------------------------------------------------------//
// EXTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
/* CAN Initialization - Buffer Initialization */
int canbuf_init(int channel)
{
    int count;
    int check;        
    
    // Validate channel
    if( canbuf_validateChannel(channel) == EXIT_FAILURE )
    {
        /***************/
        /* FATAL ERROR */
        /***************/
        #ifdef DEBUG_CANBUF_ERRORS
        debug_print("CAN: canbuf_init ERROR - Channel Error!\n");
        debug_print("- Channel: %d\n", channel);
        #endif
        return EXIT_FAILURE;
    }
    
    // Init buffers
    for(count = 0; count < CAN_NUMBER_OF_BUFFERS; count++)
    {
        if(canbufID[channel][count] < 0)
        {
            canbufID[channel][count] = buffer_init(CAN_BUFFER_SIZE);
        }
    }
    // Check buffers - All should have ID
    check = 0;
    for(count = 0; count < CAN_NUMBER_OF_BUFFERS; count++)
    {
        if(canbufID[channel][count] < 0)
        {
            #ifdef DEBUG_CANBUF_ERRORS
            debug_print("CAN: canbuf_init ERROR - Buffer Error!\n");
            debug_print("- Channel: %d\n", channel);
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

/* CAN Initialization - Socket Connection */
int canbuf_connect(int channel)
{
    int i_Check;
    int i_Return;
    int li_index;
    
    // Validate channel
    if( canbuf_validateChannel(channel) == EXIT_FAILURE )
    {
        /***************/
        /* FATAL ERROR */
        /***************/
        #ifdef DEBUG_CANBUF_ERRORS
        debug_print("CAN: canbuf_init ERROR - Channel Error!\n");
        debug_print("- Channel: %d\n", channel);
        #endif
        return EXIT_FAILURE;
    }        
    
    // Init Socket
    i_Return = EXIT_SUCCESS;
    i_Check = socketcan_open(channel);
    if (i_Check < 0)
    {
        i_Return = EXIT_FAILURE;
    }
    else
    {
        // Set socket file descriptor
        fd[channel] = i_Check;
    }
    
    // Set state if both channels are OK
    if(i_Return == EXIT_SUCCESS)
    {
        // Check previous CAN state
        if(getCANBufState(channel) != CAN_CONNECTED)
        {
            //-----------------------------------------------------------------
            // JUST CONNECTED - CLEAR BUFFERS
            //-----------------------------------------------------------------
            // LOCK BUFFERS: Protect data and timestamp buffers from being 
            // read/written at different times
            pthread_mutex_lock(&cb_read_mutex[channel]);
            pthread_mutex_lock(&cb_write_mutex[channel]);            
            for(li_index = CAN_READ_DATA_BUFFER; 
                    li_index < CAN_NUMBER_OF_BUFFERS; li_index++)
            {
                buffer_clean(canbufID[channel][li_index]);
            }
            // UNLOCK BUFFERS:
            pthread_mutex_unlock(&cb_read_mutex[channel]);
            pthread_mutex_unlock(&cb_write_mutex[channel]);
        }
        /* Set State */
        setCANBufState(channel, CAN_CONNECTED);
    }
    
    /* Return */
    return i_Return;
}

/* CAN Close connection: Close socket, free mem, re-inits buffers if needed */
int canbuf_close(int channel, int cleanBuffers)
{
    int li_index; 

    // Validate channel
    if( canbuf_validateChannel(channel) == EXIT_FAILURE )
    {
        /***************/
        /* FATAL ERROR */
        /***************/
        #ifdef DEBUG_CANBUF_ERRORS
        debug_print("CAN: canbuf_close ERROR - Channel Error!\n");
        debug_print("- Channel: %d\n", channel);
        #endif
        return EXIT_FAILURE;
    }
    
    // Set CAN State
    setCANBufState(channel, CAN_DISCONNECTED);

    // Close Sockets and Set File Descriptors
    socketcan_close(fd[channel]);
    fd[channel] = -1;

    // Check if clean buffers
    if(cleanBuffers > 0)
    {
        // LOCK BUFFERS: Protect data and timestamp buffers from being 
        // read/written at different times
        pthread_mutex_lock(&cb_read_mutex[channel]);
        pthread_mutex_lock(&cb_write_mutex[channel]);
        for(li_index = CAN_READ_DATA_BUFFER; 
                li_index < CAN_NUMBER_OF_BUFFERS; li_index++)
        {
            buffer_clean(canbufID[channel][li_index]);
        }
        // UNLOCK BUFFERS:
        pthread_mutex_unlock(&cb_read_mutex[channel]);
        pthread_mutex_unlock(&cb_write_mutex[channel]);
    }
    
    // Return
    return EXIT_SUCCESS;
}

/* CAN Get State (Socket State) */
int canbuf_getState(int channel, stateCAN_t* scp_state)
{
    // Validate channel
    if( canbuf_validateChannel(channel) == EXIT_FAILURE )
    {
        /***************/
        /* FATAL ERROR */
        /***************/
        #ifdef DEBUG_CANBUF_ERRORS
        debug_print("CAN: canbuf_getState ERROR - Channel Error!\n");
        debug_print("- Channel: %d\n", channel);
        #endif
        return EXIT_FAILURE;
    }
    else
    {
        *scp_state = getCANBufState(channel);
        // Return
        return EXIT_SUCCESS;
    }        
}

/** Set Write buffer with data from parameters */
int canbuf_setWriteMsgToBuffer(int channel, struct can_frame* pcf_Frame, 
        unsigned long long millisecondsSinceEpoch)
{    
    int li_index;
    int check[NUMBER_OF_CAN_WRITE_BUFFERS];
    
    // Validate channel
    if( canbuf_validateChannel(channel) == EXIT_FAILURE )
    {
        /***************/
        /* FATAL ERROR */
        /***************/
        #ifdef DEBUG_CANBUF_ERRORS
        debug_print("CAN: canbuf_setWriteMsgToBuffer ERROR - Channel Error!\n");
        debug_print("- Channel: %d\n", channel);
        #endif
        return CAN_SEND_PARAMETER_ERROR;
    }
    
    // Simply add data to Publish buffers
    li_index = 0;
    // LOCK BUFFERS: Protect data and timestamp buffers from being 
    // read/written at different times
    pthread_mutex_lock(&cb_write_mutex[channel]);
    check[li_index] = buffer_push(canbufID[channel][CAN_WRITE_DATA_BUFFER], 
            pcf_Frame, sizeof(*pcf_Frame));
    li_index++;
    check[li_index] = buffer_push(canbufID[channel][CAN_WRITE_STAMP_BUFFER], 
            &millisecondsSinceEpoch, sizeof(millisecondsSinceEpoch));
    // UNLOCK BUFFERS:
    pthread_mutex_unlock(&cb_write_mutex[channel]);
    /* Check for critical errors */
    for(li_index = 0; li_index < NUMBER_OF_CAN_WRITE_BUFFERS; li_index++)
    {
        if( check[li_index] != BUFFER_OK )
        {
            /***************/
            /* FATAL ERROR */
            /***************/
            #ifdef DEBUG_CANBUF_ERRORS
            debug_print("CAN: canbuf_setWriteMsgToBuffer - Buffer Error!\n");
            debug_print("- Channel: %d\n", channel);
            debug_print("- CAN Write Buffer Index: %d\n", li_index);
            debug_print("- CAN Write Buffer Error: %d\n", check[li_index]);
            #endif
            return CAN_SEND_BUFFER_ERROR;
        }
    }
    
    // Here all is good
    return CAN_SEND_OK;
}

/* CAN Send Data from Write Buffer */
int canbuf_send(int channel)
{
    unsigned long long millisecondsSinceEpoch;
    struct can_frame cf_Frame;
    int li_position;
    int li_index;
    int li_temp;
    unsigned int lui_size;
    unsigned int bufferSize[NUMBER_OF_CAN_WRITE_BUFFERS];
    int li_return;
    
    // Validate channel
    if( canbuf_validateChannel(channel) == EXIT_FAILURE )
    {
        /***************/
        /* FATAL ERROR */
        /***************/
        #ifdef DEBUG_CANBUF_ERRORS
        debug_print("CAN: canbuf_send ERROR - Channel Error!\n");
        debug_print("- Channel: %d\n", channel);
        #endif
        return CAN_SEND_PARAMETER_ERROR;
    }
    
    /**************************************************************************
    * CONSISTENCY CHECK
    *************************************************************************/
    // LOCK BUFFERS: Protect data and timestamp buffers from being read/written 
    // at different times
    pthread_mutex_lock(&cb_write_mutex[channel]);
    // Get every write buffer count
    for(li_index = 0; li_index < NUMBER_OF_CAN_WRITE_BUFFERS; li_index++)
    {        
        // Get the number of elements in the buffer
        li_position = CAN_WRITE_DATA_BUFFER + li_index;
        bufferSize[li_index] = buffer_dataCount(canbufID[channel][li_position]);
    }        
    // Check if every write buffer is not empty
    li_temp = 0;
    for(li_index = 0; li_index < NUMBER_OF_CAN_WRITE_BUFFERS; li_index++)
    {        
        if( bufferSize[li_index] != 0 )
        {
            li_temp = 1;
        }
    }
    if(li_temp == 0)
    {
        // No data to be sent - Unlock buffers and return now
        // UNLOCK BUFFERS
        pthread_mutex_unlock(&cb_write_mutex[channel]);
        return CAN_SEND_NO_DATA;
    }    
    // Check if every write buffer has the same count of elements
    for(li_index = 0; li_index < NUMBER_OF_CAN_WRITE_BUFFERS - 1; li_index++)
    {        
        if( bufferSize[li_index] != bufferSize[li_index + 1] )
        {            
            /************************/
            /* BUFFERNS OUT OF SYNC */
            /************************/
            #ifdef DEBUG_CANBUF_ERRORS
            debug_print("canbuf_send: Write Buffer ERROR!\n (pre-check)");
            debug_print("- Channel: %d\n", channel);
            #endif
            // Buffers out of sync - Unlock buffers and return now
            // UNLOCK BUFFERS
            pthread_mutex_unlock(&cb_write_mutex[channel]);
            return CAN_SEND_BUFFER_ERROR;
        }
    }            
    /* SEND DATA: At this point, the buffers are in sync, and there is data to 
     * be sent
     */        
    /*******************************************************************
    * FILL DATA - can frame, millisecondsSinceEpoch
    *******************************************************************/
    li_return = CAN_SEND_OK;
    li_position = CAN_WRITE_DATA_BUFFER;
    lui_size = buffer_popSize(canbufID[channel][li_position]);
    if(lui_size > 0)
    {
        li_temp = buffer_pop(canbufID[channel][li_position], &cf_Frame, 
                sizeof(cf_Frame));
        if( (li_temp != BUFFER_OK) || (lui_size != sizeof(cf_Frame)) )
        {
            /***************/
            /* FATAL ERROR */
            /***************/
            #ifdef DEBUG_CANBUF_ERRORS
            debug_print("canbuf_send: Write Buffer ERROR! (data pop)\n");
            debug_print("- Channel: %d\n", channel);
            debug_print("- Buffer ID: %d\n", li_position);
            debug_print("- Data Size: %d\n", lui_size);
            #endif
            li_return = CAN_SEND_BUFFER_ERROR;
        }
    }
    else
    {
        /***************/
        /* FATAL ERROR */
        /***************/
        #ifdef DEBUG_CANBUF_ERRORS
        debug_print("canbuf_send: Write Buffer ERROR - Data Size is 0!\n");
        debug_print("- Channel: %d\n", channel);
        debug_print("- Buffer ID: %d\n", li_position);
        debug_print("- Data Size: %d\n", lui_size);
        #endif
        li_return = CAN_SEND_BUFFER_ERROR;
    }
    // Pop timestamp to keep buffers sync
    li_position = CAN_WRITE_STAMP_BUFFER;
    lui_size = buffer_popSize(canbufID[channel][li_position]);
    if(lui_size > 0)
    {
        li_temp = buffer_pop(canbufID[channel][li_position], 
                &millisecondsSinceEpoch, sizeof(millisecondsSinceEpoch));
        if( (li_temp != BUFFER_OK) || 
                (lui_size != sizeof(millisecondsSinceEpoch)) )
        {
            #ifdef DEBUG_CANBUF_ERRORS
            debug_print("canbuf_send: Write Buffer ERROR! (timestamp pop)\n");
            debug_print("- Channel: %d\n", channel);
            debug_print("- Buffer ID: %d\n", li_position);
            debug_print("- Data Size: %d\n", lui_size);
            #endif
            li_return = CAN_SEND_BUFFER_ERROR;
        }
    }
    else
    {
        /***************/
        /* FATAL ERROR */
        /***************/
        #ifdef DEBUG_CANBUF_ERRORS
        debug_print("canbuf_send: Write Buffer ERROR - Data Size is 0!\n");
        debug_print("- Channel: %d\n", channel);
        debug_print("- Buffer ID: %d\n", li_position);
        debug_print("- Data Size: %d\n", lui_size);
        #endif
        li_return = CAN_SEND_BUFFER_ERROR;
    }
    // UNLOCK BUFFERS:
    pthread_mutex_unlock(&cb_write_mutex[channel]);
    // Check errors
    if(li_return != CAN_SEND_OK)
    {
        return li_return;
    }
    /*******************************************************************
    * SEND DATA
    *******************************************************************/
    // Send Data - At this point all buffers and data sizes are validated
    #ifdef DEBUG_CANBUF_SEND
    debug_printCAN("canbuf_send: There is data to be sent:\n", &cf_Frame);
    #endif
    li_temp = socketcan_write(fd[channel], &cf_Frame);
    if(li_temp < 0)
    {
        #ifdef DEBUG_CANBUF_ERRORS
        debug_print("canbuf_send: Socket Write ERROR!\n");
        debug_print("- Channel: %d\n", channel);
        debug_print("- Error: %d\n", li_temp);
        #endif
        return CAN_SEND_SOCKET_ERROR;
    }
    else
    {
        #ifdef DEBUG_CANBUF_SEND
        debug_print("canbuf_send: Data sent!\n");
        #endif
        return CAN_SEND_OK;
    }
}

/** Get data from Read buffer and set data from parameters */
int canbuf_getReadMsgFromBuffer(int channel, struct can_frame* pcf_Frame, 
        unsigned long long* millisecondsSinceEpoch)
{
    int li_index;
    unsigned int bufferSize[NUMBER_OF_CAN_WRITE_BUFFERS];
    int li_position;
    int li_temp;
    unsigned int lui_size;
    int li_return = CAN_RECEIVE_OK;
    
    // Validate channel
    if( canbuf_validateChannel(channel) == EXIT_FAILURE )
    {
        /***************/
        /* FATAL ERROR */
        /***************/
        #ifdef DEBUG_CANBUF_ERRORS
        debug_print("CAN: canbuf_getReadMsgFromBuffer - Channel Error!\n");
        debug_print("- Channel: %d\n", channel);
        #endif
        return CAN_RECEIVE_PARAMETER_ERROR;
    }    
    // LOCK BUFFERS: Protect data and timestamp buffers from being read/written 
    // at different times
    pthread_mutex_lock(&cb_read_mutex[channel]);
    /**************************************************************************
    * CONSISTENCY CHECK
    *************************************************************************/     
    // Get every write buffer count
    for(li_index = 0; li_index < NUMBER_OF_CAN_READ_BUFFERS; li_index++)
    {        
        // Get the number of elements in the buffer
        li_position = CAN_READ_DATA_BUFFER + li_index;
        bufferSize[li_index] = buffer_dataCount(canbufID[channel][li_position]);
    }        
    // Check if every buffer is not empty
    li_temp = 0;
    for(li_index = 0; li_index < NUMBER_OF_CAN_READ_BUFFERS; li_index++)
    {        
        if( bufferSize[li_index] != 0 )
        {
            li_temp = 1;
        }
    }
    if(li_temp == 0)
    {
        // No data to be sent for this channel - Unlock and return now
        // UNLOCK BUFFERS
        pthread_mutex_unlock(&cb_read_mutex[channel]);
        return CAN_RECEIVE_NO_DATA;
    }    
    // Check if every write buffer has the same count of elements
    for(li_index = 0; li_index < NUMBER_OF_CAN_READ_BUFFERS - 1; li_index++)
    {        
        if( bufferSize[li_index] != bufferSize[li_index + 1] )
        {            
            /**********************/
            /* BUFFER OUT OF SYNC */
            /**********************/
            #ifdef DEBUG_CANBUF_ERRORS
            debug_print("CAN: Read Buffer ERROR!");
            debug_print("- Channel: %d\n", channel);
            #endif
            // Buffers out of sync: Unlock buffer and return now
            // UNLOCK BUFFER
            pthread_mutex_unlock(&cb_read_mutex[channel]);
            return CAN_RECEIVE_BUFFER_ERROR;
        }
    }
    /* READ DATA: At this point, the buffers are in sync, and there is data to 
     * be read
     */        
    li_return = CAN_RECEIVE_OK;    
    /*******************************************************************
    * FILL DATA - can frame, millisecondsSinceEpoch
    *******************************************************************/
    li_position = CAN_READ_DATA_BUFFER;
    lui_size = buffer_popSize(canbufID[channel][li_position]);
    if(lui_size > 0)
    {
        li_temp = buffer_pop(canbufID[channel][li_position], pcf_Frame, 
                sizeof(*pcf_Frame));
        if( (li_temp != BUFFER_OK) || (lui_size != sizeof(*pcf_Frame)) )
        {
            /***************/
            /* FATAL ERROR */
            /***************/
            #ifdef DEBUG_CANBUF_ERRORS
            debug_print("CAN: Read Buffer ERROR!\n");
            debug_print("- Channel: %d\n", channel);
            debug_print("- Buffer ID: %d\n", li_position);
            debug_print("- Data Size: %d\n", lui_size);
            #endif
            li_return = CAN_RECEIVE_BUFFER_ERROR;
        }
    }
    else
    {
        /***************/
        /* FATAL ERROR */
        /***************/
        #ifdef DEBUG_CANBUF_ERRORS
        debug_print("CAN: Read Buffer ERROR - Data Size is 0!\n");
        debug_print("- Channel: %d\n", channel);
        debug_print("- Buffer ID: %d\n", li_position);
        debug_print("- Data Size: %d\n", lui_size);
        #endif
        li_return = CAN_RECEIVE_BUFFER_ERROR;
    }
    // Pop timestamp to keep buffers sync
    li_position = CAN_READ_STAMP_BUFFER;
    lui_size = buffer_popSize(canbufID[channel][li_position]);
    if(lui_size > 0)
    {
        li_temp = buffer_pop(canbufID[channel][li_position], 
                millisecondsSinceEpoch, sizeof(*millisecondsSinceEpoch));
        if( (li_temp != BUFFER_OK) || 
                (lui_size != sizeof(*millisecondsSinceEpoch)) )
        {
            #ifdef DEBUG_CANBUF_ERRORS
            debug_print("CAN: Read Buffer ERROR!\n");
            debug_print("- Channel: %d\n", channel);
            debug_print("- Buffer ID: %d\n", li_position);
            debug_print("- Data Size: %d\n", lui_size);
            #endif
            li_return = CAN_RECEIVE_BUFFER_ERROR;
        }
    }
    else
    {
        /***************/
        /* FATAL ERROR */
        /***************/
        #ifdef DEBUG_CANBUF_ERRORS
        debug_print("CAN: Read Buffer ERROR - Data Size is 0!\n");
        debug_print("- Channel: %d\n", channel);
        debug_print("- Buffer ID: %d\n", li_position);
        debug_print("- Data Size: %d\n", lui_size);
        #endif
        li_return = CAN_RECEIVE_BUFFER_ERROR;
    }
    // UNLOCK BUFFERS:
    pthread_mutex_unlock(&cb_read_mutex[channel]);    
    // Return
    return li_return;
}

/* CAN read Data and fill Read Buffer */
int canbuf_receive(int channel, int timeout)
{
    unsigned long long millisecondsSinceEpoch;
    int socketReturn;
    struct can_frame cf_Frame;
    int check[NUMBER_OF_CAN_READ_BUFFERS];
    int li_index;
    int li_position;
    
    // Validate channel
    if( canbuf_validateChannel(channel) == EXIT_FAILURE )
    {
        /***************/
        /* FATAL ERROR */
        /***************/
        #ifdef DEBUG_CANBUF_ERRORS
        debug_print("CAN: Socket Read ERROR - Channel Error!\n");
        #endif
        return CAN_RECEIVE_PARAMETER_ERROR;
    }
        
    // Check for new data
    socketReturn = socketcan_read(fd[channel], &cf_Frame, timeout);
    
    // Evaluate socket return
    switch(socketReturn) 
    {
        case SOCKETCAN_OK:
            // Get Timestamp
            millisecondsSinceEpoch = aux_getmsSinceEpoch();
            break;

        case SOCKETCAN_TIMEOUT:
            return CAN_RECEIVE_NO_DATA;
            break;
            
        case SOCKETCAN_ERROR:
            /***************/
            /* FATAL ERROR */
            /***************/
            #ifdef DEBUG_CANBUF_ERRORS
            debug_print("CAN: Socket Read ERROR - SOCKETCAN_ERROR!\n");
            #endif
            return CAN_RECEIVE_SOCKET_ERROR;
            break;
            
        case SOCKETCAN_OTHER_ERROR:
            /***************/
            /* FATAL ERROR */
            /***************/
            #ifdef DEBUG_CANBUF_ERRORS
            debug_print("CAN: Socket Read ERROR - SOCKETCAN_OTHER_ERROR!\n");
            #endif
            return CAN_RECEIVE_SOCKET_ERROR;
            break;

        default:
            /***************/
            /* FATAL ERROR */
            /***************/
            #ifdef DEBUG_CANBUF_ERRORS
            debug_print("CAN: Socket Read ERROR - NON-STANDARD ERROR!\n");
            #endif
            return CAN_RECEIVE_SOCKET_ERROR;
            break;
    }
    
    // Get the position depending on the channel
    li_position = CAN_READ_DATA_BUFFER;
    li_index = 0;
    
    //--------------------------------------------------------------------------
    // Add data to buffer and check results
    //--------------------------------------------------------------------------
    // LOCK BUFFERS: Protect data and timestamp buffers from being read/written 
    // at different times
    pthread_mutex_lock(&cb_read_mutex[channel]);
    check[li_index] = buffer_push(canbufID[channel][li_position], &cf_Frame, 
            sizeof(cf_Frame));
    li_position++;
    li_index++;
    check[li_index] = buffer_push(canbufID[channel][li_position], 
            &millisecondsSinceEpoch, sizeof(millisecondsSinceEpoch));
    // UNLOCK BUFFERS:
    pthread_mutex_unlock(&cb_read_mutex[channel]);
    
    /* Check for critical errors */
    for(li_index = 0; li_index < NUMBER_OF_CAN_READ_BUFFERS; li_index++)
    {
        if( check[li_index] != BUFFER_OK )
        {
            /***************/
            /* FATAL ERROR */
            /***************/
            #ifdef DEBUG_CANBUF_ERRORS
            debug_print("CAN: Socket Read ERROR - Buffer ERROR!\n");
            #endif
            return CAN_RECEIVE_BUFFER_ERROR;
        }
    }
    
    // Here - Return OK
    return CAN_RECEIVE_OK;
}