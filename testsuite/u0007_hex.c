/*
Copyright 2010 SourceGear, LLC

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

// u0007_hex.c
// test various functions in ut/sg_hex.c
//////////////////////////////////////////////////////////////////

#include <sg.h>

#include "unittests.h"

//////////////////////////////////////////////////////////////////

int u0007_hex__test_byte(SG_context * pCtx)
{
	SG_byte b;

	// test parsing 2 character hex strings into byte data.

	SG_hex__parse_hex_byte(pCtx,"12",&b);
	VERIFY_COND("test_byte", !SG_context__has_err(pCtx));
	VERIFY_COND("test_byte", (b == 0x12));
	SG_context__err_reset(pCtx);

	SG_hex__parse_hex_byte(pCtx,"ab",&b);
	VERIFY_COND("test_byte", !SG_context__has_err(pCtx));
	VERIFY_COND("test_byte", (b == 0xab));
	SG_context__err_reset(pCtx);

	SG_hex__parse_hex_byte(pCtx,"AB",&b);
	VERIFY_COND("test_byte", !SG_context__has_err(pCtx));
	VERIFY_COND("test_byte", (b == 0xab));
	SG_context__err_reset(pCtx);

	SG_hex__parse_hex_byte(pCtx,"ABcdef",&b);	// extra input ignored
	VERIFY_COND("test_byte", !SG_context__has_err(pCtx));
	VERIFY_COND("test_byte", (b == 0xab));
	SG_context__err_reset(pCtx);

	SG_hex__parse_hex_byte(pCtx,"xx",&b);		// invalid hex string
	VERIFY_COND("test_byte", SG_context__has_err(pCtx));
	SG_context__err_reset(pCtx);

	SG_hex__parse_hex_byte(pCtx,"0",&b);		// invalid odd length string
	VERIFY_COND("test_byte", SG_context__has_err(pCtx));
	SG_context__err_reset(pCtx);
	return 1;
}

int u0007_hex__test_string(SG_context * pCtx)
{
	SG_byte buf[1000];
	SG_uint32 len;

	const char * szIn = "0123456789abcdef";
	const char * szBogus = "0123xx99";

	SG_hex__parse_hex_string(pCtx,szIn,4,buf,1000,&len);
	VERIFY_COND("test_string",!SG_context__has_err(pCtx));
	VERIFY_COND("test_string",(len == 2));			// "0123" gives us 2 bytes
	VERIFY_COND("test_string",(buf[0] == 0x01));
	VERIFY_COND("test_string",(buf[1] == 0x23));
	SG_context__err_reset(pCtx);

	SG_hex__parse_hex_string(pCtx,szBogus,8,buf,1000,&len);	// invalid chars in input
	VERIFY_COND("test_string",SG_context__has_err(pCtx));
	SG_context__err_reset(pCtx);

	SG_hex__parse_hex_string(pCtx,szIn,8,buf,3,&len);		// buffer overrun
	VERIFY_COND("test_string",SG_context__has_err(pCtx));
	SG_context__err_reset(pCtx);

	return 1;
}

int u0007_hex__test_uint64(SG_context * pCtx)
{
	// test parsing of uint64 from hex string.

	SG_uint64 ui64;

	const char * szIn = "0123456789abcdef0123456789abcdef";
	const char * szBogus = "0123xx6789abcdef0123456789abcdef";

	SG_hex__parse_hex_uint64(pCtx,szIn,16,&ui64);
	VERIFY_COND("test_uint64",!SG_context__has_err(pCtx));
	VERIFY_COND("test_uint64",(ui64 == 0x0123456789abcdefULL));
	VERIFY_COND("test_uint64",((ui64 & 0xffff) == 0xcdef));
	VERIFY_COND("test_uint64",(((ui64>>48) & 0xffff) == 0x0123));
	SG_context__err_reset(pCtx);

	SG_hex__parse_hex_uint64(pCtx,&szIn[8],16,&ui64);
	VERIFY_COND("test_uint64",!SG_context__has_err(pCtx));
	VERIFY_COND("test_uint64",(ui64 == 0x89abcdef01234567ULL));
	VERIFY_COND("test_uint64",((ui64 & 0xffff) == 0x4567));
	VERIFY_COND("test_uint64",(((ui64>>48) & 0xffff) == 0x89ab));
	SG_context__err_reset(pCtx);

	SG_hex__parse_hex_uint64(pCtx,szBogus,16,&ui64);
	VERIFY_COND("test_uint64",SG_context__has_err(pCtx));
	SG_context__err_reset(pCtx);

	return 1;
}

int u0007_hex__test_format(void)
{
	// test raw byte to hex string formatting.

	char szBuf[1000];
	SG_byte byteBuf[] = { 0x12, 0x34, 0x56 };

	SG_hex__format_buf(szBuf,byteBuf,3);
	VERIFY_COND("test_format",(strcmp(szBuf,"123456")==0));

	SG_hex__format_uint8(szBuf,0xab);
	VERIFY_COND("test_format",(strcmp(szBuf,"ab")==0));

	SG_hex__format_uint16(szBuf,0xabcd);
	VERIFY_COND("test_format",(strcmp(szBuf,"abcd")==0));

	SG_hex__format_uint32(szBuf,0xabcdef01);
	VERIFY_COND("test_format",(strcmp(szBuf,"abcdef01")==0));

	SG_hex__format_uint64(szBuf,0xfedcba9876543210ULL);
	VERIFY_COND("test_format",(strcmp(szBuf,"fedcba9876543210")==0));

	return 1;
}




TEST_MAIN(u0007_hex)
{
	TEMPLATE_MAIN_START;

	BEGIN_TEST(  u0007_hex__test_byte(pCtx)  );
	BEGIN_TEST(  u0007_hex__test_string(pCtx)  );
	BEGIN_TEST(  u0007_hex__test_uint64(pCtx)  );
	BEGIN_TEST(  u0007_hex__test_format()  );

	TEMPLATE_MAIN_END;
}
