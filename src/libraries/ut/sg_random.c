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

//////////////////////////////////////////////////////////////////

void SG_random__seed(SG_uint32 x)
{
	srand((unsigned int)x);
	(void)SG_random_bool(); // For some reason rand() was always coming up 0 the first time after calling srand() on windows... After that it's random enough... ???
}

SG_bool SG_random_bool(void)
{
	return rand()%2==1;
}

SG_uint32 _sg_random_uint32(void)
{
	SG_uint32 r;
	SG_random_bytes((SG_byte*)&r,sizeof(r));
	return r;
}

SG_uint32 SG_random_uint32(SG_uint32 x)
{
	return (SG_uint32)((double)_sg_random_uint32()/(((double)SG_UINT32_MAX)+1)*(double)x);
}
SG_uint32 SG_random_uint32__2(SG_uint32 min, SG_uint32 max)
{
	return SG_random_uint32(max-min+1)+min;
}

SG_uint64 SG_random_uint64(SG_uint64 x)
{
	return (SG_uint64)((double)_sg_random_uint32()/(((double)SG_UINT32_MAX)+1)*(double)x);
}
SG_uint64 SG_random_uint64__2(SG_uint64 min, SG_uint64 max)
{
	return SG_random_uint64(max-min+1)+min;
}

void SG_random_bytes(SG_byte * pBuffer, SG_uint32 bufferLength)
{
	while(bufferLength>0)
	{
		*((unsigned char*)(void*)pBuffer) = (unsigned char)(rand()&0xFF);
		pBuffer+=1;
		bufferLength-=1;
	}
}

void SG_random__write_random_bytes_to_file(SG_context * pCtx, SG_file * pFile, SG_uint64 numBytes)
{
	SG_byte buffer[1024];
	SG_NULLARGCHECK_RETURN(pFile);

	while(numBytes>=1024)
	{
		SG_random_bytes(buffer, 1024);
		SG_ERR_CHECK(  SG_file__write(pCtx, pFile, 1024, buffer, NULL)  );
		numBytes-=1024;
	}

	if(numBytes>0)
	{
		SG_random_bytes(buffer, (SG_uint32)numBytes);
		SG_ERR_CHECK(  SG_file__write(pCtx, pFile, (SG_uint32)numBytes, buffer, NULL)  );
		numBytes-=numBytes;
	}

fail:
	;
}
void SG_random__generate_random_binary_file(SG_context * pCtx, SG_pathname * pPath, SG_uint64 length)
{
	SG_file * pFile = NULL;
	SG_NULLARGCHECK_RETURN(pPath);
	SG_ERR_CHECK(  SG_file__open__pathname(pCtx, pPath, SG_FILE_OPEN_OR_CREATE|SG_FILE_WRONLY,  0700, &pFile)  );
	SG_ERR_CHECK(  SG_random__write_random_bytes_to_file(pCtx, pFile, length)  );
	SG_ERR_CHECK(  SG_file__close(pCtx, &pFile)  );

	return;
fail:
	SG_FILE_NULLCLOSE(pCtx, pFile);
}

void SG_random__select_random_rbtree_node(SG_context * pCtx, SG_rbtree * pRbtree, const char ** ppKey, void ** ppAssocData)
{
	SG_uint32 count, i;
	SG_rbtree_iterator * p = NULL;
	SG_bool ok;
	const char * pKey;
	void * pAssocData;

	SG_NULLARGCHECK_RETURN(pRbtree);

	SG_ERR_CHECK(  SG_rbtree__count(pCtx, pRbtree, &count)  );

	SG_ARGCHECK(count!=0, pRbtree);

	i = SG_random_uint32(count);

	SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &p, pRbtree, &ok, &pKey, &pAssocData)  );
	SG_ASSERT(ok);
	while(i>0)
	{
		--i;
		SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, p, &ok, &pKey, &pAssocData)  );
		SG_ASSERT(ok);
	}

	SG_RBTREE_ITERATOR_NULLFREE(pCtx, p);

	if(ppKey!=NULL)
		*ppKey = pKey;
	if(ppAssocData!=NULL)
		*ppAssocData = pAssocData;

	return;
fail:
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, p);
}

void SG_random__select_2_random_rbtree_nodes(SG_context * pCtx, SG_rbtree * pRbtree,
	const char ** ppKey1, void ** ppAssocData1,
	const char ** ppKey2, void ** ppAssocData2)
{
	SG_uint32 count, r1, r2, i;
	SG_rbtree_iterator * p = NULL;
	SG_bool ok = SG_FALSE;
	const char * pKey = NULL;
	void * pAssocData = NULL;
	const char * pKey1 = NULL;
	void * pAssocData1 = NULL;
	const char * pKey2 = NULL;
	void * pAssocData2 = NULL;

	SG_NULLARGCHECK_RETURN(pRbtree);

	SG_ERR_CHECK(  SG_rbtree__count(pCtx, pRbtree, &count)  );

	SG_ARGCHECK(count>=2, pRbtree);

	r1 = SG_random_uint32(count);
	r2 = (r1+1+SG_random_uint32(count-1))%count;

	SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &p, pRbtree, &ok, &pKey, &pAssocData)  );
	for(i=0;i<=r1||i<=r2;++i)
	{
		SG_ASSERT(ok);
		if(i==r1)
		{
			pKey1 = pKey;
			pAssocData1 = pAssocData;
		}
		else if(i==r2)
		{
			pKey2 = pKey;
			pAssocData2 = pAssocData;
		}
		SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, p, &ok, &pKey, &pAssocData)  );
	}

	SG_RBTREE_ITERATOR_NULLFREE(pCtx, p);

	if(ppKey1!=NULL)
		*ppKey1 = pKey1;
	if(ppAssocData1!=NULL)
		*ppAssocData1 = pAssocData1;
	if(ppKey2!=NULL)
		*ppKey2 = pKey2;
	if(ppAssocData2!=NULL)
		*ppAssocData2 = pAssocData2;

	return;
fail:
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, p);
}
