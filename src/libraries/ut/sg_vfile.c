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

//#if defined(DEBUG)
#if 0
#define TRACE_VFILE 1
#else
#define TRACE_VFILE 0
#endif

struct _sg_vfile
{
	SG_file_flags mode;
	SG_file* pFile;
};

void SG_vfile__begin(
	SG_context* pCtx,
	const SG_pathname* pPath, /**< The path of the file containing the JSON text */
	SG_file_flags mode,
	SG_vhash** ppvh, /**< If there are no errors, the resulting vhash table will be returned here. */
	SG_vfile** ppvf
	)
{
	SG_vfile* pvf = NULL;
	SG_vhash* pvh = NULL;
	SG_uint32 len32;
	SG_fsobj_type t;
	SG_byte* p = NULL;
	SG_bool bExists;
	SG_fsobj_type FsObjType;
	SG_fsobj_perms FsObjPerms;

	SG_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath, &bExists, &FsObjType, &FsObjPerms)  );

	if (
		bExists
		&& (SG_FSOBJ_TYPE__REGULAR != FsObjType)
		)
	{
		SG_ERR_THROW_RETURN(SG_ERR_NOTAFILE);
	}

	if (bExists)
	{
		SG_uint64 len64;
		SG_ERR_CHECK(  SG_fsobj__length__pathname(pCtx, pPath, &len64, &t)  );

		// TODO "len" is uint64 because we can have huge files, but
		// TODO our buffer is limited to uint32 (on 32bit systems).
		// TODO verify that len will fit in uint32.
		len32 = (SG_uint32)len64;
	}
	else
	{
		len32 = 0;
	}

	SG_ERR_CHECK_RETURN(  SG_alloc1(pCtx, pvf)  );

	SG_ERR_CHECK(  SG_file__open__pathname(pCtx, pPath, mode | SG_FILE_LOCK, SG_FSOBJ_PERMS__UNUSED, &pvf->pFile)  );

	pvf->mode = mode;

#if TRACE_VFILE
	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "VFileBegin: Reading %d bytes from %s\n", len32, SG_pathname__sz(pPath))  );
#endif

	if (len32 > 0)
	{
		SG_ERR_CHECK(  SG_alloc(pCtx, 1,len32+1,&p)  );
		SG_ERR_CHECK(  SG_file__read(pCtx, pvf->pFile, len32, p, NULL)  );

		p[len32] = 0;

		SG_ERR_CHECK(  SG_VHASH__ALLOC__FROM_JSON(pCtx, &pvh, (const char*) p)  );

		SG_NULLFREE(pCtx, p);
		p = NULL;
	}
	else
	{
		pvh = NULL;
	}

	*ppvf = pvf;
	*ppvh = pvh;

	return;

fail:
	SG_FILE_NULLCLOSE(pCtx, pvf->pFile);
	SG_NULLFREE(pCtx, p);
	SG_NULLFREE(pCtx, pvf);
}

//////////////////////////////////////////////////////////////////

void SG_vfile__reread(
	SG_context* pCtx,
	SG_vfile * pvf,
	SG_vhash** ppvh)
{
	SG_vhash* pvh = NULL;
	SG_uint64 len64;
	SG_uint32 len32;
	SG_byte* p = NULL;

	SG_ERR_CHECK(  SG_file__seek_end(pCtx, pvf->pFile, &len64)  );
	SG_ERR_CHECK(  SG_file__seek(pCtx, pvf->pFile, 0)  );
	
	// TODO "len64" is uint64 because we can have huge files, but
	// TODO our buffer is limited to uint32 (on 32bit systems).
	// TODO verify that len will fit in uint32.
	len32 = (SG_uint32)len64;
	if (len32 > 0)
	{
		SG_ERR_CHECK(  SG_alloc(pCtx, 1,len32+1,&p)  );
		SG_ERR_CHECK(  SG_file__read(pCtx, pvf->pFile, len32, p, NULL)  );

		p[len32] = 0;

		SG_ERR_CHECK(  SG_VHASH__ALLOC__FROM_JSON(pCtx, &pvh, (const char*) p)  );
		*ppvh = pvh;
	}

fail:
	SG_NULLFREE(pCtx, p);
}

//////////////////////////////////////////////////////////////////

void sg_vfile__dispose(
	SG_context* pCtx,
	SG_vfile* pvf
	)
{
	if (!pvf)
		return;

	SG_FILE_NULLCLOSE(pCtx, pvf->pFile);
	SG_NULLFREE(pCtx, pvf);
}

void SG_vfile__abort(
	SG_context* pCtx,
	SG_vfile** ppvf
	)
{
	SG_vfile* pvf = NULL;

	if (!ppvf)
		return;

	pvf = *ppvf;
	if (!pvf)
		return;

	SG_FILE_NULLCLOSE(pCtx, pvf->pFile);
	SG_NULLFREE(pCtx, pvf);

	*ppvf = NULL;
}

void SG_vfile__end(
	SG_context* pCtx,
	SG_vfile** ppvf,
	const SG_vhash* pvh
	)
{
	SG_string* pstr = NULL;
	SG_vfile* pvf = NULL;

	SG_NULLARGCHECK_RETURN(ppvf);

	pvf = *ppvf;

	if (pvh)
	{
		SG_ARGCHECK_RETURN( !(pvf->mode & SG_FILE_RDONLY) , pvh );

		SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstr)  );

		SG_ERR_CHECK(  SG_vhash__to_json(pCtx, pvh,pstr)  );

		SG_ERR_CHECK(  SG_file__seek(pCtx, pvf->pFile, 0)  );

		SG_ERR_CHECK(  SG_file__truncate(pCtx, pvf->pFile)  );

#if TRACE_VFILE
		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "VFileEnd: Writing %d bytes to file.\n",
								   SG_string__length_in_bytes(pstr))  );
#endif

		SG_ERR_CHECK(  SG_file__write(pCtx, pvf->pFile, SG_string__length_in_bytes(pstr), (const SG_byte *)SG_string__sz(pstr), NULL)  );
		SG_STRING_NULLFREE(pCtx, pstr);
	}
	else
	{
		if (!(pvf->mode & SG_FILE_RDONLY))
		{
			SG_ERR_CHECK(  SG_file__seek(pCtx, pvf->pFile, 0)  );

			SG_ERR_CHECK(  SG_file__truncate(pCtx, pvf->pFile)  );

#if TRACE_VFILE
			SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "VFileEnd: Truncating file.\n")  );
#endif
		}
	}

	SG_FILE_NULLCLOSE(pCtx, pvf->pFile);

	SG_NULLFREE(pCtx, pvf);
	*ppvf = NULL;

	return;
fail:
	SG_STRING_NULLFREE(pCtx, pstr);
}

void SG_vfile__slurp(
	SG_context* pCtx,
	const SG_pathname* pPath,
	SG_vhash** ppvh /**< Caller must free */
	)
{
	SG_vfile* pvf = NULL;
	SG_vhash* pvh = NULL;

	SG_ERR_CHECK(  SG_vfile__begin(pCtx, pPath, SG_FILE_RDONLY | SG_FILE_OPEN_EXISTING, &pvh, &pvf)  );
	SG_ERR_CHECK(  SG_vfile__end(pCtx, &pvf, NULL)  );

	*ppvh = pvh;

	return;
fail:
	/* TODO */
	return;
}

void SG_vfile__add__vhash(
	SG_context* pCtx,
	const SG_pathname* pPath, /**< The path of the file containing the JSON text */
	const char* putf8Key,
	const SG_vhash* pHashValue
	)
{
	SG_vfile* pvf = NULL;
	SG_vhash* pvh = NULL;
	SG_vhash* pvhCopy = NULL;

	SG_ERR_CHECK(  SG_vfile__begin(pCtx, pPath, SG_FILE_RDWR | SG_FILE_OPEN_OR_CREATE, &pvh, &pvf)  );
	if (!pvh)
	{
		SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvh)  );
	}
	SG_ERR_CHECK(  SG_VHASH__ALLOC__COPY(pCtx, &pvhCopy, pHashValue)  );
	SG_ERR_CHECK(  SG_vhash__add__vhash(pCtx, pvh, putf8Key, &pvhCopy)  );
	SG_ERR_CHECK(  SG_vfile__end(pCtx, &pvf, pvh)  );

	SG_VHASH_NULLFREE(pCtx, pvh);

	return;
fail:
	SG_ERR_IGNORE(  sg_vfile__dispose(pCtx, pvf)  );
	SG_VHASH_NULLFREE(pCtx, pvhCopy);
	SG_VHASH_NULLFREE(pCtx, pvh);
}

void SG_vfile__update__vhash(
	SG_context* pCtx,
	const SG_pathname* pPath, /**< The path of the file containing the JSON text */
	const char* putf8Key,
	const SG_vhash* pHashValue
	)
{
	SG_vfile* pvf = NULL;
	SG_vhash* pvh = NULL;
	SG_vhash* pvhCopy = NULL;

	SG_ERR_CHECK(  SG_vfile__begin(pCtx, pPath, SG_FILE_RDWR | SG_FILE_OPEN_OR_CREATE, &pvh, &pvf)  );
	if (!pvh)
	{
		SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvh)  );
	}
	SG_ERR_CHECK(  SG_VHASH__ALLOC__COPY(pCtx, &pvhCopy, pHashValue)  );
	SG_ERR_CHECK(  SG_vhash__update__vhash(pCtx, pvh, putf8Key, &pvhCopy)  );
	SG_ERR_CHECK(  SG_vfile__end(pCtx, &pvf, pvh)  );

	SG_VHASH_NULLFREE(pCtx, pvh);

	return;
fail:
	SG_ERR_IGNORE(  sg_vfile__dispose(pCtx, pvf)  );
	SG_VHASH_NULLFREE(pCtx, pvhCopy);
	SG_VHASH_NULLFREE(pCtx, pvh);
}

void SG_vfile__remove(
	SG_context* pCtx,
	const SG_pathname* pPath, /**< The path of the file containing the JSON text */
	const char* putf8Key
	)
{
	SG_vfile* pvf = NULL;
	SG_vhash* pvh = NULL;

	SG_ERR_CHECK(  SG_vfile__begin(pCtx, pPath, SG_FILE_RDWR | SG_FILE_OPEN_OR_CREATE, &pvh, &pvf)  );
	if (pvh)
	{
		SG_ERR_CHECK(  SG_vhash__remove(pCtx, pvh, putf8Key)  );
	}
	else
	{
		SG_ERR_THROW(SG_ERR_VHASH_KEYNOTFOUND);
	}
	SG_ERR_CHECK(  SG_vfile__end(pCtx, &pvf, pvh)  );

	SG_VHASH_NULLFREE(pCtx, pvh);

	return;
fail:
	SG_ERR_IGNORE(  sg_vfile__dispose(pCtx, pvf)  );
	SG_VHASH_NULLFREE(pCtx, pvh);
}

void SG_vfile__update__string__sz(
	SG_context* pCtx,
	const SG_pathname* pPath, /**< The path of the file containing the JSON text */
	const char* putf8Key,
	const char* pszVal
	)
{
	SG_vfile* pvf = NULL;
	SG_vhash* pvh = NULL;

	SG_ERR_CHECK(  SG_vfile__begin(pCtx, pPath, SG_FILE_RDWR | SG_FILE_OPEN_OR_CREATE, &pvh, &pvf)  );
	if (!pvh)
	{
		SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvh)  );
	}
	SG_ERR_CHECK(  SG_vhash__update__string__sz(pCtx, pvh, putf8Key, pszVal)  );
	SG_ERR_CHECK(  SG_vfile__end(pCtx, &pvf, pvh)  );

	SG_VHASH_NULLFREE(pCtx, pvh);

	return;
fail:
	SG_ERR_IGNORE(  sg_vfile__dispose(pCtx, pvf)  );
	SG_VHASH_NULLFREE(pCtx, pvh);
}

void SG_vfile__update__bool(SG_context* pCtx, const SG_pathname* pPath, const char* putf8Key, SG_bool value)
{
    SG_vfile* pvf = NULL;
    SG_vhash* pvh = NULL;
    SG_ERR_CHECK(  SG_vfile__begin(pCtx, pPath, SG_FILE_RDWR | SG_FILE_OPEN_OR_CREATE, &pvh, &pvf)  );
    if (!pvh)
        SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvh)  );
    SG_ERR_CHECK(  SG_vhash__update__bool(pCtx, pvh, putf8Key, value)  );
    SG_ERR_CHECK(  SG_vfile__end(pCtx, &pvf, pvh)  );
    SG_VHASH_NULLFREE(pCtx, pvh);
    return;
fail:
    SG_ERR_IGNORE(  sg_vfile__dispose(pCtx, pvf)  );
    SG_VHASH_NULLFREE(pCtx, pvh);
}
void SG_vfile__update__double(SG_context* pCtx, const SG_pathname* pPath, const char* putf8Key, double value)
{
    SG_vfile* pvf = NULL;
    SG_vhash* pvh = NULL;
    SG_ERR_CHECK(  SG_vfile__begin(pCtx, pPath, SG_FILE_RDWR | SG_FILE_OPEN_OR_CREATE, &pvh, &pvf)  );
    if (!pvh)
        SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvh)  );
    SG_ERR_CHECK(  SG_vhash__update__double(pCtx, pvh, putf8Key, value)  );
    SG_ERR_CHECK(  SG_vfile__end(pCtx, &pvf, pvh)  );
    SG_VHASH_NULLFREE(pCtx, pvh);
    return;
fail:
    SG_ERR_IGNORE(  sg_vfile__dispose(pCtx, pvf)  );
    SG_VHASH_NULLFREE(pCtx, pvh);
}
void SG_vfile__update__int64(SG_context* pCtx, const SG_pathname* pPath, const char* putf8Key, SG_int64 value)
{
    SG_vfile* pvf = NULL;
    SG_vhash* pvh = NULL;
    SG_ERR_CHECK(  SG_vfile__begin(pCtx, pPath, SG_FILE_RDWR | SG_FILE_OPEN_OR_CREATE, &pvh, &pvf)  );
    if (!pvh)
        SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvh)  );
    SG_ERR_CHECK(  SG_vhash__update__int64(pCtx, pvh, putf8Key, value)  );
    SG_ERR_CHECK(  SG_vfile__end(pCtx, &pvf, pvh)  );
    SG_VHASH_NULLFREE(pCtx, pvh);
    return;
fail:
    SG_ERR_IGNORE(  sg_vfile__dispose(pCtx, pvf)  );
    SG_VHASH_NULLFREE(pCtx, pvh);
}
void SG_vfile__update__varray(SG_context* pCtx, const SG_pathname* pPath, const char* putf8Key, SG_varray ** ppValue)
{
    SG_vfile* pvf = NULL;
    SG_vhash* pvh = NULL;
    SG_ERR_CHECK(  SG_vfile__begin(pCtx, pPath, SG_FILE_RDWR | SG_FILE_OPEN_OR_CREATE, &pvh, &pvf)  );
    if (!pvh)
        SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvh)  );
    SG_ERR_CHECK(  SG_vhash__update__varray(pCtx, pvh, putf8Key, ppValue)  );
    SG_ERR_CHECK(  SG_vfile__end(pCtx, &pvf, pvh)  );
    SG_VHASH_NULLFREE(pCtx, pvh);
    return;
fail:
    SG_ERR_IGNORE(  sg_vfile__dispose(pCtx, pvf)  );
    SG_VHASH_NULLFREE(pCtx, pvh);
}
