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

/**
 *
 * @file u0029_bitvec256.c
 *
 */

//////////////////////////////////////////////////////////////////

#include <sg.h>
#include "unittests.h"

//////////////////////////////////////////////////////////////////

/**
 * perform basic bitvec operations.  basic allocate, set bit, clear bit.
 */
int u0029_bitvec256__basic(SG_context* pCtx)
{
	SG_bitvec256 * pbv;
	unsigned int k, j;

	// allocate a fresh bitvec, set one bit, verify that only that bit is set.

	for (k=0; k<256; k++)
	{
		VERIFY_ERR_CHECK_RETURN(  SG_bitvec256__alloc(pCtx,&pbv)  );
		VERIFY_COND("basic",(pbv != NULL));

		VERIFY_ERR_CHECK_DISCARD(  SG_bitvec256__set_bit__byte(pCtx,pbv,(SG_byte)k)  );

		for (j=0; j<256; j++)
		{
			SG_bool b;

			b = SG_bitvec256__get_bit__byte(pbv,(SG_byte)j);
			VERIFYP_COND("basic(get bit)",(b == (j==k)),("Bit[%d] not correct [j %d][k %d].",j,j,k));
		}

		SG_BITVEC256_NULLFREE(pCtx, pbv);
	}

	return 1;

}

/**
 * perform hex-string bitvec operations.
 */
int u0029_bitvec256__hexstring(SG_context* pCtx)
{
	SG_bitvec256 * pbv;
	char bufHex[65];
	unsigned int k;

	char * aszHex[] = { ("0000000000000000" "0000000000000000" "0000000000000000" "0000000000000000"),
						("0123456789abcdef" "fedcba9876543210" "1111222244448888" "9999aaaaccccffff"),
						("ffffffffffffffff" "ffffffffffffffff" "ffffffffffffffff" "ffffffffffffffff"),
	};

	VERIFY_ERR_CHECK_RETURN(  SG_bitvec256__alloc(pCtx,&pbv)  );
	VERIFY_COND("hexstring",(pbv != NULL));

	// test the hex-string interfaces.

	VERIFY_ERR_CHECK_DISCARD(  SG_bitvec256__get__hex(pCtx,pbv,bufHex)  );
	VERIFYP_COND("hexstring",(strcmp(bufHex,aszHex[0])==0),("Initial value not all zeroes. [%s]",bufHex));

	for (k=0; k<SG_NrElements(aszHex); k++)
	{
		VERIFY_ERR_CHECK_DISCARD( SG_bitvec256__set__hex(pCtx,pbv,aszHex[k])  );
		VERIFY_ERR_CHECK_DISCARD(  SG_bitvec256__get__hex(pCtx,pbv,bufHex)  );
		VERIFYP_COND("hexstring(hex)",(strcmp(bufHex,aszHex[k])==0),
				("get__hex[%s] result different from set__hex[%s]",bufHex,aszHex[k]));
	}

	VERIFY_ERR_CHECK_DISCARD(  SG_bitvec256__clear_all_bits(pCtx,pbv)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_bitvec256__get__hex(pCtx,pbv,bufHex)  );
	VERIFYP_COND("hexstring",(strcmp(bufHex,aszHex[0])==0),("Clear result not all zeroes. [%s]",bufHex));

	SG_BITVEC256_NULLFREE(pCtx, pbv);

	// test hex-string allocate interface

	for (k=0; k<SG_NrElements(aszHex); k++)
	{
		VERIFY_ERR_CHECK_DISCARD(  SG_bitvec256__alloc__hex(pCtx,&pbv, aszHex[k])  );
		VERIFY_COND("hexstring(alloc hex)",(pbv != NULL));
		VERIFY_ERR_CHECK_DISCARD(  SG_bitvec256__get__hex(pCtx,pbv,bufHex)  );
		VERIFYP_COND("hexstring(alloc hex)",(strcmp(bufHex,aszHex[k])==0),
				("get__hex[%s] result different from set__hex[%s]",bufHex,aszHex[k]));
		SG_BITVEC256_NULLFREE(pCtx, pbv);
	}

	return 1;

}

/**
 * perform user string bitvec operations.  the goal here is to take a (non-utf8) user string
 * (such as a filename) and set the bits for each character in the string.
 */
int u0029_bitvec256__string(SG_context* pCtx)
{
	SG_bitvec256 * pbv;
	char bufHex[65];
	unsigned int k;
	struct _item
	{
		SG_byte * szString;
		char * szHex;
	};

	struct _item aItems[] =
		{
			{ (SG_byte *)"",
			  ("0000000000000000" "0000000000000000" "0000000000000000" "0000000000000000"),
			},
			{ (SG_byte *)"\x01\x02\x03",
			  ("7000000000000000" "0000000000000000" "0000000000000000" "0000000000000000"),
			},
			{ (SG_byte *)"\x01\x05\x09\x0d",
			  ("4444000000000000" "0000000000000000" "0000000000000000" "0000000000000000"),
			},
			{ (SG_byte *)"\xff",
			  ("0000000000000000" "0000000000000000" "0000000000000000" "0000000000000001"),
			},
			{ (SG_byte *)"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz",
			  ("0000000000000000" "7fffffe07fffffe0" "0000000000000000" "0000000000000000"),
			},
		};

	// test the user-string interface.  allocate a bitvec, set bits from user-string, verify.

	for (k=0; k<SG_NrElements(aItems); k++)
	{
		VERIFY_ERR_CHECK_RETURN(  SG_bitvec256__alloc(pCtx,&pbv)  );
		VERIFY_COND("string",(pbv != NULL));

		VERIFY_ERR_CHECK_DISCARD( SG_bitvec256__set_bits__sz(pCtx,pbv,aItems[k].szString)  );

		VERIFY_ERR_CHECK_DISCARD(  SG_bitvec256__get__hex(pCtx,pbv,bufHex)  );
		VERIFYP_COND("string",(strcmp(bufHex,aItems[k].szHex)==0),
				("Result different for [%d]\n\treceived[%s]\n\texpected[%s]",k,bufHex,aItems[k].szHex));

		SG_BITVEC256_NULLFREE(pCtx, pbv);
	}

	return 1;

}

/**
 * Test the bitvec256 routines.
 */
TEST_MAIN(u0029_bitvec256)
{
	TEMPLATE_MAIN_START;

	BEGIN_TEST(  u0029_bitvec256__basic(pCtx)  );
	BEGIN_TEST(  u0029_bitvec256__hexstring(pCtx)  );
	BEGIN_TEST(  u0029_bitvec256__string(pCtx)  );

	TEMPLATE_MAIN_END;
}


