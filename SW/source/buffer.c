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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include "buffer.h"
#include "debug.h"

//----------------------------------------------------------------------------//
// INTERNAL DEFINITIONS
//----------------------------------------------------------------------------//
#define MAXIMUM_NUMBER_OF_BUFFERS   30
#define MAXIMUM_NUMBER_OF_BUFFER_ELEMENTS   2000

//----------------------------------------------------------------------------//
// INTERNAL TYPES
//----------------------------------------------------------------------------//
typedef struct 
{
    unsigned int head; // First free position
    unsigned int tail;  // First position to be removed
    unsigned int count;  // Number of elements in the buffer
    unsigned int elements;  // Maximum number of elements in buffer
    unsigned int* dataLen;  // Size of the data stored in data field
    void **data;
} buffer_t;

//----------------------------------------------------------------------------//
// INTERNAL GLOBAL VARIABLES
//----------------------------------------------------------------------------//
static int i_NumberOfBuffers = 0;
static buffer_t buffers[MAXIMUM_NUMBER_OF_BUFFERS];
static pthread_mutex_t buffer_initMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t buffer_Mutex[MAXIMUM_NUMBER_OF_BUFFERS];

//----------------------------------------------------------------------------//
// INTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
static unsigned int buffer_NextIndex(int id, unsigned int index);

/* Returns the next position based on the current position
 */
static unsigned int buffer_NextIndex(int id, unsigned int index)
{
    unsigned int lui_Next;
    if((index + 1) >= buffers[id].elements)
    {
        lui_Next = 0;
    }
    else
    {
        lui_Next = index + 1;
    }
    return lui_Next;
}

//----------------------------------------------------------------------------//
// EXTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
/* Buffer Initialization
 */
int buffer_init(unsigned int elements)
{
    int i_BufferID;
    // LOCK - INIT: In case more threads try to create a buffer at the same time
    pthread_mutex_lock(&buffer_initMutex);    
    // Check buffer conditions
    if(i_NumberOfBuffers >= (MAXIMUM_NUMBER_OF_BUFFERS - 1))
    {
        return BUFFER_ERROR_TOO_MANY_BUFFERS;
    }
    if( elements >= (MAXIMUM_NUMBER_OF_BUFFER_ELEMENTS - 1))
    {
        return BUFFER_ERROR_TOO_MANY_ELEMENTS;
    }    
    // Define the Buffer ID as i_NumberOfBuffers
    i_BufferID = i_NumberOfBuffers;
    i_NumberOfBuffers++;    
    // Init Buffer Mutex
    pthread_mutex_init(&buffer_Mutex[i_BufferID], NULL);
    // Fill Buffer
    buffers[i_BufferID].head = 0;
    buffers[i_BufferID].tail = 0;
    buffers[i_BufferID].count = 0;
    buffers[i_BufferID].elements = elements;
    buffers[i_BufferID].dataLen = malloc(sizeof(unsigned int*)*elements);
    buffers[i_BufferID].data = malloc(sizeof(void *)*elements);    
    // UNLOCK - INIT
    pthread_mutex_unlock(&buffer_initMutex);    
    // return BufferID
    return i_BufferID;
}

/* Returns if Buffer is Full
 */
int buffer_IsFull(int id)
{
    int i_Return;
    
    // Check Buffer
    if(buffers[id].count == buffers[id].elements)
    {
        i_Return = BUFFER_ERROR;
    }
    else
    {
        i_Return = BUFFER_OK;
    }
    
    // Return
    return i_Return;
}

/* Returns count of elements filled
 */
unsigned int buffer_dataCount(int id)
{
    // Return
    return buffers[id].count;
}

/* Add data to Buffer
 */
int buffer_push(int id, void *data, unsigned int size)
{
    int i_Return;
    int i_BufferCount;
        
    /* Get number of buffers: It is not protected because the initialization of
     * buffer to be pushed/popped is done only once, before the first push/pop.
     */
    i_BufferCount = i_NumberOfBuffers;
    
    // Check ID
    if(id >= i_BufferCount)
    {
        // Nothing is pushed/popped - wrong buffer ID
        return BUFFER_WRONG_ID;
    }
    
    // Protect in case push and pop take place at the same time
    pthread_mutex_lock(&buffer_Mutex[id]);
    
    // Check if buffer is full
    if(buffer_IsFull(id) == BUFFER_OK)
    {
        // Return OK
        i_Return = BUFFER_OK;
    }
    else
    {        
        // Return Overflow
        i_Return = BUFFER_ERROR;
        // fill buffer anyway, but first, eliminate last element
        free(buffers[id].data[buffers[id].tail]);
        buffers[id].data[buffers[id].tail] = NULL;
        // Update Tail
        buffers[id].tail = buffer_NextIndex(id, buffers[id].tail);
        buffers[id].count--;
        
    }
    // Update Data - Dynamic Allocation (keep a copy)    
    if(size > 0)
    {
        buffers[id].dataLen[buffers[id].head] = size;
        buffers[id].data[buffers[id].head] = malloc(size);
        memcpy(buffers[id].data[buffers[id].head], data, size);
    }
    else
    {
        buffers[id].dataLen[buffers[id].head] = 0;
        buffers[id].data[buffers[id].head] = NULL;
    }

    // Update Index
    buffers[id].head = buffer_NextIndex(id, buffers[id].head);
    buffers[id].count++;
    
    // Release MUTEX
    pthread_mutex_unlock(&buffer_Mutex[id]);
    
    // Return
    return i_Return;
}

/**
 * Returns the size of the next element to be removed. 
 */
unsigned int buffer_popSize(int id)
{
    unsigned int ui_Return;
    int i_BufferCount;
        
    /* Get number of buffers: It is not protected because the initialization of
     * buffer to be pushed/popped is done only once, before the first push/pop.
     */
    i_BufferCount = i_NumberOfBuffers;
    
    // Check ID
    if(id >= i_BufferCount)
    {
        // Return size 0
        return 0;
    }
    
    /* Protect in case push and pop take place at the same time.
     * Only LOCK is done here also because between this call and the call to pop
     * it is possible that a new element is added, and the tail is incremented.
     * In this case, the size returned by this function would not match the size 
     * of the pop function (pop would remove a different element).
     */    
    pthread_mutex_lock(&buffer_Mutex[id]);
    
    // Get size - Check if buffer is empty
    if(buffers[id].count == 0)
    {
        // Return empty buffer
        ui_Return = 0;
        // Unlock Mutex (maybe pop is not called) when size is 0
        pthread_mutex_unlock(&buffer_Mutex[id]);
    }
    else
    {
        ui_Return = buffers[id].dataLen[buffers[id].tail];    
    }
    // Return
    return ui_Return;
}

/**
 * Buffer Pop: Remove Element from buffer. Size of the element will be used 
 */
int buffer_pop(int id, void *data, unsigned int size)
{
    int i_Return;
    int i_BufferCount;
        
    /* Get number of buffers: It is not protected because the initialization of
     * buffer to be pushed/popped is done only once, before the first push/pop.
     */
    i_BufferCount = i_NumberOfBuffers;
    
    // Check ID
    if(id >= i_BufferCount)
    {
        // Nothing is pushed/popped - wrong buffer ID
        return BUFFER_WRONG_ID;
    }
    
    // Check if buffer is empty
    if(buffers[id].count == 0)
    {
        // Return empty buffer
        i_Return = BUFFER_ERROR;
    }
    else
    {
        // Return OK
        i_Return = BUFFER_OK;
        
        // Update Data
        if(size > 0)
        {
            // Copy Data
            memcpy(data, buffers[id].data[buffers[id].tail], size);        
            // Free data
            free(buffers[id].data[buffers[id].tail]);
            buffers[id].data[buffers[id].tail] = NULL;
        }
        else
        {
            data = NULL;                    
        }
        
        // Update Index
        buffers[id].tail = buffer_NextIndex(id, buffers[id].tail);
        buffers[id].count--;
    }
    /* See the function buffer_popSize.
     * MUTEX locked there is unlocked here
     */
    pthread_mutex_unlock(&buffer_Mutex[id]);
    
    // Return
    return i_Return;
}

/**
 * Remove all elemnts from the buffer
 */
void buffer_clean(int id)
{
    unsigned int li_count;
    
    // Protect in case push and pop take place at the same time
    pthread_mutex_lock(&buffer_Mutex[id]);
    
    // Get current number of elements
    li_count = buffers[id].count;
    while(li_count > 0)
    {
        free(buffers[id].data[buffers[id].tail]);
        buffers[id].data[buffers[id].tail] = NULL;
        buffers[id].tail = buffer_NextIndex(id, buffers[id].tail);
        li_count--;
    }
    
    // Re-Init indexes and counters
    buffers[id].head = 0;
    buffers[id].tail = 0;
    buffers[id].count = 0;    
    
    // Release MUTEX
    pthread_mutex_unlock(&buffer_Mutex[id]);
}

/**
 * For tests only - Delete a buffer
 */
void buffer_delete(int id)
{
    free(buffers[id].dataLen);
    free(buffers[id].data);
}

/**
 * Buffer Print - Debug
 */ 
void buffer_print(int id)
{
    #ifdef DEBUG_BUFFER
    unsigned int li_head;
    unsigned int li_tail;
    unsigned int li_count;
    char* lcp_Temp;
    int li_Temp;
    unsigned long long lull_Temp;
    struct can_frame cf_Frame;

    li_head = buffers[id].head;
    li_tail = buffers[id].tail;
    debug_print("----------\n");
    debug_print("Buffer: %d\n", id);
    debug_print("- Count: %u\n", buffers[id].count);
    debug_print("- Elements: %u\n", buffers[id].elements);
    debug_print("- Head: %u\n", li_head);
    debug_print("- Tail: %u\n", li_tail);
    li_count = 0;
    while(li_count < buffers[id].count)
    {
        debug_print("----------\n");
        debug_print("- Data Index: %u\n", li_count);
        switch(buffers[id].dataLen[li_tail])
        {
            case sizeof(char):
            case sizeof(int):
                li_Temp = 0;
                memcpy(&li_Temp, buffers[id].data[li_tail], buffers[id].dataLen[li_tail]);                
                debug_print("- Data: %d\n", li_Temp);
                break;
            case sizeof(unsigned long long):
                memcpy(&lull_Temp, buffers[id].data[li_tail], sizeof(lull_Temp));                
                debug_print("- Data: %llu\n", lull_Temp);
                break;
            case sizeof(struct can_frame):                
                li_Temp = buffers[id].dataLen[li_tail];
                lcp_Temp = malloc(li_Temp + 1);
                memcpy(lcp_Temp, buffers[id].data[li_tail], li_Temp);
                lcp_Temp[li_Temp] = '\0';
                debug_print("- Data (String): %s\n", lcp_Temp);
                free(lcp_Temp);
                memcpy(&cf_Frame, buffers[id].data[li_tail], li_Temp);
                debug_print("- CAN ID: 0x%08x\n", cf_Frame.can_id);
                for(li_Temp = 0; li_Temp < 8; li_Temp++)
                {
                    debug_print("- CAN Data[%d] = 0x%02x\n", li_Temp, cf_Frame.data[li_Temp]);
                }
            default:                
                li_Temp = buffers[id].dataLen[li_tail];
                lcp_Temp = malloc(li_Temp + 1);
                memcpy(lcp_Temp, buffers[id].data[li_tail], li_Temp);
                lcp_Temp[li_Temp] = '\0';
                debug_print("- Data: %s\n", lcp_Temp);
                free(lcp_Temp);
                break;            
        }
        if((li_tail + 1) >= buffers[id].elements)
        {
            li_tail = 0;
        }
        else
        {
            li_tail = li_tail + 1;
        } 
        li_count++;
    }    
    debug_print("----------\n");
    #endif     
}