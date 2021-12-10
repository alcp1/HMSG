//----------------------------------------------------------------------------//
//                               OBJECT HISTORY                               //
//----------------------------------------------------------------------------//
//  REVISION |    DATE     |                               |      AUTHOR      //
//----------------------------------------------------------------------------//
//  1.00     | 28/Nov/2021 |                               | ALCP             //
// - Creation of file                                                         //
//----------------------------------------------------------------------------//

#ifndef SOCKETCAN_H
#define SOCKETCAN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <linux/can.h>
#include <linux/can/raw.h>

//----------------------------------------------------------------------------//
// EXTERNAL DEFINITIONS
//----------------------------------------------------------------------------//
#define SOCKETCAN_OK            0
#define SOCKETCAN_ERROR         -1
#define SOCKETCAN_TIMEOUT       -2
#define SOCKETCAN_ERROR_FRAME   -3
#define SOCKETCAN_OTHER_ERROR   -4

//----------------------------------------------------------------------------//
// EXTERNAL TYPES
//----------------------------------------------------------------------------//

//----------------------------------------------------------------------------//
// EXTERNAL FUNCTIONS
//----------------------------------------------------------------------------//

/**
 * Opens the connection to the CAN-bus.
 *
 * \param channel       Channel 0: can0 / Channel 1: can1
 * \return              file pointer on success, -1 on error
 */
int socketcan_open(int channel);


/** Closes the connection to the CAN-bus */
void socketcan_close(int fd);


/**
 * Checks if there is data to be read from the CAN-bus
 * \param pcf_Frame     where to save the data
 * \param timeout       milliseconds, -1 equals no timeout
 * \return              SOCKETCAN_OK            data read OK
 *                      SOCKETCAN_ERROR         poll error (generic)
 *                      SOCKETCAN_TIMEOUT       timeout (no data)
 *                      SOCKETCAN_OTHER_ERROR   poll error or data size error
 */
int socketcan_read(int fd, struct can_frame* pcf_Frame, int timeout);


/**
 * Writes the data to the CAN-bus
 * \param pcf_Frame     data to be written
 * \return              0 on success, -1 on error
 */
int socketcan_write(int fd, struct can_frame* pcf_Frame);


#ifdef __cplusplus
}
#endif

#endif /* SOCKETCAN_H */

