//----------------------------------------------------------------------------//
//                               OBJECT HISTORY                               //
//----------------------------------------------------------------------------//
//  REVISION |    DATE     |                               |      AUTHOR      //
//----------------------------------------------------------------------------//
//  1.00     | 28/Nov/2021 |                               | ALCP             //
// - Creation of file                                                         //
//----------------------------------------------------------------------------//

#ifndef BUFFER_H
#define BUFFER_H

#ifdef __cplusplus
extern "C" {
#endif

//----------------------------------------------------------------------------//
// EXTERNAL DEFINITIONS
//----------------------------------------------------------------------------//
#define BUFFER_ERROR_TOO_MANY_BUFFERS   -1
#define BUFFER_ERROR_TOO_MANY_ELEMENTS  -2
#define BUFFER_OK       1
#define BUFFER_ERROR    -1
#define BUFFER_WRONG_ID -2


//----------------------------------------------------------------------------//
// EXTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
/**
 * Buffer Initialization.
 * 
 * \param   elements    Number of elements in the circular buffer
 * \return  If error: BUFFER_ERROR_TOO_MANY_BUFFERS / BUFFER_ERROR_TOO_MANY_ELEMENTS
 *          If OK: Buffer ID
 */
int buffer_init(unsigned int elements);

/**
 * Check if buffer is Full.
 * 
 * \param   id    Buffer ID
 * \return  If buffer full: BUFFER_ERROR
 *          If OK: BUFFER_OK
 */
int buffer_IsFull(int id);

/**
 * Returns the positions filled for a given buffer ID.
 * 
 * \param   id    Buffer ID
 * \return  number of elements in the buffer
 */
unsigned int buffer_dataCount(int id);

/**
 * Buffer Push: Add Element to buffer.
 * 
 * \param   id    Buffer ID
 * \return  If buffer overflow: BUFFER_ERROR
 *          If Wrong ID passed: BUFFER_WRONG_ID
 *          If OK: BUFFER_OK
 */
int buffer_push(int id, void *data, unsigned int size);

/**
 * Returns the size of the next element to be removed. 
 * It has to be called before pop.
 * 
 * \param   id    Buffer ID
 * \return  size of the element to be removed (pop), 0 if error or empty.
 */
unsigned int buffer_popSize(int id);

/**
 * Buffer Pop: Remove Element from buffer. Size of the element will be used
 * 
 * \param   id    Buffer ID
 * \param   data    Buffer to be filled
 * \param   size    Size to be filled
 * \return  If buffer overflow: BUFFER_ERROR
 *          If Wrong ID passed: BUFFER_WRONG_ID
 *          If OK: BUFFER_OK
 */
int buffer_pop(int id, void *data, unsigned int size);

/**
 * Remove all elements from the buffer.
 * 
 * \param   id    Buffer ID
 * \return  Nothing
 */
void buffer_clean(int id);

/**
 * For tests only - Delete a buffer
 * 
 * \param   id      Buffer ID
 * \return  nothing
 */
void buffer_delete(int id);

/**
 * Buffer Print - Debug
 * 
 * \param   id      Buffer ID
 * \return  nothing
 */
void buffer_print(int id);

#ifdef __cplusplus
}
#endif

#endif /* BUFFER_H */

