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
#include "json_gen.h"
#include "camerad_util.h"
#include <string.h>
#include <stdio.h>  /* for snprintf() */

#ifndef STD_MAX
#define STD_MAX(a,b)   ((a)>(b)?(a):(b))
#endif

#define IsSpace(c)   ( (c) == ' ' ||  (c) == '\t' || (c) == '\r' || (c) == '\n' )

#define JSONGEN_BUF_INITIAL_SIZE 256

void JSONGen_Ctor(JSONGen *me, char *psz, int nMax,
                  JSONGenReallocFunc pfnRealloc, void *pvCtx)
{
   me->psz = me->pszStatic = psz;
   me->nMax = nMax;
   me->pfnRealloc = pfnRealloc;
   me->pvCtx = pvCtx;
   me->nCurr = 0;
}

void JSONGen_Dtor(JSONGen *me)
{
   if (me->psz && me->psz != me->pszStatic && me->pfnRealloc) {
      me->pfnRealloc(me->pvCtx, 0, (void **)&me->psz);
   }
   memset(me, 0, sizeof(JSONGen));
}

static int AssertSpaceFor(JSONGen *me, int i)
{
   int nErr = JSONGEN_ERROR;

   if ((me->nMax - me->nCurr) >= i) {
      return JSONGEN_SUCCESS;
   }

   if (0 == me->pfnRealloc) {
      return nErr;
   }

   if (0 == me->psz) {
      int nSize = STD_MAX(i, JSONGEN_BUF_INITIAL_SIZE);
      nErr = me->pfnRealloc(me->pvCtx, nSize, (void**)&me->psz);
      if (JSONGEN_SUCCESS == nErr) {
         me->nMax = nSize;
      }
   } else if (me->psz == me->pszStatic) {
      int nSize = STD_MAX(me->nMax*2, me->nMax+i);
      me->psz = 0;
      nErr = me->pfnRealloc(me->pvCtx, nSize, (void**)&me->psz);
      if (JSONGEN_SUCCESS == nErr) {
         memmove(me->psz, me->pszStatic, me->nCurr);
         me->nMax = nSize;
      }
   } else {
      int nSize = STD_MAX(me->nMax*2, me->nMax+i);
      nErr = me->pfnRealloc(me->pvCtx, nSize, (void**)&me->psz);
      if (JSONGEN_SUCCESS == nErr) {
         me->nMax = nSize;
      }
   }

   return nErr;
}

static int Putc(JSONGen *me, char c)
{
   int nErr = AssertSpaceFor(me, 1);

   if (JSONGEN_SUCCESS == nErr) {
      me->psz[me->nCurr++] = c;
   }
   return nErr;
}

static int PutBuf(JSONGen *me, const char *pb, int sz)
{
   int nErr = AssertSpaceFor(me, sz);

   if (JSONGEN_SUCCESS == nErr) {
      while(sz--) {
         me->psz[me->nCurr++] = *pb++;
      }
   }
   return nErr;
}

static int PutStr(JSONGen *me, const char *pb)
{
   int sz = strlen(pb);
   int nErr = AssertSpaceFor(me, sz);

   if (JSONGEN_SUCCESS == nErr) {
      while(sz--) {
         me->psz[me->nCurr++] = *pb++;
      }
   }
   return nErr;
}

int JSONGen_NeedSeparator(JSONGen *me)
{
   const char *psz = me->psz;
   int nCurr = me->nCurr-1;
   if (nCurr < 0) {
      return 0;
   }

   //eat space
   while (psz && nCurr >= 0 && IsSpace(psz[nCurr])) nCurr--;

   if(psz[nCurr] == '[' || psz[nCurr] == '{' || psz[nCurr] == ':' || psz[nCurr] == ',') {
      return 0;
   }

   return 1;
}

int JSONGen_PutString(JSONGen *me, const char *pszString, int nLen)
{
   const char *psz = pszString;
   int nErr = AssertSpaceFor(me, nLen+4);

   if (JSONGen_NeedSeparator(me)) {
      TRY(nErr,PutStr(me, ", "));
   }

   if (JSONGEN_SUCCESS == nErr) {
      TRY(nErr,Putc(me, '"'));
      while (psz && *psz) {
         if (*psz > 0x1F /*Control character*/ && *psz != '"' && *psz != '\\') {
            TRY(nErr,Putc(me, *psz));
         } else {
            switch(*psz) {
            case '"':  TRY(nErr,PutStr(me, "\\\"")); break;
            case '\\': TRY(nErr,PutStr(me, "\\\\")); break;
            case '\b': TRY(nErr,PutStr(me, "\\b"));  break;
            case '\f': TRY(nErr,PutStr(me, "\\f"));  break;
            case '\n': TRY(nErr,PutStr(me, "\\n"));  break;
            case '\r': TRY(nErr,PutStr(me, "\\r"));  break;
            case '\t': TRY(nErr,PutStr(me, "\\t"));  break;
            default:   TRY(nErr,Putc(me, *psz));     break;
            }
         }
         psz++;
      }
      TRY(nErr,Putc(me, '"'));
   }
   CATCH(nErr){}
   return nErr;
}

int JSONGen_BeginObject(JSONGen *me)
{
   if (JSONGen_NeedSeparator(me)) {
      return PutStr(me, ", {");
   } else {
      return Putc(me, '{');
   }
}

int JSONGen_EndObject(JSONGen *me)
{
   return Putc(me, '}');
}

int JSONGen_BeginArray(JSONGen *me)
{
   if (JSONGen_NeedSeparator(me)) {
      return PutStr(me, ", [");
   } else {
      return Putc(me, '[');
   }
}

int JSONGen_EndArray(JSONGen *me)
{
   return Putc(me, ']');
}

int JSONGen_PutKey(JSONGen *me, const char *pszKey, int nLen)
{
   int nErr;

   nErr = JSONGen_PutString(me, pszKey, nLen);
   if (JSONGEN_SUCCESS == nErr) {
      nErr = PutStr(me, ": ");
   }
   return nErr;
}

int JSONGen_PutInt(JSONGen *me, int n)
{
   int nErr = JSONGEN_SUCCESS;
   char buf[16]={0};

   if (JSONGen_NeedSeparator(me)) {
      nErr = PutStr(me, ", ");
   }

   if (JSONGEN_SUCCESS == nErr) {
      snprintf(buf, 16, "%d", n);
      nErr = PutStr(me, buf);
   }

   return nErr;
}

int JSONGen_PutUInt(JSONGen *me, unsigned int n)
{
   int nErr = JSONGEN_SUCCESS;
   char buf[16]={0};

   if (JSONGen_NeedSeparator(me)) {
      nErr = PutStr(me, ", ");
   }

   if (JSONGEN_SUCCESS == nErr) {
      snprintf(buf, 16, "%u", n);
      nErr = PutStr(me, buf);
   }

   return nErr;
}

int JSONGen_PutDouble(JSONGen *me, double d)
{
   int nErr = JSONGEN_SUCCESS;
   char buf[32]={0};

   if (JSONGen_NeedSeparator(me)) {
      nErr = PutStr(me, ", ");
   }

   if (JSONGEN_SUCCESS == nErr) {
      snprintf(buf, 32, "%g", d);
      nErr = PutStr(me, buf);
   }

   return nErr;
}

int JSONGen_PutBool(JSONGen *me, int n)
{
   int nErr = JSONGEN_SUCCESS;

   if (JSONGen_NeedSeparator(me)) {
      nErr = PutStr(me, ", ");
   }

   if (JSONGEN_SUCCESS == nErr) {
      if (0 == n) {
         nErr = PutStr(me, "false");
      } else {
         nErr = PutStr(me, "true");
      }
   }

   return nErr;
}

int JSONGen_PutNull(JSONGen *me)
{
   int nErr = JSONGEN_SUCCESS;

   if (JSONGen_NeedSeparator(me)) {
      nErr = PutStr(me, ", ");
   }

   if (JSONGEN_SUCCESS == nErr) {
      nErr = PutStr(me, "null");
   }

   return nErr;
}


int JSONGen_PutJSON(JSONGen *me, const char *pszJSON, int nLen)
{
   int nErr = JSONGEN_SUCCESS;
   const char *psz = pszJSON;
   int n = 0;

   if (0 == pszJSON || 0 == nLen) {
      return JSONGEN_ERROR;
   }

   //find the first non-space char in pszJSON
   while (IsSpace(psz[n]) && n < nLen) n++;

   //if pszJSON starts with one of these, then there is no need for separator
   if (psz[n] != ']' && psz[n] != '}' && psz[n] != ',' && psz[n] != ':') {
      if (JSONGen_NeedSeparator(me)) {
         nErr = PutStr(me, ", ");
      }
   }

   if (JSONGEN_SUCCESS == nErr) {
      nErr = PutBuf(me, pszJSON, nLen);
   }

   return nErr;
}


int JSONGen_GetJSON(JSONGen *me, const char **ppcsz, int *pnSize)
{
   int nErr;

   if (me->nCurr == 0) {
      return JSONGEN_ERROR;
   }

   //null terminate before returning
   nErr = Putc(me, '\0');
   if (JSONGEN_SUCCESS == nErr) {
      me->nCurr--; //termination is not counted in case of more serialization
      if (pnSize) {
         *pnSize = me->nCurr+1; //add 1 since nCurr is zero based index
      }
      *ppcsz = me->psz;
   }

   return nErr;
}
