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
#ifndef JSON_GEN_H
#define JSON_GEN_H

/** @addtogroup json_api
@{ */

/**
@file    json_gen.h
@brief   JSON (JavaScript Object Notation) Serialization API
@details
   This library provides a set of functions to serialize JSON values to
   construct JSON text. All JSON strings are assumed to be in utf8 format.

   The API is strictly linear in building JSON. Each method appends its data
   to the end of the JSON string being generated. If the original buffer
   runs out of space, JSONGen creates a new buffer using the provided
   allocator function and expands it as necessary.

   The serialization code uses simple heuristics in inserting separators
   in appropriate places.  However, it cannot catch any significant
   formatting errors such as PutObject() followed by EndArray() etc.

**/

#ifdef __cplusplus
extern "C" {
#endif /* #ifdef __cplusplus */


typedef struct JSONGen JSONGen;

/**< Error in operation */
#define JSONGEN_ERROR       0xFFFFFFFF

/**< Operation is successful */
#define JSONGEN_SUCCESS     0

/**
 Function to allocate/reallocate/free memory. The basic functionality
is similar to the standard C library realloc() function.

A new allocation is done by reallocating a NULL pointer to the
desired size.

Freeing an allocated block is done by reallocating the
block to size zero (0).

A reallocation is done by reallocating a valid pointer to the
desired size.


 @param ppOut :  (in/out) on entry, *ppOut should have the existing pointer
 @param nSize :  the requested new size of the block, or zero (0) to request that
 @param pvCtx :  Context as provided in the Ctor()
 @return JSONGEN_SUCCESS - Allocation (or free) was successful
or other error code
**/
typedef int (* JSONGenReallocFunc)(void *pvCtx, int nSize, void **pp);

struct JSONGen {
   char *             psz;
   int                nMax;
   char *             pszStatic;
   int                nCurr;
   JSONGenReallocFunc pfnRealloc;
   void *             pvCtx;
};

/**
Initializes the JSONGen structure.

If the original buffer runs out of space and a reallocation function
is provided, then a new buffer is created with the data from original
buffer and expanded as needed.

The caller provides the JSONGen structure.  This allows the structure
to be stack declared or dynamically declared based on the caller needs.
Stack allocation is attractive option for quick parsing needs.  It is
also safe to provide stack declared buffer as the original buffer.



 @param pj :  JSON generator
 @param pvCtx :  Context to be passed back to pfnRealloc while calling
 @param psz :  Buffer to hold JSON text
 @param pfnRealloc :  Reallocation function for buffer creation and expansion
 @param nMax :  Max size of the psz buffer
 @return
None
**/
extern void         JSONGen_Ctor(JSONGen *pj, char *psz, int nMax,
                                 JSONGenReallocFunc pfnRealloc, void *pvCtx);

/**
 Frees any allocated buffers by calling the allocator

 @param pj :  JSON generator
 @return None
**/
extern void         JSONGen_Dtor(JSONGen *pj);

/**
When you are done specifying the JSON to be generated, call this
function to complete the generation and return the JSON string.

 @param ppcszJSON :   filled with JSON string pointer
 @param pj :          JSON generator
 @param pnSize :      size of the JSON string
 @return JSONGEN_SUCCESS:  on success
         JSONGEN_ERROR:    if empty JSON buffer or other error
**/
extern int          JSONGen_GetJSON(JSONGen *pj, const char **ppcszJSON, int *pnSize);

/**
Serialize start of an object

 @param pj :          JSON generator
 @return JSONGEN_SUCCESS:       on success
         JSONGEN_ERROR or other error code
**/
extern int          JSONGen_BeginObject(JSONGen *pj);

/**
Serialize end of an object

 @param pj :          JSON generator
 @return JSONGEN_SUCCESS:       on success
         JSONGEN_ERROR or other error code
**/
extern int          JSONGen_EndObject(JSONGen *pj);

/**
Serialize an object key in the utf8 format to JSON format and then
appends a colon

 @param nLen :     length of the key
 @param pj :       JSON generator
 @param pszKey :   JSON string in utf8 format
 @return JSONGEN_SUCCESS:       on success
         JSONGEN_ERROR or other error code on failure
**/
extern int          JSONGen_PutKey(JSONGen *pj, const char *pszKey, int nLen);

/**
Serialize start of an array

 @param pj :          JSON generator
 @return JSONGEN_SUCCESS:       on success
         JSONGEN_ERROR or other error code
**/
extern int          JSONGen_BeginArray(JSONGen *pj);

/**
Serialize end of an array

 @param pj :          JSON generator
 @return JSONGEN_SUCCESS:       on success
         JSONGEN_ERROR or other error code
**/
extern int          JSONGen_EndArray(JSONGen *pj);

/**
Serialize a string in the utf8 format to JSON format.

 @param nLen :     length of the key
 @param pj :       JSON generator
 @param pszKey :   JSON string in utf8 format
 @return JSONGEN_SUCCESS:       on success
         JSONGEN_ERROR or other error code on failure
**/
extern int          JSONGen_PutString(JSONGen *pj, const char *pszKey, int nLen);

/**
Serialize an integer value

 @param pj :       JSON generator
 @param num :      integer value to be serialized
 @return JSONGEN_SUCCESS:       on success
         JSONGEN_ERROR or other error code on failure
**/
extern int          JSONGen_PutInt(JSONGen *pj, int num);

/**
Serialize an unsigned integer value


 @param pj :       JSON generator
 @param num :      unsigned integer value to be serialized
 @return JSONGEN_SUCCESS:       on success
         JSONGEN_ERROR or other error code on failure
**/
extern int          JSONGen_PutUInt(JSONGen *pj, unsigned int num);

/**
Serialize a floating point value

 @param pj :       JSON generator
 @param d :        floating point value to be serialized
 @return JSONGEN_SUCCESS:       on success
         JSONGEN_ERROR or other error code on failure
**/
extern int          JSONGen_PutDouble(JSONGen *pj, double d);

/**
Serialize a boolean value (true or false)

 @param pj :       JSON generator
 @param bVal :     if 0, puts "false", otherwise puts "true"
 @return JSONGEN_SUCCESS:       on success
         JSONGEN_ERROR or other error code on failure
**/
extern int          JSONGen_PutBool(JSONGen *pj, int bVal);

/**
Serialize a null value

 @param pj :       JSON generator
 @return JSONGEN_SUCCESS:       on success
         JSONGEN_ERROR or other error code on failure
**/
extern int          JSONGen_PutNull(JSONGen *pj);

/**
Serializes given JSON formatted data as is (no further JSON escaping).
The separator insertion logic is invoked when necessary.

Examples:
PutJSON: { "settings" : [2
followed by: PutDouble: 123.345
followed by: PutJSON: ]}
generates: { "settings" : [2, 123.456 ]}

 @param nLen :      length of the string
 @param pszJSON :   JSON formatted string
 @param pj :        JSON generator
 @return JSONGEN_SUCCESS:       on success
         JSONGEN_ERROR or other error code on failure
**/
extern int          JSONGen_PutJSON(JSONGen *pj, const char *pszJSON, int nLen);

#ifdef __cplusplus
}
#endif /* #ifdef __cplusplus */

#endif
