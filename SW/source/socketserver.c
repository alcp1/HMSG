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
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include "auxiliary.h"
#include "buffer.h"
#include "config.h"
#include "debug.h"
#include "hapcan.h"
#include "socketserver.h"
#include "socketserverbuf.h"

//----------------------------------------------------------------------------//
// GLOBAL VARIABLES
//----------------------------------------------------------------------------//
static int fdListener = -1;     // Listening socket descriptor
static int fdAccepted = -1;     // Accepted socket descriptor

//----------------------------------------------------------------------------//
// INTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
static void *get_in_addr(struct sockaddr *sa);
static int get_listener_socket(void);

// Get sockaddr, IPv4 or IPv6:
static void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)  
    {
        // IPv4
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    // IPv6
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// Return a listening socket
static int get_listener_socket(void)
{
    int fd;
    int yes = 1;        // For setsockopt() SO_REUSEADDR, below
    int rv;
    struct addrinfo hints, *ai, *p;
    int check;
    char *port;
    
    //*******************************//
    // READ CONFIGURATION            //
    //*******************************//
    check = config_getString(CONFIG_GENERAL_SETTINGS_LEVEL, 0, 
            "socketServerPort", 0, NULL, &port);
    if(check != EXIT_SUCCESS)
    {
        return -1;
    }

    // Get us a socket and bind it
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;      // IPV4 or IPV6
    hints.ai_socktype = SOCK_STREAM;  // TCP
    hints.ai_flags = AI_PASSIVE;      
    if((rv = getaddrinfo(NULL, port, &hints, &ai)) != 0) 
    {
        #if defined(DEBUG_SOCKETSERVER_ERROR) || defined(DEBUG_SOCKETSERVER_OPEN)
        debug_print("Socket Server ERROR: Listener Socket: %s\n", gai_strerror(rv));
        #endif         
        check = -1;
    }
    free(port);
    if(check == -1)
    {
        return -1;
    }
    // Check all Address Infos
    for(p = ai; p != NULL; p = p->ai_next) 
    {
        fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd < 0) 
        { 
            // Go to the next Address Info
            continue;
        }
        // Lose the pesky "address already in use" error message
        if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) 
        {
            #if defined(DEBUG_SOCKETSERVER_ERROR) || defined(DEBUG_SOCKETSERVER_OPEN)
            debug_print("Socket Server ERROR: setsockopt\n");
            #endif 
            // Go to the next Address Info
            continue;
        }
        if (bind(fd, p->ai_addr, p->ai_addrlen) < 0) 
        {
            perror("bind failed. Error");
            #if defined(DEBUG_SOCKETSERVER_ERROR) || defined(DEBUG_SOCKETSERVER_OPEN)
            debug_print("Socket Server ERROR: bind\n");
            #endif 
            close(fd);
            continue;
        }
        // Bind is OK: Leave
        break;
    }
    // Free all Address Infos
    freeaddrinfo(ai);
    // Check if we have a valid Address Info
    if (p == NULL) 
    {
        return -1;
    }
    // Listen to up to 10 connection attempts
    if (listen(fd, 10) < 0) 
    {
        return -1;
    }
    // Return Listener file description
    return fd;
}

//----------------------------------------------------------------------------//
// EXTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
/**
 *     - Init the socket
 *     - Listens for connections
 *     - Accept connections
 *     - Returns the socket for the accepted connection
 * * \return            >0: socket for the accepted connection
 *                      SOCKETSERVER_ERROR         poll error (generic)
 *                      SOCKETSERVER_TIMEOUT       timeout (no data)
 *                      SOCKETSERVER_OTHER_ERROR   poll error or data size error
 */
int socketserver_open(int timeout)
{
    int i_Temp;
    struct pollfd pfds[1]; // One single connection at the same time 
    struct sockaddr_storage remoteaddr; // Client address
    socklen_t addrlen;
    
    // Check if connection is already established
    if( fdAccepted >= 0 )
    {
        return fdAccepted;
    }
    
    // Set up and get a listening socket
    if (fdListener < 0) 
    {
        fdListener = get_listener_socket();
    }

    if (fdListener < 0) 
    {
        #if defined(DEBUG_SOCKETCAN_ERROR) || defined(DEBUG_SOCKETSERVER_OPEN)
        debug_print("Socket Server ERROR: Listener error\n");
        #endif
        return SOCKETSERVER_ERROR;
    }
    #ifdef DEBUG_SOCKETSERVER_OPEN
    debug_print("Socket Server Open: Listener fd = %d\n", fdListener);
    #endif

    pfds[0].fd = fdListener;
    pfds[0].events = POLLIN;		
    
    // Check if there is data available
    i_Temp = poll(pfds, 1, timeout);
    if(i_Temp > 0)
    {
        #ifdef DEBUG_SOCKETSERVER_OPEN						
        debug_print("SocketServer: Open Poll OK!\n");
        #endif
    }
    else if(i_Temp == 0)
    {
        #ifdef DEBUG_SOCKETSERVER_OPEN						
        debug_print("SocketServer: Open Poll Error (Timeout)!\n");
        debug_print("- File: %d\n", fdListener);
        debug_print("- Error: %d\n", i_Temp);
        #endif	
        return SOCKETSERVER_TIMEOUT;	
    }
    else if(i_Temp == -1)
    {
        #if defined(DEBUG_SOCKETSERVER_OPEN) || defined(DEBUG_SOCKETSERVER_ERROR)						
        debug_print("SocketServer: Open Poll Error (Generic)!\n");
        debug_print("- File: %d\n", fdListener);
        debug_print("- Error: %d\n", i_Temp);
        #endif
        return SOCKETSERVER_ERROR;
    }
    else
    {
        #if defined(DEBUG_SOCKETSERVER_OPEN) || defined(DEBUG_SOCKETSERVER_ERROR)
        debug_print("SocketServer: Open Poll Error (Other)!\n");
        debug_print("- File: %d\n", fdListener);
        debug_print("- Error: %d\n", i_Temp);
        #endif
        return SOCKETSERVER_OTHER_ERROR;
    }
    
    // At this point, there is a connection to be accepted.
    addrlen = sizeof remoteaddr;
    fdAccepted = accept(fdListener, (struct sockaddr *)&remoteaddr, &addrlen);
    if (fdAccepted < 0) 
    {
        #if defined(DEBUG_SOCKETSERVER_OPEN) || defined(DEBUG_SOCKETSERVER_ERROR)
        debug_print("SocketServer: Accept Error!\n");
        debug_print("- File: %d\n", fdAccepted);
        #endif
        return SOCKETSERVER_ERROR;
    }
    else
    {
        #if defined(DEBUG_SOCKETSERVER_OPEN) || defined(DEBUG_SOCKETSERVER_OPENED)
        char remoteIP[INET6_ADDRSTRLEN];  // Length for IPv4 or IPv6
        debug_print("SocketServer: Accept OK!\n");
        debug_print("    - Client: %s\n", inet_ntop(remoteaddr.ss_family,
        get_in_addr((struct sockaddr*)&remoteaddr),
        remoteIP, INET6_ADDRSTRLEN));
        debug_print("    - Socket: %d\n", fdAccepted);
        #endif
        return fdAccepted;
    }
}


/** Closes the connection to the Client */
void socketserver_close(void)
{
    #ifdef DEBUG_SOCKETCAN_OPEN
    debug_print("SocketCAN: Close - Listener FD: %d\n", fdListener);
    debug_print("SocketCAN: Close - Accepted FD: %d\n", fdAccepted);
    #endif
    close(fdListener);
    close(fdAccepted);
    fdListener = -1;
    fdAccepted = -1;
}


/**
 * Checks if there is HAPCAN data to be read from the connected client
 * \return              SOCKETSERVER_OK            data read OK
 *                      SOCKETSERVER_ERROR         poll error (generic)
 *                      SOCKETSERVER_TIMEOUT       timeout (no data)
 *                      SOCKETSERVER_CLOSED        closed connection (no data)
 *                      SOCKETSERVER_OTHER_ERROR   poll error or data size error
 */
int socketserver_read(uint8_t* data, int* dataLen, int timeout)
{
    // Wait for data or timeout
    int i_ReadLength;
    int i_Temp;
    struct pollfd pfds[1];

    pfds[0].fd = fdAccepted;
    pfds[0].events = POLLIN;		
    
    // Check if there is data available
    i_Temp = poll(pfds, 1, timeout);
    if(i_Temp > 0)
    {
        #ifdef DEBUG_SOCKETSERVER_READ_FULL						
        debug_print("SocketCANServer: Read Poll OK!\n");
        #endif
    }
    else if(i_Temp == 0)
    {
        #ifdef DEBUG_SOCKETSERVER_READ_FULL						
        debug_print("SocketCANServer: Read Poll Error (Timeout)!\n");
        debug_print("- File: %d\n", fdAccepted);
        debug_print("- Error: %d\n", i_Temp);
        #endif	
        return SOCKETSERVER_TIMEOUT;	
    }
    else if(i_Temp == -1)
    {
        #if defined(DEBUG_SOCKETSERVER_READ_FULL) || defined(DEBUG_SOCKETSERVER_ERROR)						
        debug_print("SocketCANServer: Read Poll Error (Generic)!\n");
        debug_print("- File: %d\n", fdAccepted);
        debug_print("- Error: %d\n", i_Temp);
        #endif
        return SOCKETSERVER_ERROR;
    }
    else
    {
        #if defined(DEBUG_SOCKETSERVER_READ_FULL) || defined(DEBUG_SOCKETSERVER_ERROR)
        debug_print("SocketCANServer: Read Poll Error (Other)!\n");
        debug_print("- File: %d\n", fdAccepted);
        debug_print("- Error: %d\n", i_Temp);
        #endif
        return SOCKETSERVER_OTHER_ERROR;
    }

    // Try to read available data
    i_ReadLength = recv(fdAccepted, data, HAPCAN_SOCKET_DATA_LEN,0);
    if(i_ReadLength < 0) 
    {        
        // Error, no bytes read
        #ifdef DEBUG_SOCKETSERVER_READ_FULL						
        debug_print("SocketCANServer: No Bytes Read!\n");
        debug_print("- File: %d\n", fdAccepted);
        #endif
        // Return
        return SOCKETSERVER_OTHER_ERROR;
    }
    else if(i_ReadLength == 0) 
    {        
        // Connection closed by client
        #ifdef DEBUG_SOCKETSERVER_READ_FULL						
        debug_print("SocketCANServer: Connection closed by client!\n");
        debug_print("- File: %d\n", fdAccepted);
        #endif
        // Return
        return SOCKETSERVER_CLOSED;
    }
    // Number of bytes check
    if (i_ReadLength > HAPCAN_SOCKET_DATA_LEN)
    {
        #if defined(DEBUG_SOCKETSERVER_READ_FULL) || defined(DEBUG_SOCKETSERVER_ERROR)					
        debug_print("SocketCANServer: Too many Bytes Bytes Read!\n");
        debug_print("- File: %d\n", fdAccepted);
        debug_print("- N Bytes Read: %d\n", i_ReadLength);
        for( int i_index = 0; i_index < i_ReadLength; i_index++)
        {
            debug_print("- data[%d] = %d\n", i_index, data[i_index]);
        }
        #endif
        // Return
        return SOCKETSERVER_OVERFLOW;
    } 
    
    // Debug Event
    #ifdef DEBUG_SOCKETSERVER_READ_EVENTS
    debug_print("SocketCANServer Read: New Frame Read (%d bytes received). FD = %d!\n", i_ReadLength, fdAccepted);
    #endif
    
    // At this point, read is OK!
    *dataLen = i_ReadLength;
    return SOCKETSERVER_OK;
}


/* Writes HAPCAN data to the connected client */
int socketserver_write(uint8_t* data, int dataLen)
{
    int i_WriteLength;
    
    // Write Frame
    i_WriteLength = send(fdAccepted, data, dataLen, 0);
    if(i_WriteLength < 0) 
    {
        // Error, no data written
        #if defined(DEBUG_SOCKETSERVER_WRITE) || defined(DEBUG_SOCKETSERVER_ERROR)
        debug_print("SocketCANServer: Write ERROR!\n");
        #endif
        return SOCKETSERVER_ERROR;
    }
    
    // Number of bytes check
    if (i_WriteLength < dataLen)
    {
        #if defined(DEBUG_SOCKETSERVER_WRITE) || defined(DEBUG_SOCKETSERVER_ERROR)
        debug_print("SocketCANServer: Incomplete Bytes Write!\n");
        debug_print("- File: %d\n", fdAccepted);
        debug_print("- Bytes Written: %d\n", i_WriteLength);
        #endif
        // Return
        return SOCKETSERVER_ERROR;
    }
    
    // At this point, write operation is finished OK!
    #ifdef DEBUG_SOCKETSERVER_WRITE
    debug_print("SocketCANServer: Write OK!\n");
    #endif
    return SOCKETSERVER_OK;
}