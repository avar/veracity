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

// testsuite/unittests/u0001_stdint.c
// test basic types defined in libraries/include/sg_stdint.h
//////////////////////////////////////////////////////////////////

#include <sg.h>

#include "unittests.h"

//////////////////////////////////////////////////////////////////

int u0001_stdint__test_scalars(void)
{
	VERIFY_COND( "test_scalars", (sizeof(SG_int8) == 1) );
	VERIFY_COND( "test_scalars", (sizeof(SG_uint8) == 1) );

	VERIFY_COND( "test_scalars", (sizeof(SG_int16) == 2) );
	VERIFY_COND( "test_scalars", (sizeof(SG_uint16) == 2) );

	VERIFY_COND( "test_scalars", (sizeof(SG_int32) == 4) );
	VERIFY_COND( "test_scalars", (sizeof(SG_uint32) == 4) );

	VERIFY_COND( "test_scalars", (sizeof(SG_int64) == 8) );
	VERIFY_COND( "test_scalars", (sizeof(SG_uint64) == 8) );

	VERIFY_COND( "test_scalars", (sizeof(SG_byte) == 1) );

	return 1;
}

int u0001_stdint__test_system(void)
{
	VERIFY_COND( "test_system", (sizeof(size_t) >= 4) );
#if defined(LINUX) || defined(MAC)
	INFOP("test_system", ("sizeof(off_t) is %d",sizeof(off_t)));
#endif

	return 1;
}

int u0001_stdint__test_max(void)
{
	char buf[1024];

	SG_uint8  u8  = SG_UINT8_MAX;
	SG_uint16 u16 = SG_UINT16_MAX;
	SG_uint32 u32 = SG_UINT32_MAX;
	SG_uint64 u64 = SG_UINT64_MAX;

	SG_hex__format_uint8(buf,u8);
	VERIFY_COND("test_max", (strcmp(buf,"ff") == 0));

	SG_hex__format_uint16(buf,u16);
	VERIFY_COND("test_max", (strcmp(buf,"ffff") == 0));

	SG_hex__format_uint32(buf,u32);
	VERIFY_COND("test_max", (strcmp(buf,"ffffffff") == 0));

	SG_hex__format_uint64(buf,u64);
	VERIFY_COND("test_max", (strcmp(buf,"ffffffffffffffff") == 0));


	return 1;
}

TEST_MAIN(u0001_stdint)
{
	TEMPLATE_MAIN_START;

	BEGIN_TEST(  u0001_stdint__test_scalars()  );
	BEGIN_TEST(  u0001_stdint__test_system()  );
	BEGIN_TEST(  u0001_stdint__test_max()  );

	TEMPLATE_MAIN_END;
}

