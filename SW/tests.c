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
#include <stdbool.h>
#include <pthread.h>
#include "auxiliary.h"
#include "buffer.h"
#include "canbuf.h"
#include "config.h"
#include "debug.h"
#include "hapcan.h"
#include "hapcansocket.h"
#include "hapcanmqtt.h"
#include "jsonhandler.h"
#include "mqttbuf.h"
#include "mqtt.h"
#include "socketserverbuf.h"
#include "manager.h"

//----------------------------------------------------------------------------//
// INTERNAL DEFINITIONS
//----------------------------------------------------------------------------//
//#define TEST_CONFIG
//#define TEST_BUFFER
//#define TEST_BASIC_STRING
//#define TEST_MQTT_CONNECT

//----------------------------------------------------------------------------//
// INTERNAL TYPES
//----------------------------------------------------------------------------//

//----------------------------------------------------------------------------//
// INTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
#ifdef TEST_MQTT_CONNECT
static void test_mqtt_connect(void)
{
    int i;
    config_init();
    for(i = 0; i < 5; i++)
    {
        mqtt_init();
        sleep(1);
        mqtt_close();
        sleep(1);
    }
    config_end();
}
#endif

#ifdef TEST_BASIC_STRING
static void test1(char*** value, int **a_payloadlen, int *n_payloads)
{
    int len;
    int n = 3;
    int i;
    (*value) = malloc(n * sizeof(char*));
    (*a_payloadlen) = malloc(3 * sizeof (int));
    *n_payloads = 3;
    i = 0;
    len = strlen("0x01");
    (*value)[i] = malloc(len + 1);  
    strcpy((*value)[i], "0x01");
    (*a_payloadlen)[i] = len;
    i = 1;
    len = strlen("0x010");
    (*value)[i] = malloc(len + 1);  
    strcpy((*value)[i], "0x010");
    (*a_payloadlen)[i] = len;
    i = 2;
    len = strlen("0x0100");
    (*value)[i] = malloc(len + 1);  
    strcpy((*value)[i], "0x0100");
    (*a_payloadlen)[i] = len;
}
#endif

#ifdef TEST_BASIC_STRING
static void test_Strings(void)
{
    char* str;
    void **data;
    char **a_payload;
    int *a_payloadlen; 
    int n_payloads;
    
    test1(&a_payload, &a_payloadlen, &n_payloads);
    debug_print("Test String 1. Len = %d, String = %s\n", a_payloadlen[0], a_payload[0]); 
    debug_print("Test String 2. Len = %d, String = %s\n", a_payloadlen[1], a_payload[1]); 
    debug_print("Test String 3. Len = %d, String = %s\n", a_payloadlen[2], a_payload[2]); 
    
    str = malloc(3);
    strcpy(str, "D0");
    free(str);
    str = strdup("D0");
    free(str);
    str = malloc(8);
    strcpy(str, "D0");
    free(str);
    str = strdup("D0");
    free(str);
    
    
    data = malloc(sizeof(void *)*2);
    data[0] = malloc(3);
    strcpy(data[0], "D0");
    data[1] = strdup("D0");    
    free(data[0]);
    free(data[1]);
    free(data);    
}
#endif

#ifdef TEST_BUFFER
static void test_Buffers(void)
{
    int li_Test;
    int i_TestBuffer0 = 0;
    int i_TestBuffer1 = 0;
    int i_Temp = 0;
    int i;
    int *p_i;
    char a[] = "abcd";
    char b[] = "efg";
    char c[] = "hi";
    char d[] = "jklmn";
    char* pc;    
    // Test Buffer
    i_TestBuffer0 = buffer_init(4);
    i_TestBuffer1 = buffer_init(2);                
    i_Temp = buffer_push(i_TestBuffer0, a, sizeof(a));
    debug_print("Push - Buffer: %d - Return: %d\n", i_TestBuffer0, i_Temp);    
    i_Temp = buffer_push(i_TestBuffer0, b, sizeof(b));
    debug_print("Push - Buffer: %d - Return: %d\n", i_TestBuffer0, i_Temp);        
    i_Temp = buffer_push(i_TestBuffer0, c, sizeof(c));
    debug_print("Push - Buffer: %d - Return: %d\n", i_TestBuffer0, i_Temp);            
    i_Temp = buffer_push(i_TestBuffer0, d, sizeof(d));
    debug_print("Push - Buffer: %d - Return: %d\n", i_TestBuffer0, i_Temp);    
    for( i = 0; i < 4; i++)
    {
        li_Test = buffer_popSize(i_TestBuffer0);    
        debug_print("Pop - Buffer: %d - Data Length: %d\n", i_TestBuffer0, 
                li_Test);
        if(li_Test > 0)
        {
            pc = malloc(li_Test);        
            i_Temp = buffer_pop(i_TestBuffer0, pc, li_Test);
            debug_print("Pop - Buffer: %d - Data: %s - Return = %d\n", 
                    i_TestBuffer0, pc, i_Temp);
            free(pc);
            pc = NULL;    
        }
    } 
    i_Temp = buffer_push(i_TestBuffer0, a, sizeof(a));
    debug_print("Push - Buffer: %d - Return: %d\n", i_TestBuffer0, i_Temp);    
    i_Temp = buffer_push(i_TestBuffer0, b, sizeof(b));
    debug_print("Push - Buffer: %d - Return: %d\n", i_TestBuffer0, i_Temp);        
    i_Temp = buffer_push(i_TestBuffer0, c, sizeof(c));
    debug_print("Push - Buffer: %d - Return: %d\n", i_TestBuffer0, i_Temp);            
    i_Temp = buffer_push(i_TestBuffer0, d, sizeof(d));
    debug_print("Push - Buffer: %d - Return: %d\n", i_TestBuffer0, i_Temp);    
    for( i = 0; i < 4; i++)
    {
        li_Test = buffer_popSize(i_TestBuffer0);    
        debug_print("Pop - Buffer: %d - Data Length: %d\n", i_TestBuffer0, 
                li_Test);
        if(li_Test > 0)
        {
            pc = malloc(li_Test);        
            i_Temp = buffer_pop(i_TestBuffer0, pc, li_Test);
            debug_print("Pop - Buffer: %d - Data: %s - Return = %d\n", 
                    i_TestBuffer0, pc, i_Temp);
            free(pc);
            pc = NULL;    
        }
    } 
    li_Test = 21;
    i_Temp = buffer_push(i_TestBuffer1, &li_Test, sizeof(li_Test));
    debug_print("Push - Buffer: %d - Return: %d\n", i_TestBuffer1, i_Temp);
    li_Test = 42;
    i_Temp = buffer_push(i_TestBuffer1, &li_Test, sizeof(li_Test));
    debug_print("Push - Buffer: %d - Return: %d\n", i_TestBuffer1, i_Temp);
    li_Test = buffer_popSize(i_TestBuffer1);        
    debug_print("Pop - Buffer: %d - Data Length: %d\n", i_TestBuffer1, li_Test);
    p_i = malloc(li_Test);
    i_Temp = buffer_pop(i_TestBuffer1, p_i, li_Test);    
    debug_print("Pop - Buffer: %d - Data: %d - Return = %d\n", 
            i_TestBuffer1, *p_i, i_Temp);
    free(p_i);
    li_Test = buffer_popSize(i_TestBuffer1);        
    debug_print("Pop - Buffer: %d - Data Length: %d\n", i_TestBuffer1, li_Test);
    p_i = malloc(li_Test);
    i_Temp = buffer_pop(i_TestBuffer1, p_i, li_Test);    
    debug_print("Pop - Buffer: %d - Data: %d - Return = %d\n", 
            i_TestBuffer1, *p_i, i_Temp);
    free(p_i);
    // Test Underflow
    debug_print("Underflow Test...\n");
    i_Temp = buffer_pop(i_TestBuffer1, &i, 1);
    i_Temp = buffer_pop(i_TestBuffer1, &i, 1);  
    debug_print("Overflow Test...\n");
    //Test Overflow
    for( i = 0; i < 100; i++)
    {
        i_Temp = buffer_push(i_TestBuffer0, a, sizeof(a));
        i_Temp = 0;
    }    
    for( i = 0; i < 5; i++)
    {
        li_Test = buffer_popSize(i_TestBuffer0);    
        debug_print("Pop - Buffer: %d - Data Length: %d\n", i_TestBuffer0, 
                li_Test);
        if(li_Test > 0)
        {
            pc = malloc(li_Test);        
            i_Temp = buffer_pop(i_TestBuffer0, pc, li_Test);
            debug_print("Pop - Buffer: %d - Data: %s - Return = %d\n", 
                    i_TestBuffer0, pc, i_Temp);
            free(pc);
            pc = NULL;    
        }
    } 
    debug_print("Ending...:\n");
    buffer_clean(i_TestBuffer0);
    buffer_clean(i_TestBuffer1);
    buffer_delete(i_TestBuffer0);
    buffer_delete(i_TestBuffer1);
    debug_print("End:\n");
}
#endif

#ifdef TEST_CONFIG
#define TEST_JSON_FRAME_01 "{\"Frame\":266, \"Flags\":0, \"Module\":170, \"Group\":171, \"D0\":1, \"D1\":0, \"D2\":11, \"D3\":100, \"D4\":32, \"D5\":255, \"D6\":255, \"D7\":255}"
#define TEST_JSON_FRAME_02 "{\"Frame\":\"Here\",\"MyDouble\":2.1}"
static void test_Config(void)
{
    bool reloadMQTT;
    bool reload_socket_server;
    bool b_test;
    char *str_test = NULL;
    char **str_a_test;
    int n_str;
    int count = 0;
    int check;
    int i;
    double calc;
    int values[3];
    config_init();      
    // AUX: aux_parseValidateIntArray
    b_test = aux_parseValidateIntArray(values, "123,234,111",",", 3, 0, 0, 255);    
    debug_print("Test aux_parseValidateIntArray = %d, v0 = %d, "
            "v1 = %d, v2 = %d\n", b_test, values[0], values[1], values[2]);
    b_test = aux_parseValidateIntArray(values, "123,234,111",",", 4, 0, 0, 255);
    debug_print("Test aux_parseValidateIntArray = %d,*(n=4) v0 = %d, "
            "v1 = %d, v2 = %d\n", b_test, values[0], values[1], values[2]);
    b_test = aux_parseValidateIntArray(values, "123,234111",",", 3, 0, 0, 255);
    debug_print("Test aux_parseValidateIntArray = %d,*(input) v0 = %d, "
            "v1 = %d, v2 = %d\n", b_test, values[0], values[1], values[2]);
    b_test = aux_parseValidateIntArray(values, "ABDS",",", 3, 0, 0, 255);
    debug_print("Test aux_parseValidateIntArray = %d,*(input) v0 = %d, "
            "v1 = %d, v2 = %d\n", b_test, values[0], values[1], values[2]);
    b_test = aux_parseValidateIntArray(values, NULL,",", 3, 0, 0, 255);
    debug_print("Test aux_parseValidateIntArray = %d,*(input) v0 = %d, "
            "v1 = %d, v2 = %d\n", b_test, values[0], values[1], values[2]);
    b_test = aux_parseValidateIntArray(values, "123,234111",":", 3, 0, 0, 255);
    debug_print("Test aux_parseValidateIntArray = %d,*(delim.) v0 = %d, "
            "v1 = %d, v2 = %d\n", b_test, values[0], values[1], values[2]);
    b_test = aux_parseValidateIntArray(values, "123,234111",NULL, 3, 0, 0, 255);
    debug_print("Test aux_parseValidateIntArray = %d,*(delim.) v0 = %d, "
            "v1 = %d, v2 = %d\n", b_test, values[0], values[1], values[2]);
    b_test = aux_parseValidateIntArray(values, "123,234,111",",", 3, 0, 0, 10);
    debug_print("Test aux_parseValidateIntArray = %d,*(max) v0 = %d, "
            "v1 = %d, v2 = %d\n", b_test, values[0], values[1], values[2]);
    b_test = aux_parseValidateIntArray(values, "12,234,11",",", 3, 0, 200, 255);
    debug_print("Test aux_parseValidateIntArray = %d,*(min) v0 = %d, "
            "v1 = %d, v2 = %d\n", b_test, values[0], values[1], values[2]);
    // jsonhandler
    check = jh_getJFieldStringCopy("HAPCANButtons", 0, 
                    "temperature", 0, "state", &str_test);
    debug_print("Test 1: check = %d, str = %s\n", check, str_test);
    free(str_test);
    str_test = NULL;
    check = config_getBool(CONFIG_GENERAL_SETTINGS_LEVEL, 0, 
            "enableMQTT", 0, NULL, &b_test);
    if( check != EXIT_SUCCESS )
    {
        debug_print("CONFIG Test - bool test 1 ERROR\n");
    }
    else
    {
        debug_print("CONFIG Test - bool test 1 OK. value = %d\n", b_test);
    }    
    check = config_getBool(CONFIG_GENERAL_SETTINGS_LEVEL, 0, 
            "enableMQTTERROR", 0, NULL, &b_test);
    if( check != EXIT_SUCCESS )
    {
        debug_print("CONFIG Test - bool test 2 OK\n");
    }
    else
    {
        debug_print("CONFIG Test - bool test 2 ERROR. value = %d\n", b_test);
    }
    check = config_getBool(CONFIG_GENERAL_SETTINGS_LEVEL, 0, 
            NULL, 0, NULL, &b_test);
    if( check != EXIT_SUCCESS )
    {
        debug_print("CONFIG Test - bool test 3 OK\n");
    }
    else
    {
        debug_print("CONFIG Test - bool test 3 ERROR. value = %d\n", b_test);
    }    
    check = config_getString(CONFIG_GENERAL_SETTINGS_LEVEL, 0, 
        "mqttBroker", 0, NULL, &str_test);
    if( check != EXIT_SUCCESS )
    {
        debug_print("CONFIG Test - String test 1 ERROR\n");
    }
    else
    {
        debug_print("CONFIG Test - String test 1 OK. value = %s\n", str_test);
        free(str_test);
    }        
    check = config_getString(CONFIG_GENERAL_SETTINGS_LEVEL, 0, 
        "mqttBrokerERROR", 0, NULL, &str_test);
    if( check != EXIT_SUCCESS )
    {
        debug_print("CONFIG Test - String test 2 OK\n");
    }
    else
    {
        debug_print("CONFIG Test - String test 2 ERROR. value = %s\n", 
                str_test);
        free(str_test);
    }
    check = config_getString(CONFIG_GENERAL_SETTINGS_LEVEL, 0, 
        NULL, 0, NULL, &str_test);
    if( check != EXIT_SUCCESS )
    {
        debug_print("CONFIG Test - String test 3 OK\n");
    }
    else
    {
        debug_print("CONFIG Test - String test 3 ERROR. value = %s\n", 
                str_test);
        free(str_test);
    }        
    check = config_getStringArray(CONFIG_GENERAL_SETTINGS_LEVEL, 
            "subscribeTopics", &n_str, &str_a_test);
    if( check != EXIT_SUCCESS )
    {
        debug_print("CONFIG Test - String Array test 1 ERROR\n");
    }
    else
    {
        debug_print("CONFIG Test - str array test 1 OK: n = %d\n", n_str);
        for(i = 0; i < n_str; i++ )
        {
            debug_print("CONFIG Test - str array [%d] = %s\n", i, 
                    str_a_test[i]);
            free(str_a_test[i]);
        }
        free(str_a_test);
    }           
    check = config_getStringArray(CONFIG_GENERAL_SETTINGS_LEVEL, "enableMQTT", 
        &n_str, &str_a_test);
    if( check != EXIT_SUCCESS )
    {
        debug_print("CONFIG Test - String Array test 2 OK\n");
    }
    else
    {
        debug_print("CONFIG Test - str array test 2 ERROR: n = %d\n", n_str);
        for(i = 0; i < n_str; i++ )
        {
            debug_print("CONFIG Test - str array [%d] = %s\n", i, 
                    str_a_test[i]);
            free(str_a_test[i]);
        }
        free(str_a_test);
    }
    check = config_getStringArray(CONFIG_GENERAL_SETTINGS_LEVEL, NULL, 
        &n_str, &str_a_test);
    if( check != EXIT_SUCCESS )
    {
        debug_print("CONFIG Test - String Array test 3 OK\n");
    }
    else
    {
        debug_print("CONFIG Test - str array test 3 ERROR: n = %d\n", n_str);
        for(i = 0; i < n_str; i++ )
        {
            debug_print("CONFIG Test - str array [%d] = %s\n", i, 
                    str_a_test[i]);
            free(str_a_test[i]);
        }
        free(str_a_test);
    }
    while(1)
    {
        if(config_isNewConfigAvailable())
        {
            debug_print("CONFIG Test - New config available!\n");
            config_reload(&reloadMQTT, &reload_socket_server);            
        }
        // Only check every 10 seconds
        sleep(1);
        count++;
        if(count > 2)
        {
            break;
        }        
    } 
    // Test JSON
    jsonFieldData j_arr[12];
    void *payload;
    // SET PAYLOAD
    int field = 0;
    j_arr[field].field = "Frame";
    j_arr[field].value_type = JSON_TYPE_INT;
    j_arr[field].int_value = field;
    field++;
    j_arr[field].field = "Flags";
    j_arr[field].value_type = JSON_TYPE_INT;
    j_arr[field].int_value = field;
    field++;
    j_arr[field].field = "Module";
    j_arr[field].value_type = JSON_TYPE_INT;
    j_arr[field].int_value = field;
    field++;
    j_arr[field].field = "Group";
    j_arr[field].value_type = JSON_TYPE_INT;
    j_arr[field].int_value = field;
    field++;
    j_arr[field].field = "D0";
    j_arr[field].value_type = JSON_TYPE_INT;
    j_arr[field].int_value = field;
    field++;
    j_arr[field].field = "D1";
    j_arr[field].value_type = JSON_TYPE_INT;
    j_arr[field].int_value = field;
    field++;
    j_arr[field].field = "D2";
    j_arr[field].value_type = JSON_TYPE_INT;
    j_arr[field].int_value = field;
    field++;
    j_arr[field].field = "D3";
    j_arr[field].value_type = JSON_TYPE_INT;
    j_arr[field].int_value = field;
    field++;
    j_arr[field].field = "D4";
    j_arr[field].value_type = JSON_TYPE_INT;
    j_arr[field].int_value = field;
    field++;
    j_arr[field].field = "D5";
    j_arr[field].value_type = JSON_TYPE_INT;
    j_arr[field].int_value = field;
    field++;
    j_arr[field].field = "D6";
    j_arr[field].value_type = JSON_TYPE_INT;
    j_arr[field].int_value = field;
    field++;
    j_arr[field].field = "D7";
    j_arr[field].value_type = JSON_TYPE_INT;
    j_arr[field].int_value = field;
    field++;
    jh_getStringFromFieldValuePairs(j_arr, field, &str_test);            
    unsigned int size = strlen(str_test);
    payload = malloc(size + 1);
    strcpy(payload, str_test);
    debug_print("CONFIG Test - Payload = %s\n", payload);
    // free
    free(str_test);
    free(payload);
    field = 0;
    j_arr[field].field = "Integer";
    j_arr[field].value_type = JSON_TYPE_INT;
    j_arr[field].int_value = field;
    field++;
    j_arr[field].field = "String";
    j_arr[field].value_type = JSON_TYPE_STRING;
    j_arr[field].str_value = "ON";
    field++;
    jh_getStringFromFieldValuePairs(j_arr, field, &str_test);            
    size = strlen(str_test);
    payload = malloc(size + 1);
    strcpy(payload, str_test);
    debug_print("CONFIG Test - Payload = %s\n", payload);
    // free
    free(str_test);
    free(payload);
    // Object from string and get fields
    json_object *obj;
    size = strlen(TEST_JSON_FRAME_01);
    str_test = malloc(size + 1);
    memcpy(str_test, TEST_JSON_FRAME_01, size + 1);
    str_test[size] = 0;
    // Get the JSON Object
    jh_getObject(str_test, &obj);
    free(str_test);
    check = jh_getObjectFieldAsInt(obj, "Frame", &i);
    debug_print("CONFIG Test - Get Int. check = %d, value = %d\n", check, i);
    check = jh_getObjectFieldAsInt(obj, "Module", &i);
    debug_print("CONFIG Test - Get Int. check = %d, value = %d\n", check, i);
    check = jh_getObjectFieldAsInt(obj, "Group", &i);
    debug_print("CONFIG Test - Get Int. check = %d, value = %d\n", check, i);
    jh_freeObject(obj);
    size = strlen(TEST_JSON_FRAME_02);
    str_test = malloc(size + 1);
    memcpy(str_test, TEST_JSON_FRAME_02, size + 1);
    str_test[size] = 0;
    // Get the JSON Object
    jh_getObject(str_test, &obj);
    free(str_test);
    str_test = NULL;
    check = jh_getObjectFieldAsStringCopy(obj, "Frame", &str_test);
    debug_print("CONFIG Test - Get String. check = %d, value = %s\n", check, 
            str_test);
    free(str_test);
    str_test = NULL;
    check = jh_getObjectFieldAsStringCopy(obj, "Module", &str_test);
    debug_print("CONFIG Test - Get String.* check = %d, value = %s\n", check, 
            str_test);
    free(str_test);
    str_test = NULL;
    check = jh_getObjectFieldAsDouble(obj, "MyDouble", &calc);
    debug_print("CONFIG Test - Get Double. check = %d, value = %f\n", check, 
            calc);
    check = jh_getObjectFieldAsDouble(obj, "Frame", &calc);
    debug_print("CONFIG Test - Get Double*. check = %d, value = %f\n", check, 
            calc);
    jh_freeObject(obj);
    check = jh_getJArrayElements("HAPCANRelays", 0, NULL, 
            JSON_DEPTH_LEVEL, &i);
    debug_print("CONFIG Test - Get N HAPCANRelays = %d, value = %d\n", 
            check, i);
    check = jh_getJArrayElements("HAPCANRelay", 0, NULL, 
            JSON_DEPTH_LEVEL, &i);
    debug_print("CONFIG Test - Get N HAPCANRelay* = %d, value = %d\n", 
            check, i);
    check = jh_getJArrayElements("HAPCANRelays", 1, NULL, 
            JSON_DEPTH_LEVEL_AND_INDEX, &i);
    debug_print("CONFIG Test - Get N HAPCANRelays, 1 = %d, value = %d\n", 
            check, i);
    check = jh_getJArrayElements("HAPCANRelays", 5, NULL, 
            JSON_DEPTH_LEVEL_AND_INDEX, &i);
    debug_print("CONFIG Test - Get N HAPCANRelays, 5* = %d, value = %d\n", 
            check, i);
    check = jh_getJArrayElements("HAPCANRelays", 1, "relays", 
            JSON_DEPTH_FIELD, &i);
    debug_print("CONFIG Test - Get N HAPCANRelays, 1, relays = %d, "
            "value = %d\n", check, i);
    check = jh_getJArrayElements("HAPCANRelays", 1, "buttons", 
            JSON_DEPTH_FIELD, &i);
    debug_print("CONFIG Test - Get N HAPCANRelays, 1, buttons* = %d, "
            "value = %d\n", check, i);
    // Get the JSON Object
    jh_getObject("--", &obj);
    jh_freeObject(obj);
    obj = NULL;
    jh_freeObject(obj);
    config_end();
}
#endif

//----------------------------------------------------------------------------//
// INTERNAL GLOBAL VARIABLES
//----------------------------------------------------------------------------//


//----------------------------------------------------------------------------//
// EXTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
/*
 * Module Tests
 */
void tests_Init(void)
{
    #ifdef TEST_CONFIG
    test_Config();
    #endif

    #ifdef TEST_BUFFER
    test_Buffers();
    #endif

    #ifdef TEST_BASIC_STRING
    test_Strings();
    #endif   

    #ifdef TEST_MQTT_CONNECT
    test_mqtt_connect();
    #endif   
}