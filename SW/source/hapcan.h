//----------------------------------------------------------------------------//
//                               OBJECT HISTORY                               //
//----------------------------------------------------------------------------//
//  REVISION |    DATE     |                               |      AUTHOR      //
//----------------------------------------------------------------------------//
//  1.00     | 28/Nov/2021 |                               | ALCP             //
// - Creation of file                                                         //
//----------------------------------------------------------------------------//

#ifndef HAPCAN_H
#define HAPCAN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <linux/can.h>
#include <linux/can/raw.h>

//----------------------------------------------------------------------------//
// EXTERNAL DEFINITIONS
//----------------------------------------------------------------------------//    
// Function responses
enum
{
    HAPCAN_GENERIC_OK_RESPONSE = 0,
    HAPCAN_NO_RESPONSE,
    HAPCAN_SOCKET_RESPONSE,
    HAPCAN_MQTT_RESPONSE,
    HAPCAN_CAN_RESPONSE,
    HAPCAN_RESPONSE_ERROR,
    HAPCAN_MQTT_RESPONSE_ERROR,
    HAPCAN_CAN_RESPONSE_ERROR
};
// Maximum attempts to send status messages to a module without response
#define HAPCAN_CAN_STATUS_SEND_RETRIES 3
// General HAPCAN data characteristics
#define HAPCAN_DATA_LEN 8
#define HAPCAN_SOCKET_DATA_LEN 15
// SOCKET  
#define HAPCAN_MAX_RESPONSES 2
// FRAMES
// Normal Messages - Application frames handled by the functional firmware
#define HAPCAN_OPEN_COLLECTOR_FRAME_TYPE        0x309
#define HAPCAN_RGB_FRAME_TYPE                   0x308
#define HAPCAN_BLINDS_FRAME_TYPE                0x307
#define HAPCAN_DIMMER_FRAME_TYPE                0x306
#define HAPCAN_INFRARED_TRANSMITTER_FRAME_TYPE  0x305
#define HAPCAN_TEMPERATURE_FRAME_TYPE           0x304
#define HAPCAN_INFRARED_RECEIVER_FRAME_TYPE     0x303
#define HAPCAN_RELAY_FRAME_TYPE                 0x302
#define HAPCAN_BUTTON_FRAME_TYPE                0x301
#define HAPCAN_RTC_FRAME_TYPE                   0x300
#define HAPCAN_START_NORMAL_MESSAGES            0x200
// System Messages - handled by the functional firmware
#define HAPCAN_HEALTH_CHECK_REQUEST_NODE_FRAME_TYPE     0x115
#define HAPCAN_HEALTH_CHECK_REQUEST_GROUP_FRAME_TYPE    0x114
#define HAPCAN_UPTIME_REQUEST_NODE_FRAME_TYPE           0x113
#define HAPCAN_UPTIME_REQUEST_GROUP_FRAME_TYPE          0x112
#define HAPCAN_DIRECT_CONTROL_FRAME_TYPE                0x10A
#define HAPCAN_STATUS_REQUEST_NODE_FRAME_TYPE           0x109
#define HAPCAN_STATUS_REQUEST_GROUP_FRAME_TYPE          0x108  
// System Messages - handled by the bootloader in normal mode
#define HAPCAN_DEV_ID_REQUEST_NODE_FRAME_TYPE           0x111
#define HAPCAN_DEV_ID_REQUEST_GROUP_FRAME_TYPE          0x10F
#define HAPCAN_DESCRIPTION_REQUEST_NODE_FRAME_TYPE      0x10E
#define HAPCAN_DESCRIPTION_REQUEST_GROUP_FRAME_TYPE     0x10D
#define HAPCAN_SUPPLY_REQUEST_NODE_FRAME_TYPE           0x10C
#define HAPCAN_SUPPLY_REQUEST_GROUP_FRAME_TYPE          0x10B
#define HAPCAN_FW_TYPE_REQUEST_NODE_FRAME_TYPE          0x106
#define HAPCAN_FW_TYPE_REQUEST_GROUP_FRAME_TYPE         0x105
#define HAPCAN_HW_TYPE_REQUEST_NODE_FRAME_TYPE          0x104
#define HAPCAN_HW_TYPE_REQUEST_GROUP_FRAME_TYPE         0x103
    
// Ethernet module - fixed responses
#define HAPCAN_HW_ATYPE 102
#define HAPCAN_HW_AVERS 0 
#define HAPCAN_HW_FVERS 1
#define HAPCAN_HW_FREV  3
#define HAPCAN_HW_HWVER 3
#define HAPCAN_HW_HWTYPE    0x3000
#define HAPCAN_HW_FID   0x001010
#define HAPCAN_HW_ID0   0x00
#define HAPCAN_HW_ID1   0x11
#define HAPCAN_HW_ID2   0x22
#define HAPCAN_HW_ID3   0x33
#define HAPCAN_HW_BVER1 3
#define HAPCAN_HW_BVER2 4
#define HAPCAN_VOLBUS1  0x27
#define HAPCAN_VOLBUS2  0x58
#define HAPCAN_VOLCPU1  0x27
#define HAPCAN_VOLCPU2  0x58
#define HAPCAN_DEVID1   0xFF
#define HAPCAN_DEVID2   0xFF
// When the configuration file returns error on computer ID1 or computer ID2, 
// this default value is used for both
#define HAPCAN_DEFAULT_CIDx 254
    
//----------------------------------------------------------------------------//
// EXTERNAL TYPES
//----------------------------------------------------------------------------//
// HAPCAN frame
typedef struct  
{
    uint16_t frametype;
    uint8_t flags;
    uint8_t module;
    uint8_t group;
    uint8_t data[HAPCAN_DATA_LEN];
} hapcanCANData;

//----------------------------------------------------------------------------//
// EXTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
/**
 * Get HAPCAN CAN Data Struct from CAN Frame
 * \param   pcf_Frame   pointer to CAN Frame Data Struct
 *          hCD_ptr     pointer to HAPCAN CAN Data Struct
 *                      
 */
void hapcan_getHAPCANDataFromCAN(struct can_frame* pcf_Frame, 
        hapcanCANData* hCD_ptr);

/**
 * Get CAN Data Struct from HAPCAN Frame
 * \param   hCD_ptr     pointer to HAPCAN CAN Data Struct
 *          pcf_Frame   pointer to CAN Frame Data Struct
 *          
 */
void hapcan_getCANDataFromHAPCAN(hapcanCANData* hCD_ptr, 
        struct can_frame* pcf_Frame);

/**
 * Get Checksum from HAPCAN CAN Data Struct
 * \param   hCD_ptr     pointer to HAPCAN CAN Data Struct
 *                      
 * \return  Checksum
 *          
 */
uint8_t hapcan_getChecksumFromCAN(hapcanCANData* hCD_ptr);

/**
 * Set the RTC message
 * 
 * \param   hCD_ptr     pointer to HAPCAN CAN Data Struct
 *  
 **/
void hapcan_setHAPCANRTCMessage(hapcanCANData* hCD_ptr);

/**
 * Fill HAPCAN data with a system message based on destination node and group, 
 * for a given frametype
 * \param   hd_result       (OUTPUT) date to be filled                      
 *          frametype       (INPUT) frametype - system message
 *          node            (INPUT) destination node
 *          group           (INPUT) destination group
 */
void hapcan_getSystemFrame(hapcanCANData *hd_result, uint16_t frametype, 
        int node, int group);


/**
 * Init the gateway - Setup the lists based on configuration file
 *  
 **/
void hapcan_initGateway(void);

/**
 * Check the CAN message received, and add to MQTT Pub buffer the needed 
 * response(s)
 * \param   hapcanData      (INPUT) received HAPCAN Frame
 *          timestamp       (INPUT) Received message timestamp
 * 
 * \return  HAPCAN_NO_RESPONSE: No response was added to MQTT Pub Buffer
 *          HAPCAN_MQTT_RESPONSE: Response added to MQTT Buffer (OK)
 *          HAPCAN_MQTT_RESPONSE_ERROR: Error adding to MQTT Pub Buffer
 *          HAPCAN_RESPONSE_ERROR: Other error
 *          
 */
int hapcan_handleCAN2MQTT(hapcanCANData* hapcanData, 
        unsigned long long timestamp);

/**
 * Check the MQTT message received, and add to CAN Write buffer the needed 
 * response(s)
 * \param   topic           (INPUT) received topic
 *          payload         (INPUT) received payload
 *          payloadlen      (INPUT) received payload len
 *          timestamp       (INPUT) Received message timestamp
 * 
 * \return  HAPCAN_NO_RESPONSE: No response was added to CAN Write Buffer
 *          HAPCAN_CAN_RESPONSE: Response added to CAN Write Buffer (OK)
 *          HAPCAN_CAN_RESPONSE_ERROR: Error adding to MQTT Pub Buffer
 *          HAPCAN_RESPONSE_ERROR: Other error
 *          
 */
int hapcan_handleMQTT2CAN(char* topic, void* payload, int payloadlen, 
        unsigned long long timestamp);

/**
 * Add a HAPCAN Message to the CAN Write Buffer
 * \param   hapcanData      (INPUT) HAPCAN Frame to be added to CAN write buffer
 * \param   timestamp       (INPUT) Timestamp
 * \param   sendToSocket    (INPUT) If the message has to be added to the socket
 *                              Write Buffer
 * 
 * \return  HAPCAN_CAN_RESPONSE: Response added to CAN Write Buffer (OK)
 *          HAPCAN_CAN_RESPONSE_ERROR: Error adding to MQTT Pub Buffer
 *          
 */
int hapcan_addToCANWriteBuffer(hapcanCANData* hapcanData, 
        unsigned long long timestamp, bool sendToSocket);

/**
 * Add a MQTT Message to the MQTT Pub Buffer
 * \param   topic           (INPUT) received topic
 *          payload         (INPUT) received payload
 *          payloadlen      (INPUT) received payload len
 *          timestamp       (INPUT) Received message timestamp
 * 
 * \return  HAPCAN_NO_RESPONSE: No response was added to CAN Write Buffer
 *          HAPCAN_MQTT_RESPONSE: Response added to CAN Write Buffer (OK)
 *          HAPCAN_MQTT_RESPONSE_ERROR: Buffer Error
 */
int hapcan_addToMQTTPubBuffer(char* topic, void* payload, int payloadlen, 
        unsigned long long timestamp);

#ifdef __cplusplus
}
#endif

#endif /* HAPCAN_H */

