//----------------------------------------------------------------------------//
//                               OBJECT HISTORY                               //
//----------------------------------------------------------------------------//
//  REVISION |    DATE     |                               |      AUTHOR      //
//----------------------------------------------------------------------------//
//  1.00     | 10/Dec/2021 |                               | ALCP             //
// - First version                                                            //
//----------------------------------------------------------------------------//
//  1.01     | 24/Oct/2024 |                               | ALCP             //
// - Add funtions jh_getJFieldIntObj and jh_getJArrayElementsObj              //
//----------------------------------------------------------------------------//

/*
* Includes
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <errno.h>
#include "jsonhandler.h"
#include "config.h"
#include "debug.h"

//----------------------------------------------------------------------------//
// INTERNAL DEFINITIONS
//----------------------------------------------------------------------------//


//----------------------------------------------------------------------------//
// INTERNAL TYPES
//----------------------------------------------------------------------------//

//----------------------------------------------------------------------------//
// INTERNAL GLOBAL VARIABLES
//----------------------------------------------------------------------------//
static pthread_mutex_t g_json_mutex = PTHREAD_MUTEX_INITIALIZER;
static json_object *j_config;

//----------------------------------------------------------------------------//
// INTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
static int getJFieldBool(json_object *j_root, const char *field, bool *value);
static int getJFieldDouble(json_object *j_root, const char *field, 
    double *value);
static int getJFieldInt(json_object *j_root, const char *field, int *value);
static int getJFieldStringCopy(json_object *j_root, const char *field, 
    char **value);
static int getJFieldStringArrayCopy(json_object *j_root, const char *field, 
        int *elements, char ***value);
static int getObjectAndFieldObj(json_object *j_root, const char *level, 
    int levelIndex, const char *field, int fieldIndex, const char *subField, 
    json_object **data_obj, const char **data_field);
static int getJArrayLengthObj(json_object *j_root, const char *level, 
    int levelIndex, const char *field, json_depth_t depth, int *value);

/**
 * Get a boolean from a JSON object
 * 
 * \param   j_root  JSON object to be read (INPUT)
 * \param   field   Field as a string to be searched (INPUT)
 * \param   value   Value to be filled (OUTUT)
 *  
 * \return  JSON_OK             Value matches type and is not NULL
 *          JSON_ERROR_TYPE     Type does not match
 *          JSON_ERROR_OTHER    Other error
 **/
static int getJFieldBool(json_object *j_root, const char *field, bool *value)
{
    int ret;
    json_object *j_temp;
    // Get the field
    if( !json_object_object_get_ex(j_root, field, &j_temp) )
    {
        ret = JSON_ERROR_OTHER;
    }
    else
    {
        // Check if the expected type is the same as the object type
        if(json_object_get_type(j_temp) != json_type_boolean)
        {
            ret = JSON_ERROR_TYPE;
        }
        else
        {
            *value = json_object_get_boolean(j_temp);
            ret = JSON_OK;
        }
    }
    return ret;
}

/**
 * Get a double from a JSON object
 * 
 * \param   j_root  JSON object to be read (INPUT)
 * \param   field   Field as a string to be searched (INPUT)
 * \param   value   Value to be filled (OUTUT)
 *  
 * \return  JSON_OK             Value matches type and is not NULL
 *          JSON_ERROR_TYPE     Type does not match
 *          JSON_ERROR_OTHER    Other error
 **/
static int getJFieldDouble(json_object *j_root, const char *field, double *value)
{
    int ret;
    json_object *j_temp;
    // Get the field
    if( !json_object_object_get_ex(j_root, field, &j_temp) )
    {
        ret = JSON_ERROR_OTHER;
    }
    else
    {
        // Check if the expected type is the same as the object type
        switch(json_object_get_type(j_temp))
        {
            case json_type_double:
                errno = 0;
                if(errno != EINVAL && errno != ERANGE)
                {
                    *value = json_object_get_double(j_temp);
                    ret = JSON_OK;
                }
                else
                {
                    ret = JSON_ERROR_OTHER;
                }
                break;
            case json_type_int:
                *value = (double)(json_object_get_int(j_temp));
                ret = JSON_OK;
                break;
            default:
                ret = JSON_ERROR_TYPE;
                break;
        }
    }
    return ret;
}

/**
 * Get an integer from a JSON object
 * 
 * \param   j_root  JSON object to be read (INPUT)
 * \param   field   Field as a string to be searched (INPUT)
 * \param   value   Value to be filled (OUTUT)
 *  
 * \return  JSON_OK             Value matches type and is not NULL
 *          JSON_ERROR_TYPE     Type does not match
 *          JSON_ERROR_OTHER    Other error
 **/
static int getJFieldInt(json_object *j_root, const char *field, int *value)
{
    int ret;
    json_object *j_temp;
    // Get the field
    if( !json_object_object_get_ex(j_root, field, &j_temp) )
    {
        ret = JSON_ERROR_OTHER;
    }
    else
    {
        // Check if the expected type is the same as the object type
        if(json_object_get_type(j_temp) != json_type_int)
        {
            ret = JSON_ERROR_TYPE;
        }
        else
        {
            errno = 0;
            if(errno != EINVAL)
            {
                *value = json_object_get_int(j_temp);
                ret = JSON_OK;
            }
            else
            {
                ret = JSON_ERROR_OTHER;
            }
        }
    }
    return ret;
}

/**
 * Get a string copy from a JSON object - STRING HAS TO BE FREED AFTERWARDS!
 * 
 * \param   j_root  JSON object to be read (INPUT)
 * \param   field   Field as a string to be searched (INPUT)
 * \param   value   Value to be filled (OUTUT)
 *  
 * \return  JSON_OK             Value matches type and is not NULL
 *          JSON_ERROR_TYPE     Type does not match
 *          JSON_ERROR_OTHER    Null string
 **/
static int getJFieldStringCopy(json_object *j_root, const char *field, 
        char **value)
{
    int ret;
    json_object *j_temp;
    unsigned int len;
    const char *temp;
    // Get the field
    if( !json_object_object_get_ex(j_root, field, &j_temp) )
    {
        ret = JSON_ERROR_OTHER;
    }
    else
    {
        // Check if the expected type is the same as the object type
        if(json_object_get_type(j_temp) != json_type_string)
        {
            ret = JSON_ERROR_TYPE;
        }
        else
        {
            temp = json_object_get_string(j_temp);
            len = strlen(temp);
            if(len > 0)
            {
                *value = malloc(len + 1);  // Has to be freed by the application
                strcpy(*value, temp);
                ret = JSON_OK;
            }
            else
            {
                *value = NULL;
                ret = JSON_ERROR_OTHER;
            }        
        }
    }
    return ret;
}

/**
 * Get a string copy from a JSON object - STRING HAS TO BE FREED AFTERWARDS!
 * 
 * \param   j_root  JSON object to be read (INPUT)
 * \param   field   Field as a string to be searched (INPUT)
 * \param   value   Value to be filled (OUTUT)
 *  
 * \return  JSON_OK             Value matches type and is not NULL
 *          JSON_ERROR_TYPE     Type does not match
 *          JSON_ERROR_OTHER    Null string
 **/
static int getJFieldStringArrayCopy(json_object *j_root, const char *field, 
        int *elements, char ***value)
{
    int ret;
    json_object *obj_field;
    json_object *obj_index;
    unsigned int len;
    int i;
    int n;
    const char *temp;
    if(field == NULL)
    {
        ret = JSON_ERROR_OTHER;
    }
    else
    {
        // Get the field
        if( !json_object_object_get_ex(j_root, field, &obj_field) )
        {
            ret = JSON_ERROR_OTHER;
        }
        else
        {
            // Check if the expected type is the same as the object type
            ret = JSON_OK;
            if(json_object_get_type(obj_field) != json_type_array)
            {
                ret = JSON_ERROR_TYPE;
            }
            else
            {
                n = json_object_array_length(obj_field);
                (*elements) = n;
                if( n )
                {            
                    for(i = 0; i < n; i++)
                    {
                        obj_index = json_object_array_get_idx(obj_field, i);
                        if(json_object_get_type(obj_index) != json_type_string)
                        {
                            ret = JSON_ERROR_TYPE;
                            break;
                        }
                    }            
                    // Only set values if all fields are OK
                    if(ret == JSON_OK)
                    {
                        (*value) = malloc(n * sizeof(char*));
                        for (i = 0; i < n; i++)
                        {
                            obj_index = json_object_array_get_idx(obj_field, i);
                            temp = json_object_get_string(obj_index);
                            len = strlen(temp);
                            if(len > 0)
                            {
                                // Has to be freed by the application:
                                (*value)[i] = malloc(len + 1);  
                                strcpy((*value)[i], temp);
                            }
                            else
                            {
                                (*value)[i] = NULL;                        
                            }     
                        }
                    }
                    else
                    {
                        // Not all fields are strings
                        ret = JSON_ERROR_TYPE;
                    }
                }
                else
                {
                    ret = JSON_ERROR_OTHER;
                    (*value) = NULL;
                }
            }
        }
    }
    return ret;
}

/**
 * Get a JSON Object within a JSON object
 * 
 * \param   j_root  JSON object to be read (INPUT)
 * \param   field   Field as a string to be searched (INPUT)
 * \param   value   Value to be filled (OUTUT)
 *  
 * \return  JSON_OK             not NULL
 *          JSON_ERROR_TYPE     object is null
 *          JSON_ERROR_OTHER    input field is null
 **/
static int getJFieldObject(json_object *j_root, const char *field, json_object **value)
{
    int ret;
    // First check if the field is null
    if(field == NULL)
    {
        ret = JSON_ERROR_OTHER;
    }
    else
    {
        // Get the field
        if( json_object_object_get_ex(j_root, field, value) )    
        {
            ret = JSON_OK;
        }
        else
        {
            ret = JSON_ERROR_TYPE;
        }
    }
    return ret;
}

/**
 * Fill the object and the data field (string) to be read from the 
 * configuration file
 * 
 * \param   j_root      JSON object (INPUT)
 * \param   level       string for the level within the config file (INPUT)
 * \param   levelIndex  if the level returns an array, the element number 
 *                          "levelIndex" is considered for the Level. (INPUT)
 * \param   field       Field as a string to be searched (INPUT)
 * \param   fieldIndex  if the field returns an array, the element number 
 *                          "fieldIndex" is considered for the field. (INPUT)
 * \param   subField    if the field Index is an object, the SubField is used 
 *                          to get correct value. (INPUT)
 * \param   data_obj    JSON Object Pointer to be filled (OUTPUT)
 * \param   data_field  Field to be read from the JSON Object
 *  
 * \return  JSON_OK             not NULL
 *          JSON_ERROR_TYPE     object is null
 *          JSON_ERROR_OTHER    input field is null
 **/
static int getObjectAndFieldObj(json_object *j_root, const char *level, 
    int levelIndex, const char *field, int fieldIndex, const char *subField, 
    json_object **data_obj, const char **data_field)
{
    int check = JSON_OK;
    json_object *obj1;
    json_object *obj2;
    json_object *obj3;
    if(level == NULL)
    {
        check = JSON_ERROR_OTHER;
    }
    else
    {
        check = getJFieldObject(j_root, level, &obj1);
    }
    if(check == JSON_OK)
    {
        if(json_object_get_type(obj1) == json_type_array)
        {
            if( field == NULL )
            {
                check = JSON_ERROR_OTHER;
            }
            else
            {
                // Use level Index to get the correct object
                obj2 = json_object_array_get_idx(obj1, levelIndex);
                // Get the object
                check = getJFieldObject(obj2, field, &obj2);
                if(check == JSON_OK)
                {
                    if(json_object_get_type(obj2) == json_type_array)
                    {
                        // Use level Index to get the correct object
                        obj3 = json_object_array_get_idx(obj2, fieldIndex);
                        if( subField == NULL )
                        {
                            check = JSON_ERROR_OTHER;
                        }
                        else
                        {
                            *data_obj = obj3;
                            *data_field = subField;
                        }
                    }
                    else
                    {
                        if(subField != NULL)
                        {
                            *data_obj = obj2;
                            *data_field = subField;
                        }
                        else
                        {
                            // First Level is an array, but second level is not
                            obj2 = json_object_array_get_idx(obj1, levelIndex);
                            *data_obj = obj2;
                            *data_field = field;
                        }
                    }
                }
            }
        }
        else
        {
            if(check == JSON_OK)
            {
                // First level is not an array
                if( field == NULL )
                {
                    check = JSON_ERROR_OTHER;
                }
                else
                {
                    *data_obj = obj1;
                    *data_field = field;                    
                }
            }
        }
    }
    return check;
}

/**
 * Fill the nuber of elements within a given JSAN Array element from the 
 * configuration file
 * 
 * \param   j_root      JSON object (INPUT)
 * \param   level       string for the level within the config file (INPUT)
 * \param   levelIndex  if the level returns an array, the element number 
 *                          "levelIndex" is considered for the Level. (INPUT)
 * \param   field       Field as a string to be searched (INPUT)
 * \param   depth       0 = search level only 
 *                      1 = search level and levelIndex 
 *                      2 = search level and levelIndex and field (INPUT)
 * \param   value       Number of elements to be filled (OUTPUT)
 * \return  JSON_OK             not NULL
 *          JSON_ERROR_TYPE     object is null
 *          JSON_ERROR_OTHER    input field is null
 **/
static int getJArrayLengthObj(json_object *j_root, const char *level, 
    int levelIndex, const char *field, json_depth_t depth, int *value)
{
    int check = JSON_OK;
    json_object *obj1;
    json_object *obj2;
    json_object *obj3;   
    // First check if level is not NULL
    if(level == NULL)
    {
        check = JSON_ERROR_OTHER;
        *value = 0;
    }
    else
    {
        //-----------------------------
        // Check field first
        //-----------------------------        
        check = getJFieldObject(j_root, level, &obj1);
        if(check == JSON_OK)
        {
            if(json_object_get_type(obj1) == json_type_array)
            {
                // Fill value, check is already JSON_OK
                *value = json_object_array_length(obj1);
            }
            else
            {
                check = JSON_ERROR_OTHER;
                *value = 0;
            }
        }
        else
        {
            // Fill value, check is already not JSON_OK
            *value = 0;
        }
    }
    // At his point, if check is JSON_OK, obj1 has the level object, and value
    // is already set with the length of level
    if(check == JSON_OK)
    {
        if(depth != JSON_DEPTH_LEVEL)
        {
            if(levelIndex < 0 || levelIndex > json_object_array_length(obj1))
            {
                check = JSON_ERROR_OTHER;
                *value = 0;
            }
            else
            {
                obj2 = json_object_array_get_idx(obj1, levelIndex);
                if(depth == JSON_DEPTH_LEVEL_AND_INDEX)
                {
                    if(json_object_get_type(obj2) == json_type_array)
                    {                
                        // Fill value
                        *value = json_object_array_length(obj2);
                        check = JSON_OK;
                    }
                    else
                    {
                        check = JSON_ERROR_OTHER;
                        *value = 0;
                    }
                }
                else if(depth == JSON_DEPTH_FIELD)
                {
                    if(field == NULL)
                    {
                        check = JSON_ERROR_OTHER;
                        *value = 0;
                    }
                    else
                    {
                        // obj2 is already an array element. Get its "field" elements
                        check = getJFieldObject(obj2, field, &obj3);
                        if(check == JSON_OK)
                        {
                            if(json_object_get_type(obj3) == json_type_array)
                            {
                                // Fill value
                                *value = json_object_array_length(obj3);
                                check = JSON_OK;
                            }
                            else
                            {
                                check = JSON_ERROR_OTHER;
                                *value = 0;
                            }
                        }
                        else
                        {
                            // Fill value, check is already not JSON_OK
                            *value = 0;
                        }
                    }                
                }
                else
                {
                    check = JSON_ERROR_OTHER;
                    *value = 0;
                }
            }
        }
        // if depth is JSON_DEPTH_LEVEL, all is already set
    }    
    return check;
}

//----------------------------------------------------------------------------//
// EXTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
/* read config file */
void jh_readConfigFile(void)
{
    // LOCK - Use of JSON is not thread protected
    pthread_mutex_lock(&g_json_mutex);
    // Read from file
    // Getting the type of value
    j_config = json_object_from_file(JSON_CONFIG_FILE_PATH);
    // UNLOCK - Use of JSON is not thread protected
    pthread_mutex_unlock(&g_json_mutex);
    #ifdef DEBUG_JSON_ERRORS
    if (!j_config)
    {
        debug_print("jsonhandler: Error reading config file!\n");
    }
    #endif
}

/* Free memory from json object */
void jh_freeConfigFile(void)
{
    // LOCK - Use of JSON is not thread protected
    pthread_mutex_lock(&g_json_mutex);
    // Free Memory
    json_object_put(j_config);    
    // UNLOCK - Use of JSON is not thread protected
    pthread_mutex_unlock(&g_json_mutex);
}

/* Get a Bool*/
int jh_getJFieldBool(const char *level, int levelIndex, const char *field, 
        int fieldIndex, const char *subField, bool *value)
{
    int check;
    json_object *obj;
    const char  *str;
    // LOCK - Use of JSON is not thread protected
    pthread_mutex_lock(&g_json_mutex);
    check = getObjectAndFieldObj(j_config, level, levelIndex, field, fieldIndex, 
        subField, &obj, &str);
    if(check == JSON_OK)
    {
        // First level is not 
        check = getJFieldBool(obj, str, value);
    }
    // UNLOCK - Use of JSON is not thread protected
    pthread_mutex_unlock(&g_json_mutex);
    #ifdef DEBUG_JSON_ERRORS
    if(check != JSON_OK)
    {
        debug_print("Error - jh_getJFieldBool: %d - level = %s - field = %s\n", 
                check, level, field);
    }    
    #endif    
    // Return
    return check;
}

/* Get a double*/
int jh_getJFieldDouble(const char *level, int levelIndex, const char *field, 
        int fieldIndex, const char *subField, double *value)
{
    int check;
    json_object *obj;
    const char  *str;
    // LOCK - Use of JSON is not thread protected
    pthread_mutex_lock(&g_json_mutex);
    check = getObjectAndFieldObj(j_config, level, levelIndex, field, fieldIndex, 
        subField, &obj, &str);    
    if(check == JSON_OK)
    {
        check = getJFieldDouble(obj, str, value);
    }
    // UNLOCK - Use of JSON is not thread protected
    pthread_mutex_unlock(&g_json_mutex);
    #ifdef DEBUG_JSON_ERRORS
    if(check != JSON_OK)
    {
        debug_print("Error - jh_getJFieldDouble: %d - level = %s - field = %s\n", 
                check, level, field);
    }    
    #endif    
    // Return
    return check;
}

/* Get an integer */
int jh_getJFieldInt(const char *level, int levelIndex, const char *field, 
        int fieldIndex, const char *subField, int *value)
{
    int check;
    json_object *obj;
    const char  *str;
    // LOCK - Use of JSON is not thread protected
    pthread_mutex_lock(&g_json_mutex);
    check = getObjectAndFieldObj(j_config, level, levelIndex, field, fieldIndex, 
        subField, &obj, &str);    
    if(check == JSON_OK)
    {
        check = getJFieldInt(obj, str, value);
    }
    // UNLOCK - Use of JSON is not thread protected
    pthread_mutex_unlock(&g_json_mutex);
    #ifdef DEBUG_JSON_ERRORS
    if(check != JSON_OK)
    {
        debug_print("Error - jh_getJFieldInt: %d - level = %s - field = %s\n", 
                check, level, field);
    }    
    #endif    
    // Return
    return check;
}

/* Get an integer within an object */
int jh_getJFieldIntObj(const char *objStr, const char *level, int levelIndex, 
        const char *field, int fieldIndex, const char *subField, int *value)
{
    int check;
    json_object *obj1;
    json_object *obj2;
    const char  *str;
    // LOCK - Use of JSON is not thread protected
    pthread_mutex_lock(&g_json_mutex);
    // Get the JSON Object for the input object string
    check = getJFieldObject(j_config, objStr, &obj1);
    if(check == JSON_OK)
    {
        check = getObjectAndFieldObj(obj1, level, levelIndex, field, 
        fieldIndex, subField, &obj2, &str);    
        if(check == JSON_OK)
        {
            check = getJFieldInt(obj2, str, value);
        }
    }    
    // UNLOCK - Use of JSON is not thread protected
    pthread_mutex_unlock(&g_json_mutex);
    #ifdef DEBUG_JSON_ERRORS
    if(check != JSON_OK)
    {
        debug_print("Error - jh_getJFieldIntObj: %d - level = %s - field = %s\n", 
                check, level, field);
    }    
    #endif    
    // Return
    return check;
}

/* Get the copy of a string */
int jh_getJFieldStringCopy(const char *level, int levelIndex, const char *field, 
        int fieldIndex, const char *subField, char **value)
{
    int check;
    json_object *obj;
    const char  *str;
    // LOCK - Use of JSON is not thread protected
    pthread_mutex_lock(&g_json_mutex);
    check = getObjectAndFieldObj(j_config, level, levelIndex, field, fieldIndex, 
        subField, &obj, &str);
    if(check == JSON_OK)
    {        
        check = getJFieldStringCopy(obj, str, value);
    }
    // UNLOCK - Use of JSON is not thread protected
    pthread_mutex_unlock(&g_json_mutex);
    #ifdef DEBUG_JSON_ERRORS
    if(check != JSON_OK)
    {
        debug_print("Error - jh_getJFieldStringCopy: %d - level = %s - field = %s\n", 
                check, level, field);
    }    
    #endif    
    // Return
    return check;
}

/**
 * Get the number of elements of a given JSON Array 
 **/
int jh_getJArrayElements(const char *level, int levelIndex, const char *field, 
        json_depth_t depth, int *value)
{
    int check;
    // LOCK - Use of JSON is not thread protected
    pthread_mutex_lock(&g_json_mutex);
    check = getJArrayLengthObj(j_config, level, levelIndex, field, depth, 
        value);
    // UNLOCK - Use of JSON is not thread protected
    pthread_mutex_unlock(&g_json_mutex);
    #ifdef DEBUG_JSON_ERRORS
    if(check != JSON_OK)
    {
        debug_print("Error - jh_getJArrayElements: %d - level = %s "
                "levelIndex = %d - field = %s - depth = %d\n", 
                check, level, levelIndex, field, depth);
    }    
    #endif    
    // Return
    return check;
}

/**
 * Get the number of elements of a given JSON Array within an object
 **/
int jh_getJArrayElementsObj(const char *objStr, const char *level, 
        int levelIndex, const char *field, json_depth_t depth, int *value)
{
    int check;
    json_object *obj1;
    // LOCK - Use of JSON is not thread protected
    pthread_mutex_lock(&g_json_mutex);
    // Get the JSON Object for the input object string
    check = getJFieldObject(j_config, objStr, &obj1);
    if(check == JSON_OK)
    {
        check = getJArrayLengthObj(obj1, level, levelIndex, field, depth, 
            value);
    }
    // UNLOCK - Use of JSON is not thread protected
    pthread_mutex_unlock(&g_json_mutex);
    #ifdef DEBUG_JSON_ERRORS
    if(check != JSON_OK)
    {
        debug_print("Error - jh_getJArrayElementsObj: %d - level = %s "
                "levelIndex = %d - field = %s - depth = %d\n", 
                check, level, levelIndex, field, depth);
    }    
    #endif    
    // Return
    return check;
}


/* Get the copy of a string array */
int jh_getJFieldStringArrayCopy(const char *level, const char *field, 
        int *elements, char ***value)
{
    int check;
    json_object *obj;
    // LOCK - Use of JSON is not thread protected
    pthread_mutex_lock(&g_json_mutex);
    check = getJFieldObject(j_config, level, &obj);    
    if(check == JSON_OK)
    {
        check = getJFieldStringArrayCopy(obj, field, elements, value);
    }
    // UNLOCK - Use of JSON is not thread protected
    pthread_mutex_unlock(&g_json_mutex);
    #ifdef DEBUG_JSON_ERRORS
    if(check != JSON_OK)
    {
        if( (level != NULL) && (field != NULL) )
        {
            debug_print("Error - jh_getJFieldStringArrayCopy: %d - level = %s - field = %s\n", 
                check, level, field);
        }
        else
        {
            debug_print("Error - jh_getJFieldStringArrayCopy: NULL input\n");
        }
    }    
    #endif    
    // Return
    return check;
}

/* Create JSON string from input data array */
void jh_getStringFromFieldValuePairs(jsonFieldData *a_data, int n_pairs, 
        char **str)
{
    json_object *obj;
    unsigned int len;
    // LOCK - Use of JSON is not thread protected
    pthread_mutex_lock(&g_json_mutex);
    obj = json_object_new_object();    
    int i;    
    for (i = 0; i < n_pairs; i++)
    {
        switch(a_data[i].value_type)        
        {
            case JSON_TYPE_BOOL:
                json_object_object_add(obj, a_data[i].field, 
                        json_object_new_boolean(a_data[i].b_value));
                break;
            case JSON_TYPE_INT:
                json_object_object_add(obj, a_data[i].field, 
                        json_object_new_int(a_data[i].int_value));
                break;
            case JSON_TYPE_DOUBLE:
                json_object_object_add(obj, a_data[i].field, 
                        json_object_new_double(a_data[i].double_value));
                break;
            case JSON_TYPE_STRING:
                json_object_object_add(obj, a_data[i].field, 
                        json_object_new_string(a_data[i].str_value));
                break;
            default:
                break;
        }
    }
    // Copy to output
    len = strlen(json_object_to_json_string_ext(obj, JSON_C_TO_STRING_PLAIN));
    *str = malloc(len + 1);
    strcpy(*str, json_object_to_json_string_ext(obj, JSON_C_TO_STRING_PLAIN));
    // Free obj
    json_object_put(obj); 
    // UNLOCK - Use of JSON is not thread protected
    pthread_mutex_unlock(&g_json_mutex);
}

/**
 * Create a JSON Object from a tring
 **/
void jh_getObject(char *str, json_object **obj)
{
    // LOCK - Use of JSON is not thread protected
    pthread_mutex_lock(&g_json_mutex);
    *obj = json_tokener_parse(str);
    // UNLOCK - Use of JSON is not thread protected
    pthread_mutex_unlock(&g_json_mutex);
}

/**
 * Free JSON Object
 **/
void jh_freeObject(json_object *obj)
{
    // LOCK - Use of JSON is not thread protected
    pthread_mutex_lock(&g_json_mutex);    
    json_object_put(obj); 
    // UNLOCK - Use of JSON is not thread protected
    pthread_mutex_unlock(&g_json_mutex);
}

/**
 * Get the Boolean value of a JSON Object Field
 **/
int jh_getObjectFieldAsBool(json_object *obj, char *field, bool *value)
{
    int check;
    // LOCK - Use of JSON is not thread protected
    pthread_mutex_lock(&g_json_mutex);    
    check = getJFieldBool(obj, field, value);
    // UNLOCK - Use of JSON is not thread protected
    pthread_mutex_unlock(&g_json_mutex);
    return check;
}

/**
 * Get the Double value of a JSON Object Field
 **/
int jh_getObjectFieldAsDouble(json_object *obj, char *field, double *value)
{
    int check;
    // LOCK - Use of JSON is not thread protected
    pthread_mutex_lock(&g_json_mutex);    
    check = getJFieldDouble(obj, field, value);
    // UNLOCK - Use of JSON is not thread protected
    pthread_mutex_unlock(&g_json_mutex);
    return check;
}

/**
 * Get the Integer value of a JSON Object Field
 **/
int jh_getObjectFieldAsInt(json_object *obj, char *field, int *value)
{
    int check;
    // LOCK - Use of JSON is not thread protected
    pthread_mutex_lock(&g_json_mutex);    
    check = getJFieldInt(obj, field, value);
    // UNLOCK - Use of JSON is not thread protected
    pthread_mutex_unlock(&g_json_mutex);
    return check;
}

/**
 * Get the String value of a JSON Object Field
 **/
int jh_getObjectFieldAsStringCopy(json_object *obj, char *field, char **value)
{
    int check;
    // LOCK - Use of JSON is not thread protected
    pthread_mutex_lock(&g_json_mutex);    
    check = getJFieldStringCopy(obj, field, value);
    // UNLOCK - Use of JSON is not thread protected
    pthread_mutex_unlock(&g_json_mutex);
    return check;
}
