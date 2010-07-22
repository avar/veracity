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
 * @file sg_gid.c
 *
 * @details Minimal routines to create/manipulate GIDs.
 *
 */

//////////////////////////////////////////////////////////////////

#include <sg.h>

//////////////////////////////////////////////////////////////////

#if defined(MAC) || defined(LINUX)
#include <uuid/uuid.h>
#elif defined(WINDOWS)
#include <rpc.h>
#endif

//////////////////////////////////////////////////////////////////
// Stuff for UUID's (aka GUID's).
//
// It turns out that there are 5 types of UUID's.
// See:
//     http://www.ietf.org/rfc/rfc4122.txt)
//     http://en.wikipedia.org/wiki/UUID
//     http://en.wikipedia.org/wiki/Guid
//
// Both Windows and Linux now default to version-4 -- which are just *big* random
// numbers and *DO NOT* contain any MAC address or time information.  Both platforms
// have a historical version-1 available -- which are only MAC address and time.
//
// We want our unique ID's to be based upon MAC address, time, and random info.
//
// We don't feel that v4 or v1 uuid's are sufficient to guarantee the uniqueness
// that we require.  (Clocks can get reset/turned back, MAC addresses might be
// absent or shared by multiple VM's, random number generators can collide or can
// be poorly seeded).  Granted, these are low probability events, but WE CANNOT
// DETECT OR RECOVER FROM A COLLISION.
//
// Also, we don't have any need to restrict ourselves to 128 bits -- like the COM
// and DCE folks have to do for historical reasons.  And we're not interested in
// being able to have anonymous IDs -- tracability may, in fact, be a feature.
//
// So, we're going to generate "Double UUIDs".  Essentially a version-4 + version-1
// concatentated together.  This may sound like a hack and/or overkill, but it has
// the nice feature that both halves are industry-standard and accepted things.  so
// the combination should be equally accepted.  It is also fairly cheap and easy to
// create and doesn't require us to dig into the guts of UUID generation -- we get
// that for free.  It is *NOT* 100.0000000% guaranteed unique, but it would take
// nanosecond-sync'd machines with a shared MAC and identical entropy....
//
// A single UUID is 128 bits.
// This is 16 bytes as a packed binary buffer.
// This is 32 bytes when expressed as a hex string.

#define MY_SIZEOF_BINARY_UUID		16
#define MY_SIZEOF_HEX_UUID			32

// UUIDs have a fixed hex digit that tells us what type it is.
// Type 4's should have a '4' and type 1's should have a '1'.

#define MY_UUID_FIXED_DIGIT			12

//////////////////////////////////////////////////////////////////

/**
 * This structure is a template for a GID.  Publicly, a GID is just
 * a "const char *" containing an opaque string of SG_GID_ACTUAL_LENGTH
 * bytes.  We overlay the following structure to avoid some subscript
 * computations when dealing with the parts of a GID.  But this is for
 * our convenience only.  No one outside of this file should see this.
 */
struct _gid
{
	char			g;
	char			buf4[MY_SIZEOF_HEX_UUID];
	char			buf1[MY_SIZEOF_HEX_UUID];
	char			zero;
};

//////////////////////////////////////////////////////////////////

#if defined(MAC) || defined(LINUX)
void SG_gid__generate(SG_context * pCtx,
					  char * bufGid, SG_uint32 lenBuf)
{
	// Mac & Linux define a uuid as uuid_t and have "uuid_generate(uuid_t out)".
	// uuid_t is an array of 16 uint8's.  These are already in network-order.

	uuid_t u4, u1;
	struct _gid * pGid = (struct _gid *)bufGid;

	SG_ASSERT(  (sizeof(uuid_t) == MY_SIZEOF_BINARY_UUID)  );
	SG_ASSERT(  (sizeof(struct _gid) == SG_GID_BUFFER_LENGTH)  );

	SG_NULLARGCHECK_RETURN(bufGid);
	SG_ARGCHECK_RETURN( (lenBuf >= SG_GID_BUFFER_LENGTH), lenBuf );

	uuid_generate_random(u4);	// type 4 (/dev/urandom) with fallback to pseudo-random
	uuid_generate_time(u1);		// type 1 (mac+time)

	// build combined GID string in parts.

	pGid->g = SG_GID_PREFIX_CHAR;
	SG_hex__format_buf(pGid->buf4, u4, MY_SIZEOF_BINARY_UUID);
	SG_hex__format_buf(pGid->buf1, u1, MY_SIZEOF_BINARY_UUID);
	pGid->zero = 0;

	// Within each UUID there is a fixed field that indicate the type.

	SG_ASSERT(  (pGid->buf4[MY_UUID_FIXED_DIGIT] == '4')  );			// verify we got a type 4
	SG_ASSERT(  (pGid->buf1[MY_UUID_FIXED_DIGIT] == '1')  );			// verify we got a type 1
}

#elif defined(WINDOWS)

void _sg_gid__serialize_uuid(SG_byte * pbuf, UUID * pu)
{
	// convert uint's inside UUID to network-order buffer.

	int k;

	for (k=sizeof(pu->Data1)-1; k>=0; k--)
		*pbuf++ = (SG_byte)((pu->Data1 >> (k*8)) & 0xff);
	for (k=sizeof(pu->Data2)-1; k>=0; k--)
		*pbuf++ = (SG_byte)((pu->Data2 >> (k*8)) & 0xff);
	for (k=sizeof(pu->Data3)-1; k>=0; k--)
		*pbuf++ = (SG_byte)((pu->Data3 >> (k*8)) & 0xff);
	for (k=0; k<sizeof(pu->Data4); k++)
		*pbuf++ = pu->Data4[k];
}

void SG_gid__generate(SG_context * pCtx,
					  char * bufGid, SG_uint32 lenBuf)
{
	// Windows defines a uuid as a UUID and have "UuidCreate(UUID * pUuid)".
	// UUID is a struct containing a uint32, 2 uint16's, and an array of bytes.
	// So there are some byte-order issues.

	UUID u4, u1;
	SG_byte pBufBinary4[MY_SIZEOF_BINARY_UUID];
	SG_byte pBufBinary1[MY_SIZEOF_BINARY_UUID];
	struct _gid * pGid = (struct _gid *)bufGid;

	SG_ASSERT( sizeof(UUID) == MY_SIZEOF_BINARY_UUID );
	SG_ASSERT(  (sizeof(struct _gid) == SG_GID_BUFFER_LENGTH)  );

	SG_NULLARGCHECK_RETURN(bufGid);
	SG_ARGCHECK_RETURN( (lenBuf >= SG_GID_BUFFER_LENGTH), lenBuf );

	UuidCreate(&u4);				// type 4 (random)
	UuidCreateSequential(&u1);		// type 1 (mac address + time)

	_sg_gid__serialize_uuid(pBufBinary4,&u4);
	_sg_gid__serialize_uuid(pBufBinary1,&u1);

	// build combined GID string in parts.

	pGid->g = SG_GID_PREFIX_CHAR;
	SG_hex__format_buf(pGid->buf4, pBufBinary4, MY_SIZEOF_BINARY_UUID);
	SG_hex__format_buf(pGid->buf1, pBufBinary1, MY_SIZEOF_BINARY_UUID);
	pGid->zero = 0;

	// Within each UUID there is a fixed field that indicate the type.

	SG_ASSERT(  (pGid->buf4[MY_UUID_FIXED_DIGIT] == '4')  );			// verify we got a type 4
	SG_ASSERT(  (pGid->buf1[MY_UUID_FIXED_DIGIT] == '1')  );			// verify we got a type 1
}
#endif

//////////////////////////////////////////////////////////////////

/**
 * TODO Remove this function and convert all calls to the other
 * version that takes a buffer length argument.
 */
void SG_gid__generate_hack(SG_context * pCtx,
						   char * bufGid)
{
	SG_ERR_CHECK_RETURN(  SG_gid__generate(pCtx, bufGid, SG_GID_BUFFER_LENGTH)  );
}

//////////////////////////////////////////////////////////////////

void SG_gid__alloc(SG_context * pCtx, char ** ppszGid)
{
	char * pszGid_Allocated = NULL;

	SG_NULLARGCHECK_RETURN(ppszGid);

	SG_ERR_CHECK_RETURN(  SG_alloc(pCtx, 1, SG_GID_BUFFER_LENGTH, &pszGid_Allocated)  );
	SG_ERR_CHECK(  SG_gid__generate(pCtx, pszGid_Allocated, SG_GID_BUFFER_LENGTH)  );

	*ppszGid = pszGid_Allocated;
	return;

fail:
	SG_NULLFREE(pCtx, pszGid_Allocated);
}

//////////////////////////////////////////////////////////////////

void SG_gid__verify_format(SG_context * pCtx, const char * pszGid, SG_bool * pbValid)
{
	struct _gid * pGid;
	SG_bool bResult = SG_FALSE;

	SG_NULLARGCHECK_RETURN(pszGid);
	SG_NULLARGCHECK_RETURN(pbValid);

	pGid = (struct _gid *)pszGid;
	if (   (pGid->g == SG_GID_PREFIX_CHAR)
		   && (pGid->buf4[MY_UUID_FIXED_DIGIT] == '4')
		   && (pGid->buf1[MY_UUID_FIXED_DIGIT] == '1')
		   && (pGid->zero == 0)
		   && (strlen(pszGid) == SG_GID_ACTUAL_LENGTH) )
	{
//		bResult = SG_TRUE;
//		SG_byte bufBinary_u4_u1[ 2 * MY_SIZEOF_BINARY_UUID ];

		SG_hex__validate_hex_string(pCtx,
								 pGid->buf4, sizeof(pGid->buf4) + sizeof(pGid->buf1));

		bResult = !SG_context__has_err(pCtx);

		if (bResult != SG_TRUE)
			SG_context__err_reset(pCtx);
	}

	*pbValid = bResult;
}

void SG_gid__argcheck(SG_context * pCtx, const char * pszGid)
{
	SG_bool bValid;

	SG_ERR_CHECK_RETURN(  SG_gid__verify_format(pCtx, pszGid, &bValid)  );

	if (!bValid)
		SG_ERR_THROW_RETURN(  SG_ERR_GID_FORMAT_INVALID  );
}

//////////////////////////////////////////////////////////////////

void SG_gid__alloc_clone(SG_context * pCtx, const char * pszGid_Input, char ** ppszGid_Copy)
{
	SG_NULLARGCHECK_RETURN(pszGid_Input);
	SG_NULLARGCHECK_RETURN(ppszGid_Copy);

	SG_ERR_CHECK_RETURN(  SG_gid__argcheck(pCtx, pszGid_Input)  );

	SG_ERR_CHECK_RETURN(  SG_STRDUP(pCtx, pszGid_Input, ppszGid_Copy)  );
}

void SG_gid__copy_into_buffer(SG_context * pCtx, char * pBufGid, SG_uint32 lenBufGid, const char * pszGid_Input)
{
	SG_NULLARGCHECK_RETURN(pBufGid);
	SG_ARGCHECK_RETURN(  (lenBufGid >= SG_GID_BUFFER_LENGTH), lenBufGid  );
	SG_NONEMPTYCHECK_RETURN(pszGid_Input);

	SG_ERR_CHECK_RETURN(  SG_gid__argcheck(pCtx, pszGid_Input)  );

	SG_ERR_CHECK_RETURN(  SG_strcpy(pCtx, pBufGid, lenBufGid, pszGid_Input)  );
}
