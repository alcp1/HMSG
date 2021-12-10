//----------------------------------------------------------------------------//
//                               OBJECT HISTORY                               //
//----------------------------------------------------------------------------//
//  REVISION |    DATE     |                               |      AUTHOR      //
//----------------------------------------------------------------------------//
//  1.00     | 10/Dec/2021 |                               | ALCP             //
// - First version                                                            //
//----------------------------------------------------------------------------//

#ifndef TESTS_H
#define TESTS_H

#ifdef __cplusplus
extern "C" {
#endif

//#define TEST_RUN_MODULE_TESTS
    
//----------------------------------------------------------------------------//
// EXTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
/*
 * Funcion to perform tests - call it instead of managerInit in main.c
 */
void tests_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* TESTS_H */

