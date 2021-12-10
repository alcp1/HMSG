//----------------------------------------------------------------------------//
//                               OBJECT HISTORY                               //
//----------------------------------------------------------------------------//
//  REVISION |    DATE     |                               |      AUTHOR      //
//----------------------------------------------------------------------------//
//  1.00     | 28/Nov/2021 |                               | ALCP             //
// - Creation of file                                                         //
//----------------------------------------------------------------------------//

#ifndef SOCKETSERVER_H
#define SOCKETSERVER_H

#ifdef __cplusplus
extern "C" {
#endif
    
#include <stdint.h>
    
//----------------------------------------------------------------------------//
// EXTERNAL DEFINITIONS
//----------------------------------------------------------------------------//  
#define SOCKETSERVER_OK            0
#define SOCKETSERVER_ERROR         -1
#define SOCKETSERVER_TIMEOUT       -2
#define SOCKETSERVER_ERROR_FRAME   -3
#define SOCKETSERVER_OTHER_ERROR   -4
#define SOCKETSERVER_CLOSED        -5
#define SOCKETSERVER_OVERFLOW      -6

//----------------------------------------------------------------------------//
// EXTERNAL TYPES
//----------------------------------------------------------------------------//

//----------------------------------------------------------------------------//
// EXTERNAL FUNCTIONS
//----------------------------------------------------------------------------//

/**
 * Returns the file descriptor for the connected socket.
 *     - Init the socket
 *     - Listens for connections
 *     - Accept connections
 *     - Returns the socket for the accepted connection
 *
 * \return              file pointer on success, -1 on error
 */
int socketserver_open(int timeout);


/** Closes the connection to the Client 
 */
void socketserver_close(void);


/**
 * Checks if there is HAPCAN data to be read from the connected client
 * \param data     where to save the data
 * \param dataLen  size of the data
 * \param timeout       milliseconds, -1 equals no timeout
 * \return              SOCKETSERVER_OK            data read OK
 *                      SOCKETSERVER_ERROR         poll error (generic)
 *                      SOCKETSERVER_TIMEOUT       timeout (no data)
 *                      SOCKETSERVER_OTHER_ERROR   poll error or data size error
 */
int socketserver_read(uint8_t* data, int* dataLen, int timeout);


/**
 * Writes HAPCAN data to the connected client
 * \param data       data to be written
 * \param dataLen    size of data to be written
 * \return           0 on success, -1 on error
 */
int socketserver_write(uint8_t* data, int dataLen);
    


#ifdef __cplusplus
}
#endif

#endif /* SOCKETSERVER_H */

