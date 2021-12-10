//----------------------------------------------------------------------------//
//                               OBJECT HISTORY                               //
//----------------------------------------------------------------------------//
//  REVISION |    DATE     |                               |      AUTHOR      //
//----------------------------------------------------------------------------//
//  1.00     | 28/Nov/2021 |                               | ALCP             //
// - Creation of file                                                         //
//----------------------------------------------------------------------------//

#ifndef HAPCANSOCKET_H
#define HAPCANSOCKET_H

#ifdef __cplusplus
extern "C" {
#endif

//----------------------------------------------------------------------------//
// EXTERNAL DEFINITIONS
//----------------------------------------------------------------------------//    
    
//----------------------------------------------------------------------------//
// EXTERNAL TYPES
//----------------------------------------------------------------------------//
    
//----------------------------------------------------------------------------//
// EXTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
/**
 * Get HAPCAN Socket Data Byte Array from HAPCAN CAN Data Struct
 * \param   hCD_ptr     pointer to HAPCAN CAN Data Struct
 *          hSD_ptr     pointer to HAPCAN Socket Data Struct
 *                      
 */
void hs_getSocketArrayFromHAPCAN(hapcanCANData* hCD_ptr, uint8_t* hSD_ptr);

/**
 * Get HAPCAN CAN Data Struct from HAPCAN Socket Data Byte Array
 * \param   hSD_ptr     pointer to HAPCAN Socket Data Struct
 *          hCD_ptr     pointer to HAPCAN CAN Data Struct
 *                      
 */
void hs_getHAPCANFromSocketArray(uint8_t* hSD_ptr, hapcanCANData* hCD_ptr);

/**
 * Handle a message received by the Socket Server (sent by HAPCAN PROGRAMMER)
 * 
 * \param   data            Socket data (INPUT)
 * \param   dataLen         Socket data length (INPUT)
 *  
 **/
void hs_handleMsgFromSocket(uint8_t* data, int dataLen, 
        unsigned long long timestamp);


#ifdef __cplusplus
}
#endif

#endif /* HAPCAN_H */

