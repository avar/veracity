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

// sg_misc_utils.c
// misc utils.
//////////////////////////////////////////////////////////////////

#include <sg.h>

//////////////////////////////////////////////////////////////////

SG_bool SG_parse_bool(const char* psz)
{
    SG_ASSERT(psz);

    // TODO our bool parsing should probably a bit more flexible than this?
    if (0 == strcmp(psz, "true"))
    {
        return SG_TRUE;
    }
    else
    {
        return SG_FALSE;
    }
}

SG_bool SG_int64__fits_in_double(SG_int64 i64)
{
    double d;
    SG_int64 i64b;

    d = (double) i64;
    i64b = (SG_int64) d;

    return i64 == i64b;
}

SG_bool SG_int64__fits_in_int32(SG_int64 i64)
{
    SG_int32 i32;
    SG_int64 i64b;

    i32 = (SG_int32) i64;
    i64b = (SG_int64) i32;

    return i64 == i64b;
}

SG_bool SG_int64__fits_in_uint32(SG_int64 i64)
{
    SG_uint32 ui32;
    SG_int64 i64b;

    if (i64 < 0)
    {
        return SG_FALSE;
    }

    ui32 = (SG_uint32) i64;
    i64b = (SG_int64) ui32;

    return i64 == i64b;
}

SG_bool SG_uint64__fits_in_int32(SG_uint64 ui64)
{
    SG_int32 i32;
    SG_uint64 ui64b;

    i32 = (SG_int32) ui64;
    if (i32 < 0)
    {
        return SG_FALSE;
    }

    ui64b = (SG_uint64) i32;

    return ui64 == ui64b;
}

SG_bool SG_uint64__fits_in_uint32(SG_uint64 ui64)
{
    SG_uint32 ui32;
    SG_uint64 ui64b;

    ui32 = (SG_uint32) ui64;
    ui64b = (SG_uint64) ui32;

    return ui64 == ui64b;
}

SG_bool SG_double__fits_in_int64(double d)
{
	return ((double)(SG_int64)d) == d;
}

//////////////////////////////////////////////////////////////////

void SG_sleep_ms(SG_uint32 ms)
{
#if defined(WINDOWS)
	Sleep(ms);					// windows Sleep in milliseconds
#else
	sleep( (ms+999)/1000 );		// unix sleep() in seconds
#endif
}

//////////////////////////////////////////////////////////////////
// QSort is a problem.  The prototype for the compare function for
// the normal qsort() does not have a context pointer of any kind.
//
// On the Mac, we have qsort_r() -- a re-entrant version that allows
// 1 context pointer to be passed thru to the compare function.
//
// On Windows, we have qsort_s() -- a security-enhanced version that
// does the same thing.
//
// We need to pass a pCtx because everything uses it now.
//
// I created this interlude version that allows 2 -- a stock pCtx
// and any *actual* caller-data that the compare function might need.
//
// We use this interlude function with qsort_[rs]() and then re-pack
// the args and call the caller's real compare function.

typedef struct _sg_qsort_context_data
{
	SG_context *					pCtx;
	void *							pVoidCallerData;
	SG_qsort_compare_function *		pfnCompare;

} sg_qsort_context_data;

#if defined(LINUX)
static int _my_qsort_interlude(const void * pArg1,
							   const void * pArg2,
							   void * pVoidQSortContextData)
#else
static int _my_qsort_interlude(void * pVoidQSortContextData,
							   const void * pArg1,
							   const void * pArg2)
#endif
{
	sg_qsort_context_data * p = (sg_qsort_context_data *)pVoidQSortContextData;

	return (*p->pfnCompare)(p->pCtx,
							pArg1,
							pArg2,
							p->pVoidCallerData);
}

void SG_qsort(SG_context * pCtx,
			  void * pArray, size_t nrElements, size_t sizeElements,
			  SG_qsort_compare_function * pfnCompare,
			  void * pVoidCallerData)
{
	sg_qsort_context_data d;

	d.pCtx = pCtx;
	d.pVoidCallerData = pVoidCallerData;
	d.pfnCompare = pfnCompare;

#if defined(WINDOWS)
	qsort_s(pArray,nrElements,sizeElements,_my_qsort_interlude,&d);
#endif

#if defined(MAC)
	qsort_r(pArray,nrElements,sizeElements,&d,_my_qsort_interlude);
#endif

#if defined(LINUX)
	qsort_r(pArray,nrElements,sizeElements,_my_qsort_interlude,&d);
#endif
}

