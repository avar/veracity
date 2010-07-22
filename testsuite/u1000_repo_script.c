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
 * @file u1000_repo_script.c
 *
 * @details A test to create a series of REPOs and add a series of CHANGESETS
 * creating DAGs in well-defined patterns that can be validated and that we
 * can extract DAG-FRAGMENTS and know what the output look like.
 *
 * TODO create a partial clone of the repo and use the dagfrag to push a
 * portion of the dag to the other (like PUSH/PULL/UPDATE commands will
 * need to do).
 */

//////////////////////////////////////////////////////////////////

#include <sg.h>
#include "unittests.h"

//////////////////////////////////////////////////////////////////
// we define a little trick here to prefix all global symbols (type,
// structures, functions) with our test name.  this allows all of
// the tests in the suite to be #include'd into one meta-test (without
// name collisions) when we do a GCOV run.

#define MyMain()				TEST_MAIN(u1000_repo_script)
#define MyDcl(name)				u1000_repo_script__##name
#define MyFn(name)				u1000_repo_script__##name
#define MyScriptCmd(name)		u1000_repo_script__##name

//////////////////////////////////////////////////////////////////

#define MyStoreType				MyDcl(T_store)

typedef struct MyStoreType
{
	SG_rbtree *					m_pRB_RepoCache;
	SG_rbtree *					m_pRB_ChangesetCache;
	SG_rbtree *					m_pRB_DagnodeCache;

	SG_string *					m_pString_RepoName;
	SG_repo *					m_pRepoCurrent;		// we do not own this

	SG_string *					m_pString_NameOfLastCommit;

} MyStoreType;

#define gpMyStore				MyDcl(gp_store)

static MyStoreType *			gpMyStore = NULL;

//////////////////////////////////////////////////////////////////

void MyFn(free_store)(SG_context * pCtx)
{
	if (!gpMyStore)
		return;

	SG_ERR_IGNORE(  SG_rbtree__free__with_assoc(pCtx, gpMyStore->m_pRB_RepoCache,     (SG_free_callback *)SG_repo__free)  );
	SG_ERR_IGNORE(  SG_rbtree__free__with_assoc(pCtx, gpMyStore->m_pRB_ChangesetCache,(SG_free_callback *)SG_changeset__free)  );
	SG_ERR_IGNORE(  SG_rbtree__free__with_assoc(pCtx, gpMyStore->m_pRB_DagnodeCache,  (SG_free_callback *)SG_dagnode__free)  );

	SG_STRING_NULLFREE(pCtx, gpMyStore->m_pString_RepoName);

	SG_STRING_NULLFREE(pCtx, gpMyStore->m_pString_NameOfLastCommit);

	SG_NULLFREE(pCtx, gpMyStore);
	gpMyStore = NULL;
}

void MyFn(alloc_store)(SG_context * pCtx)
{
	SG_ASSERT( (gpMyStore == NULL) && "Double alloc_store." );

	gpMyStore = (MyStoreType *)SG_calloc(1,sizeof(MyStoreType));
	if (!gpMyStore)
		SG_ERR_THROW_RETURN(SG_ERR_MALLOCFAILED);

	VERIFY_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &gpMyStore->m_pRB_RepoCache)  );
	VERIFY_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &gpMyStore->m_pRB_ChangesetCache)  );
	VERIFY_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &gpMyStore->m_pRB_DagnodeCache)  );

	VERIFY_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &gpMyStore->m_pString_RepoName)  );

	VERIFY_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &gpMyStore->m_pString_NameOfLastCommit)  );

	gpMyStore->m_pRepoCurrent = NULL;

	return;
fail:
	SG_ERR_IGNORE(  MyFn(free_store)(pCtx)  );
}

//////////////////////////////////////////////////////////////////

/**
 * The DAGFRAG code now takes a generic FETCH-DAGNODE callback function
 * rather than assuming that it should call SG_repo__fetch_dagnode().
 * This allows the DAGFRAG code to not care whether it is above or below
 * the REPO API layer.
 *
 * my_fetch_dagnodes_from_repo() is a generic wrapper for
 * SG_repo__fetch_dagnode() and is used by the code in
 * sg_dagfrag to request dagnodes from disk.  we don't really
 * need this function since FN__sg_fetch_dagnode and
 * SG_repo__fetch_dagnode() have an equivalent prototype,
 * but with callbacks in loops calling callbacks calling callbacks,
 * it's easy to get lost....
 *
 */
static FN__sg_fetch_dagnode MyFn(my_fetch_dagnodes_from_repo);

void MyFn(my_fetch_dagnodes_from_repo)(SG_context * pCtx, void * pCtxFetchDagnode, const char * szHidDagnode, SG_dagnode ** ppDagnode)
{
	SG_ERR_CHECK_RETURN(  SG_repo__fetch_dagnode(pCtx, (SG_repo *)pCtxFetchDagnode,szHidDagnode,ppDagnode)  );
}

//////////////////////////////////////////////////////////////////

void MyScriptCmd(set_current_repo)(SG_context * pCtx, const char * szRepoCacheName)
{
	// select the named repo to be the "current" one (for commit operations that don't specify one).

	SG_repo * pRepo;

	if (szRepoCacheName && *szRepoCacheName)
	{
		VERIFY_ERR_CHECK(  SG_rbtree__find(pCtx, gpMyStore->m_pRB_RepoCache,szRepoCacheName,NULL,(void **)&pRepo)  );

		// remember symbolic name and the SG_repo pointer of the current repo.

		VERIFY_ERR_CHECK(  SG_string__set__sz(pCtx, gpMyStore->m_pString_RepoName,szRepoCacheName)  );
		gpMyStore->m_pRepoCurrent = pRepo;			// set the current repo
	}
	else
	{
		// allow a null arg to clear the current repo setting.  this
		// does not close or delete anything, just clear the default
		// setting.

		VERIFY_ERR_CHECK(  SG_string__clear(pCtx, gpMyStore->m_pString_RepoName)  );
		gpMyStore->m_pRepoCurrent = NULL;
	}

	return;
fail:
	return;
}

void MyScriptCmd(create_new_repo)(SG_context * pCtx, const char * szRepoCacheName, SG_UNUSED_PARAM(const char * szPath))
{
	// we add the SG_repo to our name-cache.  the name-cache now owns the pointer.
	//
	// we make the SG_repo that we created the current repo.

	SG_repo * pRepo = NULL;
	SG_repo * pOldRepo = NULL;
	SG_pathname * pPathnameRepoDir = NULL;
	SG_vhash * pvhPartialDescriptor = NULL;
	SG_vhash * pvhActualDescriptor = NULL;
	SG_string * pStringRepoDescriptor = NULL;
    char buf_repo_id[SG_GID_BUFFER_LENGTH];
    char buf_admin_id[SG_GID_BUFFER_LENGTH];

	VERIFY_ERR_CHECK(  SG_closet__get_partial_repo_instance_descriptor_for_new_local_repo(pCtx, &pvhPartialDescriptor)  );

	VERIFY_ERR_CHECK(  SG_gid__generate(pCtx, buf_repo_id, sizeof(buf_repo_id))  );
	VERIFY_ERR_CHECK(  SG_gid__generate(pCtx, buf_admin_id, sizeof(buf_admin_id))  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx, &pPathnameRepoDir, szPath)  );

	VERIFY_ERR_CHECK(  SG_repo__create_repo_instance(pCtx, pvhPartialDescriptor, SG_TRUE, NULL, buf_repo_id, buf_admin_id, &pRepo)  );
    VERIFY_ERR_CHECK(  sg_zing__init_new_repo(pCtx, pRepo)  );
    VERIFY_ERR_CHECK(  SG_user__create(pCtx, pRepo, "debug@sourcegear.com")  );
	VERIFY_ERR_CHECK(  SG_repo__get_descriptor(pCtx, pRepo, (const SG_vhash**)&pvhActualDescriptor)  );
	SG_ERR_IGNORE(  SG_closet__descriptors__remove(pCtx, szRepoCacheName)  );
	VERIFY_ERR_CHECK(  SG_closet__descriptors__add(pCtx, szRepoCacheName, pvhActualDescriptor)  );

	// add pRepo to the cache -or- replace (and close/free) an existing value.
	VERIFY_ERR_CHECK(  SG_rbtree__update__with_assoc(pCtx, gpMyStore->m_pRB_RepoCache,szRepoCacheName,
													 pRepo,(void **)&pOldRepo)  );
	pRepo = NULL;		// the rbtree now owns this.

#if 0	// less noise
	VERIFY_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pStringRepoDescriptor)  );
	VERIFY_ERR_CHECK(  SG_vhash__to_json(pCtx, pvhRepoDescriptor,pStringRepoDescriptor)  );
	VERIFY_ERR_CHECK(  SG_string__replace_bytes(pCtx, pStringRepoDescriptor,(SG_byte *)"\r",1,(SG_byte *)" ",1,SG_UINT32_MAX,SG_TRUE,NULL)  );
	VERIFY_ERR_CHECK(  SG_string__replace_bytes(pCtx, pStringRepoDescriptor,(SG_byte *)"\n",1,(SG_byte *)" ",1,SG_UINT32_MAX,SG_TRUE,NULL)  );
	INFOP("create_new_repo",("Repo[%s] : %s",
							 szRepoCacheName, SG_string__sz(pStringRepoDescriptor)));
	SG_STRING_NULLFREE(pCtx, pStringRepoDescriptor);
#endif

	SG_REPO_NULLFREE(pCtx, pOldRepo);
	SG_PATHNAME_NULLFREE(pCtx, pPathnameRepoDir);
	SG_VHASH_NULLFREE(pCtx, pvhPartialDescriptor);

	VERIFY_ERR_CHECK(  MyScriptCmd(set_current_repo)(pCtx, (char*)szRepoCacheName)  );

	return;
fail:
	SG_REPO_NULLFREE(pCtx, pOldRepo);
	SG_VHASH_NULLFREE(pCtx, pvhActualDescriptor);
	SG_PATHNAME_NULLFREE(pCtx, pPathnameRepoDir);
	SG_STRING_NULLFREE(pCtx, pStringRepoDescriptor);
	SG_REPO_NULLFREE(pCtx, pRepo);
}

void MyScriptCmd(clone_repo)(SG_context * pCtx, const char * szSrcRepoCacheName, const char * szDestRepoCacheName)
{
	SG_repo* pRepo = NULL;
	SG_repo * pOldRepo = NULL;

	// if there's already a repo with the dest name we want, remove its descriptor
	SG_ERR_IGNORE(  SG_closet__descriptors__remove(pCtx, szDestRepoCacheName)  );

	VERIFY_ERR_CHECK(  SG_repo__create_empty_clone(pCtx, szSrcRepoCacheName, szDestRepoCacheName)  );

	VERIFY_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, szDestRepoCacheName, &pRepo)  );

	// add pRepo to the cache -or- replace (and close/free) an existing value.
	VERIFY_ERR_CHECK(  SG_rbtree__update__with_assoc(pCtx, gpMyStore->m_pRB_RepoCache,szDestRepoCacheName,
		pRepo,(void **)&pOldRepo)  );
	pRepo = NULL;		// the rbtree now owns this.

	VERIFY_ERR_CHECK(  MyScriptCmd(set_current_repo)(pCtx, (char*)szDestRepoCacheName)  );

	/* fall through */
fail:
	SG_REPO_NULLFREE(pCtx, pRepo);
	SG_REPO_NULLFREE(pCtx, pOldRepo);
}

void MyScriptCmd(close_repo)(SG_context * pCtx, const char * szRepoCacheName)
{
	// close and release all memory for the given repo and remove it from our repo cache.
	//
	// this does not affect the changeset or dagnode caches.

	SG_repo * pRepo = NULL;

	// remove the repo associated with the given key from our cache.

	const char * szCurrentName = SG_string__sz(gpMyStore->m_pString_RepoName);

	if (szRepoCacheName && *szRepoCacheName)
	{
		// they gave us a name to close.  remove it from our cache.

		VERIFY_ERR_CHECK(  SG_rbtree__remove__with_assoc(pCtx, gpMyStore->m_pRB_RepoCache,szRepoCacheName,(void **)&pRepo)  );

		// if it matches the "current" repo, unset the current repo before closing it.

		if (strcmp(szCurrentName,szRepoCacheName) == 0)
			SG_ERR_IGNORE(  MyScriptCmd(set_current_repo)(pCtx, NULL)  );

		SG_REPO_NULLFREE(pCtx, pRepo);
	}
	else
	{
		// they did not give us a name to close, we assume that they
		// meant to close the "current" repo.

		if (szCurrentName && *szCurrentName)
		{
			// we have a current repo.

			VERIFY_ERR_CHECK(  SG_rbtree__remove__with_assoc(pCtx, gpMyStore->m_pRB_RepoCache,szCurrentName,(void **)&pRepo)  );

			SG_ERR_IGNORE(  MyScriptCmd(set_current_repo)(pCtx, NULL)  );

			SG_REPO_NULLFREE(pCtx, pRepo);
		}
		else
		{
			// we don't have a current repo -- they should have explicitly named one.

			VERIFY_COND("we don't have a current repo -- they should have explicitly named one.", SG_FALSE);
			SG_ERR_THROW(SG_ERR_INVALIDARG);
		}
	}

	return;
fail:
	return;
}

//////////////////////////////////////////////////////////////////

void MyFn(__get_repo_pathname)(SG_context * pCtx, SG_vhash * pVhashElement, const char ** pszPathname)
{
	// parse script and get value for "pathname".
	//
	// "pathname" is an optional parameter in various repo-related commands.
	// if omitted, we supply a path in /tmp or C:/temp.

	SG_bool bHasPathname;

	VERIFY_ERR_CHECK(  SG_vhash__has(pCtx, pVhashElement,"pathname",&bHasPathname)  );
	if (bHasPathname)
		VERIFY_ERR_CHECK(  SG_vhash__get__sz(pCtx, pVhashElement,"pathname",pszPathname)  );
	else
	{
#if defined(MAC) || defined(LINUX)
		*pszPathname = "/tmp/repo";
#endif
#if defined(WINDOWS)
		*pszPathname = "C:/temp/repo";
#endif
	}

	// fall-thru to common cleanup

fail:
	return;
}

void MyFn(__make_anon_name)(SG_context * pCtx, char * pBuf, SG_uint32 lenBuf)
{
	static SG_uint32 nrCommits = 0;		// a sequence number for unnamed commits.

	SG_ERR_IGNORE(  SG_sprintf(pCtx, pBuf,lenBuf,"Anon%09d",++nrCommits)  );
}

void MyFn(__get_commit_name)(SG_context * pCtx, SG_vhash * pVhashElement,
								 char * pBufWork, SG_uint32 lenBufWork,
								 const char ** pszName)
{
	// parse script and get value for "name".
	//
	// "name" is an optional parameter in various commit-related commands.
	// if omitted, we supply an anonymous sequence number.
	//
	// the caller must give us a work buffer in case we need to synthesize a name.
	//
	// we set *pszName to the actual name (whether it is in the given buffer or
	// a field in the vhash).

	SG_bool bHasName;

	VERIFY_ERR_CHECK(  SG_vhash__has(pCtx, pVhashElement,"name",&bHasName)  );
	if (bHasName)
		VERIFY_ERR_CHECK(  SG_vhash__get__sz(pCtx, pVhashElement,"name",pszName)  );
	else
	{
		SG_ERR_IGNORE(  MyFn(__make_anon_name)(pCtx, pBufWork,lenBufWork)  );
		*pszName = pBufWork;
	}

	// fall-thru to common cleanup

fail:
	return;
}

void MyFn(__get_parent_name)(SG_context * pCtx, SG_vhash * pVhashElement,
								 char * pBufWork, SG_uint32 lenBufWork,
								 const char ** pszParentName)
{
	// parse script and get value for "parent".
	//
	// "parent" is an optional parameter in various commit-related commands.
	// if omitted, we supply the name of the last commit.
	//
	// the caller must give us a work buffer in case we need to synthesize a name.
	//
	// we set *pszParentName to the actual name (whether it is in the given buffer or
	// a field in the vhash).

	SG_bool bHasParent;

	VERIFY_ERR_CHECK(  SG_vhash__has(pCtx, pVhashElement,"parent",&bHasParent)  );
	if (bHasParent)
		VERIFY_ERR_CHECK(  SG_vhash__get__sz(pCtx, pVhashElement,"parent",pszParentName)  );
	else
	{
		const char * szLastCommit = SG_string__sz(gpMyStore->m_pString_NameOfLastCommit);
		if (!szLastCommit || !*szLastCommit)
			SG_ERR_THROW(SG_ERR_INVALIDARG);

		VERIFY_ERR_CHECK(  SG_strcpy(pCtx, pBufWork,lenBufWork,szLastCommit)  );
		*pszParentName = pBufWork;
	}

	// fall-thru to common cleanup

fail:
	return;
}

void MyFn(__get_id_for_name)(SG_context * pCtx, const char * szCacheName, const char ** pszHid)
{
	// use given cache name and lookup the changeset/dagnode in our cache.
	// then extract the HID for it.

	SG_changeset * pChangeset;

	VERIFY_ERR_CHECK(  SG_rbtree__find(pCtx, gpMyStore->m_pRB_ChangesetCache,szCacheName,NULL,(void **)&pChangeset)  );
	VERIFY_ERR_CHECK(  SG_changeset__get_id_ref(pCtx, pChangeset,pszHid)  );

	// fall-thru to common cleanup

fail:
	return;
}

void MyFn(__create_noise)(SG_context * pCtx, SG_committing * pCommitting, const char * szCommitCacheName)
{
	// create a unique/random blob and add it to the changeset.
	// since we don't currently have any DB records or any actual
	// treenode content (and we took all the datestamp and stuff
	// out of the changeset), any 2 checkins that reference the
	// same set of parent(s) will generate the same HID.  this
	// causes confusion in our script.  so add some noise in to
	// a blob and add that to the bloblist in the changeset.
	// this should keep our HIDs unique.

	SG_int64 iTime;
	char buf_tid[SG_TID_MAX_BUFFER_LENGTH];
	char bufTime[100];
	SG_string * pString = NULL;
	char * szHidNoise = NULL;

	VERIFY_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pString)  );

	// create a random string.  This includes a TID and a timestamp.

	VERIFY_ERR_CHECK(  SG_tid__generate(pCtx, buf_tid, sizeof(buf_tid))  );
	VERIFY_ERR_CHECK(  SG_time__get_milliseconds_since_1970_utc(pCtx, &iTime)  );
	VERIFY_ERR_CHECK(  SG_time__format_utc__i64(pCtx, iTime,bufTime,SG_NrElements(bufTime))  );
	VERIFY_ERR_CHECK(  SG_string__sprintf(pCtx, pString,
										  ("This is a noise for %s.\n"
										   "It was created at %s.\n"
										   "Random TID [%s]\n"),
										  szCommitCacheName,
										  bufTime,
										  buf_tid)  );
	VERIFY_ERR_CHECK(  SG_committing__add_bytes__string(pCtx, pCommitting, NULL,
														pString,
                                                        SG_BLOB_REFTYPE__TREEUSERFILE,
														&szHidNoise)  );

	// fall-thru to common cleanup

fail:
	SG_STRING_NULLFREE(pCtx, pString);
	SG_NULLFREE(pCtx, szHidNoise);
}

void MyFn(__get_proper_repo)(SG_context * pCtx, const char * szRepoCacheName, SG_repo ** ppRepo)
{
	// helper function for commit-related commands to fetch the proper repo.
	// if one is named, we look it up in our cache.
	// if not, we try to use the "current" repo, if it has been set.

	if (szRepoCacheName && *szRepoCacheName)
	{
		SG_ERR_CHECK_RETURN(  SG_rbtree__find(pCtx, gpMyStore->m_pRB_RepoCache,szRepoCacheName,NULL,(void **)ppRepo)  );
		return;
	}
	else if (!gpMyStore->m_pRepoCurrent)
		SG_ERR_THROW_RETURN(SG_ERR_INVALIDARG);

	*ppRepo = gpMyStore->m_pRepoCurrent;
}

void MyScriptCmd(initial_commit)(SG_context * pCtx, const char * szRepoCacheName, const char * szCommitCacheName)
{
	// create a new *INITIAL* commit in the given repo.  this commit will
	// have no parents.
	//
	// cache it in our changeset and dagnode tables using the given cache name.

	SG_repo * pRepo = NULL;
	SG_committing * pCommitting = NULL;
	SG_changeset * pChangeset = NULL;
	SG_changeset * pOldChangeset = NULL;
	SG_dagnode * pDagnode = NULL;
	SG_dagnode * pOldDagnode = NULL;
	char * szGidActualRoot_unused = NULL;
    SG_audit q;

	VERIFY_ERR_CHECK(  MyFn(__get_proper_repo)(pCtx, szRepoCacheName,&pRepo)  );

    VERIFY_ERR_CHECK(  SG_audit__init(pCtx,&q,pRepo,SG_AUDIT__WHEN__NOW, SG_AUDIT__WHO__FROM_SETTINGS)  );

#if 1
	// now that we have some of the helper routines used in sg_cmd, we can use
	// higher level routines rather than building the commit the hardway.
	VERIFY_ERR_CHECK(  SG_repo__create_user_root_directory(pCtx, pRepo, "@", &pChangeset, &szGidActualRoot_unused)  );
	VERIFY_ERR_CHECK(  SG_changeset__create_dagnode(pCtx, pChangeset,&pDagnode)  );
#else
	// we are an initial checkin -- no parents.

	VERIFY_ERR_CHECK(  SG_committing__alloc(pCtx, &pCommitting,pRepo,SG_DAGNUM__VERSION_CONTROL,&q,CSET_VERSION_1)  );
	VERIFY_ERR_CHECK(  MyFn(__create_noise)(pCtx, pCommitting,szCommitCacheName)  );

	// for now, set treenode root to NULL.

	VERIFY_ERR_CHECK(  SG_committing__set_root(pCtx, pCommitting,NULL)  );

	// commit the changeset to the repo and fetch SG_changeset and SG_dagnode pointers
	// which we add to our caches.  if we replace an existing value in the cache, we
	// must free the old pointer.

	err = SG_committing__end(pCommitting,&pChangeset,&pDagnode);
	pCommitting = NULL;		// we no longer own this -- success or fail
	VERIFY_ERR_CHECK(  err  );
#endif

	VERIFY_ERR_CHECK(  SG_rbtree__update__with_assoc(pCtx, gpMyStore->m_pRB_ChangesetCache,szCommitCacheName,
													 pChangeset,(void **)pOldChangeset)  );
	pChangeset = NULL;		// we no longer own this.

	VERIFY_ERR_CHECK(  SG_rbtree__update__with_assoc(pCtx, gpMyStore->m_pRB_DagnodeCache,szCommitCacheName,
													 pDagnode,(void **)pOldDagnode)  );
	pDagnode = NULL;		// we no longer own this.

	// update NameOfLastCommit with our name.

	VERIFY_ERR_CHECK(  SG_string__set__sz(pCtx, gpMyStore->m_pString_NameOfLastCommit,szCommitCacheName)  );

	// fall-thru to common cleanup

fail:
	if (pCommitting)
		SG_ERR_IGNORE(  SG_committing__abort(pCtx, pCommitting)  );
	SG_NULLFREE(pCtx, szGidActualRoot_unused);
	SG_CHANGESET_NULLFREE(pCtx, pChangeset);
	SG_CHANGESET_NULLFREE(pCtx, pOldChangeset);
	SG_DAGNODE_NULLFREE(pCtx, pDagnode);
	SG_DAGNODE_NULLFREE(pCtx, pOldDagnode);
}

void MyScriptCmd(commit)(SG_context * pCtx, const char * szRepoCacheName, const char * szCommitCacheName, const char * szParentCacheName)
{
	// create a new single-parented commit in the given repo.
	//
	// cache it in our changeset and dagnode tables using the given cache name.

	SG_repo * pRepo = NULL;
	SG_committing * pCommitting = NULL;
	SG_changeset * pChangeset = NULL;
	SG_changeset * pOldChangeset = NULL;
	SG_dagnode * pDagnode = NULL;
	SG_dagnode * pOldDagnode = NULL;
	const char * szHidParent;
	const char * szHidChild;
    SG_audit q;


	VERIFY_ERR_CHECK(  MyFn(__get_proper_repo)(pCtx, szRepoCacheName,&pRepo)  );

    VERIFY_ERR_CHECK(  SG_audit__init(pCtx,&q,pRepo,SG_AUDIT__WHEN__NOW, SG_AUDIT__WHO__FROM_SETTINGS)  );

	// convert parent cache name into parent HID.

	VERIFY_ERR_CHECK(  MyFn(__get_id_for_name)(pCtx, szParentCacheName,&szHidParent)  );

	// create changeset with single parent.

	VERIFY_ERR_CHECK(  SG_committing__alloc(pCtx, &pCommitting,pRepo,SG_DAGNUM__VERSION_CONTROL,&q,CSET_VERSION_1)  );
	VERIFY_ERR_CHECK(  MyFn(__create_noise)(pCtx, pCommitting,szCommitCacheName)  );
    VERIFY_ERR_CHECK(  SG_committing__add_parent(pCtx, pCommitting, szHidParent)  );

	// for now, set treenode root to NULL.

	VERIFY_ERR_CHECK(  SG_committing__set_root(pCtx, pCommitting,NULL)  );

	// commit the changeset to the repo and fetch SG_changeset and SG_dagnode pointers
	// which we add to our caches.  if we replace an existing value in the cache, we
	// must free the old pointer.

	SG_committing__end(pCtx,pCommitting,&pChangeset,&pDagnode);
	pCommitting = NULL;		// we no longer own this -- success or fail
	VERIFY_CTX_IS_OK("",pCtx);

	VERIFY_ERR_CHECK(  SG_dagnode__get_id_ref(pCtx, pDagnode,&szHidChild)  );
	INFOP("commit",("Mapped[%s] to [%s]",szCommitCacheName,szHidChild));

	VERIFY_ERR_CHECK(  SG_rbtree__update__with_assoc(pCtx, gpMyStore->m_pRB_ChangesetCache,szCommitCacheName,
													 pChangeset,(void **)pOldChangeset)  );
	pChangeset = NULL;		// we no longer own this.

	VERIFY_ERR_CHECK(  SG_rbtree__update__with_assoc(pCtx, gpMyStore->m_pRB_DagnodeCache,szCommitCacheName,
													 pDagnode,(void **)pOldDagnode)  );
	pDagnode = NULL;		// we no longer own this.

	// update NameOfLastCommit with our name.

	VERIFY_ERR_CHECK(  SG_string__set__sz(pCtx, gpMyStore->m_pString_NameOfLastCommit,szCommitCacheName)  );

	// fall-thru to common cleanup

fail:
	if (pCommitting)
		SG_ERR_IGNORE(  SG_committing__abort(pCtx, pCommitting)  );
	SG_CHANGESET_NULLFREE(pCtx, pChangeset);
	SG_CHANGESET_NULLFREE(pCtx, pOldChangeset);
	SG_DAGNODE_NULLFREE(pCtx, pDagnode);
	SG_DAGNODE_NULLFREE(pCtx, pOldDagnode);
}

void MyScriptCmd(merge2)(SG_context * pCtx, const char * szRepoCacheName,
							 const char * szCommitCacheName,
							 const char * szParentCacheName, const char * szParent2CacheName)
{
	// create a new 2-parent commit in the given repo.
	//
	// cache it in our changeset and dagnode tables using the given cache name.

	SG_repo * pRepo = NULL;
	SG_committing * pCommitting = NULL;
	SG_changeset * pChangeset = NULL;
	SG_changeset * pOldChangeset = NULL;
	SG_dagnode * pDagnode = NULL;
	SG_dagnode * pOldDagnode = NULL;
	const char * szHidParent;
	const char * szHidParent2;
	const char * szHidChild;
    SG_audit q;

	VERIFY_ERR_CHECK(  MyFn(__get_proper_repo)(pCtx, szRepoCacheName,&pRepo)  );

    VERIFY_ERR_CHECK(  SG_audit__init(pCtx,&q,pRepo,SG_AUDIT__WHEN__NOW, SG_AUDIT__WHO__FROM_SETTINGS)  );

	// convert parent cache names into parent HIDs.

	VERIFY_ERR_CHECK(  MyFn(__get_id_for_name)(pCtx, szParentCacheName,&szHidParent)  );
	VERIFY_ERR_CHECK(  MyFn(__get_id_for_name)(pCtx, szParent2CacheName,&szHidParent2)  );

	// create changeset with primary parent and 1 secondary parent.

	VERIFY_ERR_CHECK(  SG_committing__alloc(pCtx, &pCommitting,pRepo,SG_DAGNUM__VERSION_CONTROL,&q,CSET_VERSION_1)  );
	VERIFY_ERR_CHECK(  MyFn(__create_noise)(pCtx, pCommitting,szCommitCacheName)  );
	VERIFY_ERR_CHECK(  SG_committing__add_parent(pCtx, pCommitting,szHidParent)  );
	VERIFY_ERR_CHECK(  SG_committing__add_parent(pCtx, pCommitting,szHidParent2)  );

	// for now, set treenode root to NULL.

	VERIFY_ERR_CHECK(  SG_committing__set_root(pCtx, pCommitting,NULL)  );

	// commit the changeset to the repo and fetch SG_changeset and SG_dagnode pointers
	// which we add to our caches.  if we replace an existing value in the cache, we
	// must free the old pointer.

	SG_committing__end(pCtx,pCommitting,&pChangeset,&pDagnode);
	pCommitting = NULL;		// we no longer own this -- success or fail
	VERIFY_CTX_IS_OK("",pCtx);

	VERIFY_ERR_CHECK(  SG_dagnode__get_id_ref(pCtx, pDagnode,&szHidChild)  );
	INFOP("commit",("Mapped[%s] to [%s]",szCommitCacheName,szHidChild));

	VERIFY_ERR_CHECK(  SG_rbtree__update__with_assoc(pCtx, gpMyStore->m_pRB_ChangesetCache,szCommitCacheName,
													 pChangeset,(void **)pOldChangeset)  );
	pChangeset = NULL;		// we no longer own this.

	VERIFY_ERR_CHECK(  SG_rbtree__update__with_assoc(pCtx, gpMyStore->m_pRB_DagnodeCache,szCommitCacheName,
													 pDagnode,(void **)pOldDagnode)  );
	pDagnode = NULL;		// we no longer own this.

	// update NameOfLastCommit with our name.

	VERIFY_ERR_CHECK(  SG_string__set__sz(pCtx, gpMyStore->m_pString_NameOfLastCommit,szCommitCacheName)  );

	// fall-thru to common cleanup

fail:
	if (pCommitting)
		SG_ERR_IGNORE(  SG_committing__abort(pCtx, pCommitting)  );
	SG_CHANGESET_NULLFREE(pCtx, pChangeset);
	SG_CHANGESET_NULLFREE(pCtx, pOldChangeset);
	SG_DAGNODE_NULLFREE(pCtx, pDagnode);
	SG_DAGNODE_NULLFREE(pCtx, pOldDagnode);
}

void MyScriptCmd(commit_linear)(SG_context * pCtx, const char * szRepoCacheName,
									const char * szPattern, SG_int32 start, SG_int32 count,
									const char * szParentCacheName)
{
	// create a linear sequence of commits.

	char bufName[100];
	SG_int32 k;

	// create pattern-based name or use anonymous name.
	// use the given parent name for the first one.

	if (szPattern)
		SG_ERR_IGNORE(  SG_sprintf(pCtx, bufName,SG_NrElements(bufName),szPattern,start)  );
	else
		SG_ERR_IGNORE(  MyFn(__make_anon_name)(pCtx, bufName,SG_NrElements(bufName))  );

	VERIFY_ERR_CHECK(  MyScriptCmd(commit)(pCtx, szRepoCacheName,bufName,szParentCacheName)  );

	// for subsequent ones, always use the $last for the parent.

	for (k=1; k<count; k++)
	{
		if (szPattern)
			SG_ERR_IGNORE(  SG_sprintf(pCtx, bufName,SG_NrElements(bufName),szPattern,start+k)  );
		else
			SG_ERR_IGNORE(  MyFn(__make_anon_name)(pCtx, bufName,SG_NrElements(bufName))  );

		VERIFY_ERR_CHECK(  MyScriptCmd(commit)(pCtx, szRepoCacheName,bufName,
											   SG_string__sz(gpMyStore->m_pString_NameOfLastCommit))  );
	}

	// fall-thru to common cleanup

fail:
	return;
}

void MyFn(__list_rbtree_callback)(SG_context * pCtx, const char * szHid, SG_UNUSED_PARAM(void * pAssocData), void * pCtxVoid)
{
	SG_string * pString_Error = (SG_string *)pCtxVoid;

	SG_UNUSED(pAssocData);

	// we are given the HID of an actual leaf that we didn't expect.
	// append " +<hid>" to error message.

	VERIFY_ERR_CHECK(  SG_string__append__sz(pCtx, pString_Error," +")  );
	VERIFY_ERR_CHECK(  SG_string__append__sz(pCtx, pString_Error,szHid)  );

	// fall-thru to common cleanup

fail:
	return;
}

void MyFn(__compute_mismatch_set_string)(SG_context * pCtx, SG_string * pString_Errors,
											 SG_varray * pVarray,
											 SG_rbtree * prbtree,
											 SG_uint32 * pnrMismatches)
{
	// walk array, lookup in cache, and then in rbtree and list any nodes
	// that are not in both.
	//
	// we trash prbtree in the process.

	SG_uint32 k, kLimit, nrRemaining;
	SG_uint32 nrMismatches = 0;

	VERIFY_ERR_CHECK(  SG_varray__count(pCtx, pVarray,&kLimit)  );
	for (k=0; k<kLimit; k++)
	{
		const char * szValue_k;
		const char * szHid_k;

		// take cache name in varray[k] and look it up in our cache and get HID.
		VERIFY_ERR_CHECK(  SG_varray__get__sz(pCtx, pVarray,k,&szValue_k)  );
		MyFn(__get_id_for_name)(pCtx, szValue_k,&szHid_k);
		if (SG_context__err_equals(pCtx,SG_ERR_NOT_FOUND))
		{
			SG_context__err_reset(pCtx);
			// we have no info (and no HID) for the value given in the array.
			// append " ?<name>" to error message.
			nrMismatches++;
			VERIFY_ERR_CHECK(  SG_string__append__sz(pCtx, pString_Errors," ?")  );
			VERIFY_ERR_CHECK(  SG_string__append__sz(pCtx, pString_Errors,szValue_k)  );
		}
		else if (SG_context__has_err(pCtx))
		{
			VERIFY_COND("u1000_repo_script MyFn(__compute_mismatch_set_string)",SG_FALSE);
			SG_ERR_RETHROW;
		}
		else if (!prbtree)
		{
			// no rbtree, report varray[k]
			// append " #<name>(<hid>)" to error message.
			nrMismatches++;
			VERIFY_ERR_CHECK(  SG_string__append__sz(pCtx, pString_Errors," #")  );
			VERIFY_ERR_CHECK(  SG_string__append__sz(pCtx, pString_Errors,szValue_k)  );
			VERIFY_ERR_CHECK(  SG_string__append__sz(pCtx, pString_Errors,"(")  );
			VERIFY_ERR_CHECK(  SG_string__append__sz(pCtx, pString_Errors,szHid_k)  );
			VERIFY_ERR_CHECK(  SG_string__append__sz(pCtx, pString_Errors,")")  );
		}
		else
		{
			// use cached HID to try to find it in the rbtree
			SG_rbtree__find(pCtx,prbtree,szHid_k,NULL,NULL);
			if (SG_context__err_equals(pCtx, SG_ERR_NOT_FOUND))
			{
				SG_context__err_reset(pCtx);
				// varray[k] is not in the rbtree.
				// append " -<name>(<hid>)" to error message.
				nrMismatches++;
				VERIFY_ERR_CHECK(  SG_string__append__sz(pCtx, pString_Errors," -")  );
				VERIFY_ERR_CHECK(  SG_string__append__sz(pCtx, pString_Errors,szValue_k)  );
				VERIFY_ERR_CHECK(  SG_string__append__sz(pCtx, pString_Errors,"(")  );
				VERIFY_ERR_CHECK(  SG_string__append__sz(pCtx, pString_Errors,szHid_k)  );
				VERIFY_ERR_CHECK(  SG_string__append__sz(pCtx, pString_Errors,")")  );
			}
			else
			{
				SG_context__err_reset(pCtx);
				// present in both sets, remove it from actual set.
				VERIFY_ERR_CHECK(  SG_rbtree__remove(pCtx, prbtree,szHid_k)  );
			}
		}
	}

	if (prbtree)
	{
		// if there is anything left in the rbtree that were
		// not listed in the varray, append their HIDs to the error message.

		VERIFY_ERR_CHECK(  SG_rbtree__count(pCtx, prbtree,&nrRemaining)  );
		if (nrRemaining > 0)
		{
			nrMismatches += nrRemaining;
			VERIFY_ERR_CHECK(  SG_rbtree__foreach(pCtx, prbtree,MyFn(__list_rbtree_callback),pString_Errors)  );
		}
	}

	*pnrMismatches = nrMismatches;

	// fall-thru to common cleanup

fail:
	return;
}


void MyScriptCmd(assert_leaves)(SG_context * pCtx, const char * szRepoCacheName, SG_varray * pVarrayLeaves)
{
	// fetch the set of leaves that the dag thinks we have and
	// compare it with the list of named leaves that we were
	// given.
	//
	// if all goes well, we should both be in agreement.

	SG_repo * pRepo;
	SG_rbtree * pRB_ActualLeaves = NULL;
	SG_string * pString_Errors = NULL;
	SG_uint32 nrMismatches;

	// build a detailed mismatch message before crapping out.

	VERIFY_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pString_Errors)  );

	// fetch the actual set of leaves that the DAG thinks it has.

	VERIFY_ERR_CHECK(  MyFn(__get_proper_repo)(pCtx, szRepoCacheName,&pRepo)  );
	VERIFY_ERR_CHECK(  SG_repo__fetch_dag_leaves(pCtx, pRepo,SG_DAGNUM__VERSION_CONTROL,&pRB_ActualLeaves)  );

	// for each item in the varray of leaves given, look it up
	// the name cache to get the HID, then look that up in rbtree.
	// assumulate any mismatches in error string.
	// report error if we have any mismatches.
	//
	// this will partially trash pRB_ActualLeaves.

	VERIFY_ERR_CHECK(  MyFn(__compute_mismatch_set_string)(pCtx, pString_Errors,pVarrayLeaves,pRB_ActualLeaves,&nrMismatches)  );

	if (nrMismatches > 0)
	{
		INFOP("assert_leaves",("Mismatch: %s",SG_string__sz(pString_Errors)));
		VERIFY_COND("assert_leaves",SG_FALSE);
		SG_ERR_THROW(SG_ERR_INVALIDARG); // ?what value makes sense here?
	}

	// fall-thru to common cleanup

fail:
	SG_STRING_NULLFREE(pCtx, pString_Errors);
	SG_RBTREE_NULLFREE(pCtx, pRB_ActualLeaves);
}

//////////////////////////////////////////////////////////////////

static void MyFn(dagfrag_callback_build_rbtree)(SG_context * pCtx,
												const SG_dagnode * pDagnodeMember,
												SG_UNUSED_PARAM(SG_dagfrag_state qs),
												void * pVoidCallerData)
{
	const char* pszHid = NULL;
	SG_rbtree* prbDagnodes = (SG_rbtree*)pVoidCallerData;

	SG_UNUSED(qs);

	SG_ERR_CHECK(  SG_dagnode__get_id_ref(pCtx, pDagnodeMember, &pszHid)  );
	SG_ERR_CHECK(  SG_rbtree__add(pCtx, prbDagnodes, pszHid)  );

	return;

fail:
	SG_NULLFREE(pCtx, pszHid);
}

void MyScriptCmd(push_dagfrag)(SG_context * pCtx,
							   const char * szRepoSrcCacheName,
							   const char * szRepoDestCacheName,
							   SG_int64 gen,
							   SG_varray * pVarrayLeaves,
							   SG_varray * pVarrayDisconnected)
{
	// fetch the set of given leaves for n generations and compute a DAGFRAG
	// from the source repo.
	//
	// then try to push it to another repo.
	//
	// we either expect this to succeed or expect some time of known error:
	// [] disconnected -- cannot complete push because graph would become disconnected.
	// [] todo

	SG_repo * pRepoSrc;
	SG_repo * pRepoDest;
	SG_dagfrag * pFrag = NULL;
	SG_string * pString_Errors = NULL;
	SG_uint32 k, kLimit;
	SG_bool bHadError = SG_FALSE;
	SG_bool bConnected = SG_FALSE;
	SG_rbtree * prbMissing = NULL;
	SG_stringarray * psa_added = NULL;
	SG_rbtree * prbActualLeaves = NULL;
    char* psz_repo_id = NULL;
    char* psz_admin_id = NULL;

	SG_client* pClient = NULL;
	SG_rbtree* prbDagnodes = NULL;
	SG_push* pPush = NULL;

	VERIFY_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pString_Errors)  );

	VERIFY_ERR_CHECK(  MyFn(__get_proper_repo)(pCtx, szRepoSrcCacheName,&pRepoSrc)  );
	VERIFY_ERR_CHECK(  MyFn(__get_proper_repo)(pCtx, szRepoDestCacheName,&pRepoDest)  );

    VERIFY_ERR_CHECK(  SG_repo__get_repo_id(pCtx, pRepoSrc, &psz_repo_id)  );
    VERIFY_ERR_CHECK(  SG_repo__get_admin_id(pCtx, pRepoSrc, &psz_admin_id)  );

	VERIFY_ERR_CHECK(  SG_dagfrag__alloc(pCtx, &pFrag,psz_repo_id,psz_admin_id,SG_DAGNUM__VERSION_CONTROL)  );
    SG_NULLFREE(pCtx, psz_repo_id);
    SG_NULLFREE(pCtx, psz_admin_id);

	if (pVarrayLeaves)
	{
		VERIFY_ERR_CHECK(  SG_varray__count(pCtx, pVarrayLeaves,&kLimit)  );
		for (k=0; k<kLimit; k++)
		{
			const char * szValue_k;
			const char * szHid_k;

			VERIFY_ERR_CHECK(  SG_varray__get__sz(pCtx, pVarrayLeaves,k,&szValue_k)  );
			MyFn(__get_id_for_name)(pCtx, szValue_k,&szHid_k);
			if (SG_context__err_equals(pCtx, SG_ERR_NOT_FOUND))
			{
				SG_context__err_reset(pCtx);
				// we have no info (and no HID) for the value given in the array.
				// append " ?<name>" to error message.
				bHadError = SG_TRUE;
				VERIFY_ERR_CHECK(  SG_string__append__sz(pCtx, pString_Errors," ?")  );
				VERIFY_ERR_CHECK(  SG_string__append__sz(pCtx, pString_Errors,szValue_k)  );
			}
			else if (SG_context__has_err(pCtx))
			{
				VERIFY_COND("push_dagfrag",SG_FALSE);
				SG_ERR_RETHROW;
			}
			else
			{
				SG_dagfrag__load_from_repo__one(pCtx, pFrag,pRepoSrc,szHid_k,(SG_int32)gen);
				if (SG_context__err_equals(pCtx, SG_ERR_NOT_FOUND))
				{
					SG_context__err_reset(pCtx);
					bHadError = SG_TRUE;
					VERIFY_ERR_CHECK(  SG_string__append__sz(pCtx, pString_Errors," ?")  );
					VERIFY_ERR_CHECK(  SG_string__append__sz(pCtx, pString_Errors,szHid_k)  );
				}
				else if (SG_context__has_err(pCtx))
				{
					VERIFY_COND("push_dagfrag",SG_FALSE);
					SG_ERR_RETHROW;
				}
			}
		}

		if (bHadError)
		{
			INFOP("push_dagfrag",("Invalid leaves: %s",SG_string__sz(pString_Errors)));
			VERIFY_COND("push_dagfrag",SG_FALSE);
			SG_ERR_THROW(SG_ERR_INVALIDARG); // ?what value makes sense here?
		}
	}
	else
	{
		// no leaves were given in script.  assume the complete set of leaves in the src repo.

		VERIFY_ERR_CHECK(  SG_repo__fetch_dag_leaves(pCtx, pRepoSrc,SG_DAGNUM__VERSION_CONTROL,&prbActualLeaves)  );
		VERIFY_ERR_CHECK(  SG_dagfrag__load_from_repo__multi(pCtx, pFrag,pRepoSrc,prbActualLeaves,(SG_int32)gen)  );
	}

#if 1 && defined(DEBUG)
	{
		SG_string * pStringDump = NULL;
		SG_ERR_IGNORE(  SG_STRING__ALLOC(pCtx, &pStringDump)  );
		SG_ERR_IGNORE(  SG_dagfrag_debug__dump(pCtx, pFrag,"Dagfrag dump",10,pStringDump)  );
		INFOP("Dump",("%s",SG_string__sz(pStringDump)));
		SG_STRING_NULLFREE(pCtx, pStringDump);
	}
#endif
#if 1 && defined(DEBUG)
	{
		SG_string * pStringSerialization = NULL;
		SG_vhash * pvhFrag = NULL;
		SG_dagfrag * pFragDeserialized = NULL;
		SG_bool bEqualFrags;

		VERIFY_ERR_CHECK(  SG_dagfrag__to_vhash__shared(pCtx, pFrag,NULL,&pvhFrag)  );
		SG_ERR_IGNORE(  SG_STRING__ALLOC(pCtx, &pStringSerialization)  );
		SG_ERR_IGNORE(  SG_vhash__to_json(pCtx, pvhFrag,pStringSerialization)  );
		INFOP("DumpSerialization",("%s",SG_string__sz(pStringSerialization)));
		SG_STRING_NULLFREE(pCtx, pStringSerialization);

		VERIFY_ERR_CHECK(  SG_dagfrag__alloc__from_vhash(pCtx, &pFragDeserialized,pvhFrag)  );
		VERIFY_ERR_CHECK(  SG_dagfrag__equal(pCtx, pFrag,pFragDeserialized,&bEqualFrags)  );
		VERIFY_COND("dagfrag serialization round-trip",(bEqualFrags));

		SG_VHASH_NULLFREE(pCtx, pvhFrag);
		SG_DAGFRAG_NULLFREE(pCtx, pFragDeserialized);
	}
#endif

	// dagfrag has been completely constructed.
	// try to insert it into the destination repo.

	VERIFY_ERR_CHECK(  SG_repo__check_dagfrag(pCtx, pRepoDest,pFrag,&bConnected,&prbMissing,&psa_added,NULL)  );
	if ((!bConnected) || pVarrayDisconnected)
	{
		// we have a disconnected fragment so we cannot insert it
		// ---OR--- we were expecting a disconnect error
		// ---OR--- both.

		if (pVarrayDisconnected)
		{
			// if we were given a disconnect-set, see if the disconnect matches what was expected.
			//
			// this trashes prbMissing.

			SG_uint32 nrMismatches;

			VERIFY_ERR_CHECK(  MyFn(__compute_mismatch_set_string)(pCtx, pString_Errors,pVarrayDisconnected,prbMissing,&nrMismatches)  );
			if (nrMismatches > 0)
			{
				INFOP("push_dagfrag",("ExpectedFailure Mismatch: %s",SG_string__sz(pString_Errors)));
				VERIFY_COND("push_dagfrag",SG_FALSE);
				SG_ERR_THROW(SG_ERR_CANNOT_CREATE_SPARSE_DAG);
			}
			else
			{
				// we have a disconnect, but everything was expected.  there's nothing more we can do.
			}
		}
		else
		{
			VERIFY_ERR_CHECK(  SG_rbtree__foreach(pCtx, prbMissing,MyFn(__list_rbtree_callback),pString_Errors)  );
			INFOP("push_dagfrag",("UnexpectedFailure Mismatch: %s",SG_string__sz(pString_Errors)));
			VERIFY_COND("push_dagfrag",SG_FALSE);
			SG_ERR_THROW(SG_ERR_CANNOT_CREATE_SPARSE_DAG);
		}

		goto done;
	}
    else
    {
		VERIFY_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prbDagnodes)  );
		VERIFY_ERR_CHECK(  SG_dagfrag__foreach_member(pCtx, pFrag, MyFn(dagfrag_callback_build_rbtree), prbDagnodes)  );

		VERIFY_ERR_CHECK(  SG_client__open(pCtx, szRepoDestCacheName, NULL_CREDENTIAL, &pClient)  );
		VERIFY_ERR_CHECK(  SG_push__begin(pCtx, pRepoSrc, pClient, &pPush)  );
		VERIFY_ERR_CHECK(  SG_push__add(pCtx, pPush, SG_DAGNUM__VERSION_CONTROL, prbDagnodes));
		VERIFY_ERR_CHECK(  SG_push__commit(pCtx, &pPush, SG_TRUE)  );
		VERIFY_ERR_CHECK(  SG_client__close_free(pCtx, pClient)  );
		pClient = NULL;

		// TODO Some kind of verification here beyond the fact that we got no errors?
    }

	if (psa_added)
	{
		// we actually added some "new to us" nodes in the destination repo.
		// this can happen when we get SG_ERR_OK or an error because we do
		// each individual node insert in a separate transaction.
		//
		// TODO do we want to dump the list of "new to us" nodes and/or compare them with
		// TODO an array given in the test script?

        SG_uint32 count;
        VERIFY_ERR_CHECK(  SG_stringarray__count(pCtx, psa_added, &count)  );
		INFOP("push_dagfrag",("Pushed %d new nodes.",count));
	}

done:
	// fall-thru to common cleanup.

fail:
	SG_STRING_NULLFREE(pCtx, pString_Errors);
	SG_DAGFRAG_NULLFREE(pCtx, pFrag);
	SG_RBTREE_NULLFREE(pCtx, prbMissing);
	SG_STRINGARRAY_NULLFREE(pCtx, psa_added);
	SG_RBTREE_NULLFREE(pCtx, prbActualLeaves);

	if (pPush)
		SG_ERR_IGNORE(  SG_push__abort(pCtx, &pPush)  );
	SG_CLIENT_NULLFREE(pCtx, pClient);
	SG_RBTREE_NULLFREE(pCtx, prbDagnodes);
}

//////////////////////////////////////////////////////////////////

void MyScriptCmd(compute_lca)(SG_context * pCtx, const char * szRepoCacheName,
								  SG_varray * pVarrayLeaves,
								  const char * szLCA_expected,
								  SG_varray * pVarrayLcaChildren,
								  SG_vhash * pVhashSPCA,
								  const char * szExpectFailReason)
{
	// use the given set of leaves and perform a DAGLCA computation.
	// then compare our results with the predicted values in the script.
	//
	// TODO add args to let script specify its guess for the SPCAs and
	// TODO the immediate descendants.

	SG_repo * pRepo;
	SG_string * pString_Errors = NULL;
	SG_uint32 k, kLimit;
	const char * szHid_k;
	const char * szValue_k;
	SG_uint32 nrErrors = 0;
	SG_daglca * pDagLca = NULL;
	const char * szHidLCA_expected = NULL;
	const char * szHidLCA_found = NULL;
	SG_rbtree * prbLCA_children = NULL;
	SG_bool bFound;
	SG_daglca_node_type nodeType;
	SG_int32 generation;
	SG_daglca_iterator * pDagLcaIter = NULL;
	SG_rbtree * prbSPCA_Inverted = NULL;
	SG_rbtree * prbSPCA_children = NULL;
	const char * szHidSPCA_found = NULL;
	const char * szSymbolicNameSPCA = NULL;

	VERIFY_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pString_Errors)  );

	VERIFY_ERR_CHECK(  MyFn(__get_proper_repo)(pCtx, szRepoCacheName,&pRepo)  );

	if (szLCA_expected && (strcmp(szLCA_expected,"*root*") != 0))
	{
		// if the script tries to predict the LCA, we first verify
		// that we actually have a HID for the given symbolic name.
		// (we do not verify the varray of immediate descendants,
		// if given -- that will come when we examine the results.)
		//
		// if the script predicts "*root*", we don't bother looking it up here.

		MyFn(__get_id_for_name)(pCtx, szLCA_expected,&szHidLCA_expected);
		if (SG_context__err_equals(pCtx, SG_ERR_NOT_FOUND))
		{
			SG_context__err_reset(pCtx);
			nrErrors++;
			VERIFY_ERR_CHECK(  SG_string__append__sz(pCtx, pString_Errors," ?")  );
			VERIFY_ERR_CHECK(  SG_string__append__sz(pCtx, pString_Errors,szLCA_expected)  );

			INFOP("compute_lca",("Expected LCA unknown token: %s",SG_string__sz(pString_Errors)));
			VERIFY_COND("compute_lca",SG_FALSE);
			SG_ERR_THROW(  SG_ERR_NOT_FOUND  );
		}
		else
		{
			VERIFY_CTX_IS_OK("compute_lca",pCtx);
		}
	}

	if (pVhashSPCA)
	{
		// if the script tries to predict the set of SPCA nodes, we first
		// verify that we actually have an HID for each SPCA symbolic name.
		// (we do not verify the varray of immediate descendants for each
		// SPCA -- that will come when we match them up with the results.)

		// pVhashSPCA looks like:
		//    spca : { <sn_1> : [ <sn_1a>, <sn_1b> ],
		//             <sn_2> : [ <sn_2a>, ... ],
		//             ...
		//           }
		//
		// the actual lca results iterator will give us a series of HIDs.
		//
		// here we compute an inverted index on the pVhashSPCA using a
		// rbtree.  this looks like:  map<hid,sn> to map the HIDs to the
		// top-level <sn_1>, <sn_2>, ....   we DO NOT bother with the
		// varray of immediate descendants.

		VERIFY_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prbSPCA_Inverted)  );

		VERIFY_ERR_CHECK(  SG_vhash__count(pCtx, pVhashSPCA,&kLimit)  );
		for (k=0; k<kLimit; k++)
		{
			const SG_variant * pVariant;
			VERIFY_ERR_CHECK(  SG_vhash__get_nth_pair(pCtx, pVhashSPCA,k,&szValue_k,&pVariant)  );
			MyFn(__get_id_for_name)(pCtx, szValue_k,&szHid_k);
			if (SG_context__err_equals(pCtx, SG_ERR_NOT_FOUND))
			{
				SG_context__err_reset(pCtx);
				nrErrors++;
				VERIFY_ERR_CHECK(  SG_string__append__sz(pCtx, pString_Errors," ?")  );
				VERIFY_ERR_CHECK(  SG_string__append__sz(pCtx, pString_Errors,szValue_k)  );
			}
			else if (SG_context__has_err(pCtx))
			{
				VERIFY_COND("compute_lca",SG_FALSE);
				SG_ERR_RETHROW;
			}
			else
			{
				VERIFY_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prbSPCA_Inverted,szHid_k,(void *)szValue_k)  );
			}

		}

		if (nrErrors > 0)
		{
			INFOP("compute_lca",("Unknown SPCA node: %s",SG_string__sz(pString_Errors)));
			VERIFY_COND("compute_lca",SG_FALSE);
			SG_ERR_THROW(  SG_ERR_NOT_FOUND  );
		}
	}

	VERIFY_ERR_CHECK(  SG_daglca__alloc(pCtx, &pDagLca,SG_DAGNUM__VERSION_CONTROL,MyFn(my_fetch_dagnodes_from_repo),pRepo)  );

	// the set of leaves given is the set of starting points in
	// the dag that they want to use.  it does not necessarily
	// correspond to what the repo's actual leaves are.

	VERIFY_ERR_CHECK(  SG_varray__count(pCtx, pVarrayLeaves,&kLimit)  );
	for (k=0; k<kLimit; k++)
	{
		// take the symbolic name in varray[k] and look it up in our cache
		// and get the associated HID.

		VERIFY_ERR_CHECK(  SG_varray__get__sz(pCtx, pVarrayLeaves,k,&szValue_k)  );
		MyFn(__get_id_for_name)(pCtx, szValue_k,&szHid_k);
		if (SG_context__err_equals(pCtx, SG_ERR_NOT_FOUND))
		{
			SG_context__err_reset(pCtx);
			// we have no info (and no HID) for the value given in the array.
			// append " ?<name>" to error message.
			nrErrors++;
			VERIFY_ERR_CHECK(  SG_string__append__sz(pCtx, pString_Errors," ?")  );
			VERIFY_ERR_CHECK(  SG_string__append__sz(pCtx, pString_Errors,szValue_k)  );
		}
		else if (SG_context__has_err(pCtx))
		{
			VERIFY_COND("comput_lca",SG_FALSE);
			SG_ERR_RETHROW;
		}
		else
		{
			SG_daglca__add_leaf(pCtx, pDagLca,szHid_k);
			if ((SG_context__err_equals(pCtx, SG_ERR_DAGNODE_ALREADY_EXISTS)) && (strcmp(szExpectFailReason,"duplicate")==0))
			{
				INFOP("compute_lca",("Duplicated leaf[%s,%s]",szValue_k,szHid_k));
				goto quit;
			}
			VERIFY_CTX_IS_OK("DagLca",pCtx);
		}
	}

	if (nrErrors > 0)
	{
		INFOP("compute_lca",("Unknown Requested Leaves: %s",SG_string__sz(pString_Errors)));
		VERIFY_COND("compute_lca",SG_FALSE);
		SG_ERR_THROW(  SG_ERR_NOT_FOUND  );
	}

	// compute the full LCA using only the leaves requested.

	SG_daglca__compute_lca(pCtx, pDagLca);
	if ((SG_context__err_equals(pCtx, SG_ERR_DAGLCA_LEAF_IS_ANCESTOR)) && (strcmp(szExpectFailReason,"ancestor")==0))
	{
		INFOP("DagLca",("One of the given leaves is an ancestor of another."));
		goto quit;
	}
	VERIFY_CTX_IS_OK("compute_lca",pCtx);

#if 1 && defined(DEBUG)
	{
		SG_string * pStringDump = NULL;
		SG_ERR_IGNORE(  SG_STRING__ALLOC(pCtx, &pStringDump)  );
		VERIFY_ERR_CHECK(  SG_daglca_debug__dump(pCtx, pDagLca,"compute_lca",8,pStringDump)  );
		INFOP("Dump",("%s",SG_string__sz(pStringDump)));
		SG_STRING_NULLFREE(pCtx, pStringDump);
	}
#endif

	VERIFY_ERR_CHECK(  SG_daglca__iterator__first(pCtx, &pDagLcaIter,
												  pDagLca,SG_FALSE,
												  &szHidLCA_found,
												  &nodeType,&generation,
												  &prbLCA_children)  );

	INFOP("daglca lca",("Computed LCA: [szHid %s][nodetype %d][generation %d]",
						szHidLCA_found,nodeType,generation));

	if (szHidLCA_expected && (strcmp(szHidLCA_expected,szHidLCA_found) != 0))
	{
		INFOP("daglca lca",("daglca computed lca [%s] not what was expected [%s]",
							szHidLCA_found,szHidLCA_expected));
		nrErrors++;
	}

	if (pVarrayLcaChildren)
	{
		SG_uint32 nrMismatches;

		VERIFY_ERR_CHECK(  MyFn(__compute_mismatch_set_string)(pCtx, pString_Errors,
															   pVarrayLcaChildren,
															   prbLCA_children,
															   &nrMismatches)  );
		if (nrMismatches > 0)
		{
			INFOP("LCA Children",("Mismatch: %s",SG_string__sz(pString_Errors)));

			if (szExpectFailReason && (strcmp(szExpectFailReason,"descendants")==0))
				;
			else
				nrErrors++;
		}
	}

	SG_RBTREE_NULLFREE(pCtx, prbLCA_children);

	if (pVhashSPCA)
	{
		while (1)
		{
			// this gives us the HID of the SPCA and a rbtree of the HIDs
			// of the immediate descendants.

			VERIFY_ERR_CHECK(  SG_daglca__iterator__next(pCtx, pDagLcaIter,
														 &bFound,
														 &szHidSPCA_found,
														 &nodeType,&generation,
														 &prbSPCA_children)  );
			if (!bFound)
				break;

			INFOP("daglca spca",("Computed SPCA: [szHid %s][nodetype %d][generation %d]",
								 szHidSPCA_found,nodeType,generation));

			// lookup HID of the SPCA in the inverted index.

			VERIFY_ERR_CHECK(  SG_rbtree__find(pCtx, prbSPCA_Inverted,szHidSPCA_found,&bFound,(void **)&szSymbolicNameSPCA)  );
			if (!bFound)
			{
				// LCA code computed a SPCA and gave us the HID.  but we could
				// not find that HID in the inverted index.  therefore, no symbolic
				// names in the script predicted this HID.

				INFOP("SPCA",("Unpredicted SPCA: %s",szHidSPCA_found));

				if (szExpectFailReason && (strcmp(szExpectFailReason,"missing_spca") == 0))
					;
				else
					nrErrors++;
			}
			else
			{
				SG_uint32 nrMismatches;
				SG_varray * pVarray;

				// we found the symbolic name in the inverted index.  use it to lookup
				// the entry in the original script SPCA vhash and get the varray of immediate
				// descendants.

				VERIFY_ERR_CHECK(  SG_vhash__get__varray(pCtx, pVhashSPCA,szSymbolicNameSPCA,&pVarray)  );

				// use the normal varray-vs-rbtree to look for mismatches in the set of descendants.

				SG_ERR_IGNORE(  SG_string__clear(pCtx, pString_Errors)  );
				VERIFY_ERR_CHECK(  MyFn(__compute_mismatch_set_string)(pCtx, pString_Errors,
																	   pVarray,prbSPCA_children,
																	   &nrMismatches)  );
				if (nrMismatches > 0)
				{
					INFOP("SPCA children",("Mismatches for [%s(%s)]: %s\n",
										   szSymbolicNameSPCA,szHidSPCA_found,
										   SG_string__sz(pString_Errors)));

					if (szExpectFailReason && (strcmp(szExpectFailReason,"descendants")==0))
						;
					else
						nrErrors++;
				}

				// remove the item from the script SPCA vhash (so that we can later
				// see if they included any that we did not compute).

				VERIFY_ERR_CHECK(  SG_vhash__remove(pCtx, pVhashSPCA,szSymbolicNameSPCA)  );
				VERIFY_ERR_CHECK(  SG_rbtree__remove(pCtx, prbSPCA_Inverted,szHidSPCA_found)  );
			}

			SG_RBTREE_NULLFREE(pCtx, prbSPCA_children);
		}

		// see if there were any left over predictions

		VERIFY_ERR_CHECK(  SG_vhash__count(pCtx, pVhashSPCA,&kLimit)  );
		if (kLimit > 0)
		{
			for (k=0; k<kLimit; k++)
			{
				const SG_variant * pVariant;
				VERIFY_ERR_CHECK(  SG_vhash__get_nth_pair(pCtx, pVhashSPCA,k,&szSymbolicNameSPCA,&pVariant)  );

				INFOP("SPCA",("Incorrectly predicted SPCA: %s",szSymbolicNameSPCA));

				if (szExpectFailReason && (strcmp(szExpectFailReason,"extra_spca") == 0))
					;
				else
					nrErrors++;
			}
		}
	}

	if (nrErrors > 0)
	{
		VERIFY_COND("compute_lca",SG_FALSE);
		SG_ERR_THROW(SG_ERR_UNSPECIFIED);
	}

	// fall-thru to common cleanup

quit:
	SG_context__err_reset(pCtx);

fail:
	SG_DAGLCA_ITERATOR_NULLFREE(pCtx, pDagLcaIter);
	SG_STRING_NULLFREE(pCtx, pString_Errors);
	SG_DAGLCA_NULLFREE(pCtx, pDagLca);
	SG_RBTREE_NULLFREE(pCtx, prbLCA_children);
	SG_RBTREE_NULLFREE(pCtx, prbSPCA_children);
	SG_RBTREE_NULLFREE(pCtx, prbSPCA_Inverted);
}

//////////////////////////////////////////////////////////////////

void MyFn(invoke_a_command)(SG_context * pCtx, SG_UNUSED_PARAM(void * pCtxVoid),
								SG_UNUSED_PARAM(const SG_varray * pva),
								SG_uint32 ndx,
								const SG_variant * pv)
{
	// we get an element of the "commands" varray.
	//
	// our element is a vhash containing the name of the command and
	// the various parameters.
	//
	// all commands have the following parameters in common:
	//    cmd  -- required, a string, the name of the function to call
	//    repo -- a string, a symbolic name to refer to an opened repo.
	//            may be required or optional depending on command.
	//    comment -- optional, a string, a comment message that we may or may not deal with

	SG_vhash * pVhashElement;	// we don't own this.
	const char * szCmd;			// we don't own this.
	const char * szRepo = NULL;
	SG_bool bHasRepo;

	SG_UNUSED(pva);
	SG_UNUSED(pCtxVoid);

	VERIFY_ERR_CHECK(  SG_variant__get__vhash(pCtx, pv,&pVhashElement)  );

	// "cmd" is required.
	VERIFY_ERR_CHECK(  SG_vhash__get__sz(pCtx, pVhashElement,"cmd",&szCmd)  );

	// "repo" may or may not be required.
	VERIFY_ERR_CHECK(  SG_vhash__has(pCtx, pVhashElement,"repo",&bHasRepo)  );
	if (bHasRepo)
		VERIFY_ERR_CHECK(  SG_vhash__get__sz(pCtx, pVhashElement,"repo",&szRepo)  );

	if (strcmp(szCmd,"create_new_repo") == 0)
	{
		// create and initialize a brand new repository.  this will generate a new RepoID,
		// but we don't care about it (because we can't reference it from the script).
		//
		// we allow the following command-specific parameters:
		//    repo -- required
		//    pathname -- optional, a string, a pathname for the <RepoContainerDirectory>
		//                if omitted, a path in /tmp or C:/temp will be used.

		const char * szPathname;

		if (!szRepo)	// "repo" is required
		{
			VERIFY_COND("invoke_a_command",SG_FALSE);
			SG_ERR_THROW(SG_ERR_INVALIDARG);
		}

		// get "pathname"
		VERIFY_ERR_CHECK(  MyFn(__get_repo_pathname)(pCtx, pVhashElement,&szPathname)  );

		INFOP("invoke_a_command",("Command[%d]: %s(%s,%s)",ndx, szCmd,szRepo,szPathname));
		VERIFY_ERR_CHECK(  MyScriptCmd(create_new_repo)(pCtx, szRepo,szPathname)  );
		goto done;
	}
	else if (strcmp(szCmd,"clone_repo") == 0)
	{
		const char* szRepoDest = NULL;
		if (!szRepo)	// "repo" is required
		{
			VERIFY_COND("invoke_a_command",SG_FALSE);
			SG_ERR_THROW(SG_ERR_INVALIDARG);
		}

		// "repodest" is required.
		VERIFY_ERR_CHECK(  SG_vhash__get__sz(pCtx, pVhashElement,"repodest",&szRepoDest)  );


		INFOP("invoke_a_command",("Command[%d]: %s(%s,%s)",ndx, szCmd,szRepo,szRepoDest));
		VERIFY_ERR_CHECK(  MyScriptCmd(clone_repo)(pCtx, szRepo, szRepoDest)  );
		goto done;
	}
	else if (strcmp(szCmd,"set_current_repo") == 0)
	{
		// select the repo with the given symbolic name and make it the
		// current repo.  subsequent commits and etc that do not explicitly
		// name a repo will default to this one.
		//
		// we allow the following command-specific parameters:
		//    repo -- required

		if (!szRepo)	// "repo" is required
		{
			VERIFY_COND("invoke_a_command",SG_FALSE);
			SG_ERR_THROW(SG_ERR_INVALIDARG);
		}

		INFOP("invoke_a_command",("Command[%d]: %s(%s)",ndx, szCmd,szRepo));
		VERIFY_ERR_CHECK(  MyScriptCmd(set_current_repo)(pCtx, szRepo)  );
		goto done;
	}
	else if (strcmp(szCmd,"close_repo") == 0)
	{
		// close the repo with the given symbolic name and remove it from
		// our caches.  this does not affect changeset and dagnodes that
		// we may have gotten from this repo.
		//
		// we allow the following command-specific parameters:
		//    repo -- optional, if omitted, the current repo will be closed.

		INFOP("invoke_a_command",("Command[%d]: %s(%s)",ndx, szCmd,((szRepo) ? szRepo : "<default>")));
		VERIFY_ERR_CHECK(  MyScriptCmd(close_repo)(pCtx, szRepo)  );
		goto done;
	}
	else if (strcmp(szCmd,"initial_commit") == 0)
	{
		// create an initial-commit (one with no parents) in the current
		// or named repo.
		//
		// TODO for now, we just create minimal changesets as we don't have
		// TODO any of the working-folder or pending stuff completed.
		//
		// we allow the following command-specific parameters:
		//    repo -- optional, if omitted, the current repo will be used.
		//    name -- optional, if omitted, a sequence number will be used.

		const char * szName = NULL;
		char bufWork[100];

		// get "name"
		VERIFY_ERR_CHECK(  MyFn(__get_commit_name)(pCtx, pVhashElement,bufWork,SG_NrElements(bufWork),&szName)  );

		INFOP("invoke_a_command",("Command[%d]: %s(%s,%s)",ndx, szCmd,((szRepo) ? szRepo : "<default>"),szName));
		VERIFY_ERR_CHECK(  MyScriptCmd(initial_commit)(pCtx, szRepo,szName)  );
		goto done;
	}
	else if (strcmp(szCmd,"commit") == 0)
	{
		// create a regular single-parent commit.
		//
		// we allow the following command-specific parameters:
		//    name -- optional, a string, if ommitted, a sequence number will be used.
		//    parent -- optional, a string, if omitted, the name of the last commit will be used.

		const char * szName = NULL;
		const char * szParent = NULL;
		char bufWork1[100];
		char bufWork2[100];

		// get "name"
		VERIFY_ERR_CHECK(  MyFn(__get_commit_name)(pCtx, pVhashElement,bufWork1,SG_NrElements(bufWork1),&szName)  );

		// get "parent"
		VERIFY_ERR_CHECK(  MyFn(__get_parent_name)(pCtx, pVhashElement,bufWork2,SG_NrElements(bufWork2),&szParent)  );

		INFOP("invoke_a_command",("Command[%d]: %s(%s,%s,%s)",ndx,szCmd,((szRepo) ? szRepo : "<default>"),szName,szParent));
		VERIFY_ERR_CHECK(  MyScriptCmd(commit)(pCtx, szRepo,szName,szParent)  );
		goto done;
	}
	else if (strcmp(szCmd,"merge2") == 0)
	{
		// create a regular 2-parent commit.
		//
		// we allow the following command-specific parameters:
		//    name -- optional, a string, if ommitted, a sequence number will be used.
		//    parent -- optional, a string, if omitted, the name of the last commit will be used.
		//              this is the primary parent.
		//    parent2 -- required, a string, the name of the secondary parent.

		const char * szName = NULL;
		const char * szParent = NULL;
		const char * szParent2 = NULL;
		char bufWork1[100];
		char bufWork2[100];

		// get "name"
		VERIFY_ERR_CHECK(  MyFn(__get_commit_name)(pCtx, pVhashElement,bufWork1,SG_NrElements(bufWork1),&szName)  );

		// get "parent"
		VERIFY_ERR_CHECK(  MyFn(__get_parent_name)(pCtx, pVhashElement,bufWork2,SG_NrElements(bufWork2),&szParent)  );

		// get "parent2"
		VERIFY_ERR_CHECK(  SG_vhash__get__sz(pCtx, pVhashElement,"parent2",&szParent2)  );

		INFOP("invoke_a_command",("Command[%d]: %s(%s,%s,%s,%s)",
								  ndx,
								  szCmd,((szRepo) ? szRepo : "<default>"),
								  szName,szParent,szParent2));
		VERIFY_ERR_CHECK(  MyScriptCmd(merge2)(pCtx, szRepo,szName,szParent,szParent2)  );
		goto done;
	}
	else if (strcmp(szCmd,"commit_linear") == 0)
	{
		// create a linear sequence of single parents commits from a
		// given starting point.
		//
		// this will produce a single new leaf.
		//
		// we allow the following command-specific parameters:
		//    pattern -- optional, a printf format string containing a single "%d"
		//               if omitted, a sequence number will be generated.
		//    start -- optional, integer, the number of the first node.
		//             if omitted, 0 is used.
		//    count -- optional, integer, the number of node to create.
		//             if omitted, 1 is used.
		//    parent -- optional, a string, the parent of the first node.
		//              if omitted, the name of the last commit will be used.
		//
		// for example "pattern" : "trunk%d", "start" : 1, "count" : 5, "parent" : "x"
		// will create:
		//
		// x <--- trunk1 <--- trunk2 <--- ... <--- trunk5

		const char * szPattern = NULL;
		const char * szParent = NULL;
		char bufWork2[100];
		SG_int64 start = 0;
		SG_int64 count = 1;
		SG_bool bHasPattern, bHasStart, bHasCount;

		// get "pattern"
		VERIFY_ERR_CHECK(  SG_vhash__has(pCtx, pVhashElement,"pattern",&bHasPattern)  );
		if (bHasPattern)
			VERIFY_ERR_CHECK(  SG_vhash__get__sz(pCtx, pVhashElement,"pattern",&szPattern)  );

		// get "parent"
		VERIFY_ERR_CHECK(  MyFn(__get_parent_name)(pCtx, pVhashElement,bufWork2,SG_NrElements(bufWork2),&szParent)  );

		VERIFY_ERR_CHECK(  SG_vhash__has(pCtx, pVhashElement,"start",&bHasStart)  );
		if (bHasStart)
			VERIFY_ERR_CHECK(  SG_vhash__get__int64(pCtx, pVhashElement,"start",&start)  );

		VERIFY_ERR_CHECK(  SG_vhash__has(pCtx, pVhashElement,"count",&bHasCount)  );
		if (bHasCount)
			VERIFY_ERR_CHECK(  SG_vhash__get__int64(pCtx, pVhashElement,"count",&count)  );

		INFOP("invoke_a_command",("Command[%d]: %s(%s,%s,%d,%d,%s)",
								  ndx,
								  szCmd,((szRepo) ? szRepo : "<default>"),
								  szPattern,(SG_int32)start,(SG_int32)count,
								  szParent));
		VERIFY_ERR_CHECK(  MyScriptCmd(commit_linear)(pCtx, szRepo,szPattern,(SG_int32)start,(SG_int32)count,szParent)  );
		goto done;

	}
	else if (strcmp(szCmd,"assert_leaves") == 0)
	{
		// asset that the set of leaves in the dag matches the list we're given.
		//
		// we allow the following command-specific parameters:
		//    leaves -- required, an array of strings of names that we believe are leaves.

		SG_varray * pVarrayLeaves;		// we don't own this.

		// get "leaves" array
		VERIFY_ERR_CHECK(  SG_vhash__get__varray(pCtx, pVhashElement,"leaves",&pVarrayLeaves)  );

		INFOP("invoke_a_command",("Command[%d]: %s(%s,TODO_list_leaves)",ndx,szCmd,((szRepo) ? szRepo : "<default>")));
		VERIFY_ERR_CHECK(  MyScriptCmd(assert_leaves)(pCtx, szRepo,pVarrayLeaves)  );
		goto done;
	}
	else if (strcmp(szCmd,"push_dagfrag") == 0)
	{
		// try to push a fragment of the dag from one repo to another.
		//
		// we allow the following command-specific parameters:
		//    repo     -- optional, the name of the source repo.
		//                if omitted, we use the current repo.
		//    repodest -- required, a string, the name of the destination repo.
		//    leaves   -- optional, an array of strings of names that we want to start from.
		//                (they don't actually need to be leaves).
		//                if omitted, we use the actual leaves.
		//    generations -- optional, an integer, number of generations to put in fragment.
		//                   must be >= 0.
		//                   if omitted, we build the entire graph (==0).
		//    expect_fail_disconnected -- optional, an array of string of names that we expect
		//                                to cause us to fail because the push would cause a
		//                                disconnected graph.
		//                                the test passes if we get an disconnected error, but
		//                                the set matches what we expect.
		//                                the test fails if the set is different or if this
		//                                parameter is omitted and we get an disconnected error.

		SG_int64 gen;
		SG_int_to_string_buffer bufGen;
		SG_bool bHasLeaves, bHasGenerations, bHasDisconnected;
		const char * szRepoDest;
		SG_varray * pVarrayLeaves = NULL;
		SG_varray * pVarrayDisconnected = NULL;

		// "repodest" is required.
		VERIFY_ERR_CHECK(  SG_vhash__get__sz(pCtx, pVhashElement,"repodest",&szRepoDest)  );

		// "leaves" is optional.
		VERIFY_ERR_CHECK(  SG_vhash__has(pCtx, pVhashElement,"leaves",&bHasLeaves)  );
		if (bHasLeaves)
			VERIFY_ERR_CHECK(  SG_vhash__get__varray(pCtx, pVhashElement,"leaves",&pVarrayLeaves)  );

		// "generations" is optional.
		VERIFY_ERR_CHECK(  SG_vhash__has(pCtx, pVhashElement,"generations",&bHasGenerations)  );
		if (bHasGenerations)
			VERIFY_ERR_CHECK(  SG_vhash__get__int64(pCtx, pVhashElement,"generations",&gen)  );
		else
			gen = 0;

		// "expect_fail_disconnected" is optional.
		VERIFY_ERR_CHECK(  SG_vhash__has(pCtx, pVhashElement,"expect_fail_disconnected",&bHasDisconnected)  );
		if (bHasDisconnected)
			VERIFY_ERR_CHECK(  SG_vhash__get__varray(pCtx, pVhashElement,"expect_fail_disconnected",&pVarrayDisconnected)  );

		// TODO other parameters???

		SG_int64_to_sz(gen, bufGen);
		INFOP("invoke_a_command",("Command[%d]: %s(%s,%s,%s,TODO list other fields)",ndx,szCmd,
								  ((szRepo) ? szRepo : "<default>"),szRepoDest,
								  bufGen));
		VERIFY_ERR_CHECK(  MyScriptCmd(push_dagfrag)(pCtx, szRepo,szRepoDest,gen,pVarrayLeaves,pVarrayDisconnected)  );
		goto done;
	}
	else if (strcmp(szCmd,"compute_lca") == 0)
	{
		// run the LCA algorithm on the given set of leaves (which may or may not
		// have anything to do with the actual leaves in the REPO) and see if it
		// computes what we expect.
		//
		// we allow the following command-specific parameters:
		//    leaves -- required, an array of symbolic names for HIDs.
		//
		//    lca -- optional, the symbolic name of the expected LCA.
		//
		//    lca_children -- optional, an array of the symbolic names of the expected
		//                    immediate descendants of the LCA.
		//
		//    expect_fail -- optional, a string containing the reason why the computation
		//                   should fail.
		//                   values: duplicate, ancestor, missing_spca, extra_spca, descendants
		//
		//    spca -- optional, a vhash containing the expected SPCA nodes.
		//            within this vhash, there is a symbolic name key and varray of its children.

		SG_varray * pVarrayLeaves;		// we don't own this.
		SG_bool bHasLCA, bHasLcaChildren, bHasExpectFail, bHasSPCA;
		const char * szLCA = NULL;
		SG_varray * pVarrayLcaChildren = NULL;	// we don't own this.
		const char * szExpectFailReason = NULL;
		SG_vhash * pVhashSPCA = NULL;	// we don't own this.

		// get "leaves" array
		VERIFY_ERR_CHECK(  SG_vhash__get__varray(pCtx, pVhashElement,"leaves",&pVarrayLeaves)  );

		// get "lca" value
		VERIFY_ERR_CHECK(  SG_vhash__has(pCtx, pVhashElement,"lca",&bHasLCA)  );
		if (bHasLCA)
			VERIFY_ERR_CHECK(  SG_vhash__get__sz(pCtx, pVhashElement,"lca",&szLCA)  );

		// get "lca_children" array
		VERIFY_ERR_CHECK(  SG_vhash__has(pCtx, pVhashElement,"lca_children",&bHasLcaChildren)  );
		if (bHasLcaChildren)
			VERIFY_ERR_CHECK(  SG_vhash__get__varray(pCtx, pVhashElement,"lca_children",&pVarrayLcaChildren)  );

		// get "expect_fail" string
		VERIFY_ERR_CHECK(  SG_vhash__has(pCtx, pVhashElement,"expect_fail",&bHasExpectFail)  );
		if (bHasExpectFail)
			VERIFY_ERR_CHECK(  SG_vhash__get__sz(pCtx, pVhashElement,"expect_fail",&szExpectFailReason)  );

		// get "spca" vhash
		VERIFY_ERR_CHECK(  SG_vhash__has(pCtx, pVhashElement,"spca",&bHasSPCA)  );
		if (bHasSPCA)
			VERIFY_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pVhashElement,"spca",&pVhashSPCA)  );

		INFOP("invoke_a_command",("Command[%d]: %s(%s,TODO_list_leaves,lca=%s,TODO_list_expected_children,fail=%s)",
								  ndx,szCmd,((szRepo) ? szRepo : "<default>"),
								  ((szLCA) ? szLCA : "<omitted>"),
								  ((szExpectFailReason) ? szExpectFailReason : "<omitted>")));

		VERIFY_ERR_CHECK(  MyScriptCmd(compute_lca)(pCtx, szRepo,pVarrayLeaves,szLCA,pVarrayLcaChildren,pVhashSPCA,szExpectFailReason)  );
		goto done;
	}
	else
	{
		INFOP("invoke_a_command",("Command[%d]: Unimplemented [%s]",ndx,szCmd));
		VERIFY_COND("invoke_a_command",SG_FALSE);
		SG_ERR_THROW(SG_ERR_NOTIMPLEMENTED);
	}

done:
	// fall-thru to common cleanup

fail:
	return;
}

//////////////////////////////////////////////////////////////////

void MyFn(load_a_script)(SG_context * pCtx, SG_vhash ** ppVhashScript,
							 const SG_pathname * pPathnameScriptDirectory,
							 const char * szScriptName)
{
	// load a script from a file.  we assume it is a file located
	// in <ScriptDirectory>/<ScriptName>.
	//
	// we assume it is a JSON script and represents a top-level vhash.

	SG_pathname * pPathname = NULL;
	SG_file * pFile = NULL;
	SG_uint64 lenFile;
	char * pBuf = NULL;
	SG_vhash * pVhash = NULL;

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathname,pPathnameScriptDirectory,szScriptName)  );

	INFOP("load_a_script",("Loading script: [%s]",SG_pathname__sz(pPathname)));

	VERIFY_ERR_CHECK(  SG_file__open__pathname(pCtx, pPathname,SG_FILE_RDONLY|SG_FILE_OPEN_EXISTING,
											   SG_FSOBJ_PERMS__UNUSED,&pFile)  );
	VERIFY_ERR_CHECK(  SG_file__seek_end(pCtx, pFile,&lenFile)  );
	VERIFY_ERR_CHECK(  SG_file__seek(pCtx, pFile,0)  );

	pBuf = (char *)SG_calloc(1,(SG_uint32)lenFile+1);
	if (!pBuf)
	{
		VERIFY_COND("load_a_script",SG_FALSE);
		SG_ERR_THROW(  SG_ERR_MALLOCFAILED  );
	}

	VERIFY_ERR_CHECK(  SG_file__read(pCtx, pFile,(SG_uint32)lenFile,(SG_byte *)pBuf,NULL)  );
	VERIFY_ERR_CHECK(  SG_VHASH__ALLOC__FROM_JSON(pCtx, &pVhash,pBuf)  );

	*ppVhashScript = pVhash;

	SG_PATHNAME_NULLFREE(pCtx, pPathname);
	SG_ERR_IGNORE(  SG_file__close(pCtx, &pFile)  );
	SG_NULLFREE(pCtx, pBuf);

	return;
fail:
	SG_VHASH_NULLFREE(pCtx, pVhash);
	SG_PATHNAME_NULLFREE(pCtx, pPathname);
	SG_FILE_NULLCLOSE(pCtx, pFile);
	SG_NULLFREE(pCtx, pBuf);
}

void MyFn(run_a_master_script)(SG_context * pCtx, const SG_pathname * pPathnameScriptDirectory,
							   const char * szScriptName)
{
	// given the directory of master scripts and the filename of
	// an individual master script.
	//
	// combine them into an absolute pathname to the script and
	// run it.
	//
	// we assume that a master script is a stand-alone test and
	// that we should initialize our cache before and destroy it
	// afterwards.
	//
	// we assume that the master script is a top-level vhash that
	// contains a varray named "commands" that contains a series
	// of vhashes.  each sub-vhash represents a single command to
 	// invoke.

	SG_vhash * pVhashMasterScript = NULL;
	SG_varray * pVarrayCommands = NULL;			// we do not own this.
	SG_context* pCallersCtx = NULL;

	VERIFY_ERR_CHECK(  MyFn(alloc_store)(pCtx)  );

	VERIFY_ERR_CHECK(  MyFn(load_a_script)(pCtx, &pVhashMasterScript,pPathnameScriptDirectory,szScriptName)  );

	VERIFY_ERR_CHECK(  SG_vhash__get__varray(pCtx, pVhashMasterScript,"commands",&pVarrayCommands)  );


	// Get full stack traces for errors executing commands.
	pCallersCtx = pCtx;
	SG_context__alloc(&pCtx);
	VERIFY_ERR_CHECK(  SG_varray__foreach(pCtx, pVarrayCommands,
										  MyFn(invoke_a_command),
										  NULL)  );

	// fall-thru for common cleanup

fail:
	// report results of this script as a top-level result (shows up in
	// the pass/fail report like all individual unit tests).

	_report_and_exit_script(__FILE__,szScriptName,pCtx);

	// if pCallersCtx is non-null, we allocated our own private SG_context and stuck it in pCtx.
	if (pCallersCtx)
	{
		SG_CONTEXT_NULLFREE(pCtx);
		pCtx = pCallersCtx;
	}

	SG_VHASH_NULLFREE(pCtx, pVhashMasterScript);
	SG_ERR_IGNORE(  MyFn(free_store)(pCtx)  );

	// we don't propagate script errors to the next script
	// or prevent subsequent scripts from running.
}

//////////////////////////////////////////////////////////////////

void MyFn(run_all_callback)(SG_context * pCtx, const char * szKeyFileName, SG_UNUSED_PARAM(void * pAssocData), void * pCtxVoid)
{
	// a SG_rbtree__foreach_callback()

	const SG_pathname * pPathnameScriptDirectory = (const SG_pathname *)pCtxVoid;

	SG_UNUSED(pAssocData);

	SG_ERR_IGNORE(  MyFn(run_a_master_script)(pCtx, pPathnameScriptDirectory,szKeyFileName)  );
}

//////////////////////////////////////////////////////////////////

void MyFn(run_all_master_scripts)(SG_context * pCtx, const SG_pathname * pPathnameScriptDirectory)
{
	// read the given directory and execute all master scripts in it.

	SG_error errReadStat;
	SG_dir * pDir = NULL;
	SG_string * pStringScriptName = NULL;
	SG_rbtree * prbTests = NULL;

	VERIFY_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prbTests)  );
	VERIFY_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pStringScriptName)  );

	VERIFY_ERR_CHECK(  SG_dir__open(pCtx, pPathnameScriptDirectory,&pDir,&errReadStat,pStringScriptName,NULL)  );
	while (SG_IS_OK(errReadStat))
	{
		const char * szString = SG_string__sz(pStringScriptName);
		SG_uint32 lenString = (SG_uint32)strlen(szString);

		// poor-man's filter for "master*.json"

		if ( (strncmp(szString,"master",6) == 0)  &&  (strncmp(szString+lenString-5,".json",5) == 0) )
		{
			VERIFY_ERR_CHECK(  SG_rbtree__add(pCtx, prbTests,szString)  );
		}

		SG_dir__read(pCtx, pDir,pStringScriptName,NULL);
		SG_context__get_err(pCtx, &errReadStat);
		SG_context__err_reset(pCtx);
	}
	if (errReadStat != SG_ERR_NOMOREFILES)
	{
		VERIFY_COND("run_all_master_scripts",SG_FALSE);
		SG_ERR_THROW(errReadStat);
	}

	// run them all -- in alphabetical order.

	VERIFY_ERR_CHECK(  SG_rbtree__foreach(pCtx, prbTests,MyFn(run_all_callback),(void *)pPathnameScriptDirectory)  );

	// fall-thru to common cleanup

fail:
	SG_ERR_IGNORE(  SG_dir__close(pCtx, pDir)  );
	SG_STRING_NULLFREE(pCtx, pStringScriptName);
	SG_RBTREE_NULLFREE(pCtx, prbTests);

	// we don't propagate script errors to the next script
	// or prevent subsequent scripts from running.
}

//////////////////////////////////////////////////////////////////

/**
 * The main test.
 */
MyMain()
{
	SG_pathname * pPathnameScriptDirectory = NULL;

	TEMPLATE_MAIN_START;

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathnameScriptDirectory, pDataDir, "datasets")  );

	BEGIN_TEST(  MyFn(run_all_master_scripts)(pCtx, pPathnameScriptDirectory)  );

	SG_PATHNAME_NULLFREE(pCtx, pPathnameScriptDirectory);

	// fall-thru to common cleanup

fail:
	TEMPLATE_MAIN_END;
}

#undef MyMain
#undef MyDcl
#undef MyFn
#undef MyScriptCmd
#undef MyStoreType
#undef gpMyStore
