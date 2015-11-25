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
#include "json_parser.h"
#include <stdlib.h>
#include <math.h>

typedef unsigned int uint;

#define IsInRange(c,a,b) \
   ( (unsigned)(c) - (unsigned)(a) <= (unsigned) ((b)-(a)) )

#define IsAlpha(c)   ( IsInRange((c)|32, 'a', 'z') )
#define IsHexAlpha(c)( IsInRange((c)|32, 'a', 'f') )
#define IsDigit(c)   ( IsInRange(c, '0', '9') )
#define IsHex(c)     ( IsDigit(c) || IsHexAlpha(c) )
#define IsSpace(c)   ( (c) == ' ' ||  (c) == '\t' || (c) == '\r' || (c) == '\n' )
#define IsTagBeginChar(c) ( IsAlpha(c) || (c) == '_' )
#define IsTagNonBeginChar(c) ( IsAlpha(c) || (c) == '_' || IsDigit(c))


#define IsString(c)  ( (c) == '\"' )
#define Lower(c)     ( (c) + 32 )

#define IsNumStart(c) ( IsDigit(c) || (c) == '-' )

#define IsNumChar(c) ( IsNumStart(c) || (c) == '.' ||  (c) == 'e' || (c) == 'E')

#define IsKeyWord(c) ( (c)=='t' || (c)=='r' || (c)=='u' || (c)=='e' || \
                       (c)=='f' || (c)=='a' || (c)=='l' || (c)=='s' || (c)=='n' )

#define IsWordChar(c)  ( IsNumChar(c) || IsKeyWord(c) )

#define IsWordBeginChar(c)  ( IsNumChar(c) || IsTagBeginChar(c) )

#define IsWordNonBeginChar(c)  ( IsNumChar(c) || IsTagNonBeginChar(c) )

#define ReadHexChar(c) ( IsDigit(c) ? (c)-'0': \
                                      ( IsInRange((c),'a','f') ? (c)-'a'+10: \
                                                                 (IsInRange((c),'A','F')?(c)-'A'+10:0xFF)))

#define DigitValue(c)  ((c) - '0')
#define ToLower(c)  ((c) - 32)

#define ERR_RET(e) do { if (JSONPARSER_SUCCESS != (e)) { return JSONPARSER_ERROR; } } while(0)

#define JSON_MAX_INT32 2147483647
#define JSON_MIN_INT32  (~0x7fffffff)   /* -2147483648 is unsigned */
#define JSON_MAX_UINT32 4294967295u

#define JSONPARSER_SUCCESS_MINUS 1

// Skip string at psz.  Assumes psz[0] == '\"'
//
static __inline int SkipString(const char *psz, uint uMax, uint i)
{
   while (++i < uMax && psz[i] != '\"') {
      if (psz[i] == '\\') {
         ++i;    // jump over \\ and \"
      }
   }
   return i+1;
}


// Skip number or keyword at psz.  Assumes IsWord(psz[0]).
//
static __inline int SkipWord(const char *psz, uint uMax, uint i)
{
  if (IsWordBeginChar(psz[i])) {
    do {
      ++i;
    } while (i < uMax && IsWordNonBeginChar(psz[i]));
  }
  return i;
}

// Skip number or keyword at psz.  Assumes IsWord(psz[0]).
//
static __inline int SkipNumber(const char *psz, uint uMax, uint i)
{
   do {
      ++i;
   } while (i < uMax && IsNumChar(psz[i]));
   return i;
}

// Skip spaces (zero or more) at psz.
//
static __inline uint SkipSpace(const char *psz, uint uMax, uint i)
{
   while (i < uMax && IsSpace(psz[i])) {
      ++i;
   }
   return i;
}

// Skip a value or separator (skips nested values within arrays/objects)
//
static int Skip(const char *psz, uint uMax, uint i)
{
   int lvl = 0;

   do {
      char ch;

      if (i >= uMax) {
         return uMax;
      }

      ch = psz[i];
      if ( IsString(ch) ) {
         i = SkipString(psz, uMax, i);
      } else if (IsWordBeginChar(ch)) {
         i = SkipWord(psz, uMax, i);   // skip number/keyword
      } else {
         ++i;
         if ( ch == ':' || ch == ',' ) {
            // nothing
         } else if ( ch == '{' || ch == '[' ) {
            ++lvl;
         } else if ( ch == '}' || ch == ']' ) {
            --lvl;
         } else {
            return JSONPARSER_ERROR;
         }
      }
      i = SkipSpace(psz, uMax, i);
   } while (lvl > 0);

   return i;
}


static int ObjectNext(const char *psz, uint uMax, uint i, int *pnKey, int *pnValue)
{
   uint iColon = Skip(psz, uMax, i);
   uint iValue = Skip(psz, uMax, iColon);
   uint iSep  = Skip(psz, uMax, iValue);
   uint iNext = iSep;

   if (i >= uMax) { //we're alreay at the end
      iNext = i = JSONPARSER_ERROR;
   } else if (psz[i] == '}') {
      iNext = i = JSONPARSER_ENOMORE;
   }
   // Validate that we saw:   STRING ":" VALUE ( "," | "}" )
   else if (iSep >= uMax ||
       !(IsString(psz[i]) || IsTagBeginChar(psz[i])) ||
       psz[iColon] != ':'  ||
       (psz[iSep] != ',' && psz[iSep] != '}') ) {
      iNext = iValue = i = JSONPARSER_ERROR;
   } else if (psz[iSep] == ',') {
      iNext = Skip(psz, uMax, iSep);
   }

   *pnKey = (int) i;
   *pnValue = (int)iValue;
   return iNext;
}


static int ArrayNext(const char *psz, uint uMax, uint i, int *pnValue)
{
   uint iSep = Skip(psz, uMax, i);
   uint iNext = iSep;

   if (i >= uMax) { //we're alreay at the end
      iNext = i = JSONPARSER_ERROR;
   } else if (psz[i] == ']') {
      iNext = i = JSONPARSER_ENOMORE;
   } else if (iSep >= uMax || (psz[iSep] != ',' && psz[iSep] != ']')) {
      iNext = i = JSONPARSER_ERROR;
   } else if ( psz[iSep] == ',') {
      iNext = Skip(psz, uMax, iSep);
   }
   *pnValue = (int) i;
   return iNext;
}


//----------------------------------------------------------------
// JSONParser API
//----------------------------------------------------------------

static int JSONParser_strlen(const char* s)
{
   const char *sEnd = s;

   if (!*s) return 0;
   while (*++sEnd)
      ;
   return sEnd - s;
}

void JSONParser_Ctor(JSONParser *me, const char *psz, int nSize)
{
   me->psz = psz;
   me->uMax = (uint) (nSize > 0 ? nSize : JSONParser_strlen(psz));
}

int JSONParser_GetType(JSONParser *me, JSONID id, JSONType *pjt)
{
   char ch = '\0';
   if ( (unsigned)id < me->uMax) {
      ch = me->psz[id];
   } else {
      return JSONPARSER_ERROR;
   }

   if (IsString(ch)) {
      *pjt = JSONString;
   } else if (ch == '[') {
      *pjt = JSONArray;
   } else if (ch == '{') {
      *pjt = JSONObject;
   } else if (ch == 'n') {
      *pjt = JSONNull;
   } else if (ch == 't' || ch == 'f') {
      *pjt = JSONBool;
   } else if (IsNumStart(ch)) {
      *pjt = JSONNumber;
   } else {
      *pjt = JSONUnknown;
   }
   return JSONPARSER_SUCCESS;
}


//----------------------------------------------------------------
// Object Enumeration
//----------------------------------------------------------------

static int JSONParser_EnumInit(JSONParser *me, JSONID id, JSONEnumState *pst, char match)
{
   char ch;

   if (0 == id ) {
      id = SkipSpace(me->psz, me->uMax, id);
   }

   ch = me->psz[id];
   if (ch == match) { //'{' or '['
      //go past and skip space
      *pst = SkipSpace(me->psz, me->uMax, ++id);
   } else {
      return JSONPARSER_ERROR;
   }

   return ((uint) (*pst) < me->uMax ? JSONPARSER_SUCCESS : JSONPARSER_ERROR);
}

//Enter an Object - Returns the first member name and value IDs
int JSONParser_ObjectEnumInit(JSONParser *me, JSONID id, JSONEnumState *pEnumState)
{
   return JSONParser_EnumInit(me, id, pEnumState, '{');
}

//Enter an Array - return the ID of the first value
int JSONParser_ArrayEnumInit (JSONParser *me, JSONID id, JSONEnumState *pEnumState)
{
   return JSONParser_EnumInit(me, id, pEnumState, '[');
}

//Given the Enum state, find the next member and value IDs and update enum state to next member
int JSONParser_ObjectNext(JSONParser *me, JSONEnumState *pst, JSONID *pMemberID, JSONID *pValueID)
{
   *pst = ObjectNext(me->psz, me->uMax, (*pst), pMemberID, pValueID);
   return ((uint) (*pst) < me->uMax ? JSONPARSER_SUCCESS : *pst);
}

int JSONParser_ArrayNext(JSONParser *me, JSONEnumState *pst, JSONID *pArrayID)
{
   *pst = ArrayNext(me->psz, me->uMax, (*pst), pArrayID);
   return ((uint) (*pst) < me->uMax ? JSONPARSER_SUCCESS : *pst);
}

//Given an array id, find the n'th value id
int JSONParser_ArrayLookup(JSONParser *me, JSONID arrayID, int index, JSONID *pValueID)
{
   JSONEnumState st;
   int i,nErr = JSONPARSER_SUCCESS;

   ERR_RET(JSONParser_ArrayEnumInit(me, arrayID, &st));
   for (i=0; i<=index && (JSONPARSER_SUCCESS == nErr); i++) {
      nErr = JSONParser_ArrayNext(me, &st, pValueID);
   }
   return nErr;
}

static int ReadUnicodePoint(const char *psz, uint *pnUTF16)
{
   int i, uc = 0;
   for (i=0; i < 4; i++) {
      char c = psz[i];
      if (IsDigit(c)) {
         uc |= c - '0';
      } else if (IsInRange(c, 'a','f')) {
         uc |= c - 'a' + 10;
      } else if (IsInRange(c, 'A','F')) {
         uc |= c - 'A' + 10;
      } else {
         return JSONPARSER_ERROR;
      }
      if (i<3) uc <<= 4;
   }
   *pnUTF16 = uc;
   return JSONPARSER_SUCCESS;
}


static int ReadEscape(JSONParser *me, JSONID id, JSONID uMax, char *esc, int *pn, int *pnID)
{
   const char *psz = me->psz;
   int nLen=1;

   *pnID = 1;

   if(id >= (int)me->uMax) { return JSONPARSER_ERROR; }

    switch(psz[id]) {
    case '"':  *esc = '"';  break;
    case '\\': *esc = '\\'; break;
    case '/':  *esc = '/';  break;
    case 'b':  *esc = '\b'; break;
    case 'f':  *esc = '\f'; break;
    case 'n':  *esc = '\n'; break;
    case 'r':  *esc = '\r'; break;
    case 't':  *esc = '\t'; break;
    case 'u':  {
       uint uc=0;
       if ( id+4 >= (int)me->uMax) { return JSONPARSER_ERROR; }
       ERR_RET( ReadUnicodePoint(&psz[id+1], &uc));
       // utf8 length
       nLen = (uc < 0x80) ? 1 : ((uc < 0x800) ? 2 : 3);
       switch (nLen) {
          case 3: esc[2] =((uc | 0x80) & 0xBF); uc >>= 6;
          case 2: esc[1] =((uc | 0x80) & 0xBF); uc >>= 6;
          case 1: esc[0] = (uc | ((nLen==1)?0:((nLen==2)?0xC0:0xE0)) );
       }
       *pnID = 5;
    }
    break;
    default:
       return JSONPARSER_ERROR;
    }
    *pn = nLen;
    return JSONPARSER_SUCCESS;
}

/*
 * pszKey - utf8 string
 * nLen   - length of pszKey
 *        - if 0, string is NULL-terminated
 * id     - JSONID of the string to compare against
 * Return: 0 on Success
 *         1 on Failure
 */
static int JSONParser_StrCmp(JSONParser *me, JSONID id, const char *pszKey, int nLen, int *pnLenConsumed)
{
   const char *psz = me->psz;
   const char *pcKey = pszKey;
   const char *pcEnd;
   if (0 == nLen) {
      nLen = JSONParser_strlen(pszKey);
   }
   pcEnd = pcKey + nLen;
   id++; // eat quote
   while ((uint)id < me->uMax && psz[id] != '\"' && pcKey < pcEnd && *pcKey != '.') {
      if (psz[id] == '\\') {
         char esc[3];
         int n, nID;
         if (JSONPARSER_SUCCESS != ReadEscape(me, ++id, me->uMax, esc, &n, &nID)) {
            return JSONPARSER_ERROR;
         }
         if (pcKey+n >= pcEnd) {return JSONPARSER_ERROR; }
         switch(n) {
         case 3: if (pcKey[2] != esc[2]) { return 1; }
         case 2: if (pcKey[1] != esc[1]) { return 1; }
         case 1: if (pcKey[0] != esc[0]) { return 1; }
         }
         pcKey += n;
         id+=nID;
      } else if (psz[id++] != *pcKey++) {
         return JSONPARSER_ERROR;
      }
   }

   if ((uint)id >= me->uMax ||
       psz[id] != '\"' ||
       (*pcKey != 0 && *pcKey != '.')) {
      return JSONPARSER_ERROR;
   }

   if (pnLenConsumed) {
      *pnLenConsumed = pcKey - pszKey;
   }
   return JSONPARSER_SUCCESS;
}

int JSONParser_ObjectLookup(JSONParser *me, JSONID ObjectID, const char *pchKey, int nLen,
                                JSONID *pKeyID, JSONID *pValueID)
{
   JSONEnumState st;
   int nErr;

   nErr = JSONParser_ObjectEnumInit(me, ObjectID, &st);
   while (JSONPARSER_SUCCESS == nErr) {
      nErr = JSONParser_ObjectNext(me, &st, pKeyID, pValueID);
      if (JSONPARSER_SUCCESS == nErr && JSONPARSER_SUCCESS == JSONParser_StrCmp(me, *pKeyID, pchKey, nLen,  0)) {
         return JSONPARSER_SUCCESS;
      }
   }
   return nErr;
}

#define COPY_CHAR(c) do { nTotal++; if (pt && pt < pte) { *pt++ = c; } } while(0)

static int JSONParser_GetTag(JSONParser *me, JSONID id, char *pch, int nSize,
                             int *pnReq)
{
  int nTotal=0;
  char *pt = pch;
  char *pte = pch+nSize;
  const char *psz = me->psz;

  if (IsWordBeginChar(psz[id])) {
    do {
      COPY_CHAR(psz[id]); id++;
    } while ((unsigned)id < me->uMax &&
             (IsWordNonBeginChar(psz[id]) || psz[id] == '.'));
  }

  COPY_CHAR(0);   /* null terminate */

  if (pnReq) {
     *pnReq = nTotal;
  }

  return JSONPARSER_SUCCESS;
}

int JSONParser_GetKey(JSONParser *me, JSONID id, char *pch, int nSize, int *pnReq)
{

  if ( (unsigned)id >= me->uMax) {
    return JSONPARSER_ERROR;
  }

  if (IsString(me->psz[id])) {
    return JSONParser_GetString(me, id, pch, nSize, pnReq);
  }

  if (IsWordBeginChar(me->psz[id])) {
    return JSONParser_GetTag(me, id, pch, nSize, pnReq);
  }

  return JSONPARSER_ERROR;
}

int JSONParser_GetString(JSONParser *me, JSONID id, char *pch, int nSize, int *pnReq)
{
   int nTotal=0;
   char *pt = pch;
   char *pte = pch+nSize;
   const char *psz = me->psz;
   JSONType jt;

   if (JSONPARSER_SUCCESS != JSONParser_GetType(me, id, &jt) || JSONString != jt) {
      return JSONPARSER_ERROR;
   }

   //first time, eats the double quote
   id++;
   while( id < (int)me->uMax) {
      switch(psz[id]) {
      case '"':
         if (pnReq) {
            *pnReq = nTotal + 1;
         }
         COPY_CHAR(0);
         return JSONPARSER_SUCCESS;

      case '\\': {
         char esc[3];
         int n, nID;
         if ( JSONPARSER_SUCCESS != (unsigned int)ReadEscape(me, ++id, me->uMax, esc, &n, &nID)) {
            return JSONPARSER_ERROR;
         }

         nTotal+=n;

         if (0 != pt) {
            if (pt+n >= pte) {return JSONPARSER_ERROR; }
            switch(n) {
            case 3: pt[2] = esc[2];
            case 2: pt[1] = esc[1];
            case 1: pt[0] = esc[0];
            }
            pt += n;
         }
         id += nID;
         }
         break;

      default:
         COPY_CHAR(psz[id]); id++;
      }
   }
   return JSONPARSER_ERROR;
}

// read a number until a dot(.) is seen
static int ReadUintFromKey(const char *szKey, uint *puNum)
{
   const char *psz = szKey;
   uint num = 0;

   while (*psz == '0') { psz++; }

   while (IsDigit(*psz)) {  // how to avoid overflow here?
      num = 10*num + (*psz++ - '0');
   }

   if (0 == *psz || '.' == *psz) {
      *puNum = num;
      return JSONPARSER_SUCCESS;
   }

   return JSONPARSER_ERROR;
}

static int JSONParser_LookupSegment(JSONParser *me, JSONID id, const char *szKey, int nLen, JSONID *pValID)
{
   JSONType t=JSONUnknown;

   if (0 == id ) {
      id = SkipSpace(me->psz, me->uMax, id);
   }

   JSONParser_GetType(me, id, &t);
   if (JSONArray == t) {
      uint index;
      if (JSONPARSER_ERROR == (unsigned int)ReadUintFromKey(szKey, &index)) {
         return JSONPARSER_ERROR;
      }
      return JSONParser_ArrayLookup(me, id, (int)index, pValID);
   } else if (JSONObject == t) {
      JSONID keyID;
      return JSONParser_ObjectLookup(me, id, szKey, nLen, &keyID, pValID);
   }

   return JSONPARSER_ERROR;
}

static int GetSegmentLength(const char *sz, int nLen)
{
   int n=0;
   while (sz[n] != '.' && n < nLen) { n++; }
   return n;
}

int JSONParser_Lookup(JSONParser *me, JSONID id, const char *szKey, int nLen, JSONID *pValID)
{
   int nConsumed=0;
   JSONID ValueID=id;
   int nErr;

   if (0 == nLen) {
      nLen = JSONParser_strlen(szKey);
   }

   do {
      int nSegLen = GetSegmentLength(szKey+nConsumed, nLen-nConsumed);
      nErr = JSONParser_LookupSegment(me, ValueID, szKey+nConsumed, nSegLen, &ValueID);
      if (JSONPARSER_SUCCESS != nErr) {
         return nErr;
      }
      nConsumed += (nSegLen + 1); //1 for the '.'
   } while(nConsumed < nLen);

   if (JSONPARSER_SUCCESS == nErr) {
      *pValID = ValueID;
   }

   return nErr;
}



static char *ScanU(char *psz, const char *pszMax, long long *pll)
{
   long long lResult = 0;
   // TODO: detect overflow
   for ( ; psz < pszMax && IsDigit(*psz); ++psz) {
      lResult = lResult * 10 + DigitValue(*psz);
   }
   *pll = lResult;
   return psz;
}

static char my_tolower(char c)
{
   if ((c >= 'A') && (c <= 'Z')) {
      c |= 32;
   }
   return c;
}

// Result code:
//   JSON_SUCCESS
//   JSON_SUCCESS_MINUS
//   JSON_OVERVLOW        (larger than MAX_UINT)
//   JSON_ERROR           (fractional, parse error, etc.)
//
static int JSONParser_GetU(JSONParser *me, JSONID id, int *pi)
{
   char *psz = (char *)me->psz + id;
   const char *pszMax = psz + me->uMax;
   const char *pszFrac, *pszNum, *pszE;
   long long lResult;
   long long lExp;
   int bMinusExp = 0, bMinus = 0;

   if (psz < pszMax && *psz == '-') {
      bMinus = 1;
      ++psz;
   }
   pszNum = psz;

   psz = ScanU(psz, pszMax, &lResult);
   if (psz < pszMax && *psz == '.') {
      ++psz;
   }

   if (lResult > JSON_MAX_UINT32) {
      return JSONPARSER_EOVERFLOW;
   }

   // skip decimal portion (for now)
   pszFrac = psz;
   while ( psz < pszMax && IsDigit(*psz) ) {
      ++psz;
   }

   // handle exponent
   pszE = psz;
   if ( psz < pszMax && my_tolower(*psz) == 'e') {
      ++psz;
      if (psz < pszMax && *psz == '-') {
         bMinusExp = 1;
         ++psz;
      } else if (psz < pszMax && *psz == '+') {
         bMinusExp = 0;
         ++psz;
      }

      psz = ScanU(psz, pszMax, &lExp);

      // TODO: range-check exponent
      if (bMinusExp == 0) {
         while (lExp > 0) {
            if (pszFrac < pszE) {
               lResult = lResult*10 + DigitValue(*pszFrac);
            } else {
               lResult = lResult*10;
            }

            if (lResult > JSON_MAX_UINT32) {
               return JSONPARSER_EOVERFLOW;
            }
            lExp--;
            pszFrac++;
         }
      } else {
         while (lExp > 0 && pszFrac > pszNum) {
            lResult = lResult/10;
            lExp--;
            pszFrac--;
         }
         if (lExp) {
            return JSONPARSER_ERROR;
         }
      }
   }

   // are there un-consumed decimals?  IsDigit(*pszFrac)?  => warn/error
   while (pszFrac < pszE) { //fraction must be all zeros
      if (!IsDigit(*pszFrac) || DigitValue(*pszFrac) != 0) {
         return JSONPARSER_ERROR;
      }
      pszFrac++;
   }

   *pi = (int)lResult;
   return bMinus ? JSONPARSER_SUCCESS_MINUS : JSONPARSER_SUCCESS;
}


int JSONParser_GetInt(JSONParser *me, JSONID id, int *pi)
{
   unsigned int u32;

   int nErr = JSONParser_GetU(me, id, (int *)&u32);
   if (nErr == JSONPARSER_SUCCESS) {
      *pi = (int) u32;
      if (u32 > JSON_MAX_INT32) {
         nErr = JSONPARSER_EOVERFLOW;
      }
   }
   if (nErr == JSONPARSER_SUCCESS_MINUS) {
      *pi = - (int) u32;
      //      if (u32 > - (unsigned)JSON_MIN_INT32) {
      if (u32 > ((unsigned)JSON_MAX_INT32+1)) {
         nErr = JSONPARSER_EOVERFLOW;
      } else {
         nErr = JSONPARSER_SUCCESS;
      }
   }
   return nErr;
}

int JSONParser_GetUInt(JSONParser *me, JSONID id, unsigned int *pu)
{
   int nErr = JSONParser_GetU(me, id, (int *)pu);
   if (nErr == JSONPARSER_SUCCESS_MINUS) {
      nErr = JSONPARSER_EOVERFLOW;
   }
   return nErr;
}

int JSONParser_GetDouble(JSONParser *me, JSONID id, double *pd)
{
   const char *psz = me->psz + id;
   int i = SkipNumber(me->psz, me->uMax, id);

   if (i <= id) {
      return JSONPARSER_ERROR;
   }

   *pd = strtod(psz, NULL);
   if (*pd == HUGE_VAL) { /*STD_DTOA_FP_POSITIVE_INF*/
      return JSONPARSER_EOVERFLOW;
   }
   return JSONPARSER_SUCCESS;
}



int JSONParser_GetBool(JSONParser *me, JSONID id, int *pb)
{
   JSONType jt;
   int n, nErr = JSONPARSER_ERROR;
   const char *psz = me->psz;

   if (JSONPARSER_SUCCESS != JSONParser_GetType(me, id, &jt) || jt != JSONBool) {
      return JSONPARSER_ERROR;
   }

   n = (psz[id] == 't') ? 3 : 4;
   if (id + n < (int)me->uMax) {
      if (psz[id] == 't' && psz[id+1] == 'r' && psz[id+2] == 'u' && psz[id+3] == 'e') {
         *pb = 1;
         nErr = JSONPARSER_SUCCESS;
      } else if (psz[id] == 'f' && psz[id+1] == 'a' && psz[id+2] == 'l' && psz[id+3] == 's' && psz[id+4] == 'e' ) {
         *pb = 0;
         nErr = JSONPARSER_SUCCESS;
      }
   }

   if (id+n+1 < (int)me->uMax && (IsNumChar(psz[id+n+1]) || IsAlpha(psz[id+n+1]))) {
      nErr = JSONPARSER_ERROR;
   }

   return nErr;
}

int JSONParser_IsNull(JSONParser *me, JSONID id)
{
   JSONType jt;
   const char *psz = me->psz;
   int nErr = JSONPARSER_ERROR;

   if (JSONPARSER_SUCCESS != JSONParser_GetType(me, id, &jt) || jt != JSONNull) {
      return JSONPARSER_ERROR;
   }

   if (id+3 < (int)me->uMax) {
      if (psz[id] == 'n' && psz[id+1] == 'u' && psz[id+2] == 'l' && psz[id+3] == 'l') {
         nErr = JSONPARSER_SUCCESS;
      }
   }

   if (id+4 < (int)me->uMax && (IsNumChar(psz[id+4]) || IsAlpha(psz[id+4]))) {
      nErr = JSONPARSER_ERROR;
   }

   return nErr;
}

int JSONParser_GetJSON(JSONParser *me, JSONID id, const char **ppcsz, int *pnSize)
{
   int i = Skip(me->psz, me->uMax, id);
   char *psz = (char *)me->psz + id;

   if (i > id && i <= (int)me->uMax) {
      char *pszE = (char *)me->psz+i;

      //trim trailing spaces
      while(pszE > psz && IsSpace(*(pszE-1))) {
         pszE--;
      }

      if (i > id && i <= (int)me->uMax) {
         *ppcsz = me->psz + id;
         *pnSize = pszE - psz;
         return JSONPARSER_SUCCESS;
      }
   }

   return JSONPARSER_ERROR;
}

