//----------------------------------------------------------------------------//
//                               OBJECT HISTORY                               //
//----------------------------------------------------------------------------//
//  REVISION |    DATE     |                               |      AUTHOR      //
//----------------------------------------------------------------------------//
//  1.00     | 10/Dec/2021 |                               | ALCP             //
// - First version                                                            //
//----------------------------------------------------------------------------//

/*
* Includes
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "manager.h"
#include "tests.h"

int main(int argc, char *argv[])
{    
    setbuf(stdout, NULL); // disable buffering on stdout: Needed for immediate debug
    
    // Init manager or perform module tests - see tests.h
    #ifdef TEST_RUN_MODULE_TESTS
    tests_Init();
    #else
    managerInit();
    #endif
}