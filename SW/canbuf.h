//----------------------------------------------------------------------------//
//                               OBJECT HISTORY                               //
//----------------------------------------------------------------------------//
//  REVISION |    DATE     |                               |      AUTHOR      //
//----------------------------------------------------------------------------//
//  1.00     | 28/Nov/2021 |                               | ALCP             //
// - Creation of file                                                         //
//----------------------------------------------------------------------------//

#ifndef CANBUF_H
#define CANBUF_H

#ifdef __cplusplus
extern "C" {
#endif

#include <linux/can.h>
#include <linux/can/raw.h>
    
//----------------------------------------------------------------------------//
// EXTERNAL DEFINITIONS
//----------------------------------------------------------------------------//
#define CAN_BUFFER_SIZE    60
enum
{
    SOCKETCAN_CHANNEL_0 = 0, // First CAN channel
    // SOCKETCAN_CHANNEL_1, // Second CAN channel
    SOCKETCAN_CHANNELS
};
enum
{
    CAN_READ_DATA_BUFFER = 0,
    CAN_READ_STAMP_BUFFER,
    CAN_WRITE_DATA_BUFFER,
    CAN_WRITE_STAMP_BUFFER,
    CAN_NUMBER_OF_BUFFERS
};

//-------------------------------------------------------------------------//
// EXTERNAL TYPES
//-------------------------------------------------------------------------//
// Send
#define CAN_SEND_OK                 1
#define CAN_SEND_NO_DATA            0
#define CAN_SEND_BUFFER_ERROR       -1  // Re-Init and clean Buffers: canbuf_close(1)
#define CAN_SEND_SOCKET_ERROR       -2  // Re-Init and keep Buffers: canbuf_close(0)
#define CAN_SEND_PARAMETER_ERROR    -3
// Receive
#define CAN_RECEIVE_OK              1
#define CAN_RECEIVE_NO_DATA         0
#define CAN_RECEIVE_BUFFER_ERROR    -1  // Re-Init and clean Buffers: canbuf_close(1)
#define CAN_RECEIVE_SOCKET_ERROR    -2  // Re-Init and keep Buffers: canbuf_close(0)
#define CAN_RECEIVE_PARAMETER_ERROR -3  

typedef enum
{
  CAN_DISCONNECTED,
  CAN_CONNECTED
}stateCAN_t;

//----------------------------------------------------------------------------//
// EXTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
/**
 * CAN Buffers Initialization
 * \param   channel     Channel 0 (SOCKETCAN_CHANNEL_0): can0 
 *                      Channel 1 (SOCKETCAN_CHANNEL_1): can1
 * \return  EXIT_SUCCESS
 *          EXIT_FAILURE (Close and Reinit Buffers)
 */
int canbuf_init(int channel);

/**
 * CAN Initialization - Socket Connection
 * \param   channel     Channel 0 (SOCKETCAN_CHANNEL_0): can0 
 *                      Channel 1 (SOCKETCAN_CHANNEL_1): can1
 * \return  EXIT_SUCCESS
 *          EXIT_FAILURE (Close and Reinit Buffers)
 */
int canbuf_connect(int channel);

/**
 * CAN Close connection: Close socket, re-inits buffers if needed.
 * \param   channel     Channel 0 (SOCKETCAN_CHANNEL_0): can0 
 *                      Channel 1 (SOCKETCAN_CHANNEL_1): can1
 * \param   cleanBuffers    if buffers shall be reinitialized (>0 = Yes)
 * \return  EXIT_SUCCESS / EXIT_FAILURE
 */
int canbuf_close(int channel, int cleanBuffers);

/**
 * CAN Get State (Socket State)
 * \param   channel     Channel 0 (SOCKETCAN_CHANNEL_0): can0 
 *                      Channel 1 (SOCKETCAN_CHANNEL_1): can1
 * \param   scp_state   State to be filled (CAN_DISCONNECTED / CAN_CONNECTED)
 * \return  EXIT_SUCCESS / EXIT_FAILURE
 */
int canbuf_getState(int channel, stateCAN_t* scp_state);

/**
 * Set Write buffer with data from parameters.
 * 
 * \param   pcf_Frame
 * \param   millisecondsSinceEpoch
 * 
 * \return  CAN_SEND_OK                 if data was set to buffer
 *          CAN_SEND_BUFFER_ERROR       if no data was set due to buffer error
 *          CAN_SEND_PARAMETER_ERROR    if no data was set due to channel error
 */
int canbuf_setWriteMsgToBuffer(int channel, struct can_frame* pcf_Frame, unsigned long long millisecondsSinceEpoch);

/**
 * CAN Send Data from Write Buffer
 * 
 * \param   channel     Channel 0 (SOCKETCAN_CHANNEL_0): can0 
 *                      Channel 1 (SOCKETCAN_CHANNEL_1): can1
 * \return  CAN_SEND_OK                 if data was sent
 *          CAN_SEND_NO_DATA            if no data available to be sent
 *          CAN_SEND_BUFFER_ERROR       if no data was sent due to buffer error
 *          CAN_SEND_SOCKET_ERROR       if no data was sent due to socket error
 *          CAN_SEND_PARAMETER_ERROR    if selected wrong channel
 */
int canbuf_send(int channel);

/**
 * Get data from Read buffer and set data from parameters.
 * 
 * \param   pcf_Frame
 * \param   millisecondsSinceEpoch
 * 
 * \return  CAN_RECEIVE_OK              if data was set to buffer
 *          CAN_RECEIVE_NO_DATA         if there is no data on the buffer
 *          CAN_RECEIVE_BUFFER_ERROR    if no data was set due to buffer error
 *          CAN_RECEIVE_PARAMETER_ERROR if no data was set due to channel error
 */
int canbuf_getReadMsgFromBuffer(int channel, struct can_frame* pcf_Frame, unsigned long long* millisecondsSinceEpoch);

/**
 * CAN read Data and fill Read Buffer
 * \param   timeout     timeout to wait for data on each channel in milliseconds
 *                      -1 equals to no timeout
 * \param   channel     Channel 0 (SOCKETCAN_CHANNEL_0): can0 
 *                      Channel 1 (SOCKETCAN_CHANNEL_1): can1
 * \return  CAN_RECEIVE_OK              if data was received
 *          CAN_RECEIVE_NO_DATA         if no data was received due to timeout
 *          CAN_RECEIVE_BUFFER_ERROR    if no data was received due to buffer error
 *          CAN_RECEIVE_SOCKET_ERROR    if no data was received due to socket error
 *          CAN_RECEIVE_PARAMETER_ERROR if selected wrong channel
 */
int canbuf_receive(int channel, int timeout);

#ifdef __cplusplus
}
#endif

#endif /* CANBUF_H */

