//----------------------------------------------------------------------------//
//                               OBJECT HISTORY                               //
//----------------------------------------------------------------------------//
//  REVISION |    DATE     |                               |      AUTHOR      //
//----------------------------------------------------------------------------//
//  1.00     | 28/Nov/2021 |                               | ALCP             //
// - Creation of file                                                         //
//----------------------------------------------------------------------------//

#ifndef JSONHANDLER_H
#define JSONHANDLER_H

#ifdef __cplusplus
extern "C" {
#endif
    
/*
* Includes
*/
#include <json-c/json.h>

//----------------------------------------------------------------------------//
// EXTERNAL DEFINITIONS
//----------------------------------------------------------------------------//
/* ERRORS */
#define JSON_OK 0
#define JSON_ERROR_FILE  -1
#define JSON_ERROR_TYPE  -2
#define JSON_ERROR_OTHER -3

/* Other Definitions */
typedef enum
{
    JSON_DEPTH_LEVEL = 0,
    JSON_DEPTH_LEVEL_AND_INDEX,
    JSON_DEPTH_FIELD
}json_depth_t;
    
//----------------------------------------------------------------------------//
// EXTERNAL TYPES
//----------------------------------------------------------------------------//
/* Json Types */
typedef enum
{
    JSON_TYPE_BOOL = 0,
    JSON_TYPE_INT,
    JSON_TYPE_DOUBLE,
    JSON_TYPE_STRING
}json_pairs_t;

/* Field / Data pair */
typedef struct  
{
    char *field;
    json_pairs_t value_type;
    bool b_value;
    int int_value;
    double double_value;
    char *str_value;
} jsonFieldData;

//----------------------------------------------------------------------------//
// EXTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
/**
 * Read the JSON configuration file
 **/
void jh_readConfigFile(void);

/**
 * Free allocated memory of config file object
 **/
void jh_freeConfigFile(void);

/**
 * Get a boolean from a JSON object
 * 
 * \param   level       string for the level within the config file (INPUT)
 * \param   levelIndex  if the level returns an array, the element number 
 *                          "levelIndex" is considered for the Level. (INPUT)
 * \param   field       Field as a string to be searched (INPUT)
 * \param   fieldIndex  if the field returns an array, the element number 
 *                          "fieldIndex" is considered for the field. (INPUT)
 * \param   subField    if the field Index is an object, the SubField is used 
 *                          to get correct value. (INPUT)
 * \param   value   Value to be filled (OUTUT)
 *  
 * \return  JSON_OK             Value matches type and is not NULL
 *          JSON_ERROR_TYPE     Type does not match
 **/
int jh_getJFieldBool(const char *level, int levelIndex, const char *field, 
        int fieldIndex, const char *subField, bool *value);

/**
 * Get a double from a JSON object
 * 
 * \param   level       string for the level within the config file (INPUT)
 * \param   levelIndex  if the level returns an array, the element number 
 *                          "levelIndex" is considered for the Level. (INPUT)
 * \param   field       Field as a string to be searched (INPUT)
 * \param   fieldIndex  if the field returns an array, the element number 
 *                          "fieldIndex" is considered for the field. (INPUT)
 * \param   subField    if the field Index is an object, the SubField is used 
 *                          to get correct value. (INPUT)
 * \param   value   Value to be filled (OUTUT)
 *  
 * \return  JSON_OK             Value matches type and is not NULL
 *          JSON_ERROR_TYPE     Type does not match
 **/
int jh_getJFieldDouble(const char *level, int levelIndex, const char *field, 
        int fieldIndex, const char *subField, double *value);

/**
 * Get an integer from a JSON object
 * 
 * \param   level       string for the level within the config file (INPUT)
 * \param   levelIndex  if the level returns an array, the element number 
 *                          "levelIndex" is considered for the Level. (INPUT)
 * \param   field       Field as a string to be searched (INPUT)
 * \param   fieldIndex  if the field returns an array, the element number 
 *                          "fieldIndex" is considered for the field. (INPUT)
 * \param   subField    if the field Index is an object, the SubField is used 
 *                          to get correct value. (INPUT)
 * \param   value   Value to be filled (OUTUT)
 *  
 * \return  JSON_OK             Value matches type and is not NULL
 *          JSON_ERROR_TYPE     Type does not match
 **/
int jh_getJFieldInt(const char *level, int levelIndex, const char *field, 
        int fieldIndex, const char *subField, int *value);

/**
 * Get a string copy from a JSON object 
 * - STRING HAS TO BE FREED AFTERWARDS!
 * 
 * \param   level       string for the level within the config file (INPUT)
 * \param   levelIndex  if the level returns an array, the element number 
 *                          "levelIndex" is considered for the Level. (INPUT)
 * \param   field       Field as a string to be searched (INPUT)
 * \param   fieldIndex  if the field returns an array, the element number 
 *                          "fieldIndex" is considered for the field. (INPUT)
 * \param   subField    if the field Index is an object, the SubField is used 
 *                          to get correct value. (INPUT)
 * \param   value   Value to be filled (OUTUT)
 * 
 * \return  JSON_OK             Value matches type and is not NULL
 *          JSON_ERROR_TYPE     Type does not match
 *          JSON_ERROR_OTHER    Null string
 **/
int jh_getJFieldStringCopy(const char *level, int levelIndex, const char *field, 
        int fieldIndex, const char *subField, char **value);

/**
 * Get the number of elements of a given JSON Array 
 * 
 * \param   level       string for the level within the config file (INPUT)
 * \param   levelIndex  if the level returns an array, the element number 
 *                          "levelIndex" is considered for the Level. (INPUT)
 * \param   field       Field as a string to be searched (INPUT)
 * \param   depth       Where to search within config file:
 *                      JSON_DEPTH_LEVEL = search level only 
 *                      JSON_DEPTH_LEVEL_AND_INDEX = search level and levelIndex 
 *                      JSON_DEPTH_FIELD = search level and levelIndex and field
 * \param   value       Number of elements in the JSON Array (OUTUT)
 *  
 * \return  JSON_OK             Value matches type and is not NULL
 *          JSON_ERROR_TYPE     Type does not match
 *          JSON_ERROR_OTHER    Other Error
 **/
int jh_getJArrayElements(const char *level, int levelIndex, const char *field, 
        json_depth_t depth, int *value);

/**
 * Get a string array copy from a JSON object
 * - STRINGS HAVE TO BE FREED AFTERWARDS!
 * 
 * \param   level   string for the level within the configuraton file (INPUT)
 * \param   field   Field as a string to be searched (INPUT)
 * \param   value   Value to be filled (OUTUT)
 *  
 * \return  JSON_OK             Value matches type and is not NULL
 *          JSON_ERROR_TYPE     Type does not match
 *          JSON_ERROR_OTHER    Null string
 **/
int jh_getJFieldStringArrayCopy(const char *level, const char *field, 
        int *elementslen, char ***value);

/**
 * Create a string based on Field / Value pairs of the input array
 * 
 * \param   a_data      array with all Field / values to be filled (INPUT)
 * \param   n_pairs     number of field / value pairs (INPUT)
 * \param   str         string to be filled (OUTUT)
 *  
 **/
void jh_getStringFromFieldValuePairs(jsonFieldData *a_data, int n_pairs, 
        char **str);

/**
 * Create a JSON Object from a tring
 * 
 * \param   str         (INPUT) String to be set to JSON obj
 * \param   obj         (OUTPUT) JSON Obj
 *  
 **/
void jh_getObject(char *str, json_object **obj);

/**
 * Free JSON Object
 * 
 * \param   obj         (INPUT) JSON Obj
 *  
 **/
void jh_freeObject(json_object *obj);

/**
 * Get the Boolean value of a JSON Object Field
 * 
 * \param   obj         (INPUT) JSON Obj
 * \param   value       (OUTPUT) value to be set
 * 
 * * \return    JSON_OK             Value matches type and is not NULL
 *              JSON_ERROR_TYPE     Type does not match
 *              JSON_ERROR_OTHER    Nothing found / NULL Found
 *  
 **/
int jh_getObjectFieldAsBool(json_object *obj, char *field, bool *value);

/**
 * Get the Double value of a JSON Object Field
 * 
 * \param   obj         (INPUT) JSON Obj
 * \param   value       (OUTPUT) value to be set
 * 
 * * \return    JSON_OK             Value matches type and is not NULL
 *              JSON_ERROR_TYPE     Type does not match
 *              JSON_ERROR_OTHER    Nothing found / NULL Found
 *  
 **/
int jh_getObjectFieldAsDouble(json_object *obj, char *field, double *value);

/**
 * Get the Integer value of a JSON Object Field
 * 
 * \param   obj         (INPUT) JSON Obj
 * \param   value       (OUTPUT) value to be set
 * 
 * * \return    JSON_OK             Value matches type and is not NULL
 *              JSON_ERROR_TYPE     Type does not match
 *              JSON_ERROR_OTHER    Nothing found / NULL Found
 *  
 **/
int jh_getObjectFieldAsInt(json_object *obj, char *field, int *value);

/**
 * Get the String value of a JSON Object Field
 * 
 * \param   obj         (INPUT) JSON Obj
 * \param   value       (OUTPUT) value to be set
 * 
 * * \return    JSON_OK             Value matches type and is not NULL
 *              JSON_ERROR_TYPE     Type does not match
 *              JSON_ERROR_OTHER    Nothing found / NULL Found
 *  
 **/
int jh_getObjectFieldAsStringCopy(json_object *obj, char *field, char **value);


#ifdef __cplusplus
}
#endif

#endif /* CONFIG_H */