//----------------------------------------------------------------------------//
//                               OBJECT HISTORY                               //
//----------------------------------------------------------------------------//
//  REVISION |    DATE     |                               |      AUTHOR      //
//----------------------------------------------------------------------------//
//  1.00     | 28/Nov/2021 |                               | ALCP             //
// - Creation of file                                                         //
//----------------------------------------------------------------------------//

#ifndef GATEWAY_H
#define GATEWAY_H

#ifdef __cplusplus
extern "C" {
#endif

/*
* Includes
*/
#include "hapcan.h"
    
//----------------------------------------------------------------------------//
// EXTERNAL DEFINITIONS
//----------------------------------------------------------------------------//
enum
{
    GATEWAY_MQTT2CAN_LIST = 0,
    GATEWAY_CAN2MQTT_LIST,
    NUMBER_OF_GATEWAY_LISTS
};
    
//----------------------------------------------------------------------------//
// EXTERNAL TYPES
//----------------------------------------------------------------------------//

    
//----------------------------------------------------------------------------//
// EXTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
/**
 * Init Gateway data:
 * - empty the list and if list is available, free used memory
 * 
 **/
void gateway_init(void);

/**
 * Add an element to the list that will be used to match frames / topics
 * 
 * USED FOR MATCHING HAPCAN FRAME (CAN2MQTT):
 * \param   list        (INPUT) List to be added: GATEWAY_MQTT2CAN_LIST or 
 *                              GATEWAY_CAN2MQTT_LIST
 * \param   phd_mask    (INPUT) Mask for checking HAPCAN Frames
 * \param   phd_check   (INPUT) Check for checking HAPCAN Frames
 * \param   state_topic (INPUT) Topic to be returned in case of HAPCAN Frame 
 * 
 * USED FOR MATCHING TOPIC (MQTT2CAN): 
 * \param   command_topic   (INPUT) Topic to be matched
 * \param   phd_result      (INPUT) HAPCAN Frame to be returned
 *  
 * \return  EXIT_FAILURE     Wrong list used
 *          EXIT_SUCCESS     Element added to the list
 **/
int gateway_AddElementToList(int list, hapcanCANData *phd_mask, 
        hapcanCANData *phd_check, char *state_topic, char* command_topic,         
        hapcanCANData *phd_result);

/**
 * Search for a HAPCAN Frame starting from position offset
 * 
 * \param   phd_received    (INPUT) The HAPCAN Frame to be searched
 * \param   offset          (INPUT) Starting list index for the search
 *  
 * \return  >=0 index of the found match
 *          -1  Nothing found
 **/
int gateway_searchMQTTFromCAN(hapcanCANData *phd_received, int offset);

/**
 * Return the state topic of a given list index (offset)
 * 
 * \param   offset  (INPUT) Starting list index for the search
 * \param   topic   (OUTPUT) topic to be populated
 *  
 * \return  EXIT_SUCCESS    topic is copied (to be freed by application)
 *          EXIT_FAILURE    Nothing to get
 **/
int gateway_getMQTTFromCAN(int offset, char** topic);

/**
 * Search for a topic starting from position offset
 * 
 * \param   phd_received    (INPUT) The topic to be searched
 * \param   offset          (INPUT) Starting list index for the search
 *  
 * \return  >=0 index of the found match
 *          -1  Nothing found
 **/
int gateway_searchCANFromMQTT(char* const topic, int offset);

/**
 * Return the HAPCAN Frame of a given list index (offset)
 * 
 * \param   offset  (INPUT) Starting list index for the search
 * \param   p_hd    (OUTPUT) HAPCAN frame to be populated
 *  
 * \return  EXIT_SUCCESS    topic is copied (to be freed by application)
 *          EXIT_FAILURE    Nothing to get
 **/
int gateway_getCANFromMQTT(int offset, hapcanCANData *p_hd);

/**
 * USED FOR DEBUG ONLY
 * Print the gateway list
 * \param   list    (INPUT) List to be added: GATEWAY_MQTT2CAN_LIST or 
 *                      GATEWAY_CAN2MQTT_LIST
 **/
void gateway_printList(int list);

#ifdef __cplusplus
}
#endif

#endif /* GATEWAY_H */

