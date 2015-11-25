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
#include "stdafx.h"

#include "json_parser.h"

#include <stdio.h>
#include <string.h>

#include "js.c"

#define STD_ARRAY_SIZE(a)             ((int)((sizeof((a))/sizeof((a)[0]))))

int SkipStringTest(void)
{
    int n;
    const char *tests[] = {
        "\"\",",
        "\"a\":",
        "\"a\\\"b\",",
        "\"a\\\"\\\"b\",",
        "\"a\\\\\",",
        "\"a\\\\\\\\\",",
    };

    for (n = 0; n < STD_ARRAY_SIZE(tests); ++n) {
        const char *psz = tests[n];
        unsigned int iMax = strlen(psz);
        unsigned int i = SkipString(psz, iMax, 0);
        if (i > iMax || (psz[i] != ',' && psz[i] != ':')) {
            printf("SkipString(%s) i=%d\n", psz, i);
            return 1;
        }
    }
    return 0;
}


int SkipWordTest(void)
{
    int n;
    const char *tests[] = {
        "1,",
        "1]",
        "true,",
        "false]",
        "-1.2234e27,",
        "-0.2234e27,",
        "2345,",
        "null,",
    };

    for (n = 0; n < STD_ARRAY_SIZE(tests); ++n) {
        const char *psz = tests[n];
        uint iMax = strlen(psz);
        uint i = SkipWord(psz, iMax, 0);
        if (i > iMax || (psz[i] != ',' && psz[i] != ']')) {
            printf("SkipWord(%s) i=%d\n", psz, i);
            return 1;
        }
    }
    return 0;
}


int SkipTest(void)
{
    int n;
    struct {
        const char *psz;
        const char *pszEnd;
    } tests[] = {
        { "\"s\" .",             "." },
        { "-12.34 9",            "9" },
        { "[1,2] 9",             "9" },
        { "{\"a\":\"b\"} 9",     "9" },
        { "[{\"a\":\"b\"},2] 9", "9" },
        { "[{{{}}},{{}}]9",      "9" },
        { "\"a\" : 9",           ": 9" },
        { ": 9",                 "9" },

    };

    for (n = 0; n < STD_ARRAY_SIZE(tests); ++n) {
        const char *psz = tests[n].psz;
        const char *pszEnd = tests[n].pszEnd;
        unsigned int ulen = strlen(psz);
        unsigned int i;

        i = Skip(psz, ulen, 0);

        if (i > ulen) {
            printf("i=%d > ulen=%d: psz=[%s]\n", i, ulen, psz);
            return 1;
        }
        if (0 != strcmp(psz + i, pszEnd)) {
            printf("psz == [%s]\n", psz);
            printf("psz+i=[%s] != pszEnd=[%s]\n", psz + i, pszEnd);
            return 1;
        }
    }
    return 0;
}

int ReadHexTest(void)
{
    int n;
    struct {
        const char *psz;
        int num;
    } tests[] = {
        { "f345",  0xf345 },
        { "4589",  0x4589 },
        { "0000",  0 },
        { "ffff",  0xffff },
        { "04dF",  0x4df },
        { "FFdd",  0xffdd },
        { "0041",  'A' },

    };

    for (n = 0; n < STD_ARRAY_SIZE(tests); ++n) {
        const char *psz = tests[n].psz;
        int num = tests[n].num;
        unsigned int i, nRead = 0;
        i = ReadUnicodePoint(psz, &nRead);
        if (i != JSONPARSER_SUCCESS || nRead != num) {
            printf("Failed to read num: %s with %x\n", psz, nRead);
            return 1;
        }
    }
    return 0;
}

int ReadIntTest(void)
{
    int n;
    struct {
        const char *psz;
        int num;
    } tests[] = {
        { "2147483647",  2147483647 },
        { "0",  0 },
        { "-2147483648",  (-2147483647 - 1) },
        { "989348",  989348 },
        { "-1.0e+0",  -1 },
        { "-1234.122e+3",  -1234122 },
        { "-2345.8484E4",  -23458484 },
        { "-1000e-2",  -10 },
        { "-10E3",  -10000 },
        { "10E+3",  10000 },
    };


    for (n = 0; n < STD_ARRAY_SIZE(tests); ++n) {
        const char *psz = tests[n].psz;
        int i, num = 0;
        JSONParser js = { 0 };
        JSONParser_Ctor(&js, psz, 0);

        i = JSONParser_GetInt(&js, 0, &num);
        if (i != JSONPARSER_SUCCESS || tests[n].num != num) {
            printf("Failed to read %s: Read %d, Expected: %d\n", psz, num, tests[n].num);
            return 1;
        }
    }
    return 0;
}


int ReadUIntTest(void)
{
    int n;
    struct {
        const char *psz;
        unsigned int num;
    } tests[] = {
        { "989348",  989348 },
        { "4294967295",  4294967295u },
        { "0",  0 },
        { "1.0e+0",  1 },
        { "1234.122e+3",  1234122 },
        { "1000e-2",  10 },
        { "10E3",  10000 },
        { "10E+3",  10000 },
    };


    for (n = 0; n < STD_ARRAY_SIZE(tests); ++n) {
        const char *psz = tests[n].psz;
        unsigned int i, num = 0;
        JSONParser js = { 0 };
        JSONParser_Ctor(&js, psz, 0);

        i = JSONParser_GetUInt(&js, 0, &num);
        if (i != JSONPARSER_SUCCESS || tests[n].num != num) {
            printf("Failed to read %s: Read %u, Expected: %u\n", psz, num, tests[n].num);
            return 1;
        }
    }
    return 0;
}

int ReadDoubleTest(void)
{
    int n;
    struct {
        const char *psz;
        double num;
    } tests[] = {
        { "0.23",  0.23 },
        { "10.23",  10.23 },
        { "10.235e+2",  1023.5 },
        { "110.23e-2",  1.1023 },
    };

    for (n = 0; n < STD_ARRAY_SIZE(tests); ++n) {
        const char *psz = tests[n].psz;
        unsigned int i;
        double num = 0;
        JSONParser js = { 0 };
        JSONParser_Ctor(&js, psz, 0);

        i = JSONParser_GetDouble(&js, 0, &num);
        if (i != JSONPARSER_SUCCESS || tests[n].num != num) {
            printf("Failed to read %s: Read %f, Expected: %f\n", psz, num, tests[n].num);
            return 1;
        }
    }
    return 0;
}

int GetBoolTest(void)
{
    int n;
    struct {
        const char *psz;
        unsigned int num;
    } tests[] = {
        { "true",  1 },
        { "false",  0 },
    };


    for (n = 0; n < STD_ARRAY_SIZE(tests); ++n) {
        const char *psz = tests[n].psz;
        unsigned int i;
        int b = -1;
        JSONParser js = { 0 };
        JSONParser_Ctor(&js, psz, 0);

        i = JSONParser_GetBool(&js, 0, &b);
        if (i != JSONPARSER_SUCCESS || tests[n].num != b) {
            printf("Failed to read %s: Read %u, Expected: %u\n", psz, b, tests[n].num);
            return 1;
        }
    }
    return 0;
}

int IsNullTest(void)
{
    int n;
    struct {
        const char *psz;
        int num;
    } tests[] = {
        { "null",  0 },
        { "nulld",  -1 },
    };

    for (n = 0; n < STD_ARRAY_SIZE(tests); ++n) {
        const char *psz = tests[n].psz;
        unsigned int i;
        JSONParser js = { 0 };
        JSONParser_Ctor(&js, psz, 0);

        i = JSONParser_IsNull(&js, 0);
        if (i != tests[n].num) {
            printf("Failed to read %s: %d, %d\n", psz, i, tests[n].num);
            return 1;
        }
    }
    return 0;
}


int StrCmpTest(void)
{
    int n;
    struct {
        const char *psz;
        const char *pszMatch;
    } tests[] = {
        { "\"Hello\"",  "Hello" },
        { "\"Welcome to Foo!\"",  "Welcome to Foo!" },
        { "\"ss@ss_com\"",  "ss@ss_com" },
        { "\"team\"",  "team.player" },
        { "\"te\\u0061m\"",  "team.34" },

    };

    for (n = 0; n < STD_ARRAY_SIZE(tests); ++n) {
        const char *psz = tests[n].psz;
        const char *pszMatch = tests[n].pszMatch;
        unsigned int i;

        JSONParser js = { 0 };
        JSONParser_Ctor(&js, psz, 0);

        i = JSONParser_StrCmp(&js, 0, pszMatch, 0, 0);
        if (i != 0) {
            printf("Failed to compare strings: %s with %s\n", psz, pszMatch);
            return 1;
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    return
        SkipStringTest() ||
        SkipWordTest() ||
        SkipTest() ||
        ReadHexTest() ||
        StrCmpTest() ||
        ReadUIntTest() ||
        ReadIntTest() ||
        ReadDoubleTest() ||
        GetBoolTest() ||
        IsNullTest();
}
