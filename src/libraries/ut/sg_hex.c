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

// sg_hex.c
// utility routines for parsing hex strings.
//////////////////////////////////////////////////////////////////

#include <sg.h>

//////////////////////////////////////////////////////////////////

void SG_hex__parse_one_hex_char(SG_context* pCtx, char c, SG_uint8* pResult)
{
	// parse single character in [0..9a..f] into hex value.

	if ((c >= '0') && (c <= '9'))
	{
		*pResult = c - '0';
		return;
	}
	else if ((c >= 'a') && (c <= 'f'))
	{
		*pResult = c - 'a' + 10;
		return;
	}
	else if ((c >= 'A') && (c <= 'F'))
	{
		*pResult = c - 'A' + 10;
		return;
	}
	else
	{
		SG_ARGCHECK_RETURN( (0), c );
	}
}


void SG_hex__parse_hex_byte(SG_context* pCtx, const char* s, SG_byte* pResult)
{
	// parse 2 character sequence in [00..ff] into hex uint8 byte value.

	SG_uint8 c0 = 0;
	SG_uint8 c1 = 0;

	SG_ERR_CHECK_RETURN(  SG_hex__parse_one_hex_char(pCtx, s[0], &c0)  );

	SG_ERR_CHECK_RETURN(  SG_hex__parse_one_hex_char(pCtx, s[1], &c1)  );

	*pResult = (c0 << 4) | c1;
}

void SG_hex__parse_hex_string(SG_context* pCtx, const char * szIn, SG_uint32 lenIn,
								  SG_byte * pByteBuf, SG_uint32 lenByteBuf,
								  SG_uint32 * pLenInBytes)
{
	// parse a hex string into a buffer of bytes.

	SG_uint32 k, kLimit;

	SG_ARGCHECK_RETURN((lenIn % 2) == 0, lenIn);

	kLimit = lenIn / 2;

	SG_ARGCHECK_RETURN(kLimit <= lenByteBuf, lenByteBuf);

	for (k=0; k<kLimit; k++)
	{
		SG_ERR_CHECK_RETURN(  SG_hex__parse_hex_byte(pCtx, szIn,&pByteBuf[k])  );
		szIn += 2;
	}

	if (pLenInBytes)
		*pLenInBytes = kLimit;

}
void SG_hex__validate_hex_string(SG_context* pCtx, const char * szIn, SG_uint32 lenIn)
{
	SG_uint32 k;

	SG_ARGCHECK_RETURN((lenIn % 2) == 0, lenIn);

	for (k=0; k<lenIn; k++)
	{

		if(!(((*szIn >= '0') && (*szIn <= '9')) ||
			 ((*szIn >= 'a') && (*szIn <= 'f')) ||
			 ((*szIn >= 'A') && (*szIn <= 'F')) ) )
		{
			SG_ARGCHECK_RETURN( (0), *szIn );
		}
		szIn++;
	}

}

void SG_hex__parse_hex_uint8(SG_context* pCtx, const char * szIn, SG_uint8 * pui8Result)
{
	SG_ERR_CHECK_RETURN(  SG_hex__parse_hex_byte(pCtx, szIn,(SG_byte *)pui8Result)  );
}

void SG_hex__parse_hex_uint64(SG_context* pCtx, const char * szIn, SG_uint32 lenIn, SG_uint64 * pui64Result)
{
	// parse hex string as a uint64.  we assume that we have
	// full leading zeroes and a 16 character string giving
	// an 8 byte integer.
	//
	// the first char is the MSB.

	SG_uint64 v;
	int k;

	SG_NONEMPTYCHECK_RETURN(szIn);
	SG_ARGCHECK_RETURN(lenIn == 16, lenIn);

	v = 0;
	for (k=0; k<8; k++)
	{
		SG_uint8 ui8 = 0;

		SG_ERR_CHECK_RETURN(  SG_hex__parse_hex_byte(pCtx, szIn,&ui8)  );

		v = (v << 8) | (SG_uint64)ui8;
		szIn += 2;
	}

	*pui64Result = v;
}

void SG_hex__parse_hex_uint32(SG_context* pCtx, const char * szIn, SG_uint32 lenIn, SG_uint32 * pui32Result)
{
	// parse hex string as a uint32.  we assume that we have
	// full leading zeroes and a 8 character string giving
	// an 8 byte integer.
	//
	// the first char is the MSB.

	SG_uint32 v;
	int k;

	SG_NONEMPTYCHECK_RETURN(szIn);
	SG_ARGCHECK_RETURN(lenIn == 8, lenIn);

	v = 0;
	for (k=0; k<4; k++)
	{
		SG_uint8 ui8 = 0;

		SG_ERR_CHECK_RETURN(  SG_hex__parse_hex_byte(pCtx, szIn,&ui8)  );

		v = (v << 8) | (SG_uint32)ui8;
		szIn += 2;
	}

	*pui32Result = v;
}

//////////////////////////////////////////////////////////////////

char * SG_hex__format_buf(char * pbuf, const SG_byte * pV, SG_uint32 lenV)
{
	// format a byte buffer as a hex string into the given
	// buffer and return pointer to the null terminator.
	//
	// caller must provide a large enough buffer (lenV*2+1).

	static const char* hex = "0123456789abcdef";

	SG_uint32 k;

	for (k=0; k<lenV; k++)
	{
		pbuf[0] = hex[ (pV[k] >> 4) & 0x0f ];
		pbuf[1] = hex[ (pV[k]     ) & 0x0f ];
		pbuf += 2;
	}

	*pbuf = 0;

	return pbuf;
}

char * SG_hex__format_uint8(char * pbuf, SG_uint8 v)
{
	// format a SG_uint8 as a hex string into the given buffer.
	// return pointer to the null terminator.
	//
	// caller must provide a large enough buffer (3 bytes).
	//
	// this is basically a sprintf(pbuf,"%02x",v)

	static const char* hex = "0123456789abcdef";

	pbuf[0] = hex[ (v >> 4) & 0x0f ];
	pbuf[1] = hex[ (v     ) & 0x0f ];
	pbuf[2] = 0;

	return &pbuf[2];
}

char * SG_hex__format_uint16(char * pbuf, SG_uint16 v)
{
	// format a SG_uint16 as a hex string into the given
	// buffer and return pointer to the null terminator.
	//
	// caller must provide a large enough buffer (5 bytes).
	//
	// this is basically a sprintf(pbuf,"%04x",v)

	int k;

	for (k=sizeof(SG_uint16)-1; k>=0; k--)
		pbuf = SG_hex__format_uint8(pbuf, (SG_uint8)((v >> (k*8)) & 0xff));

	return pbuf;
}

char * SG_hex__format_uint32(char * pbuf, SG_uint32 v)
{
	// format a SG_uint32 as a hex string into the given
	// buffer and return pointer to the null terminator.
	//
	// caller must provide a large enough buffer (9 bytes).
	//
	// this is basically a sprintf(pbuf,"%08x",v)

	int k;

	for (k=sizeof(SG_uint32)-1; k>=0; k--)
		pbuf = SG_hex__format_uint8(pbuf, (SG_uint8)((v >> (k*8)) & 0xff));

	return pbuf;
}

char * SG_hex__format_uint64(char * pbuf, SG_uint64 v)
{
	// format a SG_uint64 as a hex string into the given
	// buffer and return pointer to the null terminator.
	//
	// caller must provide a large enough buffer (17 bytes).
	//
	// this is basically a sprintf but avoids the platform
	// specific printf-flags to get a ui64.
	//
	// MSBs are output first.

	int k;

	for (k=sizeof(SG_uint64)-1; k>=0; k--)
		pbuf = SG_hex__format_uint8(pbuf, (SG_uint8)((v >> (k*8)) & 0xff));

	return pbuf;
}



