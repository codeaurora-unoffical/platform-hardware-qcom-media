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
#ifndef JSON_TREE_H
#define JSON_TREE_H

/** @addtogroup json_api
@{ */

/**
@file    json_tree.h
@brief   JSONTree is used to create, manipulate, query and serialize
         JSON (JavaScript Object Notation) objects.
**/

#include "json_types.h"
#include <stdint.h>
typedef struct JSValue    JSValue;
typedef struct JSPair     JSPair;

struct JSPair {
   char *    pszKey;
   JSValue * pjv;
};


/**
 A JSValue represents a node in the tree and it is a data structure exposed to
 user as an opaque pointer.  Each node has an associated type and is one of the listed
 JSONType values.
**/
struct JSValue {
    JSONType type;
    int nMax;
    int nUsed;
    union {
        char *   psz;  // string or number
        int      num;
        JSPair  *pPairs;
        JSValue **ppValues;
    } u;
};

/**
 @brief JSONTree class functions are used to create, manipulate, query and serialize
   JSON (Java Script Object Notation) objects.

 @detail
   JSONTree works as a DOM type of tree representation of the JSON data. A JSValue
   represents a node in the tree and it is a data structure exposed to user as an
   opaque pointer.  Each node has an associated type and is one of the listed
   JSONType values.

   This interface is context-free in the sense that each method in general takes a
   JSValue as an argument which provides the context.

   Each function expects a JSValue of certain type and returns JSONTree::EBADTYPE
   if the JSValue is of wrong type.

   JSValues are created using one of the CreateXXX, ParseJSON or CloneValue methods.
   Once a JSValue is created, it is the user's responsibility to delete it later
   to free the memory using DeleteValue() function, unless consumed by other methods.

   In general, the tree has a root node (object is most common) and various methods
   can be used to build the tree (such as ObjectSet, ArraySet, PathSet etc).  When
   such methods succeed, the "value node" is incorporated into the "container node".
   The ownership transfers from the user to the "container node" and user must not
   call DeleteValue() on these consumed "value nodes".  When the "container node" gets
   deleted, all the child nodes are also deleted.

   Pathget, ArrayGet, ObjectGet and ObjectGetByPos methods are used to query for a
   particular child node.  The child node is returned as a JSValue and in these cases,
   the ownership still lies with the container node and the JSValue must not be deleted
   by the user. If the user wishes a copy of this node, CloneValue() provides a
   deep copy.

   All JSON data strings are assumed to be in utf8 format.

 @note
   All methods return a value of JSONTree::SUCCESS of success or an appropriate error value.
   Most relevant error codes are documented with each method.  However, other errno.h
   codes may be returned and should be handled gracefully.

   In general, a user would create a root node by parsing a JSON string using
   ParseJSON() method.  Alternatively, user could use the CreateXXX() functions
   to create nodes and build a tree using the XXXSet() functions.  Once a tree node,
   it is queryable using one of the GetXXX() methods.  The tree nodes can be updated
   using one of the XXXSet() functions.  Once the final tree node is prepared, it can
   be serialized using Serialize() function to create a JSON string data.

**/
class JSONTree
{
    char*    pcBuf;
    uint32_t   pcSize;
    uint32_t   maxBufSize;

    JSValue  jvTrue;
    JSValue  jvFalse;
    JSValue  jvNull;

    int CreateValue(int type, int nExtra, JSValue **ppv);
    int CreateNumber(const char *cpsz, int nSize, JSValue **ppjv);
    int ObjectSetInternal(JSValue *pvObj, const char *cpszKey, int nLen, JSValue *pjv,
        bool bAdd);  /* True to allow duplicate keys */
    int ObjectAdd(JSValue *pvObj, const char *cpszKey, JSValue *pjv);
    int CreatePathObjects(JSValue *pjvContainer, const char *cpszPath, JSValue *pjv);
    int CloneObject(JSValue *pjv, JSValue **ppvNew);
    int CloneArray(JSValue *pjv, JSValue **ppvNew);
    int ReadValue(JSONParser *p, JSONID id, JSValue **ppv);
    int SerializeValue(JSONGen *pg, JSValue *pjv);

public:
    JSONTree() {
        jvTrue.type = JSONBool;
        jvTrue.u.num = 1;
        jvFalse.type = JSONBool;
        jvFalse.u.num = 0;
        jvNull.type = JSONNull;
    }

   static const int SUCCESS = 0;

   static const int EFAILED = 0x991a500;

   static const int EBADKEY = 0x991a501;

   static const int EBADTYPE = 0x991a502;

   static const int EBADFORMAT = 0x991a503;

   static const int EOVERFLOW = 0x991a504;

   /**
   This function parses the given JSON string into a corresponding JSValue.

   @param cpszJSON: JSON string
   @param nSize:    size of JSON string
   @param JSValue:  JSValue of the JSON String
   @return JSONTree::SUCCESS:           on success
           JSONTree::EBADFORMAT:  if JSON is badly formed
           JSONTree::EBADTYPE:    if JSON is badly formed/typed
   **/
   int ParseJSON ( const char* cpszJSON,  int nSize,  JSValue** ppjv);

   /**
   This function serializes given JSValue to a JSON formatted string

   @param pjv :    Pointer to a JSValue Structure
   @param sz :    Buffer that holds the serialized JSON
   @param szLen :   Length of psz buffer
   @param szLenReq :  on return, contains the size of a buffer necessary to hold the full
   @return JSONTree::SUCCESS:  on success
      or any of the declared error values
   **/
   int Serialize(JSValue* pjv,  char* sz,  int szLen,  int* szLenReq);

   /**
   This function creates an empty JSValue object.

   @param ppjv :   Filled with the new JSValue object
   @return Returns JSONTree::SUCCESS on success or any of the declared error values
   **/
   int CreateObject(JSValue** ppjv);

   /**
   This function creates an empty JSValue array.

   @param ppjv :   Filled with the new JSValue array
   @return Returns JSONTree::SUCCESS on success or any of the declared error values
   **/
   int CreateArray(JSValue** ppjv);

   /**
   This function creates an JSValue to represent a string in utf8 format.
   The type of this value is JSONString.

   @param cpsz :   string to be stored
   @param nSize :  size of the cpsz string
   @param ppjv :   filled with the new JSValue object
   @return Returns JSONTree::SUCCESS on success or any of the declared error values
   **/
   int CreateString(const char* cpsz,  int nSize,  JSValue** ppjv);

   /**
   This function creates a JSValue to represent an integer.
   The type of this value is JSONNumber.

   @param num :    Integer to be stored in the JSValue
   @param ppjv :   Filled with the new JSValue object
   @return Returns JSONTree::SUCCESS on success or any of the declared error values
   **/
   int CreateInt(int num,  JSValue** ppjv);

   /**
   This function creates a JSValue to represent an unsigned integer.
   The type of this value is JSONNumber.

   @param num :    Unsigned integer to be stored in the JSValue
   @param ppjv :   Filled with the new JSValue object
   @return Returns JSONTree::SUCCESS on success or any of the declared error values
   **/
   int CreateUInt(unsigned int num,  JSValue** ppjv);

   /**
   This function creates an JSValue to represent a floating point number.
   The type of this value is JSONNumber.

   @param d :      floating point number to be stored in the JSValue
   @param ppjv :   filled with the new JSValue object
   @return Returns JSONTree::SUCCESS on success or any of the declared error values
   **/
   int CreateDouble(double d,  JSValue** ppjv);

   /**
   This function creates an JSValue to represent a boolean value.
   The type of this value is JSONBool.

   @param b :      boolean value to store
   @param ppjv :   Filled with the new JSValue object
   @return Returns JSONTree::SUCCESS on success or any of the declared error values
   **/
   int CreateBool(bool b,  JSValue** ppjv);

   /**
   This function creates an JSValue to represent a null value.
   The type of this value is JSONNull.

   @param ppjv :   Filled with the new JSValue object
   @return Returns JSONTree::SUCCESS on success or any of the declared error values
   **/
   int CreateNull(JSValue** ppjv);

   /**
   This function retrieves the string value from the given JSValue of
   JSONString type.

   @param pjv :   JSValue of type JSONString
   @param cppsz :   filled with a pointer to JSON string
   @param pnLen :   length of the cppsz
   @return JSONTree::SUCCESS:         on success
           JSONTree::EBADTYPE:  if JSValue is wrong type
   **/
   int GetString(JSValue* pjv,  const char ** cppsz,  int* pnLen);

   /**
   This function retrieves the integer value from the given JSValue of
   JSONNumber type.  The underlying number must be in the range of C "int" type

   @param pjv :    JSValue of type JSONNumber
   @param pn :     filled with number
   @return JSONTree::SUCCESS:         on success
           JSONTree::EBADTYPE:  if JSValue is wrong type
           JSONTree::EOVERFLOW: if number is not in valid range of C "int" type
           JSONTree::EFAILED:   failed to parse or other internal error
   **/
   int GetInt(JSValue* pjv,  int* pn);

   /**
   This function retrieves the unsigned integer value from the JSValue of
   JSONNumber type. The number must be in the range of C "unsigned int" type

   @param pjv :    JSValue of type JSONNumber
   @param pu :     filled with number
   @return JSONTree::SUCCESS:         on success
           JSONTree::EBADTYPE:  if JSValue is wrong type
           JSONTree::EOVERFLOW: if number is not in valid range of C "unsigned int" type
           JSONTree::EFAILED:   failed to parse or other internal error
   **/
   int GetUInt(JSValue* pjv,  unsigned int* pu);

   /**
   This function retrieves the floating point value from the given JSValue of
   JSONNumber type.

   @param pjv :   JSValue of type JSONNumber
   @param pd :    Filled with double value
   @return JSONTree::SUCCESS:         on success
           JSONTree::EBADTYPE:  if JSValue is wrong type
           JSONTree::EOVERFLOW: if number is not in valid range of C "double" type
           JSONTree::EFAILED:   failed to parse or other internal error
   **/
   int GetDouble(JSValue* pjv,  double* pd);

   /**
   This function retrieves the boolean value from the given JSValue of
   JSONBool type.

   @param pjv :    JSValue of JSONBool
   @param pb :     filled with the boolean value
   @return JSONTree::SUCCESS:         on success
           JSONTree::EBADTYPE:  if JSValue is wrong type
   **/
   int GetBool(JSValue* pjv,  bool* pb);

   /**
   Use this function to find out the type of \ref JSValue.

   @param pjv :      JSValue of whose type needs to be retrieved
   @param pjt :      filled with JSONType
   @return Returns JSONTree::SUCCESS on success or any of the declared error values
   **/
   int GetType(JSValue* pjv,  JSONType* pjt);

   /**
   Use this function to find out the size of a given array or object.
   Array size is number of elements and size of object is number of pairs.

   @param pjvArrayOrObj :  JSValue must be an array or object
   @param pnEntries :      filled with number of items in the array/object
   @return JSONTree::SUCCESS:   on success
           JSONTree::EBADTYPE:  if JSValue is not of JSONObject or JSONArray
   **/
   int GetNumEntries(JSValue* pjvArrayOrObj,  int* pnEntries);

   /**
   Given an array and a position, this function provides the corresponding JSValue

   @param pjvArray :    JSValue array
   @param nPos :        zero based array index to lookup value
   @param ppjv :        filled with the JSValue at the given position
   @return JSONTree::SUCCESS:     on success and ppjv is filled with corresponding JSValue
           JSONTree::EBADTYPE:    if pjvArray is not an array
           JSONTree::EOUTOFRANGE: if nPos in out of range
   **/
   int ArrayGet(JSValue* pjvArray,  int nPos,  JSValue** ppjv);

   /**
   Given an array and index, this function sets the given JSValue.

   For an existing index, the old value is deleted (its memory freed) and
   replaced with the given JSValue.

   If the index is past the end of the existing array, the array is extended to be
   large enough to contain the index passed in. All values between the end of the
   original array and the supplied index are filled with JSONNull values.

   If index is out of range, then the array is extended with JSONNull values
   until the supplied index and then is set with the given JSValue.

   @param pjvArray :    JSValue array
   @param nPos :        zero based array index at which pjv will be set
   @param pjv :         JSValue to set at the given index
   @return JSONTree::SUCCESS:   on success
           JSONTree::EBADTYPE:  if pjvArray is not an array
           ENOMEM:              if out of memory
   **/
   int ArraySet(JSValue* pjvArray,  int nPos,  JSValue* pjv);

   /**
   This function appends the JSValue to end of the array and thus expands it
   by one entry.

   @param pjvArray :    JSValue array
   @param pjv :         JSValue to append to the array
   @return JSONTree::SUCCESS   : on success
           JSONTree::EBADTYPE  : if pjvArray is not an array
           ENOMEM              : if out of memory
   **/
   int ArrayAppend(JSValue* pjvArray,  JSValue* pjv);

   /**
   A JSON object is a set of key/value pairs.  However, these pairs can also be
   accessed by their zero based index similar to arrays and this combined with
   the JSONTree::GetNumEntries is the way to traverse all pairs in an object.
   This function, given an object and an index, retrieves the corresponding pair.

   @param pjvObj :   JSValue object
   @param nPos :     zeo-based index at which to retrieve the pair
   @param pjsp :     resulting JSPair
   @return JSONTree::SUCCESS:     success and ppjv is filled with corresponding JSValue
           JSONTree::EBADTYPE:    if pjvObj is not an object
           JSONTree::EOUTOFRANGE: if nPos in out of range
   **/
   int ObjectGetByPos(JSValue* pjvObj,  int nPos,  JSPair* pjp);

   /**
   This function, given an object and a key, retrieves the corresponding pair
   matching the key.

   @param pjvObj :   JSValue object
   @param cpszKey :  key value for lookup
   @param pjsp :     resulting JSPair
   @return JSONTree::SUCCESS   : on success
           JSONTree::EBADTYPE  : if pjvObj is not an object
           AEE_ENOSUCH         : if key is not found
   **/
   int ObjectGet(JSValue* pjvObj,  const char* cpszKey,  JSPair* pjp);

   /**
   This function sets the value for the given key.

   If the the key is already present, then the corresponding value is deleted
   (its memory freed) and replaced with the given JSValue.
   If the key is not present, then a new key and value pair are added.

   @param pjvObj :   JSValue object
   @param cpszKey :  key value for lookup
   @param pjv :      JSValue for the key
   @return JSONTree::SUCCESS   : on success
           JSONTree::EBADTYPE  : if pjvObj is not an object
           ENOMEM              : if out of memory
   **/
   int ObjectSet(JSValue* pjvObj,  const char* cpszKey,  JSValue* pjv);

   /**
   The function provides a generic mechanism to access any nested JSON value
   using the path string format described below.

   The format uses period (".") as the separator and hence strings with periods
   cannot use this function for lookup.  The string is divided into multiple
   segments with each segment except the last pointing to an object or array name.
   The last segment identifies the leaf node; for objects this would be the
   name of the key and for arrays an index number.


   Examples:
   @verbatim
      JSON: { "Address" : { "Street Name" : { "Name": "Main Blvd", "Number": 1111 }}}
      Path: "Address.Street Name.Number"
      Retrieves: 1111

      JSON: [0, 1, { "x" : 234} ]
      Path: "2.x"
      Retrieves: 234

      JSON: { "auth": { "options" : ["rc4", "aes"] } }
      Path: "auth.options.0"
      Retrieves: "rc4"
   @endverbatim

   @param pjvRoot :  root object to start from - must be an array or object
   @param cpszPath :  path in the format described above
   @param ppjv :  filled with the found JSValue
   @return JSONTree::SUCCESS   : on success
           JSONTree::EBADTYPE  : pjvRoot is neither an array nor an object
           ENOMEM              : if out of memory
   **/
   int PathGet(JSValue* pjvRoot,  const char* cpszPath,  JSValue** ppjv);

   /**
   The function provides a generic mechanism to set any nested JSON value
   using the path string format described in JSONTree::PathGet.

   If the path segment values are not present, then they are created and the leaf
   node is set with the given JSValue.  If path node is present, then it is
   replaced with the given JSValue and the old value is deleted and its memory freed.

   Please refer to ArraySet() and ObjectSet() for more details on how Set() behaves.

   @param pjvRoot :  root object to start from - must be an array or object
   @param cpszPath :  path in the format described above
   @param pjv :  JSValue to be set
   @return JSONTree::SUCCESS  : on success
           JSONTree::EBADTYPE : pjvRoot is neither an array nor an object
           ENOMEM             : if out of memory
   @sa:  ArrayGet(), ObjectSet()
   **/
   int PathSet(JSValue* pjvRoot,  const char* cpszPath,  JSValue* pjv);

   /**
   The function deletes and frees the given JSValue and all its children elements.

   @param pjv :  JSValue to be deleted
   @return JSONTree::SUCCESS:   on success
   **/
   int DeleteValue(JSValue* pjv);

   /**
   The function clones the given JSValue by performing a deep copy.

   @param pjvSource :  JSValue to be cloned
   @param ppjvCloned :  filled with the cloned JSValue
   @return JSONTree::SUCCESS:   on success
   **/
   int CloneValue(JSValue* pjvSource,  JSValue** ppjvCloned);
};

/** @} */ /* json_api */
#endif /* #ifndef JSON_TREE_H */
