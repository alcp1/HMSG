//----------------------------------------------------------------------------//
//                               OBJECT HISTORY                               //
//----------------------------------------------------------------------------//
//  REVISION |    DATE     |                               |      AUTHOR      //
//----------------------------------------------------------------------------//
//  1.00     | 10/Dec/2021 |                               | ALCP             //
// - First version                                                            //
//----------------------------------------------------------------------------//

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include "debug.h"
#include "auxiliary.h"
#include "socketcan.h"

//----------------------------------------------------------------------------//
// GLOBAL VARIABLES
//----------------------------------------------------------------------------//

//----------------------------------------------------------------------------//
// INTERNAL FUNCTIONS
//----------------------------------------------------------------------------//

//----------------------------------------------------------------------------//
// EXTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
// Opens the connection to the CAN-bus
int socketcan_open(int channel) 
{
    int fd;
    struct ifreq ifr;
    struct sockaddr_can addr;
    
    // Check channel selected 
    if(channel == 0)
    {
        strcpy(ifr.ifr_name, "can0");
    }
    else if(channel == 1)
    {
        strcpy(ifr.ifr_name, "can1");
    }
    else
    {
        #if defined(DEBUG_SOCKETCAN_ERROR) || defined(DEBUG_SOCKETCAN_OPEN)
        debug_print("SocketCAN: Invalid Channel: %d\n", channel);
        #endif 
        return -1;
    }
    
    // Create the Socket
    fd = -1;
    fd = socket( PF_CAN, SOCK_RAW, CAN_RAW );
    if(fd  == -1) 
    {
        #if defined(DEBUG_SOCKETCAN_ERROR) || defined(DEBUG_SOCKETCAN_OPEN)
        debug_print("SocketCAN: Open Error - Socket Channel: %d\n", channel);
        #endif  
        return fd;
    }
    
    // Find the Socket Index
    if (ioctl(fd, SIOCGIFINDEX, &ifr) < 0) 
    {
        #if defined(DEBUG_SOCKETCAN_ERROR) || defined(DEBUG_SOCKETCAN_OPEN)
        debug_print("SocketCAN: Open Error - Socket Index not found - Channel: %d\n", channel);
        #endif
        // Close file and return
        close(fd);
        return -1;
    }

    /* To set the filters to zero filters is quite obsolete as to not read 
     * data causes the raw socket to discard the received CAN frames.
     * Therefore, no need to use setsockopt
     */    
    
    // Select that CAN interface, and bind the socket to it.    
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) 
    {
        #if defined(DEBUG_SOCKETCAN_ERROR) || defined(DEBUG_SOCKETCAN_OPEN)
        debug_print("SocketCAN: Bind Error - Channel: %d\n", channel);
        #endif
        // Close file and return
        close(fd);
        return -1;
    }

    // Set to non-blocking
    if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0) 
    {
        #if defined(DEBUG_SOCKETCAN_ERROR) || defined(DEBUG_SOCKETCAN_OPEN)
        debug_print("SocketCAN: Set to Non-Blocking Error - Channel: %d\n", channel);
        #endif
        // Close file and return
        close(fd);
        return -1;
    }

    // Return file descriptor
    #if defined(DEBUG_SOCKETCAN_OPEN) || defined(DEBUG_SOCKETCAN_OPENED)
    debug_print("SocketCAN: Open Successful - Socket Channel: %d\n", channel);
    debug_print("SocketCAN: Open Successful - FD: %d\n", fd);
    #endif
    return fd;
}

/* Closes the connection to the CAN-bus */
void socketcan_close(int fd) 
{
    /* "shutdown" is a flexible way to block communication in one or both 
     * directions. When the second parameter is SHUT_RDWR, it will block both 
     * sending and receiving (like close).
     * Example: shutdown(fd,SHUT_WR);
     * 
     * "close" is the way to actually destroy a socket.
     * 
     * With shutdown, you will still be able to receive pending data the peer 
     * already sent */
    #ifdef DEBUG_SOCKETCAN_OPEN
    debug_print("SocketCAN: Close - FD: %d\n", fd);
    #endif
    close(fd);
}

/* Checks if there is data to be read from the CAN-bus */
int socketcan_read(int fd, struct can_frame* pcf_Frame, int timeout)
{
    // Wait for data or timeout
    int i_ReadLength;
    int i_Temp;
    int i_Return;
    struct pollfd pfds[1];

    pfds[0].fd = fd;
    pfds[0].events = POLLIN;		
    
    // Check if there is data available
    i_Temp = poll(pfds, 1, timeout);
    if(i_Temp > 0)
    {
        #ifdef DEBUG_SOCKETCAN_READ_FULL						
        debug_print("SocketCAN: Read Poll OK!\n");
        #endif
        // Only set return to 0 in the end of the function - read could still have errors
        i_Return = SOCKETCAN_OTHER_ERROR;
    }
    else if(i_Temp == 0)
    {
        #ifdef DEBUG_SOCKETCAN_READ_FULL						
        debug_print("SocketCAN: Read Poll Error (Timeout)!\n");
        debug_print("- File: %d\n", fd);
        debug_print("- Error: %d\n", i_Temp);
        #endif	
        i_Return = SOCKETCAN_TIMEOUT;	
    }
    else if(i_Temp == -1)
    {
        #if defined(DEBUG_SOCKETCAN_READ_FULL) || defined(DEBUG_SOCKETCAN_ERROR)						
        debug_print("SocketCAN: Read Poll Error (Generic)!\n");
        debug_print("- File: %d\n", fd);
        debug_print("- Error: %d\n", i_Temp);
        #endif
        i_Return = SOCKETCAN_ERROR;
    }
    else
    {
        #if defined(DEBUG_SOCKETCAN_READ_FULL) || defined(DEBUG_SOCKETCAN_ERROR)
        debug_print("SocketCAN: Read Poll Error (Other)!\n");
        debug_print("- File: %d\n", fd);
        debug_print("- Error: %d\n", i_Temp);
        #endif
        i_Return = SOCKETCAN_OTHER_ERROR;
    }

    // Try to read available data
    i_ReadLength = read(fd, pcf_Frame, sizeof(struct can_frame));
    if(i_ReadLength <= 0) 
    {        
        // Error, no bytes read
        #ifdef DEBUG_SOCKETCAN_READ_FULL						
        debug_print("SocketCAN: No Bytes Read!\n");
        debug_print("- File: %d\n", fd);
        #endif
        // Return
        return i_Return;
    }

    // Number of bytes check (according to manual, it is a paranoid check)
    if (i_ReadLength < sizeof(struct can_frame))
    {
        #if defined(DEBUG_SOCKETCAN_READ_FULL) || defined(DEBUG_SOCKETCAN_ERROR)					
        debug_print("SocketCAN: Incomplete Bytes Read!\n");
        debug_print("- File: %d\n", fd);
        debug_print("- Bytes Read: %d\n", i_ReadLength);
        #endif
        // Return
        return i_Return;
    } 
    // Check for error frame
    if(pcf_Frame->can_id & CAN_ERR_FLAG)
    {
        #if defined(DEBUG_SOCKETCAN_READ_FULL) || defined(DEBUG_SOCKETCAN_ERROR)
        debug_print("SocketCAN ERROR: Error Frame Detected!\n");
        #endif
        return SOCKETCAN_ERROR_FRAME;
    }
    // Clear Flags
    pcf_Frame->can_id = (pcf_Frame->can_id & CAN_EFF_MASK);
    
    // Debug Event
    #ifdef DEBUG_SOCKETCAN_READ_EVENTS
    debug_print("SocketCAN Read: New Frame Read. FD = %d!\n", fd);
    #endif
    
    // At this point, read is OK!
    return SOCKETCAN_OK;
}

/* Writes the data to the CAN-bus */
int socketcan_write(int fd, struct can_frame* pcf_Frame)
{
    int i_WriteLength;
    
    // Adjust to Extended ID
    pcf_Frame->can_id = pcf_Frame->can_id | CAN_EFF_FLAG;

    // Write Frame
    i_WriteLength = write(fd, pcf_Frame, sizeof(struct can_frame));
    if(i_WriteLength < 0) 
    {
        // Error, no data written
        #if defined(DEBUG_SOCKETCAN_WRITE) || defined(DEBUG_SOCKETCAN_ERROR)
        debug_print("SocketCAN: Write ERROR!\n");
        #endif
        return SOCKETCAN_ERROR;
    }
    
    // Number of bytes check (according to manual, it is a paranoid check)
    if (i_WriteLength < sizeof(struct can_frame))
    {
        #if defined(DEBUG_SOCKETCAN_WRITE) || defined(DEBUG_SOCKETCAN_ERROR)
        debug_print("SocketCAN: Incomplete Bytes Write!\n");
        debug_print("- File: %d\n", fd);
        debug_print("- Bytes Written: %d\n", i_WriteLength);
        #endif
        // Return
        return SOCKETCAN_ERROR;
    }
    
    // At this point, write operation is finished OK!
    #ifdef DEBUG_SOCKETCAN_WRITE
    debug_print("SocketCAN: Write OK!\n");
    #endif
    return 0;
}