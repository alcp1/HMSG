//----------------------------------------------------------------------------//
//                               OBJECT HISTORY                               //
//----------------------------------------------------------------------------//
//  REVISION |    DATE     |                               |      AUTHOR      //
//----------------------------------------------------------------------------//
//  1.00     | 28/Nov/2021 |                               | ALCP             //
// - Creation of file                                                         //
//----------------------------------------------------------------------------//

#ifndef SOCKETSERVERBUF_H
#define SOCKETSERVERBUF_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

//----------------------------------------------------------------------------//
// EXTERNAL DEFINITIONS
//----------------------------------------------------------------------------//
#define SOCKETSERVER_BUFFER_SIZE    60
enum
{
    SOCKETSERVER_READ_DATA_BUFFER = 0,
    SOCKETSERVER_READ_STAMP_BUFFER,
    SOCKETSERVER_WRITE_DATA_BUFFER,
    SOCKETSERVER_WRITE_STAMP_BUFFER,
    SOCKETSERVER_NUMBER_OF_BUFFERS
};

//-------------------------------------------------------------------------//
// EXTERNAL TYPES
//-------------------------------------------------------------------------//
// Send
#define SOCKETSERVER_SEND_OK                 1
#define SOCKETSERVER_SEND_NO_DATA            0
#define SOCKETSERVER_SEND_BUFFER_ERROR       -1  // Re-Init and clean Buffers: socketserverbuf_close(1)
#define SOCKETSERVER_SEND_SOCKET_ERROR       -2  // Re-Init and keep Buffers: socketserverbuf_close(0)
#define SOCKETSERVER_SEND_PARAMETER_ERROR    -3
// Receive
#define SOCKETSERVER_RECEIVE_OK              1
#define SOCKETSERVER_RECEIVE_NO_DATA         0
#define SOCKETSERVER_RECEIVE_BUFFER_ERROR    -1  // Re-Init and clean Buffers: socketserverbuf_close(1)
#define SOCKETSERVER_RECEIVE_SOCKET_ERROR    -2  // Re-Init and keep Buffers: socketserverbuf_close(0)
#define SOCKETSERVER_RECEIVE_CLOSED_ERROR    -3  // Re-Init and keep Buffers: socketserverbuf_close(0)
#define SOCKETSERVER_RECEIVE_OVERFLOW        -4  // Re-Init and keep Buffers: socketserverbuf_close(0)

typedef enum
{
  SOCKETSERVER_DISCONNECTED,
  SOCKETSERVER_CONNECTED
}stateSocketServer_t;

//----------------------------------------------------------------------------//
// EXTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
/**
 * Socket Server Buffers Initialization
 * \return  EXIT_SUCCESS
 *          EXIT_FAILURE (Close and Reinit Buffers)
 */
int socketserverbuf_init(void);

/**
 * Socket Server Initialization - Socket Connection
 * \param   timeout   timeout in ms to wait for connection 
 * \return  EXIT_SUCCESS
 *          EXIT_FAILURE (Close and Reinit Buffers)
 */
int socketserverbuf_connect(int timeout);

/**
 * Socket Server Close connection: Close socket, re-inits buffers if needed.
 * \param   cleanBuffers    if buffers shall be reinitialized (>0 = Yes)
 * \return  EXIT_SUCCESS / EXIT_FAILURE
 */
int socketserverbuf_close(int cleanBuffers);

/**
 * Socket Server Get State (Socket State)
 * \param   scp_state   State to be filled (SOCKETSERVER_DISCONNECTED / SOCKETSERVER_CONNECTED)
 * \return  EXIT_SUCCESS / EXIT_FAILURE
 */
int socketserverbuf_getState(stateSocketServer_t* s_state);

/**
 * Set Write buffer with data from parameters.
 * 
 * \param   data                                    HAPCAN Socket Data
 * \param   data                                    HAPCAN Socket Data Length
 * \param   millisecondsSinceEpoch                  Time Stamp
 * 
 * \return  SOCKETSERVER_SEND_OK                if data was set to buffer
 *          SOCKETSERVER_SEND_BUFFER_ERROR      if no data was set due to buffer error
 *          SOCKETSERVER_SEND_NO_DATA           if socket is disconnected
 */
int socketserverbuf_setWriteMsgToBuffer(uint8_t* data, int dataLen, unsigned long long millisecondsSinceEpoch);

/**
 * Socket Server Send Data from Write Buffer
 * 
 * \return  SOCKETSERVER_SEND_OK                 if data was sent
 *          SOCKETSERVER_SEND_NO_DATA            if no data available to be sent
 *          SOCKETSERVER_SEND_BUFFER_ERROR       if no data was sent due to buffer error
 *          SOCKETSERVER_SEND_SOCKET_ERROR       if no data was sent due to socket error
 */
int socketserverbuf_send(void);

/**
 * Get data from Read buffer and set data from parameters.
 * 
 * \param   data                                        HAPCAN Socket Data
 * \param   millisecondsSinceEpoch                      Time Stamp
 *  
 * \return  SOCKETSERVER_RECEIVE_OK              	if data was set to buffer
 *          SOCKETSERVER_RECEIVE_NO_DATA         	if there is no data on the buffer
 *          SOCKETSERVER_RECEIVE_BUFFER_ERROR    	if no data was set due to buffer error
 */
int socketserverbuf_getReadMsgFromBuffer(uint8_t* data, int* dataLen, unsigned long long* millisecondsSinceEpoch);

/**
 * Socket Server read Data and fill Read Buffer
 * \param   timeout     timeout to wait for data on each channel in milliseconds
 *                      -1 equals to no timeout
 * \return  SOCKETSERVER_RECEIVE_OK              if data was received
 *          SOCKETSERVER_RECEIVE_NO_DATA         if no data was received due to timeout
 *          SOCKETSERVER_RECEIVE_BUFFER_ERROR    if no data was received due to buffer error
 *          SOCKETSERVER_RECEIVE_SOCKET_ERROR    if no data was received due to socket error
 */
int socketserverbuf_receive(int timeout);

#ifdef __cplusplus
}
#endif

#endif /* SOCKETSERVERBUF_H */

