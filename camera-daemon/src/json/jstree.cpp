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
#include "json_gen.h"
#include "json_tree.h"
#include <string.h>
#include <cstdio>
#include <cstdlib>
#include <errno.h>

/* =============================================
   some helpers
 ============================================= */
#define STD_MIN(a,b)   ((a)<(b)?(a):(b))
#define STD_MAX(a,b)   ((a)>(b)?(a):(b))

static int strlcpy(char *pcDst, const char *cpszSrc, int nDestSize)
{
    int nLen = strlen(cpszSrc);

    if (0 < nDestSize) {
        int n;

        n = STD_MIN(nLen, nDestSize - 1);
        (void)memmove(pcDst, cpszSrc, n);

        pcDst[n] = 0;
    }

    return nLen;
}

static int env_ErrReallocF(int nSize, void **ppOut, bool fZero)
{
    void *result;

    if (NULL == ppOut || nSize < 0) {
        return EINVAL;
    }

    if (NULL == *ppOut && nSize == 0) {
        return 0;
    }

    if (0 == nSize) {
        free(*ppOut);
        *ppOut = NULL;
        return 0;
    }

    if (0 == *ppOut) {
        *ppOut = malloc(nSize);
        if (NULL != *ppOut) {
            if (fZero) {
                memset((char *)*ppOut, 0, nSize);
            }

            return 0;
        }

        return ENOMEM;
    }

    result = realloc(*ppOut, nSize);
    if (0 < nSize) {   /** allocation or resize */

        if (0 == result) {
            return ENOMEM;
        }

        *ppOut = result;
    }

    return 0;
}

static __inline int env_ErrRealloc(int nSize, void **ppOut)
{
    return env_ErrReallocF(nSize, ppOut, true);
}

static __inline int env_ErrReallocNoZI(int nSize, void **ppOut)
{
    return env_ErrReallocF(nSize, ppOut, false);
}

static __inline int env_ErrMallocNoZI(int nSize, void** pp)
{
    *pp = 0;
    return env_ErrReallocNoZI(nSize, pp);
}


#define ENV_FREEIF(p) \
   do { if (p) { (void)env_ErrRealloc(0,(void**)&(p)); } } while (0)

int JSONTree::CreateValue(int type, int nExtra, JSValue **ppv)
{
   int nErr;

   *ppv = 0;
   nErr = env_ErrMallocNoZI(sizeof(JSValue) + nExtra, (void **)ppv);
   if (SUCCESS == nErr) {
      memset(*ppv, 0, sizeof(JSValue));
      (*ppv)->type = (JSONType)type;
   }

   return nErr;
}

int JSONTree::CreateArray(JSValue **ppjv)
{
   return CreateValue(JSONArray, 0, ppjv);
}

int JSONTree::CreateObject(JSValue **ppjv)
{
   return CreateValue(JSONObject, 0, ppjv);
}

int JSONTree::CreateNumber(const char *cpsz, int nSize, JSValue **ppjv)
{
   int nErr = CreateValue(JSONNumber, nSize+1, ppjv);
   if (SUCCESS == nErr) {
      (*ppjv)->u.psz = (char *)((*ppjv)+1);
      memmove((*ppjv)->u.psz, cpsz, nSize);
      (*ppjv)->nUsed = nSize;
   }
   return nErr;
}

int JSONTree::CreateString(const char *cpsz, int nSize, JSValue **ppjv)
{
   int nErr = CreateValue(JSONString, nSize+1, ppjv);
   if (SUCCESS == nErr) {
      JSValue *pjv = *ppjv;
      pjv->u.psz = (char *)(pjv+1);
      memmove(pjv->u.psz, cpsz, nSize);
      pjv->u.psz[nSize] = 0; //null terminate
      pjv->nUsed = nSize;
   }
   return nErr;
}

int JSONTree::CreateInt(int num, JSValue **ppjv)
{
   char buf[16] = {0};
   snprintf(buf, 16, "%d", num);

   return CreateNumber(buf, strlen(buf), ppjv);
}

int JSONTree::CreateUInt(unsigned int num, JSValue **ppjv)
{
   char buf[16] = {0};
   snprintf(buf, 16, "%u", num);

   return CreateNumber(buf, strlen(buf), ppjv);
}

int JSONTree::CreateDouble(double num, JSValue **ppjv)
{
   char buf[32] = {0};
   snprintf(buf, 32, "%g", num);

   return CreateNumber(buf, strlen(buf), ppjv);
}

int JSONTree::CreateBool(bool b, JSValue **ppjv)
{
   if (b) {
      *ppjv = &jvTrue;
   } else {
      *ppjv = &jvFalse;
   }
   return SUCCESS;
}

int JSONTree::CreateNull(JSValue **ppjv)
{
   *ppjv = &jvNull;
   return SUCCESS;
}

int JSONTree::GetInt(JSValue *pjv, int *pn)
{
   int nErr;
   JSONParser parser={0};

   if (pjv->type != JSONNumber) {
      return EINVAL;
   }

   JSONParser_Ctor(&parser, pjv->u.psz, pjv->nUsed);
   nErr = JSONParser_GetInt(&parser, 0, pn);
   if (JSONPARSER_SUCCESS == nErr) {
      return nErr;
   } else if (JSONPARSER_EOVERFLOW == (unsigned int)nErr) {
      return EOVERFLOW;
   } else {
      return EFAILED;
   }
}

int JSONTree::GetUInt(JSValue *pjv, unsigned int *pu)
{
   int nErr;
   JSONParser parser={0};

   if (pjv->type != JSONNumber) {
      return EINVAL;
   }

   JSONParser_Ctor(&parser, pjv->u.psz, pjv->nUsed);
   nErr = JSONParser_GetUInt(&parser, 0, pu);
   if (JSONPARSER_SUCCESS == nErr) {
      return nErr;
   } else if (JSONPARSER_EOVERFLOW == (unsigned int)nErr) {
      return EOVERFLOW;
   } else {
      return EFAILED;
   }
}

int JSONTree::GetDouble(JSValue *pjv, double *pd)
{
   int nErr;
   JSONParser parser={0};

   if (pjv->type != JSONNumber) {
      return EINVAL;
   }

   JSONParser_Ctor(&parser, pjv->u.psz, pjv->nUsed);
   nErr = JSONParser_GetDouble(&parser, 0, pd);
   if (JSONPARSER_SUCCESS == nErr) {
      return nErr;
   } else if (JSONPARSER_EOVERFLOW == (unsigned int)nErr) {
      return EOVERFLOW;
   } else {
      return EFAILED;
   }
}

int JSONTree::GetString(JSValue *pjv, const char **cppsz, int *pnLen)
{
   if (pjv->type != JSONString) {
      return EINVAL;
   }

   *cppsz = pjv->u.psz;
   *pnLen = pjv->nUsed;
   return SUCCESS;
}

int JSONTree::GetBool(JSValue *pjv, bool *pb)
{
   if (pjv->type == JSONBool) {
      *pb = (pjv->u.num != 0);
      return SUCCESS;
   } else {
      return EINVAL;
   }
}

int JSONTree::GetType(JSValue *pjv, JSONType *pjt)
{
   *pjt = pjv->type;

   return SUCCESS;
}

int JSONTree::GetNumEntries(JSValue *pjv, int *pnEntries)
{
   if (pjv->type == JSONArray || pjv->type == JSONObject) {
      *pnEntries = pjv->nUsed;
      return SUCCESS;
   } else {
      return EINVAL;
   }
}

int JSONTree::ArrayGet(JSValue *pvArray, int nPos, JSValue **ppjv)
{
   if (pvArray->type != JSONArray) {
      return EINVAL;
   }

   if (nPos >= pvArray->nUsed) {
      return ENOENT;
   }

   *ppjv = pvArray->u.ppValues[nPos];
   return SUCCESS;
}

int JSONTree::ArraySet(JSValue *pvArray, int nPos, JSValue *pjv)
{
   int i, nErr=SUCCESS;
   JSValue *pjvOld=0;

   if (pvArray->type != JSONArray) {
      return EINVAL;
   }

      //remove entry if exists
   if (0 == pjv && nPos >= pvArray->nUsed) {
      nErr = ENOENT;
   } else if (nPos+1 > pvArray->nMax) {
      int nMax = STD_MAX(nPos+1, pvArray->nMax+10);
      nErr = env_ErrRealloc(nMax*sizeof(JSValue*), (void **)&pvArray->u.ppValues);
      if (SUCCESS == nErr) {
         pvArray->nMax  = nMax;
      }
   }

   if (SUCCESS != nErr) {
      return nErr;
   }

   if (nPos < pvArray->nUsed) {
      pjvOld = pvArray->u.ppValues[nPos];
   }

   //remove entry
   if (0 == pjv) {
      DeleteValue(pvArray->u.ppValues[nPos]);

      //Move array values one up
      for(i=nPos; i<(pvArray->nUsed-1); i++) {
         pvArray->u.ppValues[i]=pvArray->u.ppValues[i+1];
      }
      pvArray->nUsed--;
      pvArray->u.ppValues[pvArray->nUsed-1] = 0;

   } else {
      for (i=pvArray->nUsed; i<nPos; i++) {
         pvArray->u.ppValues[i]=&jvNull;
      }
      pvArray->u.ppValues[nPos] = pjv;
      pvArray->nUsed = nPos+1;
   }
   if (pjvOld) {
      //@@Free
   }

   return nErr;
}

int JSONTree::ArrayAppend(JSValue *pvArray, JSValue *pjv)
{
   return ArraySet(pvArray, pvArray->nUsed, pjv);
}

int JSONTree::ObjectGetByPos(JSValue *pvObj, int nPos, JSPair * pjsp)
{
   if (pvObj->type != JSONObject) {
      return EINVAL;
   }

   if (nPos >= pvObj->nUsed) {
      return ENOENT;
   }

   *pjsp = pvObj->u.pPairs[nPos];
   return SUCCESS;
}

int JSONTree::ObjectGet(JSValue *pvObj, const char *cpszKey, JSPair* pjsp)
{
   int i;

   if (pvObj->type != JSONObject) {
      return EINVAL;
   }

   for(i=0; i < pvObj->nUsed; i++) {
      if (0 == strcmp(cpszKey, pvObj->u.pPairs[i].pszKey)) {
         *pjsp = pvObj->u.pPairs[i];
         return SUCCESS;
      }
   }
   return ENOENT;
}

int JSONTree::ObjectSetInternal(JSValue *pvObj,
                                const char *cpszKey, int nLen, JSValue *pjv,
                                bool bAdd)  /* True to allow duplicate keys */
{
   int i, nErr=SUCCESS;

   if (pvObj->type != JSONObject) {
      return EINVAL;
   }

   if (!bAdd) {
     //Key found case
     for(i=0; i < pvObj->nUsed; i++) {
        if ((size_t)nLen == strlen(pvObj->u.pPairs[i].pszKey) &&
            0 == strncmp(cpszKey, pvObj->u.pPairs[i].pszKey, nLen)) {
           if (0 == pjv) { //delete entry case
              int j;

              DeleteValue(pvObj->u.pPairs[i].pjv);
              ENV_FREEIF(pvObj->u.pPairs[i].pszKey);

              //Move array values one up
              for(j=i; j<(pvObj->nUsed-1); j++) {
                 pvObj->u.pPairs[j] = pvObj->u.pPairs[j+1];
              }
              pvObj->nUsed--;
              pvObj->u.pPairs[pvObj->nUsed].pszKey = 0;
              pvObj->u.pPairs[pvObj->nUsed].pjv = 0;

           } else {
              DeleteValue(pvObj->u.pPairs[i].pjv);
              pvObj->u.pPairs[i].pjv = pjv;
           }
           return SUCCESS;
        }
     }
   }

   if (pvObj->nUsed >= pvObj->nMax) {
      int nMax = pvObj->nMax+10;
      nErr = env_ErrRealloc(nMax*sizeof(JSPair), (void **)&pvObj->u.pPairs);
      if (SUCCESS == nErr) {
         pvObj->nMax  = nMax;
      }
   }

   if (SUCCESS == nErr) {
      pvObj->u.pPairs[pvObj->nUsed].pszKey = 0;
      nErr = env_ErrRealloc(nLen+1, (void **)&pvObj->u.pPairs[pvObj->nUsed].pszKey);
      if (SUCCESS == nErr) {
         strlcpy((char *)pvObj->u.pPairs[pvObj->nUsed].pszKey, cpszKey, nLen+1);
         pvObj->u.pPairs[pvObj->nUsed].pjv = pjv;
         pvObj->nUsed++;
      }
   }

   return nErr;
}

int JSONTree::ObjectSet(JSValue *pvObj, const char *cpszKey, JSValue *pjv)
{
   return ObjectSetInternal(pvObj, cpszKey, strlen(cpszKey), pjv, false);
}

int JSONTree::ObjectAdd(JSValue *pvObj, const char *cpszKey, JSValue *pjv)
{
  return ObjectSetInternal(pvObj, cpszKey, strlen(cpszKey), pjv, true);
}

static int ReadUintFromPath(const char *szKey, unsigned int *puNum)
{
   const char *pc = szKey;
   unsigned int num = 0;

   while (*pc == '0') { pc++; }

   while (*pc >= '0' && *pc <= '9') {
      num = 10*num + (*pc++ - '0');
   }

   if (pc > szKey && (0 == *pc || '.' == *pc)) {
      *puNum = num;
      return 0;
   }

   return EINVAL;
}

static int LookupSegment(JSValue ***pppv, const char *szKey, int nLen)
{
   JSValue *pv = **pppv;
   int i, nErr = EINVAL;

   if (pv->type == JSONObject) {
      for(i=0; i < pv->nUsed; i++) {
         if ((size_t)nLen == strlen(pv->u.pPairs[i].pszKey) &&
             0 == strncmp(szKey, pv->u.pPairs[i].pszKey, nLen)) {
            *pppv = &pv->u.pPairs[i].pjv;
            return 0;
         }
      }
      return ENOENT;
   } else if (pv->type == JSONArray) {
      unsigned int index;
      nErr = ReadUintFromPath(szKey, &index);
      if (0 == nErr) {
         if ((int)index < pv->nUsed) {
            *pppv = &pv->u.ppValues[index];
            return 0;
         } else {
            return ENOENT;
         }
      }
   }

   return nErr;
}

int JSONTree::PathGet(JSValue *pvRoot, const char *cpszPath, JSValue **ppjvResult)
{
   int nErr, nLen, nSegLen, nConsumed=0;
   char *psz;
   JSValue **ppjv = (JSValue **)&pvRoot;

   // must be one of array or object
   if (pvRoot->type != JSONObject && pvRoot->type != JSONArray) {
      return EINVAL;
   }

   nLen = strlen(cpszPath);

   do {
      psz = (char *) memchr((void *)(cpszPath+nConsumed), '.', nLen-nConsumed);

      if (0 == psz) { // last segment
         nSegLen = nLen-nConsumed;
      } else {
         nSegLen = psz-((char*)cpszPath+nConsumed);
      }

      nErr = LookupSegment(&ppjv, cpszPath+nConsumed, nSegLen);
      if (SUCCESS == nErr) {
         nConsumed += (nSegLen + 1); //1 for the '.'
      }

   } while(psz && !nErr);

   if (SUCCESS == nErr) {
      *ppjvResult = *ppjv;
   }

   return nErr;
}


int JSONTree::CreatePathObjects(JSValue *pjvContainer, const char *cpszPath, JSValue *pjv)
{
   JSValue *pvRoot = pjv;
   int nLen, nConsumed=0;
   int nErr = EINVAL;
   unsigned int index;
   char *psz;

   nLen = strlen(cpszPath);

   do {
      int nSegLen;
      psz = (char *) memrchr((void *)cpszPath, '.', nLen-nConsumed);

      if (0 == psz) { //key
         if (pjvContainer->type == JSONObject) {
            nErr = ObjectSetInternal(pjvContainer, cpszPath, nLen-nConsumed, pvRoot, false);
         } else if (pjvContainer->type == JSONArray) {
            nErr = ReadUintFromPath(cpszPath, &index);
            if (SUCCESS == nErr) { //this is an index
               nErr = ArraySet(pjvContainer, index, pvRoot);
            }
         } else {
            nErr = EINVAL;
         }
         return nErr;
      }

      nSegLen = (nLen - nConsumed) - (psz-(char*)cpszPath)-1;

      nErr = ReadUintFromPath(psz+1, &index);
      if (SUCCESS == nErr) { //this is an index
         nErr = CreateArray(&pvRoot);
         if (SUCCESS == nErr) {
            nErr = ArraySet(pvRoot, (int)index, pjv);
            if (SUCCESS == nErr) {
               pjv = pvRoot;
            }
         }
      } else {
         nErr = CreateObject(&pvRoot);
         if (SUCCESS == nErr) {
            nErr = ObjectSetInternal(pvRoot, psz+1, nSegLen, pjv, false);
            if (SUCCESS == nErr) {
               pjv = pvRoot;
            }
         }
      }
      if (SUCCESS != nErr) {
         return nErr;
      }
      nConsumed += (nSegLen + 1); //1 for the '.'
   } while(psz);

   return nErr;
}



int JSONTree::PathSet(JSValue *pvRoot, const char *cpszPath, JSValue *pjv)
{
   int nLen, nSegLen, nConsumed=0;
   int nErr;
   char *psz;
   JSValue **ppjv = &pvRoot;

   // must be one of array or object
   if (pvRoot->type != JSONObject && pvRoot->type != JSONArray) {
      return EINVAL;
   }

   nLen = strlen(cpszPath);

   do {
      psz = (char *) memchr((void *)(cpszPath+nConsumed), '.', nLen-nConsumed);

      if (0 == psz) { // last segment
         nSegLen = nLen-nConsumed;
      } else {
         nSegLen = psz-((char*)cpszPath+nConsumed);
      }

      nErr = LookupSegment(&ppjv, cpszPath+nConsumed, nSegLen);
      if (SUCCESS == nErr) {
         nConsumed += (nSegLen + 1); //1 for the '.'
      }
   } while(psz && !nErr);

   if (ENOENT == nErr) {
      nErr = CreatePathObjects(*ppjv, cpszPath+nConsumed, pjv);
   } else if (SUCCESS == nErr) {
      DeleteValue(*ppjv);
      *ppjv = pjv;
   }

   return nErr;
}


/*===========================================================================
  CopyObject - Copy the Object
===========================================================================*/
int JSONTree::CloneObject(JSValue *pjv, JSValue **ppvNew) {
   int   i, nErr;
   JSValue *pjvNew=0;
   JSPair pair={0};

   nErr = CreateObject(&pjvNew);
   for (i=0; SUCCESS == nErr && i < pjv->nUsed; i++) {
      nErr = ObjectGetByPos(pjv, i, &pair);
      if (SUCCESS == nErr) {
         JSValue *pjVal;
         nErr = CloneValue(pair.pjv, &pjVal);
         if (SUCCESS == nErr) {
            nErr = ObjectSet(pjvNew, pair.pszKey, pjVal);
         }
      }
   }

   if (SUCCESS == nErr) {
      *ppvNew = pjvNew;
   } else if (pjvNew) {
      DeleteValue(pjvNew);
   }

   return nErr;
}

int JSONTree::CloneArray(JSValue *pjv, JSValue **ppvNew) {
   int   i, nErr;
   JSValue *pjvNew=0;
   JSValue *pjvIndex=0;

   nErr = CreateArray(&pjvNew);
   for (i=0; SUCCESS == nErr && i < pjv->nUsed; i++) {
      nErr = ArrayGet(pjv, i, &pjvIndex);
      if (SUCCESS == nErr) {
         JSValue *pjVal;
         nErr = CloneValue(pjvIndex, &pjVal);
         if (SUCCESS == nErr) {
            nErr = ArraySet(pjvNew, i, pjVal);
         }
      }
   }

   if (SUCCESS == nErr) {
      *ppvNew = pjvNew;
   } else if (pjvNew) {
      DeleteValue(pjvNew);
   }

   return nErr;
}


/*===========================================================================
  Clone - Copy the JSValue value
===========================================================================*/
int JSONTree::CloneValue(JSValue *pjv, JSValue **ppvClone) {
   int nErr;
   JSValue *pjvNew = 0;

   if (0 == pjv) {
      *ppvClone = 0;
      return SUCCESS;
   }

   switch ( pjv->type ) {
   case JSONObject:
      nErr = CloneObject(pjv, &pjvNew);
      break;
   case JSONArray:
      nErr = CloneArray(pjv, &pjvNew);
      break;
   case JSONString:
      nErr = CreateString(pjv->u.psz, pjv->nUsed, &pjvNew);
      break;
   case JSONNumber:
      nErr = CreateNumber(pjv->u.psz, pjv->nUsed, &pjvNew);
      break;
   case JSONBool:
      nErr = CreateBool(pjv->u.num != 0, &pjvNew);
      break;
   case JSONNull:
      nErr = CreateNull(&pjvNew);
      break;
   default:
      nErr = EINVAL;
   }

   if (SUCCESS == nErr) {
      *ppvClone = pjvNew;
      return SUCCESS;
   } else if (pjvNew) {
      DeleteValue(pjvNew);
   }

   return nErr;
}

/*===========================================================================
  DeleteValue
===========================================================================*/
int JSONTree::DeleteValue(JSValue *pjv)
{
   int i;

   if (0 == pjv || JSONBool == pjv->type || JSONNull == pjv->type ) {
      return SUCCESS;
   }

   if (JSONArray == pjv->type) {
      for(i=0;i<pjv->nUsed;i++) {
         DeleteValue(pjv->u.ppValues[i]);
      }
      ENV_FREEIF(pjv->u.ppValues);
      ENV_FREEIF(pjv);
   } else if (JSONObject == pjv->type) {
      for(i=0;i<pjv->nUsed;i++) {
         DeleteValue(pjv->u.pPairs[i].pjv);
         ENV_FREEIF(pjv->u.pPairs[i].pszKey);
      }
      ENV_FREEIF(pjv->u.pPairs);
      ENV_FREEIF(pjv);
   } else {
      ENV_FREEIF(pjv);
   }
   return SUCCESS;
}

int JSONTree::ReadValue(JSONParser *p, JSONID id, JSValue **ppv)
{
   int nErr = EBADFORMAT;
   JSONType jt;

   if (JSONPARSER_SUCCESS != JSONParser_GetType(p, id, &jt)) {
      return nErr;
   }

   switch(jt) {
   case JSONObject: {
      JSValue *pObj=0;
      JSONEnumState st;

      if (SUCCESS == JSONParser_ObjectEnumInit(p, id, &st)) {
         JSONID keyid, valueid;

         nErr = CreateObject(&pObj);
         while (SUCCESS == nErr) {
            JSValue *pjv = 0;

            nErr = JSONParser_ObjectNext(p, &st, &keyid, &valueid);
            if (SUCCESS != nErr) {
               nErr = SUCCESS; // since this indicates end of enum
               break;
            }
            nErr = ReadValue(p, valueid, &pjv);
            if (SUCCESS == nErr) {
               int nReq=0;
               nErr = JSONParser_GetKey(p, keyid, 0, 0, &nReq);
               if (SUCCESS == nErr) {
                  char *psz=0;
                  nErr = env_ErrMallocNoZI(nReq, (void **)&psz);
                  if (SUCCESS == nErr) {
                     nErr = JSONParser_GetKey(p, keyid, psz, nReq, 0);
                     if (SUCCESS == nErr) {
                        nErr = ObjectAdd(pObj, psz, pjv);
                     }
                  }
                  ENV_FREEIF(psz);
               }
            }

            if (SUCCESS != nErr) {
               DeleteValue(pjv);
            }
         }
      }

      if (SUCCESS == nErr) {
         *ppv = pObj;
      } else {
         DeleteValue(pObj);
      }
      return nErr;
   } /*case Object */

   case JSONArray: {
      JSValue *pArray=0;
      JSONEnumState st;

      if (SUCCESS == JSONParser_ArrayEnumInit(p, id, &st)) {
         JSONID valueid;

         nErr = CreateArray(&pArray);
         while (SUCCESS == nErr) {
            JSValue *pjv=0;

            nErr = JSONParser_ArrayNext(p, &st, &valueid);
            if (SUCCESS != nErr) {
               nErr = SUCCESS; // since this indicates end of enum
               break;
            }

            nErr = ReadValue(p, valueid, &pjv);
            if (SUCCESS == nErr) {
               nErr = ArrayAppend(pArray, pjv);
            }
            if (SUCCESS != nErr) {
               DeleteValue(pjv);
            }
         }
      }

      if (SUCCESS == nErr) {
         *ppv = pArray;
      } else {
         DeleteValue(pArray);
      }
      return nErr;
   } /*case Array */

   case JSONString: {
      int nErr, nReq=0;
      JSValue *pjv=0;

      nErr = JSONParser_GetString(p, id, 0, 0, &nReq);
      if (SUCCESS == nErr) {
         nErr = CreateValue(JSONString, nReq, &pjv);
         if (SUCCESS == nErr) {
            pjv->u.psz=(char *)(pjv+1);
            pjv->nUsed=nReq;
            nErr = JSONParser_GetString(p, id, pjv->u.psz, nReq, 0);
         }
         if (SUCCESS != nErr) {
            DeleteValue(pjv);
         } else {
            *ppv = pjv;
         }
      }
      return nErr;
   } /* case String */

   case JSONNumber: {
      int nSize;
      const char *pszNum=0;

      nErr = JSONParser_GetJSON(p, id, &pszNum, &nSize);
      if (SUCCESS == nErr) {
         nErr = CreateNumber(pszNum, nSize, ppv);
      }
      return nErr;
   } /* case Number */

   case JSONBool: {
      int n;

      nErr = JSONParser_GetBool(p, id, &n);
      if (SUCCESS == nErr) {
         nErr = CreateBool(n != 0, ppv);
      }
      return nErr;
   } /* case bool */

   case JSONNull: {
      nErr = CreateNull(ppv);
      return nErr;
   } /* case null */
   default:
      nErr = EINVAL;
   }
   return nErr;
}

int JSONTree::ParseJSON(const char *psz, int nSize, JSValue **ppjv)
{
   JSONParser parser={0};

   //construct parser
   JSONParser_Ctor(&parser, psz, nSize);

   //start reading at ID 0
   return ReadValue(&parser, 0, ppjv);
}


int JSONTree::SerializeValue(JSONGen *pg, JSValue *pjv)
{
   int i, nErr;

   switch(pjv->type) {
   case JSONObject:
      {
         nErr = JSONGen_BeginObject(pg);
         for (i=0; SUCCESS == nErr && i < pjv->nUsed; i++) {
            JSPair pair = {0};
            nErr = ObjectGetByPos(pjv, i, &pair);
            if (SUCCESS == nErr) {
               nErr = JSONGen_PutKey(pg, pair.pszKey, strlen(pair.pszKey));
               if (SUCCESS == nErr) {
                  nErr = SerializeValue(pg, pair.pjv);
               }
            }
         }
         if (SUCCESS == nErr) {
            nErr = JSONGen_EndObject(pg);
         }
         return nErr;
      }
   case JSONArray:
      {
         nErr = JSONGen_BeginArray(pg);
         for (i=0; SUCCESS == nErr && i < pjv->nUsed; i++) {
            JSValue *pja=0;
            nErr = ArrayGet(pjv, i, &pja);
            if (SUCCESS == nErr) {
               nErr = SerializeValue(pg, pja);
            }
         }
         if (SUCCESS == nErr) {
            nErr = JSONGen_EndArray(pg);
         }
         return nErr;
      }
   case JSONString:
      nErr = JSONGen_PutString(pg, pjv->u.psz, pjv->nUsed);
      break;
   case JSONNumber:
      nErr = JSONGen_PutJSON(pg, pjv->u.psz, pjv->nUsed);
      break;
   case JSONBool:
      nErr = JSONGen_PutBool(pg, pjv->u.num);
      break;
   case JSONNull:
      nErr = JSONGen_PutNull(pg);
      break;
   default:
      nErr = EINVAL;
      break;
   }
   return nErr;
}

static int MyRealloc(void *pv, int nLen, void **pp)
{
   return env_ErrReallocNoZI(nLen, pp);
}

int JSONTree::Serialize(JSValue *pjv, char *psz, int nLen, int *pnReq)
{
   int nErr;
   JSONGen gen;
   const char *cpsz;
   int nLenGot;

   JSONGen_Ctor(&gen, 0, 0, MyRealloc, 0);

   nErr = SerializeValue(&gen, (JSValue *)pjv);
   if (SUCCESS == nErr) {
      nErr = JSONGen_GetJSON(&gen, &cpsz, &nLenGot);
      if (SUCCESS == nErr) {
         memmove(psz, cpsz, STD_MIN(nLen, nLenGot));
         if (pnReq) {
            *pnReq = nLenGot;
         }
      }
   }

   JSONGen_Dtor(&gen);

   return nErr;
}
