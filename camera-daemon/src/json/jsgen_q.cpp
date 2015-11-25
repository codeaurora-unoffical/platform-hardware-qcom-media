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

#include "jsgen.c"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int ReallocFunc(void *p, int nSize, void **ppv)
{
   void *pv = realloc(*ppv, nSize);
   if (pv) {
      *ppv = pv;
      return JSONGEN_SUCCESS;
   }
   return JSONGEN_ERROR;
}

int OverflowTest(void)
{
   JSONGen gen;
   char buf[128];

   JSONGen_Ctor(&gen, buf, 1,ReallocFunc,0);
   JSONGen_BeginObject(&gen);
   JSONGen_BeginObject(&gen);

   return 0;
}

int SerailizeTestOne(void)
{
   JSONGen gen;
   char buf[128];
   const char *psz;

   JSONGen_Ctor(&gen, buf, 1,ReallocFunc,0);
   JSONGen_BeginObject(&gen);
   JSONGen_PutKey(&gen, "Hello", 0);
   JSONGen_PutInt(&gen, 47437);
   JSONGen_PutKey(&gen, "Dude\b\n", 0);
   JSONGen_PutString(&gen, "I am here", 0);

   JSONGen_BeginArray(&gen);
   JSONGen_PutString(&gen, "smit eello", 0);
   JSONGen_PutDouble(&gen, 34566666.22334);
   JSONGen_EndArray(&gen);

   JSONGen_PutInt(&gen, -3);
   JSONGen_EndObject(&gen);

   JSONGen_GetJSON(&gen,&psz,0);

   printf("%s", psz);

   return 0;
}

int PutTest(void)
{
   JSONGen gen;
   char buf[128];
   const char *psz;

   JSONGen_Ctor(&gen, buf, 1,ReallocFunc,0);
   JSONGen_PutBool(&gen, 345);
   JSONGen_GetJSON(&gen,&psz,0);
   printf("%s\n", psz);
   if (0 != strcmp(psz, "true")) {
      return 1;
   }
   JSONGen_Dtor(&gen);


   JSONGen_Ctor(&gen, buf, 1,ReallocFunc,0);
   JSONGen_PutInt(&gen, -234094);
   JSONGen_GetJSON(&gen,&psz,0);
   printf("%s\n", psz);
   if (0 != strcmp(psz, "-234094")) {
      return 1;
   }
   JSONGen_Dtor(&gen);

   JSONGen_Ctor(&gen, buf, 1,ReallocFunc,0);
   JSONGen_PutUInt(&gen, 84774333);
   JSONGen_GetJSON(&gen,&psz,0);
   printf("%s\n", psz);
   if (0 != strcmp(psz, "84774333")) {
      return 1;
   }
   JSONGen_Dtor(&gen);

   JSONGen_Ctor(&gen, buf, 1,ReallocFunc,0);
   JSONGen_PutDouble(&gen, -3838.37);
   JSONGen_GetJSON(&gen,&psz,0);
   printf("%s\n", psz);
   if (0 != strcmp(psz, "-3838.37")) {
      return 1;
   }
   JSONGen_Dtor(&gen);

   JSONGen_Ctor(&gen, buf, 1,ReallocFunc,0);
   JSONGen_PutJSON(&gen, "[-9393e-2, 2, true, false]", strlen("[-9393e-2, 2, true, false]"));
   JSONGen_GetJSON(&gen,&psz,0);
   printf("%s\n", psz);
   if (0 != strcmp(psz, "[-9393e-2, 2, true, false]")) {
      return 1;
   }
   JSONGen_Dtor(&gen);

   JSONGen_Ctor(&gen, buf, 128, 0,0);
   JSONGen_PutJSON(&gen, "{\"settings: [2, ", strlen("{\"settings: [2, "));
   JSONGen_PutDouble(&gen, 123.345);
   JSONGen_PutJSON(&gen, "]}", strlen("]}"));
   JSONGen_GetJSON(&gen,&psz,0);
   printf("%s\n", psz);
   if (0 != strcmp(psz, "{\"settings: [2, 123.345]}")) {
      return 1;
   }
   JSONGen_Dtor(&gen);

   JSONGen_Ctor(&gen, buf, 128, 0,0);
   JSONGen_PutJSON(&gen, "[2", strlen("[2"));
   JSONGen_PutJSON(&gen, "3]", strlen("3]"));
   JSONGen_GetJSON(&gen,&psz,0);
   printf("%s\n", psz);
   if (0 != strcmp(psz, "[2, 3]")) {
      return 1;
   }
   JSONGen_Dtor(&gen);


   return 0;
}

static int APITest(void)
{
   JSONGen gen = {0};
   int nErr=JSONGEN_ERROR;
   const char *psz;
   int nSize;

   JSONGen_Ctor(&gen, 0,0, ReallocFunc, 0);

   TRY(nErr, JSONGen_BeginObject(&gen));
   TRY(nErr, JSONGen_EndObject(&gen));
   TRY(nErr, JSONGen_GetJSON(&gen, &psz, &nSize));
   if (0 != strncmp(psz, "{}", nSize)) {
      nErr = JSONGEN_ERROR;
   }

   CATCH(nErr) {}

   JSONGen_Dtor(&gen);
   return nErr;
}

int main(int argc, char **argv)
{
   return OverflowTest() ||
      SerailizeTestOne() ||
      PutTest() ||
      APITest()
      ;
}
