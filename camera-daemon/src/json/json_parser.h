/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
#ifndef JSON_PARSER_H
#define JSON_PARSER_H

/** @addtogroup json_api
@{ */

/**
@file    json_parser.h
@brief   API to parse JSON (JavaScript Object Notation) strings
@details
   This library provides a set of functions to parse JSON data.
   The parsing and accessing functions provide a simple and generic mechanism
   which requires no memory allocation.

   A JSONID (or simply ID) refer to a specific value within JSON data and identifies
   a particular JSONType (or just type) value. Given a JSONID, GetType() returns
   its type.

   The root of JSON data is represented by the special JSONID defined as JSONID_ROOT.
   Use this as the jumping point for looking up other IDs.

   Once the parser is constructed using Ctor(), one of the Lookup() methods
   can be used to query the nested JSON data. EnumInit() and Next() methods provide
   a way to enumerate arrays and objects to retrieve their member IDs.

   Relevant JSONIDs can then be passed to one of the GetXXX() methods to retrieve
   their underlying values.

   Use GetType() to find out the JSONType at a particular JSONID.

   A JSONParser can be declared on the stack for quick parsing needs.
**/

#ifdef __cplusplus
extern "C" {
#endif /* #ifdef __cplusplus */

#include "json_types.h"

typedef struct JSONParser JSONParser;
typedef int JSONEnumState;
typedef int JSONID;

#define JSONID_ROOT 0

/**< Operation is successful */
#define JSONPARSER_SUCCESS    0

/**< Error in operation */
#define JSONPARSER_ERROR      0xFFFFFFFF

/**< exceeded the max value */
#define JSONPARSER_EOVERFLOW  0xFFFFFFFD

/**< No more value */
#define JSONPARSER_ENOMORE    0xFFFFFFFC

struct JSONParser {
   const char *  psz;
   unsigned int  uMax;
};

/**
 Initializes the JSONParser structure with the provided JSON text string
and the size.  If size is 0, then string is considered null-terminated.

The caller provides the JSONParser structure.  This allows this structure
to be stack declared or dynamically declared based on the caller needs.
Stack allocation is attractive option for quick parsing needs.

Example:
const char *szJSON[] = "{\"company name\" : \"Foo\" }";
JSONParser jp={0};
JSONParser_Ctor(&jp, szJSON, 0);


 @param psz :  JSON data
 @param pj :  JSON context
 @param nSize :  Length of the JSON data passed.
**/
extern void JSONParser_Ctor(JSONParser *pj, const char *psz, int nSize);

/**
 Returns the type of the JSON object at given JSONID


 @param id :  ID at which the JSONType is identified
 @param pj :  JSON context
 @param pjt :  filled with JSONType
 @return
JSONPARSER_SUCCESS on success, otherwise JSONPARSER_ERROR
**/
extern int  JSONParser_GetType(JSONParser *pj, JSONID id, JSONType *pjt);

/**
 Caller supplies JSON data to construct the parser. This function makes a copy
of this buffer at the given JSONID value upto its logical end. The ID must
point to a valid JSONType or JSONID_ROOT.

For example, if JSONID starts a string, then the copied buffer size is the
size of such string.  However, if it is an array, then the size would be
upto the end of such array, including all its nested values.



 @param id :      ID at which the JSON data is copied
 @param ppcsz :   filled with underlying JSON data pointer
 @param pj :      JSON context
 @param pnSize :  size of the valid JSON data
 @return
JSONPARSER_SUCCESS on success, otherwise JSONPARSER_ERROR
**/
extern int  JSONParser_GetJSON(JSONParser *pj, JSONID id, const char **ppcsz, int *pnSize);

/**
 Initializes the enumeration state (pstEnum) to point to the first pair of
the object at the given JSON ID.



 @param idObject :  JSONObject ID
 @param pj :  JSON context
 @param pstEnum :  Enumeration state is updated on success
 @return JSONPARSER_SUCCESS on success, otherwise JSONPARSER_ERROR
**/
extern int  JSONParser_ObjectEnumInit(JSONParser *pj, JSONID idObject,
                                      JSONEnumState *pstEnum);

/**
 Given the enumeration state, reads the key and value ids of the
current pair and updates the enumeration state to next pair or end.


 @param pst :  current enumeration state. On success, is updated to next pair or end
 @param pidKey :  Key ID of the current pair
 @param pj :  JSON context
 @param pidValue :  Value ID
 @return JSONPARSER_SUCCESS : success -- pidKey and pidValue are updated with relevant IDs
JSONPARSER_ENOMORE : if no more items to enumarate
JSONPARSER_ERROR   : for all other errors
**/
extern int  JSONParser_ObjectNext(JSONParser *pj, JSONEnumState *pst,
                                  JSONID *pidKey, JSONID *pidValue);

/**
 Initializes the enumeration state (pstEnum) to point to the first value of
the array at the given JSON ID.


 @param idArray :  JSONArray ID
 @param pj :  JSON context
 @param pstEnum :  Enumeration state is updated on success
 @return JSONPARSER_SUCCESS on success, otherwise JSONPARSER_ERROR
**/
extern int  JSONParser_ArrayEnumInit (JSONParser *pj, JSONID idArray,
                                      JSONEnumState *pstEnum);

/**
 Given the enumeration state, reads the value id at the current state
and updates the enumeration state to next value or end.


 @param pst :  current enumeration state. On success, is updated to next value or end
 @param pj :  JSON context
 @param pidValue :  Value ID
 @return JSONPARSER_SUCCESS : success -- pidValue is updated with relevant ID
JSONPARSER_ENOMORE : if no more items to enumarate
JSONPARSER_ERROR   : for all other errors
**/
extern int  JSONParser_ArrayNext (JSONParser *pj, JSONEnumState *pst, JSONID *pidValue);

/**
 Given an Object ID and a key name, finds the matching key and if found,
updates the key and value IDs


 @param pszName :  JSON string to be looked up
 @param pidKey :  Key ID of the current pair
 @param idObject :  JSONObject ID
 @param pidValue :  Value ID
 @param pj :  JSON context
 @return JSONPARSER_SUCCESS on success, otherwise JSONPARSER_ERROR
On success, pidKey and pidValue are updated with relevant IDs
**/
extern int  JSONParser_ObjectLookup(JSONParser *pj, JSONID idObject,
                                    const char *pszName, int nLen,
                                    JSONID *pidKey, JSONID *pidValue);

/**
 Given an array ID and the 0-based index, finds the corresponding value
and if found, updates the value ID


 @param idArray :  Array ID
 @param nIndex :  0-based array index to lookup the value
 @param pj :  JSON context
 @param pidValue :  Value ID
 @return JSONPARSER_SUCCESS on success, otherwise JSONPARSER_ERROR
On success, pidValue is updated with relevant ID
**/
extern int  JSONParser_ArrayLookup(JSONParser *pj, JSONID idArray,
                                   int nIndex, JSONID *pidValue);

/**
 The function provides a generic mechanism to access any JSON value
using the string format described below.

Format uses period (".") as the separator and hence strings with periods
cannot use this function for lookup.  Format is divided into multiple
segments with each segment except last forming an hierarchy of containers.
The last segment identifies the leaf node and for objects this would be a
key and for arrays an index number.

Examples:
JSON: { "Address" : {"Street Name": {"Name": "Main Blvd", "Number": 1111 }}}
Path: "Address.Street Name.Number"
Retrieves: 1111

JSON: [0, 1, { "x" : 234.45} ]
Path: "2.x"
Retrieves: 234.45

JSON: { "auth": { "options" : ["rc4", "aes"] } }
Path: "auth.options.0"
Retrieves: "rc4"


 @param szPath :  string parsed to identify the value location
 @param id :  JSONObject or JSONArray ID
 @param pValID :  Value ID
 @param nLen :  length of the string
 @param pj :  JSON context
 @return JSONPARSER_SUCCESS on success, otherwise JSONPARSER_ERROR
On success, pValID is updated with relevant ID
**/
extern int  JSONParser_Lookup(JSONParser *pj, JSONID id,
                              const char *szPath, int nLen, JSONID *pValID);

/**
 Given a JSONString, retrieve the string value.

 @param pnReq :     on return, contains the size of a buffer necessary to hold the full
 @param pj :     JSON context
 @param psz :     filled with string
 @param id :     JSONString ID
 @param nSize :     size of the psz or 0 to query for the size
 @return JSONPARSER_SUCCESS : on success. Check *pnReq to make sure string is
not truncated
otherwise JSONPARSER_ERROR
**/
extern int  JSONParser_GetString(JSONParser *pj, JSONID id,
                                 char *psz, int nSize, int *pnReq);

extern int  JSONParser_GetKey(JSONParser *pj, JSONID id,
                                 char *psz, int nSize, int *pnReq);

/**
 Given JSONNumber, retrieve its integer value.  The underlying number must be
an integer in the range of C int data type


 @param id :        JSONNumber ID
 @param pi :        filled with integer value at id
 @param pj :        JSON context
 @return JSONPARSER_SUCCESS    : on success
JSONPARSER_EOVERFLOW  : if number is out of range of C "int" type
otherwise JSONPARSER_ERROR
**/
extern int  JSONParser_GetInt (JSONParser *pj, JSONID id, int *pi);

/**
 Given JSONNumber, retrieve its unsigned integer value.  The underlying number
must be an unsigned integer in the range of C "unsigned int" data type


 @param id :        JSONNumber ID
 @param pj :        JSON context
 @param pu :        filled with unsigned integer value at id
 @return JSONPARSER_SUCCESS    : on success
JSONPARSER_EOVERFLOW  : if number is out of range of C "unsigned int" type
otherwise JSONPARSER_ERROR
**/
extern int  JSONParser_GetUInt(JSONParser *pj, JSONID id, unsigned int *pu);

/**
   Given JSONNumber, retrieve its floating point value.

 @param id :        JSONNumber ID
 @param pj :        JSON context
 @param pd :        filled with double value at id
 @return JSONPARSER_SUCCESS    : on success
JSONPARSER_EOVERFLOW  : if number is out of range of C "double" type
otherwise JSONPARSER_ERROR
**/
extern int  JSONParser_GetDouble(JSONParser *pj, JSONID id, double *pd);

/**
 Given JSONNumber, retrieve its boolean value.


 @param pb :        filled with boolean value at id
 @param pj :        JSON context
 @param id :        JSBool ID
 @return JSONPARSER_SUCCESS:   on success,
otherwise JSONPARSER_ERROR
**/
extern int  JSONParser_GetBool(JSONParser *pj, JSONID id, int *pb);

/**
 Given a JSONID, returns success if its a null value


 @param pj :        JSON context
 @param id :        JSONNull location
 @return JSONPARSER_SUCCESS:  if there is a null value at id
otherwise JSONPARSER_ERROR
**/
extern int  JSONParser_GetNull(JSONParser *pj, JSONID id);

#ifdef __cplusplus
}
#endif /* #ifdef __cplusplus */

/** @} */ /* json_api */
#endif
