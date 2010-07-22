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

// sg_malloc.h
// Declarations for our memory allocation wrappers
//////////////////////////////////////////////////////////////////

#ifndef H_SG_MALLOC_H
#define H_SG_MALLOC_H

BEGIN_EXTERN_C

//////////////////////////////////////////////////////////////////

#if defined(DEBUG)

// TODO deprecate this in favor of SG_alloc()....
void* _sg_malloc_debug(SG_uint32 siz, const char* pszFile, int iLine);

// TODO deprecate this in favor of SG_alloc()....
void* _sg_calloc_debug(SG_uint32 count, SG_uint32 siz, const char* pszFile, int iLine);

void  _sg_free_debug(SG_context * pCtx, void* p);
void _sg_free_debug__no_ctx(void* p);

void _sg_alloc_debug(SG_context* pCtx, SG_uint32 count, SG_uint32 siz, const char* pszFile, int iLine, void ** ppNew);
SG_error _sg_alloc_debug_err(SG_uint32 count, SG_uint32 siz, const char* pszFile, int iLine, void ** ppNew);

void _sg_mem__set_caller_data(void * p, const char * pszFile, int iLine, const char * pszType);

// TODO deprecate this in favor of SG_alloc()....
#define SG_malloc(x)		(_sg_malloc_debug(x,__FILE__,__LINE__))

// TODO deprecate this in favor of SG_alloc()....
#define SG_calloc(x,y)      (_sg_calloc_debug(x,y,__FILE__,__LINE__))

#define SG_free(_pCtx_,x)         (_sg_free_debug(_pCtx_,(void*)(x)))
#define SG_free__no_ctx(x)            (_sg_free_debug__no_ctx((void*)(x)))

#define SG_alloc(_pCtx_,x,y,pp)   (_sg_alloc_debug(_pCtx_,x,y,__FILE__,__LINE__,(void**)(pp)))
#define SG_alloc_err(x,y,pp)      (_sg_alloc_debug_err(x,y,__FILE__,__LINE__,(void**)(pp)))

int SG_mem__check_for_leaks(void);

#else

// TODO deprecate this in favor of SG_alloc()....
void* _sg_malloc(SG_uint32 siz);

// TODO deprecate this in favor of SG_alloc()....
void* _sg_calloc(SG_uint32 count, SG_uint32 siz);

void _sg_free(SG_context * pCtx, void* p);
void _sg_free__no_ctx(void* p);

void _sg_alloc(SG_context* pCtx, SG_uint32 count, SG_uint32 siz, void ** ppNew);
SG_error _sg_alloc_err(SG_uint32 count, SG_uint32 siz, void ** ppNew);

// TODO deprecate this in favor of SG_alloc()....
#define SG_malloc(x)		(_sg_malloc(x))

// TODO deprecate this in favor of SG_alloc()....
#define SG_calloc(x,y)      (_sg_calloc(x,y))

#define SG_free(_pCtx_,x)       (_sg_free(_pCtx_,(void*)(x)))
#define SG_free__no_ctx(x)          (_sg_free__no_ctx((void*)(x)))

#define SG_alloc(_pCtx_,x,y,pp) (_sg_alloc(_pCtx_,x,y,(void**)(pp)))
#define SG_alloc_err(x,y,pp)    (_sg_alloc_err(x,y,(void**)(pp)))

#endif

#define SG_allocN(_pCtx_,n,p)      (SG_alloc(_pCtx_,n,sizeof(*p),&p))
#define SG_alloc1(_pCtx_,p)        (SG_alloc(_pCtx_,1,sizeof(*p),&p))

#define SG_memcpy2(src,dest) (memcpy((void*)&(dest),(const void*)&(src),sizeof(dest)))
#define SG_zero(x) ((void)memset(&x,0,sizeof(x)))

//////////////////////////////////////////////////////////////////

END_EXTERN_C

#endif//H_SG_MALLOC_H
