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

#include <sg.h>

typedef struct _sg_hashitem
{
	const char* key;
	SG_variant* pv;
	struct _sg_hashitem* pNext;
} sg_hashitem;

struct _SG_vhash
{
	SG_uint32 bucket_count;
	SG_uint32 bucket_bits;
	SG_uint32 bucket_mask;
	SG_strpool *pStrPool;
	SG_bool strpool_is_mine;
	SG_varpool *pVarPool;
	SG_bool varpool_is_mine;
	sg_hashitem** aBuckets;
	SG_uint32 count;
	const char** aKeys;
	SG_uint32 space_keys;
};

void sg_hashitem__freecontents(SG_context * pCtx, sg_hashitem* pv)
{
	switch (pv->pv->type)
	{
	case SG_VARIANT_TYPE_VARRAY:
		SG_VARRAY_NULLFREE(pCtx, pv->pv->v.val_varray);
		break;
	case SG_VARIANT_TYPE_VHASH:
		SG_VHASH_NULLFREE(pCtx, pv->pv->v.val_vhash);
		break;
	}
	pv->pv = NULL;
}

void sg_hashitem__free(SG_context * pCtx, sg_hashitem* pv)
{
    sg_hashitem* p = pv;

    while (p)
    {
        sg_hashitem* pnext = p->pNext;

        sg_hashitem__freecontents(pCtx, p);

        SG_NULLFREE(pCtx, p);

        p = pnext;
    }
}

SG_uint8 sg_vhash__calc_bits_for_guess(SG_uint32 guess)
{
	SG_uint8 log = 0;
	while (guess > 1)
	{
		guess = guess >> 1;
		log++;
	}

	if (log < 3)
	{
		log = 3;
	}
	else if (log > 18)
	{
		log = 16;
	}

	return log;
}

void SG_vhash__alloc__shared(SG_context* pCtx, SG_vhash** ppResult, SG_uint32 guess, SG_vhash* pvhShared)
{
	if (pvhShared)
	{
		SG_ERR_CHECK_RETURN(  SG_vhash__alloc__params(pCtx, ppResult, guess, pvhShared->pStrPool, pvhShared->pVarPool)  );	// do not use SG_VHASH__ALLOC__PARAMS
	}
	else
	{
		SG_ERR_CHECK_RETURN(  SG_vhash__alloc__params(pCtx, ppResult, guess, NULL, NULL)  );	// do not use SG_VHASH__ALLOC__PARAMS
	}
}

void SG_vhash__copy_items(
    SG_context* pCtx,
    const SG_vhash* pvh_from,
    SG_vhash* pvh_to
	)
{
    SG_uint32 i = 0;
    SG_vhash* pvh_sub = NULL;
    SG_varray* pva_sub = NULL;

    for (i=0; i<pvh_from->count; i++)
    {
        const char* psz_key = NULL;
        const SG_variant* pv = NULL;

        SG_ERR_CHECK(  SG_vhash__get_nth_pair(pCtx, pvh_from, i, &psz_key, &pv)  );
        switch (pv->type)
        {
        case SG_VARIANT_TYPE_DOUBLE:
            SG_ERR_CHECK(  SG_vhash__add__double(pCtx, pvh_to, psz_key, pv->v.val_double)  );
            break;

        case SG_VARIANT_TYPE_INT64:
            SG_ERR_CHECK(  SG_vhash__add__int64(pCtx, pvh_to, psz_key, pv->v.val_int64)  );
            break;

        case SG_VARIANT_TYPE_BOOL:
            SG_ERR_CHECK(  SG_vhash__add__bool(pCtx, pvh_to, psz_key, pv->v.val_bool)  );
            break;

        case SG_VARIANT_TYPE_NULL:
            SG_ERR_CHECK(  SG_vhash__add__null(pCtx, pvh_to, psz_key)  );
            break;

        case SG_VARIANT_TYPE_SZ:
            SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh_to, psz_key, pv->v.val_sz)  );
            break;

        case SG_VARIANT_TYPE_VHASH:
            SG_ERR_CHECK(  SG_vhash__addnew__vhash(pCtx, pvh_to, psz_key, &pvh_sub)  );
            SG_ERR_CHECK(  SG_vhash__copy_items(pCtx, pv->v.val_vhash, pvh_sub)  );
            break;

        case SG_VARIANT_TYPE_VARRAY:
            SG_ERR_CHECK(  SG_vhash__addnew__varray(pCtx, pvh_to, psz_key, &pva_sub)  );
            SG_ERR_CHECK(  SG_varray__copy_items(pCtx, pv->v.val_varray, pva_sub)  );
            break;
        }
    }

fail:
    ;
}

void SG_vhash__alloc__copy(
    SG_context* pCtx,
	SG_vhash** ppResult,
	const SG_vhash* pvhOther
	)
{
	SG_vhash* pvh = NULL;

    SG_ERR_CHECK(  SG_vhash__alloc__params(pCtx, &pvh, pvhOther->count, NULL, NULL)  );
    SG_ERR_CHECK(  SG_vhash__copy_items(pCtx, pvhOther, pvh)  );

	*ppResult = pvh;
    pvh = NULL;

fail:
    SG_VHASH_NULLFREE(pCtx, pvh);
}

void SG_vhash__alloc__params(SG_context* pCtx, SG_vhash** ppResult, SG_uint32 guess, SG_strpool* pStrPool, SG_varpool* pVarPool)
{
	SG_vhash * pThis;
	SG_uint8 num_bits;

	SG_ERR_CHECK(  SG_alloc1(pCtx, pThis)  );

	if (pStrPool)
	{
		pThis->strpool_is_mine = SG_FALSE;
		pThis->pStrPool = pStrPool;
	}
	else
	{
		pThis->strpool_is_mine = SG_TRUE;
		SG_ERR_CHECK(  SG_STRPOOL__ALLOC(pCtx, &pThis->pStrPool, guess * 10 *3)  );
	}

	if (pVarPool)
	{
		pThis->varpool_is_mine = SG_FALSE;
		pThis->pVarPool = pVarPool;
	}
	else
	{
		pThis->varpool_is_mine = SG_TRUE;
		SG_ERR_CHECK(  SG_VARPOOL__ALLOC(pCtx, &pThis->pVarPool, guess)  );
	}

	num_bits = sg_vhash__calc_bits_for_guess(guess);
	pThis->bucket_bits = num_bits;
	pThis->bucket_count = (1 << pThis->bucket_bits);
	pThis->bucket_mask = (pThis->bucket_count-1);

	SG_ERR_CHECK(  SG_alloc(pCtx, pThis->bucket_count, sizeof(sg_hashitem*), &pThis->aBuckets)  );

	pThis->space_keys = pThis->bucket_count;

	SG_ERR_CHECK(  SG_alloc(pCtx, pThis->space_keys, sizeof(char*), &pThis->aKeys)  );

	*ppResult = pThis;

	return;

fail:
	if (pThis)
	{
		if (pThis->strpool_is_mine && pThis->pStrPool)
		{
			SG_STRPOOL_NULLFREE(pCtx, pThis->pStrPool);
		}

		if (pThis->varpool_is_mine && pThis->pVarPool)
		{
			SG_VARPOOL_NULLFREE(pCtx, pThis->pVarPool);
		}

        SG_NULLFREE(pCtx, pThis->aBuckets);

        SG_NULLFREE(pCtx, pThis->aKeys);

		SG_NULLFREE(pCtx, pThis);
	}
}

void SG_vhash__alloc(SG_context* pCtx, SG_vhash** ppResult)
{
	SG_vhash__alloc__params(pCtx, ppResult, 32, NULL, NULL);	// do not use SG_VHASH__ALLOC__PARAMS
}

void SG_vhash__free(SG_context * pCtx, SG_vhash* pThis)
{
	SG_uint32 i;

	if (!pThis)
		return;

	if (pThis->aBuckets)
	{
		for (i=0; i<pThis->bucket_count; i++)
		{
			if (pThis->aBuckets[i])
			{
				sg_hashitem__free(pCtx, pThis->aBuckets[i]);
			}
		}

		SG_NULLFREE(pCtx, pThis->aBuckets);
	}

	if (pThis->strpool_is_mine && pThis->pStrPool)
	{
		SG_STRPOOL_NULLFREE(pCtx, pThis->pStrPool);
	}

	if (pThis->varpool_is_mine && pThis->pVarPool)
	{
		SG_VARPOOL_NULLFREE(pCtx, pThis->pVarPool);
	}

	if (pThis->aKeys)
	{
		SG_NULLFREE(pCtx, pThis->aKeys);
	}

	SG_NULLFREE(pCtx, pThis);
}

void SG_vhash__count(SG_context* pCtx, const SG_vhash* pHash, SG_uint32* piResult)
{
	SG_NULLARGCHECK_RETURN(pHash);
	SG_NULLARGCHECK_RETURN(piResult);

	*piResult = pHash->count;
}

/*
-------------------------------------------------------------------------------
lookup3.c, by Bob Jenkins, May 2006, Public Domain.
*/

#define hashsize(n) ((SG_uint32)1<<(n))
#define hashmask(n) (hashsize(n)-1)
#define rot(x,k) (((x)<<(k)) | ((x)>>(32-(k))))

/*
-------------------------------------------------------------------------------
mix -- mix 3 32-bit values reversibly.

This is reversible, so any information in (a,b,c) before mix() is
still in (a,b,c) after mix().

If four pairs of (a,b,c) inputs are run through mix(), or through
mix() in reverse, there are at least 32 bits of the output that
are sometimes the same for one pair and different for another pair.
This was tested for:
* pairs that differed by one bit, by two bits, in any combination
  of top bits of (a,b,c), or in any combination of bottom bits of
  (a,b,c).
* "differ" is defined as +, -, ^, or ~^.  For + and -, I transformed
  the output delta to a Gray code (a^(a>>1)) so a string of 1's (as
  is commonly produced by subtraction) look like a single 1-bit
  difference.
* the base values were pseudorandom, all zero but one bit set, or
  all zero plus a counter that starts at zero.

Some k values for my "a-=c; a^=rot(c,k); c+=b;" arrangement that
satisfy this are
    4  6  8 16 19  4
    9 15  3 18 27 15
   14  9  3  7 17  3
Well, "9 15 3 18 27 15" didn't quite get 32 bits diffing
for "differ" defined as + with a one-bit base and a two-bit delta.  I
used http://burtleburtle.net/bob/hash/avalanche.html to choose
the operations, constants, and arrangements of the variables.

This does not achieve avalanche.  There are input bits of (a,b,c)
that fail to affect some output bits of (a,b,c), especially of a.  The
most thoroughly mixed value is c, but it doesn't really even achieve
avalanche in c.

This allows some parallelism.  Read-after-writes are good at doubling
the number of bits affected, so the goal of mixing pulls in the opposite
direction as the goal of parallelism.  I did what I could.  Rotates
seem to cost as much as shifts on every machine I could lay my hands
on, and rotates are much kinder to the top and bottom bits, so I used
rotates.
-------------------------------------------------------------------------------
*/
#define mix(a,b,c) \
{ \
  a -= c;  a ^= rot(c, 4);  c += b; \
  b -= a;  b ^= rot(a, 6);  a += c; \
  c -= b;  c ^= rot(b, 8);  b += a; \
  a -= c;  a ^= rot(c,16);  c += b; \
  b -= a;  b ^= rot(a,19);  a += c; \
  c -= b;  c ^= rot(b, 4);  b += a; \
}

/*
-------------------------------------------------------------------------------
final -- final mixing of 3 32-bit values (a,b,c) into c

Pairs of (a,b,c) values differing in only a few bits will usually
produce values of c that look totally different.  This was tested for
* pairs that differed by one bit, by two bits, in any combination
  of top bits of (a,b,c), or in any combination of bottom bits of
  (a,b,c).
* "differ" is defined as +, -, ^, or ~^.  For + and -, I transformed
  the output delta to a Gray code (a^(a>>1)) so a string of 1's (as
  is commonly produced by subtraction) look like a single 1-bit
  difference.
* the base values were pseudorandom, all zero but one bit set, or
  all zero plus a counter that starts at zero.

These constants passed:
 14 11 25 16 4 14 24
 12 14 25 16 4 14 24
and these came close:
  4  8 15 26 3 22 24
 10  8 15 26 3 22 24
 11  8 15 26 3 22 24
-------------------------------------------------------------------------------
*/
#define final(a,b,c) \
{ \
  c ^= b; c -= rot(b,14); \
  a ^= c; a -= rot(c,11); \
  b ^= a; b -= rot(a,25); \
  c ^= b; c -= rot(b,16); \
  a ^= c; a -= rot(c,4);  \
  b ^= a; b -= rot(a,14); \
  c ^= b; c -= rot(b,24); \
}

/*
-------------------------------------------------------------------------------
hashlittle() -- hash a variable-length key into a 32-bit value
  k       : the key (the unaligned variable-length array of bytes)
  length  : the length of the key, counting by bytes
  initval : can be any 4-byte value
Returns a 32-bit value.  Every bit of the key affects every bit of
the return value.  Two keys differing by one or two bits will have
totally different hash values.

The best hash table sizes are powers of 2.  There is no need to do
mod a prime (mod is sooo slow!).  If you need less than 32 bits,
use a bitmask.  For example, if you need only 10 bits, do
  h = (h & hashmask(10));
In which case, the hash table should have hashsize(10) elements.

If you are hashing n strings (SG_uint8 **)k, do it like this:
  for (i=0, h=0; i<n; ++i) h = hashlittle( k[i], len[i], h);

By Bob Jenkins, 2006.  bob_jenkins@burtleburtle.net.  You may use this
code any way you wish, private, educational, or commercial.  It's free.

Use for hash table lookup, or anything where one collision in 2^^32 is
acceptable.  Do NOT use for cryptographic purposes.
-------------------------------------------------------------------------------
*/

SG_uint32 sg_vhash__hashlittle( const SG_uint8 *k, size_t length)
{
	SG_uint32 a,b,c;                                          /* internal state */

	/* Set up the internal state */
	a = b = c = 0xdeadbeef + ((SG_uint32)length);

	/*--------------- all but the last block: affect some 32 bits of (a,b,c) */
	while (length > 12)
	{
		a += k[0];
		a += ((SG_uint32)k[1])<<8;
		a += ((SG_uint32)k[2])<<16;
		a += ((SG_uint32)k[3])<<24;
		b += k[4];
		b += ((SG_uint32)k[5])<<8;
		b += ((SG_uint32)k[6])<<16;
		b += ((SG_uint32)k[7])<<24;
		c += k[8];
		c += ((SG_uint32)k[9])<<8;
		c += ((SG_uint32)k[10])<<16;
		c += ((SG_uint32)k[11])<<24;
		mix(a,b,c);
		length -= 12;
		k += 12;
	}

	/*-------------------------------- last block: affect all 32 bits of (c) */
	switch(length)                   /* all the case statements fall through */
	{
	case 12: c+=((SG_uint32)k[11])<<24;
	case 11: c+=((SG_uint32)k[10])<<16;
	case 10: c+=((SG_uint32)k[9])<<8;
	case 9 : c+=k[8];
	case 8 : b+=((SG_uint32)k[7])<<24;
	case 7 : b+=((SG_uint32)k[6])<<16;
	case 6 : b+=((SG_uint32)k[5])<<8;
	case 5 : b+=k[4];
	case 4 : a+=((SG_uint32)k[3])<<24;
	case 3 : a+=((SG_uint32)k[2])<<16;
	case 2 : a+=((SG_uint32)k[1])<<8;
	case 1 : a+=k[0];
		break;
	case 0 : return c;
	}

	final(a,b,c);
	return c;
}

// http://www.burtleburtle.net/bob/hash/doobs.html

SG_uint32       sg_vhash__calc(const SG_vhash* pHash,  const char* key)
{
	SG_uint32 hash = sg_vhash__hashlittle((SG_uint8*)key, strlen(key));
	hash &= pHash->bucket_mask;
    return hash;
}

void sg_vhash__grow_keys_if_needed(SG_context* pCtx, SG_vhash* pvh)
{
	if ((pvh->count + 1) > pvh->space_keys)
	{
		SG_uint32 new_space_keys = pvh->space_keys * 2;
		const char** new_aKeys;

		SG_ERR_CHECK_RETURN(  SG_alloc(pCtx, new_space_keys, sizeof(char*), &new_aKeys)  );

		memcpy((char **)new_aKeys, pvh->aKeys, pvh->count * sizeof(char*));
		SG_NULLFREE(pCtx, pvh->aKeys);
		pvh->aKeys = new_aKeys;
		pvh->space_keys = new_space_keys;
	}
}

void sg_vhash__add(SG_context* pCtx, SG_vhash* pHash, sg_hashitem* pv)
{
	SG_int32 iBucket;

	SG_ERR_CHECK_RETURN(  sg_vhash__grow_keys_if_needed(pCtx, pHash)  );

	pHash->aKeys[pHash->count] = pv->key;

	iBucket = sg_vhash__calc(pHash, pv->key);

	if (!(pHash->aBuckets[iBucket]))
	{
		pHash->aBuckets[iBucket] = pv;
		pHash->count++;
		return;
	}
	else
	{
		sg_hashitem* pCur = pHash->aBuckets[iBucket];
		sg_hashitem* pPrev = NULL;

		while (pCur)
		{
			int cmp = strcmp(pv->key, pCur->key);
			if (cmp == 0)
			{
				SG_ERR_THROW_RETURN(  SG_ERR_VHASH_DUPLICATEKEY  );
			}
			else if (cmp < 0)
			{
				// put pv before this one
				if (pPrev)
				{
					pPrev->pNext = pv;
				}
				else
				{
					pHash->aBuckets[iBucket] = pv;
				}

				pv->pNext = pCur;

				pHash->count++;

				return;
			}
			else
			{
				pPrev = pCur;
				pCur = pCur->pNext;
			}
		}
		pPrev->pNext = pv;

		pHash->count++;
	}
}

void sg_vhash__new_hashitem(SG_context* pCtx, SG_vhash* pvh, const char* putf8Key, sg_hashitem** pp)
{
	sg_hashitem* pNew = NULL;

	SG_ERR_CHECK(  SG_alloc1(pCtx, pNew)  );

	SG_ERR_CHECK(  SG_strpool__add__sz(pCtx, pvh->pStrPool, putf8Key, &(pNew->key))  );

	SG_ERR_CHECK(  SG_varpool__add(pCtx, pvh->pVarPool, &(pNew->pv))  );

	*pp = pNew;

	return;

fail:
    SG_NULLFREE(pCtx, pNew);
}

/* Note that if the key is not found, this routine returns SG_ERR_OK and the resulting hashitem is set to NULL */
void sg_vhash__find(SG_UNUSED_PARAM(SG_context* pCtx), const SG_vhash* pHash, const char* putf8Key, sg_hashitem** ppResult, SG_uint32* piBucket, sg_hashitem** ppPrevItem)
{
	SG_uint32 iBucket = sg_vhash__calc(pHash, putf8Key);
	sg_hashitem* pPrev = NULL;
	sg_hashitem* pCur = pHash->aBuckets[iBucket];

	SG_UNUSED(pCtx);

	if (piBucket)
	{
		*piBucket = iBucket;
	}

	if (ppPrevItem)
	{
		*ppPrevItem = NULL;
	}

	while (pCur)
	{
		int cmp = strcmp(putf8Key, pCur->key);
		if (cmp == 0)
		{
			if (ppPrevItem)
			{
				*ppPrevItem = pPrev;
			}

			*ppResult = pCur;
			return;
		}
		else if (cmp < 0)
		{
			*ppResult = NULL;
			return;
		}
		else
		{
			pPrev = pCur;
			pCur = pCur->pNext;
		}
	}
	*ppResult = NULL;
}

void        SG_vhash__update__string__sz(SG_context* pCtx, SG_vhash* pHash, const char* putf8Key, const char* putf8Value)
{
	SG_ERR_CHECK_RETURN(  SG_vhash__update__string__buflen(pCtx, pHash, putf8Key, putf8Value, 0)  );
}

void        SG_vhash__update__string__buflen(SG_context* pCtx, SG_vhash* pHash, const char* putf8Key, const char* putf8Value, SG_uint32 len)
{
	sg_hashitem *pv = NULL;

	if (putf8Value)
	{
		SG_ERR_CHECK_RETURN(  sg_vhash__find(pCtx, pHash, putf8Key, &pv, NULL, NULL)  );
		if (!pv)
		{
			SG_ERR_CHECK_RETURN(  SG_vhash__add__string__sz(pCtx, pHash, putf8Key, putf8Value)  );
			return;
		}

		sg_hashitem__freecontents(pCtx, pv);

		SG_ERR_CHECK(  SG_varpool__add(pCtx, pHash->pVarPool, &(pv->pv))  );

		pv->pv->type = SG_VARIANT_TYPE_SZ;

		SG_ERR_CHECK(  SG_strpool__add__buflen__sz(pCtx, pHash->pStrPool, putf8Value, len, &(pv->pv->v.val_sz))  );

		return;
	}
	else
	{
		SG_ERR_CHECK_RETURN(  SG_vhash__update__null(pCtx, pHash, putf8Key)  );
		return;
	}

fail:
	return;
}

void        SG_vhash__update__int64(SG_context* pCtx, SG_vhash* pHash, const char* putf8Key, SG_int64 ival)
{
	sg_hashitem *pv = NULL;

	SG_ERR_CHECK_RETURN(  sg_vhash__find(pCtx, pHash, putf8Key, &pv, NULL, NULL)  );
	if (!pv)
	{
		SG_ERR_CHECK_RETURN(  SG_vhash__add__int64(pCtx, pHash, putf8Key, ival)  );
		return;
	}

	sg_hashitem__freecontents(pCtx, pv);

	SG_ERR_CHECK(  SG_varpool__add(pCtx, pHash->pVarPool, &(pv->pv))  );

	pv->pv->type = SG_VARIANT_TYPE_INT64;
	pv->pv->v.val_int64 = ival;

	return;

fail:
	return;
}

void        SG_vhash__update__double(SG_context* pCtx, SG_vhash* pHash, const char* putf8Key, double val)
{
	sg_hashitem *pv = NULL;

	SG_ERR_CHECK_RETURN(  sg_vhash__find(pCtx, pHash, putf8Key, &pv, NULL, NULL)  );
	if (!pv)
	{
		SG_ERR_CHECK_RETURN(  SG_vhash__add__double(pCtx, pHash, putf8Key, val)  );
		return;
	}

	sg_hashitem__freecontents(pCtx, pv);

	SG_ERR_CHECK(  SG_varpool__add(pCtx, pHash->pVarPool, &(pv->pv))  );

	pv->pv->type = SG_VARIANT_TYPE_DOUBLE;
	pv->pv->v.val_double = val;

	return;

fail:
	return;
}

void        SG_vhash__update__bool(SG_context* pCtx, SG_vhash* pHash, const char* putf8Key, SG_bool val)
{
	sg_hashitem *pv = NULL;

	SG_ERR_CHECK_RETURN(  sg_vhash__find(pCtx, pHash, putf8Key, &pv, NULL, NULL)  );
	if (!pv)
	{
		SG_ERR_CHECK_RETURN(  SG_vhash__add__bool(pCtx, pHash, putf8Key, val)  );
		return;
	}

	sg_hashitem__freecontents(pCtx, pv);

	SG_ERR_CHECK(  SG_varpool__add(pCtx, pHash->pVarPool, &(pv->pv))  );

	pv->pv->type = SG_VARIANT_TYPE_BOOL;
	pv->pv->v.val_bool = val;

	return;

fail:
	return;
}

void        SG_vhash__update__vhash(SG_context* pCtx, SG_vhash* pHash, const char* putf8Key, SG_vhash** ppvh_val)
{
	sg_hashitem *pv = NULL;

	if (ppvh_val!=NULL && *ppvh_val!=NULL)
	{
		SG_ERR_CHECK_RETURN(  sg_vhash__find(pCtx, pHash, putf8Key, &pv, NULL, NULL)  );
		if (!pv)
		{
			SG_ERR_CHECK_RETURN(  SG_vhash__add__vhash(pCtx, pHash, putf8Key, ppvh_val)  );
			return;
		}

		sg_hashitem__freecontents(pCtx, pv);

		SG_ERR_CHECK(  SG_varpool__add(pCtx, pHash->pVarPool, &(pv->pv))  );

		pv->pv->type = SG_VARIANT_TYPE_VHASH;
		pv->pv->v.val_vhash = *ppvh_val;
        *ppvh_val = NULL;

		return;
	}
	else
	{
		SG_ERR_CHECK_RETURN(  SG_vhash__update__null(pCtx, pHash, putf8Key)  );
		return;
	}

fail:
	return;
}

void        SG_vhash__update__varray(SG_context* pCtx, SG_vhash* pHash, const char* putf8Key, SG_varray** ppva_val)
{
	sg_hashitem *pv = NULL;

	if (ppva_val!=NULL && *ppva_val!=NULL)
	{
		SG_ERR_CHECK_RETURN(  sg_vhash__find(pCtx, pHash, putf8Key, &pv, NULL, NULL)  );
		if (!pv)
		{
			SG_ERR_CHECK_RETURN(  SG_vhash__add__varray(pCtx, pHash, putf8Key, ppva_val)  );
			return;
		}

		sg_hashitem__freecontents(pCtx, pv);

		SG_ERR_CHECK(  SG_varpool__add(pCtx, pHash->pVarPool, &(pv->pv))  );

		pv->pv->type = SG_VARIANT_TYPE_VARRAY;
		pv->pv->v.val_varray = *ppva_val;
		*ppva_val = NULL;
		return;
	}
	else
	{
		SG_ERR_CHECK_RETURN(  SG_vhash__update__null(pCtx, pHash, putf8Key)  );
		return;
	}

fail:
	return;
}

void        SG_vhash__update__null(SG_context* pCtx, SG_vhash* pHash, const char* putf8Key)
{
	sg_hashitem *pv = NULL;

	SG_ERR_CHECK_RETURN(  sg_vhash__find(pCtx, pHash, putf8Key, &pv, NULL, NULL)  );
	if (!pv)
	{
		SG_ERR_CHECK_RETURN(  SG_vhash__add__null(pCtx, pHash, putf8Key)  );
		return;
	}

	sg_hashitem__freecontents(pCtx, pv);

	SG_ERR_CHECK(  SG_varpool__add(pCtx, pHash->pVarPool, &(pv->pv))  );

	pv->pv->type = SG_VARIANT_TYPE_NULL;

	return;

fail:
	return;
}

void        SG_vhash__add__string__sz(SG_context* pCtx, SG_vhash* pHash, const char* putf8Key, const char* putf8Value)
{
	SG_ERR_CHECK_RETURN(  SG_vhash__add__string__buflen(pCtx, pHash, putf8Key, putf8Value, 0)  );
}

void        SG_vhash__add__string__buflen(SG_context* pCtx, SG_vhash* pHash, const char* putf8Key, const char* putf8Value, SG_uint32 len)
{
	sg_hashitem* pNew = NULL;

	SG_NULLARGCHECK_RETURN(putf8Key);
	SG_ARGCHECK_RETURN(*putf8Key != 0, putf8Key);

	if (putf8Value)
	{
		SG_ERR_CHECK(  sg_vhash__new_hashitem(pCtx, pHash, putf8Key, &pNew)  );

		pNew->pv->type = SG_VARIANT_TYPE_SZ;

		SG_ERR_CHECK(  SG_strpool__add__buflen__sz(pCtx, pHash->pStrPool, putf8Value, len, &(pNew->pv->v.val_sz))  );

		SG_ERR_CHECK(  sg_vhash__add(pCtx, pHash, pNew)  );

		return;
	}
	else
	{
		SG_ERR_CHECK_RETURN(  SG_vhash__add__null(pCtx, pHash, putf8Key)  );
		return;
	}

fail:
    SG_NULLFREE(pCtx, pNew);
}

void SG_vhash__addtoval__int64(
    SG_context* pCtx,
	SG_vhash* pHash,
	const char* putf8Key,
    SG_int64 addend
	)
{
	sg_hashitem *pv = NULL;

	SG_ERR_CHECK(  sg_vhash__find(pCtx, pHash, putf8Key, &pv, NULL, NULL)  );
	if (!pv)
	{
		SG_ERR_CHECK(  SG_vhash__add__int64(pCtx, pHash, putf8Key, addend)  );
		return;
	}

    // TODO verify that this is legit
    if (SG_VARIANT_TYPE_INT64 == pv->pv->type)
    {
        pv->pv->v.val_int64 += addend;
    }
    else
    {
        SG_ERR_THROW(  SG_ERR_VARIANT_INVALIDTYPE  );
    }

fail:
    ;
}

void        SG_vhash__add__int64(SG_context* pCtx, SG_vhash* pHash, const char* putf8Key, SG_int64 ival)
{
	sg_hashitem* pNew = NULL;

	SG_ERR_CHECK(  sg_vhash__new_hashitem(pCtx, pHash, putf8Key, &pNew)  );

	pNew->pv->type = SG_VARIANT_TYPE_INT64;
	pNew->pv->v.val_int64 = ival;

	SG_ERR_CHECK(  sg_vhash__add(pCtx, pHash, pNew)  );

	return;

fail:
    SG_NULLFREE(pCtx, pNew);
}

void        SG_vhash__add__double(SG_context* pCtx, SG_vhash* pHash, const char* putf8Key, double fv)
{
	sg_hashitem* pNew = NULL;

	SG_ERR_CHECK(  sg_vhash__new_hashitem(pCtx, pHash, putf8Key, &pNew)  );

	pNew->pv->type = SG_VARIANT_TYPE_DOUBLE;
	pNew->pv->v.val_double = fv;

	SG_ERR_CHECK(  sg_vhash__add(pCtx, pHash, pNew)  );

	return;

fail:
    SG_NULLFREE(pCtx, pNew);
}

void        SG_vhash__add__bool(SG_context* pCtx, SG_vhash* pHash, const char* putf8Key, SG_bool b)
{
	sg_hashitem* pNew = NULL;

	SG_ERR_CHECK(  sg_vhash__new_hashitem(pCtx, pHash, putf8Key, &pNew)  );

	pNew->pv->type = SG_VARIANT_TYPE_BOOL;
	pNew->pv->v.val_bool = b;

	SG_ERR_CHECK(  sg_vhash__add(pCtx, pHash, pNew)  );

	return;

fail:
    SG_NULLFREE(pCtx, pNew);
}

void SG_vhash__addnew__varray(
    SG_context* pCtx,
	SG_vhash* pvh,
	const char* putf8Key,
	SG_varray** pResult
	)
{
    SG_varray* pva_sub = NULL;
    SG_varray* pva_sub_ref = NULL;

    SG_ERR_CHECK(  SG_varray__alloc__params(pCtx, &pva_sub, 4, pvh->pStrPool, pvh->pVarPool)  );
    pva_sub_ref = pva_sub;
    SG_ERR_CHECK(  SG_vhash__add__varray(pCtx, pvh, putf8Key, &pva_sub)  );
    *pResult = pva_sub_ref;

fail:
    SG_VARRAY_NULLFREE(pCtx, pva_sub);
}

void SG_vhash__addnew__vhash(
    SG_context* pCtx,
	SG_vhash* pvh,
	const char* putf8Key,
	SG_vhash** pResult
	)
{
    SG_vhash* pvh_sub = NULL;
    SG_vhash* pvh_sub_ref = NULL;

    SG_ERR_CHECK(  SG_vhash__alloc__params(pCtx, &pvh_sub, 4, pvh->pStrPool, pvh->pVarPool)  );
    pvh_sub_ref = pvh_sub;
    SG_ERR_CHECK(  SG_vhash__add__vhash(pCtx, pvh, putf8Key, &pvh_sub)  );
    if (pResult)
    {
        *pResult = pvh_sub_ref;
    }

fail:
    SG_VHASH_NULLFREE(pCtx, pvh_sub);
}

void        SG_vhash__add__vhash(SG_context* pCtx, SG_vhash* pHash, const char* putf8Key, SG_vhash** ppvh_val)
{
	sg_hashitem* pNew = NULL;

	if (ppvh_val!=NULL && *ppvh_val!=NULL)
	{
		SG_ERR_CHECK(  sg_vhash__new_hashitem(pCtx, pHash, putf8Key, &pNew)  );
		SG_ERR_CHECK(  sg_vhash__add(pCtx, pHash, pNew)  );

		pNew->pv->type = SG_VARIANT_TYPE_VHASH;
		pNew->pv->v.val_vhash = *ppvh_val;
        *ppvh_val = NULL;
		return;
	}
	else
	{
		SG_ERR_CHECK_RETURN(  SG_vhash__add__null(pCtx, pHash, putf8Key)  );
		return;
	}

fail:
    SG_NULLFREE(pCtx, pNew);
}

void        SG_vhash__add__varray(SG_context* pCtx, SG_vhash* pHash, const char* putf8Key, SG_varray** ppva_val)
{
	sg_hashitem* pNew = NULL;

	if (ppva_val!=NULL && *ppva_val!=NULL)
	{
		SG_ERR_CHECK(  sg_vhash__new_hashitem(pCtx, pHash, putf8Key, &pNew)  );
		SG_ERR_CHECK(  sg_vhash__add(pCtx, pHash, pNew)  );

		pNew->pv->type = SG_VARIANT_TYPE_VARRAY;
		pNew->pv->v.val_varray = *ppva_val;
        *ppva_val = NULL;
		return;
	}
	else
	{
		SG_ERR_CHECK_RETURN(  SG_vhash__add__null(pCtx, pHash, putf8Key)  );
		return;
	}

fail:
    SG_NULLFREE(pCtx, pNew);
}

void        SG_vhash__add__null(SG_context* pCtx, SG_vhash* pHash, const char* putf8Key)
{
	sg_hashitem* pNew = NULL;

	SG_ERR_CHECK(  sg_vhash__new_hashitem(pCtx, pHash, putf8Key, &pNew)  );

	pNew->pv->type = SG_VARIANT_TYPE_NULL;

	SG_ERR_CHECK(  sg_vhash__add(pCtx, pHash, pNew)  );

	return;

fail:
    SG_NULLFREE(pCtx, pNew);
}

#define SG_VHASH_GET_PRELUDE													\
	sg_hashitem *pv = NULL;														\
																				\
	SG_NULLARGCHECK_RETURN(pvh);														\
	SG_NULL_PP_CHECK_RETURN(putf8Key);													\
	SG_ERR_CHECK_RETURN(sg_vhash__find(pCtx, pvh, putf8Key, &pv, NULL, NULL));	\
	if (!pv)																	\
	{																			\
		SG_ERR_THROW_RETURN(  SG_ERR_VHASH_KEYNOTFOUND  );						\
	}


void        SG_vhash__get__sz(SG_context* pCtx, const SG_vhash* pvh, const char* putf8Key, const char** pputf8Value)
{
	SG_VHASH_GET_PRELUDE;

	SG_ERR_CHECK_RETURN(  SG_variant__get__sz(pCtx, pv->pv, pputf8Value)  );
}

void        SG_vhash__get__int64(SG_context* pCtx, const SG_vhash* pvh, const char* putf8Key, SG_int64* pResult)
{
	SG_VHASH_GET_PRELUDE;

	SG_ERR_CHECK_RETURN(  SG_variant__get__int64(pCtx, pv->pv, pResult)  );
}

void        SG_vhash__get__int64_or_double(SG_context* pCtx, const SG_vhash* pvh, const char* putf8Key, SG_int64* pResult)
{
	SG_VHASH_GET_PRELUDE;

	SG_ERR_CHECK_RETURN(  SG_variant__get__int64_or_double(pCtx, pv->pv, pResult)  );
}

void        SG_vhash__get__uint32(
    SG_context* pCtx,
	const SG_vhash* pvh,
	const char* putf8Key,
	SG_uint32* pResult
	)
{
    SG_int64 i64 = 0;
    SG_ERR_CHECK(  SG_vhash__get__int64(pCtx, pvh, putf8Key, &i64)  );
    if (SG_int64__fits_in_uint32(i64))
    {
        *pResult = (SG_uint32) i64;
        return;
    }
    else
    {
        SG_ERR_THROW_RETURN(  SG_ERR_INTEGER_OVERFLOW  );
    }

fail:
    return;
}

void        SG_vhash__get__double(SG_context* pCtx, const SG_vhash* pvh, const char* putf8Key, double* pResult)
{
	SG_VHASH_GET_PRELUDE;

	SG_ERR_CHECK_RETURN(  SG_variant__get__double(pCtx, pv->pv, pResult)  );
}

void        SG_vhash__get__bool(SG_context* pCtx, const SG_vhash* pvh, const char* putf8Key, SG_bool* pResult)
{
	SG_VHASH_GET_PRELUDE;

	SG_ERR_CHECK_RETURN(  SG_variant__get__bool(pCtx, pv->pv, pResult)  );
}

void        SG_vhash__get__vhash(SG_context* pCtx, const SG_vhash* pvh, const char* putf8Key, SG_vhash** ppResult)
{
	SG_VHASH_GET_PRELUDE;

	SG_ERR_CHECK_RETURN(  SG_variant__get__vhash(pCtx, pv->pv, ppResult)  );
}

void        SG_vhash__get__varray(SG_context* pCtx, const SG_vhash* pvh, const char* putf8Key, SG_varray** ppResult)
{
	SG_VHASH_GET_PRELUDE;

	SG_ERR_CHECK_RETURN(  SG_variant__get__varray(pCtx, pv->pv, ppResult)  );
}

void        SG_vhash__has(SG_context* pCtx, const SG_vhash* pHash, const char* putf8Key, SG_bool* pbResult)
{
	sg_hashitem *pv = NULL;

	SG_ERR_CHECK_RETURN(  sg_vhash__find(pCtx, pHash, putf8Key, &pv, NULL, NULL)  );

	*pbResult = (pv != NULL);
}

void        SG_vhash__check__sz(SG_context* pCtx, const SG_vhash* pvh, const char* putf8Key, SG_bool* pb, const char** pputf8Value)
{
	sg_hashitem *pv = NULL;

	SG_ERR_CHECK_RETURN(  sg_vhash__find(pCtx, pvh, putf8Key, &pv, NULL, NULL)  );
    if (!pv)
    {
        *pb = SG_FALSE;
    }
    else
    {
        *pb = SG_TRUE;
        SG_ERR_CHECK_RETURN(  SG_variant__get__sz(pCtx, pv->pv, pputf8Value)  );
    }
}

void        SG_vhash__check__vhash(SG_context* pCtx, const SG_vhash* pvh, const char* putf8Key, SG_bool* pb, SG_vhash** ppvh)
{
	sg_hashitem *pv = NULL;

	SG_ERR_CHECK_RETURN(  sg_vhash__find(pCtx, pvh, putf8Key, &pv, NULL, NULL)  );
    if (!pv)
    {
        *pb = SG_FALSE;
    }
    else
    {
        *pb = SG_TRUE;
        SG_ERR_CHECK_RETURN(  SG_variant__get__vhash(pCtx, pv->pv, ppvh)  );
    }
}

void        SG_vhash__check__varray(SG_context* pCtx, const SG_vhash* pvh, const char* putf8Key, SG_bool* pb, SG_varray** ppva)
{
	sg_hashitem *pv = NULL;

	SG_ERR_CHECK_RETURN(  sg_vhash__find(pCtx, pvh, putf8Key, &pv, NULL, NULL)  );
    if (!pv)
    {
        *pb = SG_FALSE;
    }
    else
    {
        *pb = SG_TRUE;
        SG_ERR_CHECK_RETURN(  SG_variant__get__varray(pCtx, pv->pv, ppva)  );
    }
}

void        SG_vhash__key(SG_context* pCtx, const SG_vhash* pvh, const char* putf8Key, const char** pp)
{
	sg_hashitem *pv = NULL;

	SG_ERR_CHECK_RETURN(  sg_vhash__find(pCtx, pvh, putf8Key, &pv, NULL, NULL)  );

    if (pv)
    {
        *pp = pv->key;
    }
    else
    {
        *pp = NULL;
    }
}

void        SG_vhash__remove(SG_context* pCtx, SG_vhash* pHash, const char* putf8Key)
{
	sg_hashitem *pv = NULL;
	sg_hashitem *prev = NULL;
	SG_uint32 iBucket;
	SG_uint32 i;
	SG_uint32 key_ndx = pHash->count+1;

	SG_ERR_CHECK_RETURN(  sg_vhash__find(pCtx, pHash, putf8Key, &pv, &iBucket, &prev)  );

	for (i=0; i<pHash->count; i++)
	{
		if (0 == strcmp(putf8Key, pHash->aKeys[i]))
		{
			key_ndx = i;
			break;
		}
	}

	if (key_ndx == (pHash->count+1))
	{
		SG_ERR_THROW_RETURN(  SG_ERR_VHASH_KEYNOTFOUND  );
	}

	memmove((char**)&(pHash->aKeys[key_ndx]), &(pHash->aKeys[key_ndx+1]), (pHash->space_keys - key_ndx - 1) * sizeof(char*));

	if (prev)
	{
		prev->pNext = pv->pNext;
	}
	else
	{
		pHash->aBuckets[iBucket] = pv->pNext;
	}
	pv->pNext = NULL;	// sg_hashitem__free() recursively deletes the forward chain.

	pHash->count--;

	sg_hashitem__free(pCtx, pv);
}

void        SG_vhash__typeof(SG_context* pCtx, const SG_vhash* pHash, const char* putf8Key, SG_uint16* pResult)
{
	sg_hashitem *pv = NULL;

	SG_ERR_CHECK_RETURN(  sg_vhash__find(pCtx, pHash, putf8Key, &pv, NULL, NULL)  );

	if (!pv)
	{
		*pResult = 0;
		SG_ERR_THROW_RETURN(  SG_ERR_VHASH_KEYNOTFOUND  );
	}

	*pResult = pv->pv->type;
}

struct sg_vhash_stackentry
{
	SG_vhash* pvh;
	SG_varray* pva;
	char hkey[256];
	struct sg_vhash_stackentry* pNext;
};

struct sg_vhash_json_context
{
	struct sg_vhash_stackentry* ptop;
	SG_vhash* pvhResult;
	SG_strpool* pStrPool;
	SG_varpool* pVarPool;
};

static void sg_vhash__json_cb__object_begin(SG_context* pCtx, struct sg_vhash_json_context * p)
{
	struct sg_vhash_stackentry* pse = NULL;
	SG_vhash* pvh = NULL;

	/*
	  All vhash/varray objects created during a json parse
	  will share the pools from the top vhash
	*/

	// TODO change the following line to pass in a better guess

	SG_ERR_CHECK(  SG_VHASH__ALLOC__PARAMS(pCtx, &pvh, 10, p->pStrPool, p->pVarPool)  );

    /* If the ctx has no pool pointers, then this is the top object, so
     * remember them for later */

	if (!p->pStrPool)
	{
		p->pStrPool = pvh->pStrPool;
	}

	if (!p->pVarPool)
	{
		p->pVarPool = pvh->pVarPool;
	}

	SG_ERR_CHECK(  SG_alloc1(pCtx, pse)  );

	pse->pvh = pvh;
	pse->pNext = p->ptop;

	if (p->ptop)
	{
		if (p->ptop->pva)
		{
			SG_ERR_CHECK(  SG_varray__append__vhash(pCtx, p->ptop->pva, &pvh)  );
			p->ptop = pse;
		}
		else
		{
			if (p->ptop->hkey[0])
			{
				SG_ERR_CHECK(  SG_vhash__add__vhash(pCtx, p->ptop->pvh, p->ptop->hkey, &pvh)  );
				p->ptop = pse;
			}
			else
			{
				SG_ERR_THROW(  SG_ERR_JSONPARSER_SYNTAX  );
			}
		}
	}
	else
	{
		p->ptop = pse;
        p->pvhResult = pvh;
	}

	return;

fail:
	if (pse)
	{
		SG_NULLFREE(pCtx, pse);
	}

	if (pvh)
	{
		SG_VHASH_NULLFREE(pCtx, pvh);
	}
}

static void sg_vhash__json_cb__array_begin(SG_context* pCtx, struct sg_vhash_json_context * p)
{
	struct sg_vhash_stackentry* pse = NULL;
	SG_varray* pva = NULL;

	// TODO change the following line to pass in a better guess

	SG_ERR_CHECK(  SG_VARRAY__ALLOC__PARAMS(pCtx, &pva, 10, p->pStrPool, p->pVarPool)  );

	SG_ERR_CHECK(  SG_alloc1(pCtx, pse)  );

	pse->pva = pva;
	pse->pNext = p->ptop;

	if (p->ptop)
	{
		if (p->ptop->pva)
		{
			SG_ERR_CHECK(  SG_varray__append__varray(pCtx, p->ptop->pva, &pva)  );
			p->ptop = pse;
		}
		else
		{
			if (p->ptop->hkey[0])
			{
				SG_ERR_CHECK(  SG_vhash__add__varray(pCtx, p->ptop->pvh, p->ptop->hkey, &pva)  );
				p->ptop = pse;
			}
			else
			{
				SG_ERR_THROW(  SG_ERR_JSONPARSER_SYNTAX  );
			}
		}
	}
	else
	{
		SG_ERR_THROW(  SG_ERR_JSONPARSER_SYNTAX  );
	}

	return;

fail:
	if (pse)
	{
		SG_NULLFREE(pCtx, pse);
	}

	if (pva)
	{
		SG_VARRAY_NULLFREE(pCtx, pva);
	}
}

void sg_vhash__json_cb(SG_context* pCtx, void* ctx, int type, const SG_jsonparser_value* value)
{
	struct sg_vhash_json_context* p = (struct sg_vhash_json_context*) ctx;

    switch(type)
	{
	case SG_JSONPARSER_TYPE_OBJECT_BEGIN:
		{
			SG_ERR_CHECK(  sg_vhash__json_cb__object_begin(pCtx, p)  );
			break;
		}
    case SG_JSONPARSER_TYPE_ARRAY_BEGIN:
		{
			SG_ERR_CHECK(  sg_vhash__json_cb__array_begin(pCtx, p)  );
			break;
		}
    case SG_JSONPARSER_TYPE_ARRAY_END:
		{
			if (p->ptop)
			{
				if (p->ptop->pva)
				{
					struct sg_vhash_stackentry* pse = p->ptop;
					p->ptop = pse->pNext;
					SG_NULLFREE(pCtx, pse);

					break;
				}
				else
				{
					SG_ERR_THROW(  SG_ERR_JSONPARSER_SYNTAX  );
				}
			}
			else
			{
				SG_ERR_THROW(  SG_ERR_JSONPARSER_SYNTAX  );
			}
		}
    case SG_JSONPARSER_TYPE_OBJECT_END:
		{
			if (p->ptop)
			{
				if (p->ptop->pvh)
				{
					struct sg_vhash_stackentry* pse = p->ptop;
					p->ptop = pse->pNext;

					SG_NULLFREE(pCtx, pse);

					break;
				}
				else
				{
					SG_ERR_THROW(  SG_ERR_JSONPARSER_SYNTAX  );
				}
			}
			else
			{
				SG_ERR_THROW(  SG_ERR_JSONPARSER_SYNTAX  );
			}
		}
    case SG_JSONPARSER_TYPE_INTEGER:
		{
			if (p->ptop)
			{
				if (p->ptop->pva)
				{
					SG_ERR_CHECK_RETURN(  SG_varray__append__int64(pCtx, p->ptop->pva, value->vu.integer_value)  );
				}
				else
				{
					if (p->ptop->hkey[0])
					{
						SG_ERR_CHECK_RETURN(  SG_vhash__add__int64(pCtx, p->ptop->pvh, p->ptop->hkey, value->vu.integer_value)  );
					}
					else
					{
						SG_ERR_THROW(  SG_ERR_JSONPARSER_SYNTAX  );
					}
				}
			}
			else
			{
				SG_ERR_THROW(  SG_ERR_JSONPARSER_SYNTAX  );
			}

			break;
		}
    case SG_JSONPARSER_TYPE_FLOAT:
		{
			if (p->ptop)
			{
				if (p->ptop->pva)
				{
					SG_ERR_CHECK_RETURN(  SG_varray__append__double(pCtx, p->ptop->pva, (double) value->vu.float_value)  );
				}
				else
				{
					if (p->ptop->hkey[0])
					{
						SG_ERR_CHECK_RETURN(  SG_vhash__add__double(pCtx, p->ptop->pvh, p->ptop->hkey, (double) value->vu.float_value)  );
					}
					else
					{
						SG_ERR_THROW(  SG_ERR_JSONPARSER_SYNTAX  );
					}
				}
			}
			else
			{
				SG_ERR_THROW(  SG_ERR_JSONPARSER_SYNTAX  );
			}
			break;
		}
    case SG_JSONPARSER_TYPE_NULL:
		{
			if (p->ptop)
			{
				if (p->ptop->pva)
				{
					SG_ERR_CHECK_RETURN(  SG_varray__append__null(pCtx, p->ptop->pva)  );
				}
				else
				{
					if (p->ptop->hkey[0])
					{
						SG_ERR_CHECK_RETURN(  SG_vhash__add__null(pCtx, p->ptop->pvh, p->ptop->hkey)  );
					}
					else
					{
						SG_ERR_THROW(  SG_ERR_JSONPARSER_SYNTAX  );
					}
				}
			}
			else
			{
				SG_ERR_THROW(  SG_ERR_JSONPARSER_SYNTAX  );
			}
			break;
		}
    case SG_JSONPARSER_TYPE_TRUE:
		{
			if (p->ptop)
			{
				if (p->ptop->pva)
				{
					SG_ERR_CHECK_RETURN(  SG_varray__append__bool(pCtx, p->ptop->pva, SG_TRUE)  );
				}
				else
				{
					if (p->ptop->hkey[0])
					{
						SG_ERR_CHECK_RETURN(  SG_vhash__add__bool(pCtx, p->ptop->pvh, p->ptop->hkey, SG_TRUE)  );
					}
					else
					{
						SG_ERR_THROW(  SG_ERR_JSONPARSER_SYNTAX  );
					}
				}
			}
			else
			{
				SG_ERR_THROW(  SG_ERR_JSONPARSER_SYNTAX  );
			}
			break;
		}
    case SG_JSONPARSER_TYPE_FALSE:
		{
			if (p->ptop)
			{
				if (p->ptop->pva)
				{
					SG_ERR_CHECK_RETURN(  SG_varray__append__bool(pCtx, p->ptop->pva, SG_FALSE)  );
				}
				else
				{
					if (p->ptop->hkey[0])
					{
						SG_ERR_CHECK_RETURN(  SG_vhash__add__bool(pCtx, p->ptop->pvh, p->ptop->hkey, SG_FALSE)  );
					}
					else
					{
						SG_ERR_THROW(  SG_ERR_JSONPARSER_SYNTAX  );
					}
				}
			}
			else
			{
				SG_ERR_THROW(  SG_ERR_JSONPARSER_SYNTAX  );
			}
			break;
		}
    case SG_JSONPARSER_TYPE_KEY:
		{
			if (p->ptop)
			{
				if (p->ptop->pva)
				{
					SG_ERR_THROW(  SG_ERR_JSONPARSER_SYNTAX  );
				}
				else
				{
					if (p->ptop->hkey[0])
					{
						SG_ERR_THROW(  SG_ERR_JSONPARSER_SYNTAX  );
					}
					else
					{
						SG_ERR_CHECK_RETURN(  SG_strcpy(pCtx, p->ptop->hkey, sizeof(p->ptop->hkey), value->vu.str.value)  );
					}
				}
			}
			else
			{
				SG_ERR_THROW(  SG_ERR_JSONPARSER_SYNTAX  );
			}
			break;
		}
    case SG_JSONPARSER_TYPE_STRING:
		{
			if (p->ptop)
			{
				if (p->ptop->pva)
				{
					SG_ERR_CHECK_RETURN(  SG_varray__append__string__sz(pCtx, p->ptop->pva, value->vu.str.value)  );
				}
				else
				{
					if (p->ptop->hkey[0])
					{
						SG_ERR_CHECK_RETURN(  SG_vhash__add__string__sz(pCtx, p->ptop->pvh, p->ptop->hkey, value->vu.str.value)  );
					}
					else
					{
						SG_ERR_THROW(  SG_ERR_JSONPARSER_SYNTAX  );
					}
				}
			}
			else
			{
				SG_ERR_THROW(  SG_ERR_JSONPARSER_SYNTAX  );
			}
			break;
		}
	default:
		SG_ERR_THROW(  SG_ERR_JSONPARSER_SYNTAX  );
    }

	if (type != SG_JSONPARSER_TYPE_KEY)
	{
		if (p->ptop)
		{
			p->ptop->hkey[0] = 0;
		}
	}

fail:
	return;
}

#if 0
/* TODO this is my original version of parsing a json
 * object into a vhash.  The jsonparser code has its
 * own stuff to handle utf8 (and other encodings), but
 * I wasn't sure I trusted it.  So I converted everything
 * to utf32 before calling it.  Unfortunately, and somewhat
 * predictably, this has a performance impact.  So I have
 * replaced this function with one below it that simply
 * passes utf8 directly into the jsonparser.  This means
 * we need to do more testing of the jsonparser code
 * with funky utf8 json objects. */

void SG_vhash__alloc__from_json(SG_context* pCtx, SG_vhash** pResult, const char* pszJson)
{
	SG_int32* putf32 = NULL;
	SG_uint32 len = 0;
	SG_jsonparser* jc = NULL;
	SG_uint32 i;
	struct sg_vhash_json_context ctx;

	ctx.ptop = NULL;
	ctx.pvhResult = NULL;
	ctx.pStrPool = NULL;
	ctx.pVarPool = NULL;

	SG_ERR_CHECK(  SG_jsonparser__alloc(pCtx, &jc, sg_vhash__json_cb, &ctx)  );

	/*
	 * TODO do we have to convert this to utf32 ?
	 * The jsonparser code claimed to be able to
	 * handle utf8 data stuffed in one byte at
	 * a time.  I'm just not sure I trust it.
	 */

	SG_utf8__to_utf32__sz(pszJson, &putf32, &len);
	for (i=0; i<len; i++)
	{
		SG_ERR_CHECK(  SG_jsonparser__char(pCtx, jc, putf32[i])  );
    }
	if (!SG_context__has_err(pCtx))
	{
		SG_ERR_CHECK(  SG_jsonparser__done(pCtx, jc)  );
	}

	SG_JSONPARSER_NULLFREE(pCtx, jc);

	SG_NULLFREE(pCtx, putf32);

	*pResult = ctx.pvhResult;

fail:
	return;
}

#else

void SG_vhash__alloc__from_json(SG_context* pCtx, SG_vhash** pResult, const char* pszJson)
{
	SG_jsonparser* jc = NULL;
	struct sg_vhash_json_context ctx;

	ctx.ptop = NULL;
	ctx.pvhResult = NULL;
	ctx.pStrPool = NULL;
	ctx.pVarPool = NULL;

	SG_ERR_CHECK(  SG_jsonparser__alloc(pCtx, &jc, sg_vhash__json_cb, &ctx)  );

    /* This version of this function trusts the jsonparser to
     * deal with utf8 itself. */

    SG_ERR_CHECK(  SG_jsonparser__chars(pCtx, jc, pszJson)  );

	if (!SG_context__has_err(pCtx))
	{
		SG_ERR_CHECK(  SG_jsonparser__done(pCtx, jc)  );
	}

	SG_JSONPARSER_NULLFREE(pCtx, jc);

	*pResult = ctx.pvhResult;

	return;

fail:
	SG_JSONPARSER_NULLFREE(pCtx, jc);
	while (ctx.ptop)
	{
		struct sg_vhash_stackentry * pse = ctx.ptop;
		ctx.ptop = pse->pNext;
		SG_NULLFREE(pCtx, pse);
	}
	SG_VHASH_NULLFREE(pCtx, ctx.pvhResult);
}

#endif

void SG_vhash__foreach(SG_context* pCtx, const SG_vhash* pvh, SG_vhash_foreach_callback* cb, void* ctx)
{
	SG_uint32 i;

	SG_NULLARGCHECK_RETURN(pvh);
	SG_NULLARGCHECK_RETURN(cb);

	for (i=0; i<pvh->count; i++)
	{
		sg_hashitem *pv = NULL;

		SG_ERR_CHECK_RETURN(  sg_vhash__find(pCtx, pvh, pvh->aKeys[i], &pv, NULL, NULL)  );

		SG_ERR_CHECK_RETURN(  cb(pCtx, ctx, pvh, pvh->aKeys[i], pv->pv)  );
	}
}

void sg_vhash__write_json_callback(SG_context* pCtx, void* ctx, SG_UNUSED_PARAM(const SG_vhash* pvh), const char* putf8Key, const SG_variant* pv)
{
	SG_jsonwriter* pjson = (SG_jsonwriter*) ctx;

	SG_UNUSED(pvh);

    SG_ERR_CHECK_RETURN(  SG_jsonwriter__write_pair__variant(pCtx, pjson, putf8Key, pv)  );
}

void SG_vhash__write_json(SG_context* pCtx, const SG_vhash* pvh, SG_jsonwriter* pjson)
{
	SG_NULLARGCHECK_RETURN(pvh);
	SG_NULLARGCHECK_RETURN(pjson);

	SG_ERR_CHECK_RETURN(  SG_jsonwriter__write_start_object(pCtx, pjson)  );

	SG_ERR_CHECK_RETURN(  SG_vhash__foreach(pCtx, pvh, sg_vhash__write_json_callback, pjson)  );

	SG_ERR_CHECK_RETURN(  SG_jsonwriter__write_end_object(pCtx, pjson)  );
}

void       SG_vhash__to_json(SG_context* pCtx, const SG_vhash* pvh, SG_string* pStr)
{
	SG_jsonwriter* pjson = NULL;

	SG_NULLARGCHECK_RETURN(pvh);
	SG_NULLARGCHECK_RETURN(pStr);

	SG_ERR_CHECK(  SG_jsonwriter__alloc(pCtx, &pjson, pStr)  );

	SG_ERR_CHECK(  SG_vhash__write_json(pCtx, pvh, pjson)  );

fail:
    SG_JSONWRITER_NULLFREE(pCtx, pjson);
}

void SG_vhash__get__variant(SG_context* pCtx, const SG_vhash* pvh, const char* putf8Key, const SG_variant** ppResult)
{
	SG_VHASH_GET_PRELUDE;

	*ppResult = pv->pv;
}

void SG_vhash__equal(SG_context* pCtx, const SG_vhash* pv1, const SG_vhash* pv2, SG_bool* pbResult)
{
	SG_uint32 i;

	SG_NULLARGCHECK_RETURN(pv1);
	SG_NULLARGCHECK_RETURN(pv2);

	if (pv1 == pv2)
	{
		*pbResult = SG_TRUE;
		return;
	}

	if (pv1->count != pv2->count)
	{
		*pbResult = SG_FALSE;
		return;
	}

	for (i=0; i<pv1->count; i++)
	{
		sg_hashitem *pv = NULL;
		const SG_variant* v1 = NULL;
		const SG_variant* v2 = NULL;
		SG_bool b = SG_FALSE;

		SG_ERR_CHECK_RETURN(  sg_vhash__find(pCtx, pv1, pv1->aKeys[i], &pv, NULL, NULL)  );

		v1 = pv->pv;

		SG_vhash__get__variant(pCtx, pv2, pv1->aKeys[i], &v2);
		if (SG_context__err_equals(pCtx, SG_ERR_VHASH_KEYNOTFOUND))
		{
			SG_context__err_reset(pCtx);
			*pbResult = SG_FALSE;
			return;
		}
		SG_ERR_CHECK_RETURN_CURRENT;

		SG_ERR_CHECK_RETURN(  SG_variant__equal(pCtx, v1, v2, &b)  );

		if (!b)
		{
			*pbResult = SG_FALSE;
			return;
		}
	}

	*pbResult = SG_TRUE;
}

//////////////////////////////////////////////////////////////////
// This hack is needed to make VS2005 happy.  It didn't like
// arbitrary void * data pointers and function pointers mixing.

struct _recursive_sort_foreach_callback_data
{
	SG_qsort_compare_function * pfnCompare;
};

static void _sg_vhash__recursive_sort__foreach_callback(SG_context* pCtx,
															void * pVoidData,
															SG_UNUSED_PARAM(const SG_vhash * pvh),
															SG_UNUSED_PARAM(const char * szKey),
															const SG_variant * pvar)
{
	// iterate over all values in this vhash and sort the ones that are also vhashes.

	SG_vhash * pvhValue;
	struct _recursive_sort_foreach_callback_data * pData = (struct _recursive_sort_foreach_callback_data *)pVoidData;

	SG_UNUSED(pvh);
	SG_UNUSED(szKey);

	SG_variant__get__vhash(pCtx, pvar,&pvhValue);
	if (SG_context__has_err(pCtx) || !pvhValue)
	{
		SG_context__err_reset(pCtx);
		return;
	}

	SG_ERR_CHECK_RETURN(  SG_vhash__sort(pCtx,pvhValue,SG_TRUE,pData->pfnCompare)  );
}

int SG_vhash_sort_callback__increasing(SG_UNUSED_PARAM(SG_context * pCtx),
									   const void * pVoid_pszKey1, // const char ** pszKey1
									   const void * pVoid_pszKey2, // const char ** pszKey2
									   SG_UNUSED_PARAM(void * pVoidCallerData))
{
	const char ** pszKey1 = (const char **)pVoid_pszKey1;
	const char ** pszKey2 = (const char **)pVoid_pszKey2;

	SG_UNUSED(pCtx);
	SG_UNUSED(pVoidCallerData);

	// a simple increasing, case sensitive ordering
	return strcmp(*pszKey1,*pszKey2);
}

void SG_vhash__sort(SG_context* pCtx, SG_vhash * pvh, SG_bool bRecursive, SG_qsort_compare_function pfnCompare)
{
	// sort keys in the given vhash.
	// optionally, sort the values which are vhashes.

	SG_NULLARGCHECK_RETURN(pvh);
	SG_ARGCHECK_RETURN( (pfnCompare!=NULL), pfnCompare);

	SG_ERR_CHECK_RETURN(  SG_qsort(pCtx,
								   ((void *)pvh->aKeys),pvh->count,sizeof(char *),
								   pfnCompare,
								   NULL)  );

	if (!bRecursive)
		return;

	{
		// This hack is needed to make VS2005 happy.  It didn't like
		// arbitrary void * data pointers and function pointers mixing.

		struct _recursive_sort_foreach_callback_data data;
		data.pfnCompare = pfnCompare;

		SG_ERR_CHECK_RETURN(  SG_vhash__foreach(pCtx, pvh,
												_sg_vhash__recursive_sort__foreach_callback,
												&data)  );
	}
}

//////////////////////////////////////////////////////////////////

void SG_vhash__get_nth_pair(
	SG_context* pCtx,
	const SG_vhash* pThis,
	SG_uint32 n,
	const char** putf8Key,
	const SG_variant** ppResult
	)
{
	sg_hashitem *pv = NULL;

	if (n >= pThis->count)
	{
		SG_ERR_THROW_RETURN(  SG_ERR_ARGUMENT_OUT_OF_RANGE  );
	}

    if (ppResult)
    {
        SG_ERR_CHECK_RETURN(  sg_vhash__find(pCtx, pThis, pThis->aKeys[n], &pv, NULL, NULL)  );
    }

	if (putf8Key)
    {
		*putf8Key = pThis->aKeys[n];
    }

	if (ppResult)
    {
		*ppResult = pv->pv;
    }
}

void SG_vhash__get_keys(
	SG_context* pCtx,
	const SG_vhash* pvh,
	SG_varray** ppResult
	)
{
	SG_varray* pva = NULL;
	SG_uint32 i;

	SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pva)  );

	for (i=0; i<pvh->count; i++)
	{
		SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva, pvh->aKeys[i])  );
	}

	*ppResult = pva;

	return;

fail:
    SG_VARRAY_NULLFREE(pCtx, pva);
}

//////////////////////////////////////////////////////////////////

#if defined(DEBUG)
void SG_vhash_debug__dump(SG_context* pCtx, const SG_vhash * pvh, SG_string * pStrOut)
{
	SG_NULLARGCHECK_RETURN(pStrOut);

	if (pvh)
		SG_vhash__to_json(pCtx,pvh,pStrOut);
	else
		SG_string__append__sz(pCtx, pStrOut, "NULL");
}

/**
 * Dump a vhash to stderr.  You should be able to call this from GDB
 * or from the VS command window.
 */
void SG_vhash_debug__dump_to_console(SG_context* pCtx, const SG_vhash * pvh)
{
	SG_ERR_CHECK_RETURN(  SG_vhash_debug__dump_to_console__named(pCtx, pvh, NULL)  );
}

void SG_vhash_debug__dump_to_console__named(SG_context* pCtx, const SG_vhash * pvh, const char* pszName)
{
	SG_string * pStrOut = NULL;

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pStrOut)  );
	SG_ERR_CHECK(  SG_vhash_debug__dump(pCtx, pvh,pStrOut)  );
	if (!pszName)
		SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDERR,"VHash:\n%s\n",SG_string__sz(pStrOut))  );
	else
		SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDERR,"%s:\n%s\n",pszName,SG_string__sz(pStrOut))  );

	SG_ERR_CHECK(  SG_console__flush(pCtx, SG_CS_STDERR)  );

	// fall thru to common cleanup

fail:
	SG_STRING_NULLFREE(pCtx, pStrOut);
}

#endif

void SG_vhash__path__get__vhash(
    SG_context* pCtx,
	const SG_vhash* pvh,
    SG_vhash** ppvh_result,
    SG_bool* pb,
    ...
    )
{
    SG_vhash* pvh_cur = NULL;
    va_list ap;

    va_start(ap, pb);

    pvh_cur = (SG_vhash*) pvh;
    do
    {
        const char* psz_key = va_arg(ap, const char*);
        SG_vhash* pvh_temp = NULL;
        SG_bool b = SG_FALSE;

        if (!psz_key)
        {
            break;
        }

        SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh_cur, psz_key, &b)  );
        if (!b)
        {
            if (pb)
            {
                pvh_cur = NULL;
                break;
            }
            else
            {
                SG_ERR_THROW(  SG_ERR_NOT_FOUND  );
            }
        }

        SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvh_cur, psz_key, &pvh_temp)  );
        SG_ASSERT(pvh_temp);
        pvh_cur = pvh_temp;
    } while (1);

    if (pb)
    {
        *pb = pvh_cur ? SG_TRUE : SG_FALSE;
    }
    *ppvh_result = (SG_vhash*) pvh_cur;

    return;

fail:
    return;
}

static void SG_vhash__slashpath__parse(
    SG_context* pCtx,
	const char* psz_path,
    SG_varray** ppva
    )
{
    SG_varray* pva = NULL;
    const char* pcur = NULL;
    const char* pbegin = NULL;

    SG_NULLARGCHECK_RETURN(psz_path);

    SG_ERR_CHECK(  SG_varray__alloc(pCtx, &pva)  );

    pcur = psz_path;
    while (*pcur)
    {
        SG_uint32 len = 0;

        pbegin = pcur;
        while (*pcur && ('/' != *pcur))
        {
            pcur++;
        }

        len = pcur - pbegin;
        if (len)
        {
            SG_ERR_CHECK(  SG_varray__append__string__buflen(pCtx, pva, pbegin, len)  );
        }

        if ('/' == *pcur)
        {
            pcur++;
        }
    }

    *ppva = pva;
    pva = NULL;

fail:
    SG_VARRAY_NULLFREE(pCtx, pva);
}

static void SG_vhash__slashpath__walk(
    SG_context* pCtx,
	const SG_vhash* pvh,
	const char* psz_path,
    SG_bool b_create,
    SG_vhash** ppvh,
    const char** ppsz_leaf
    )
{
    SG_varray* pva = NULL;
    SG_vhash* pvh_cur = NULL;
    SG_uint32 count = 0;
    SG_uint32 i = 0;
    const char* psz_leaf = NULL;
    SG_uint32 len = 0;

    SG_NULLARGCHECK_RETURN(pvh);
	SG_NONEMPTYCHECK_RETURN(psz_path);
    SG_ARGCHECK_RETURN('/' != psz_path[0], psz_path);

    len = strlen(psz_path);
    SG_ARGCHECK_RETURN('/' != psz_path[len-1], psz_path);

    psz_leaf = strrchr(psz_path, '/');
    if (psz_leaf)
    {
        psz_leaf++;
    }
    else
    {
        psz_leaf = psz_path;
    }

    SG_ERR_CHECK(  SG_vhash__slashpath__parse(pCtx, psz_path, &pva)  );
    pvh_cur = (SG_vhash*) pvh;
    SG_ERR_CHECK(  SG_varray__count(pCtx, pva, &count)  );
    for (i=0; i<count; i++)
    {
        const char* psz = NULL;
        SG_vhash* pvh_next = NULL;

        SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pva, i, &psz)  );
        if (i == (count-1))
        {
            SG_ASSERT(0 == strcmp(psz_leaf, psz));
        }
        else
        {
            SG_bool b_has = SG_FALSE;

            SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh_cur, psz, &b_has)  );
            if (b_has)
            {
                SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvh_cur, psz, &pvh_next)  );
            }
            else
            {
                if (b_create)
                {
                    SG_ERR_CHECK(  SG_vhash__addnew__vhash(pCtx, pvh_cur, psz, &pvh_next)  );
                }
            }
        }
        if (pvh_next)
        {
            pvh_cur = pvh_next;
        }
        else
        {
            break;
        }
    }

    *ppvh = pvh_cur;
    *ppsz_leaf = psz_leaf;

fail:
    SG_VARRAY_NULLFREE(pCtx, pva);
}

void SG_vhash__slashpath__get__variant(
    SG_context* pCtx,
	const SG_vhash* pvh,
	const char* psz_path,
	const SG_variant** ppv
    )
{
    SG_vhash* pvh_container = NULL;
    const char* psz_leaf = NULL;
    const SG_variant* pv = NULL;

    SG_NULLARGCHECK_RETURN(ppv);

    SG_ERR_CHECK(  SG_vhash__slashpath__walk(
                pCtx,
                pvh,
                psz_path,
                SG_FALSE,
                &pvh_container,
                &psz_leaf
                )  );

    if (pvh_container && psz_leaf)
    {
        SG_bool b_has = SG_FALSE;

        SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh_container, psz_leaf, &b_has)  );
        if (b_has)
        {
            SG_ERR_CHECK(  SG_vhash__get__variant(pCtx, pvh_container, psz_leaf, &pv)  );
        }
    }

    *ppv = pv;

fail:
    ;
}

void SG_vhash__slashpath__update__string__sz(
	SG_context* pCtx,
	SG_vhash* pvh,
	const char* psz_path,
	const char* psz_val
	)
{
    SG_vhash* pvh_container = NULL;
    const char* psz_leaf = NULL;

    SG_NULLARGCHECK_RETURN(psz_val);

    SG_ERR_CHECK(  SG_vhash__slashpath__walk(
                pCtx,
                pvh,
                psz_path,
                SG_TRUE,
                &pvh_container,
                &psz_leaf
                )  );

    SG_ASSERT (pvh_container && psz_leaf);
    SG_ERR_CHECK(  SG_vhash__update__string__sz(pCtx, pvh_container, psz_leaf, psz_val)  );

fail:
    ;
}

void SG_vhash__slashpath__update__varray(
	SG_context* pCtx,
	SG_vhash* pvh,
	const char* psz_path,
    SG_varray** ppva
	)
{
    SG_vhash* pvh_container = NULL;
    const char* psz_leaf = NULL;

    SG_NULLARGCHECK_RETURN(ppva);
    SG_NULLARGCHECK_RETURN(*ppva);

    SG_ERR_CHECK(  SG_vhash__slashpath__walk(
                pCtx,
                pvh,
                psz_path,
                SG_TRUE,
                &pvh_container,
                &psz_leaf
                )  );

    SG_ASSERT (pvh_container && psz_leaf);
    SG_ERR_CHECK(  SG_vhash__update__varray(pCtx, pvh_container, psz_leaf, ppva)  );

fail:
    ;
}


