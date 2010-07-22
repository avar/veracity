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
 * @file sg_bitvec.c
 *
 */

//////////////////////////////////////////////////////////////////

#include <sg.h>

//////////////////////////////////////////////////////////////////

struct SG_bitvec256
{
	/**
	 * A 256-bit bit-vectory.  Actually an array of bytes.
	 *
	 * In the bit-vector, we map byte 0x00 to the MSB of bv[0] and
	 * byte 0xff to the LSB of bv[31].
	 */
	SG_byte			m_aBytes[32];
};

//////////////////////////////////////////////////////////////////

void SG_bitvec256__alloc(SG_context* pCtx, SG_bitvec256 ** ppNew)
{
	SG_bitvec256 * pbv;

	SG_ERR_CHECK_RETURN(  SG_alloc1(pCtx, pbv)  );

	*ppNew = pbv;
	return;
}

void SG_bitvec256__alloc__hex(SG_context* pCtx, SG_bitvec256 ** ppNew, const char * szHex)
{
	SG_bitvec256 * pbv = NULL;

	SG_ERR_CHECK(  SG_bitvec256__alloc(pCtx, &pbv)  );

	SG_ERR_CHECK(  SG_bitvec256__set__hex(pCtx, pbv,szHex)  );

	*ppNew = pbv;

	return;

fail:
	SG_BITVEC256_NULLFREE(pCtx, pbv);
}

void SG_bitvec256__free(SG_context * pCtx, SG_bitvec256 * pbv)
{
	if (!pbv)
		return;

	SG_NULLFREE(pCtx, pbv);
}

//////////////////////////////////////////////////////////////////

void SG_bitvec256__set_bit__byte(SG_context* pCtx, SG_bitvec256 * pbv, SG_byte v)
{
	SG_NULLARGCHECK_RETURN(pbv);

	pbv->m_aBytes[ v / 8 ] |= (1 << (7 - (v % 8)));
}

SG_bool SG_bitvec256__get_bit__byte(const SG_bitvec256 * pbv, SG_byte v)
{
	SG_byte bit;

	SG_ASSERT( (pbv != NULL) && "Invalid Argument" );

	bit = (pbv->m_aBytes[ v / 8 ] >> (7 - (v % 8))) & 0x01;

	return (bit ? SG_TRUE : SG_FALSE);
}

//////////////////////////////////////////////////////////////////

void SG_bitvec256__set_bits__sz(SG_context* pCtx, SG_bitvec256 * pbv, const SG_byte * sz)
{
	SG_NULLARGCHECK_RETURN(pbv);

	if (!sz)
		return;

	while (*sz)
	{
		SG_byte v = *sz++;

		pbv->m_aBytes[ v / 8 ] |= (1 << (7 - (v % 8)));
	}
}

//////////////////////////////////////////////////////////////////

void SG_bitvec256__set__hex(SG_context* pCtx, SG_bitvec256 * pbv, const char * szHex)
{
	SG_uint32 lenHex;

	SG_NULLARGCHECK_RETURN(pbv);
	SG_NULLARGCHECK_RETURN(szHex);

	lenHex = (SG_uint32)strlen(szHex);
	SG_ARGCHECK_RETURN(lenHex, lenHex == 64);

	SG_ERR_CHECK_RETURN(  SG_hex__parse_hex_string(pCtx, szHex,lenHex, pbv->m_aBytes,sizeof(pbv->m_aBytes),NULL)  );
}

void SG_bitvec256__get__hex(SG_context* pCtx, const SG_bitvec256 * pbv, char * bufHex)
{
	SG_NULLARGCHECK_RETURN(pbv);
	SG_NULLARGCHECK_RETURN(bufHex);

	SG_hex__format_buf(bufHex,pbv->m_aBytes,sizeof(pbv->m_aBytes));
}

//////////////////////////////////////////////////////////////////

void SG_bitvec256__clear_all_bits(SG_context* pCtx, SG_bitvec256 * pbv)
{
	SG_NULLARGCHECK_RETURN(pbv);

	memset(pbv->m_aBytes,0,sizeof(pbv->m_aBytes));
}

void SG_bitvec256__equal(SG_context* pCtx, const SG_bitvec256 * pbv1, const SG_bitvec256 * pbv2, SG_bool * pbEqual)
{
	SG_NULLARGCHECK_RETURN(pbv1);
	SG_NULLARGCHECK_RETURN(pbv2);
	SG_NULLARGCHECK_RETURN(pbEqual);

	*pbEqual = (memcmp(pbv1->m_aBytes,pbv2->m_aBytes,sizeof(pbv1->m_aBytes)) == 0);
}
