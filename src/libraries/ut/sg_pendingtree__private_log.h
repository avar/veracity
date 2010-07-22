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
 * @file sg_pendingtree__private_log.h
 *
 * @details
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_PENDINGTREE__PRIVATE_LOG_H
#define H_SG_PENDINGTREE__PRIVATE_LOG_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

static void _pt_log__move(SG_context * pCtx,
						  SG_pendingtree * pPendingTree,
						  const char * pszGid,
						  const char * pszPathSrc,
						  const char * pszPathDest,
						  const char * pszReason)
{
	SG_vhash * pvhItem = NULL;

	SG_NULLARGCHECK_RETURN(pPendingTree);
	SG_NONEMPTYCHECK_RETURN(pszGid);
	SG_NONEMPTYCHECK_RETURN(pszPathSrc);
	SG_NONEMPTYCHECK_RETURN(pszPathDest);
	// pszReason is optional

	if (!pPendingTree->pvaActionLog)
		SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pPendingTree->pvaActionLog)  );

	SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvhItem)  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "action",        "move"     )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "gid",           pszGid     )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "source",        pszPathSrc )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "destination",   pszPathDest)  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "reason",        pszReason  )  );

	SG_ERR_CHECK(  SG_varray__append__vhash(pCtx, pPendingTree->pvaActionLog, &pvhItem)  );

	return;

fail:
	SG_VHASH_NULLFREE(pCtx, pvhItem);
}

static void _pt_log__mkdir(SG_context * pCtx,
						   SG_pendingtree * pPendingTree,
						   const char * pszGid,
						   const char * pszPathDir,
						   const char * pszReason)
{
	SG_vhash * pvhItem = NULL;

	SG_NULLARGCHECK_RETURN(pPendingTree);
	SG_NONEMPTYCHECK_RETURN(pszGid);
	SG_NONEMPTYCHECK_RETURN(pszPathDir);
	// pszReason is optional

	if (!pPendingTree->pvaActionLog)
		SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pPendingTree->pvaActionLog)  );

	SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvhItem)  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "action",        "mkdir"    )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "gid",           pszGid     )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "destination",   pszPathDir )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "reason",        pszReason  )  );

	SG_ERR_CHECK(  SG_varray__append__vhash(pCtx, pPendingTree->pvaActionLog, &pvhItem)  );

	return;

fail:
	SG_VHASH_NULLFREE(pCtx, pvhItem);
}


static void _pt_log__add_item(SG_context * pCtx,
						   SG_pendingtree * pPendingTree,
						   const char * pszGid,
						   const char * pszPathDir,
						   const char * pszReason)
{
	SG_vhash * pvhItem = NULL;

	SG_NULLARGCHECK_RETURN(pPendingTree);
	SG_NONEMPTYCHECK_RETURN(pszGid);
	SG_NONEMPTYCHECK_RETURN(pszPathDir);
	// pszReason is optional

	if (!pPendingTree->pvaActionLog)
		SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pPendingTree->pvaActionLog)  );

	SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvhItem)  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "action",        "add"    )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "gid",           pszGid     )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "destination",   pszPathDir )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "reason",        pszReason  )  );

	SG_ERR_CHECK(  SG_varray__append__vhash(pCtx, pPendingTree->pvaActionLog, &pvhItem)  );

	return;

fail:
	SG_VHASH_NULLFREE(pCtx, pvhItem);
}

static void _pt_log__remove_item(SG_context * pCtx,
						   SG_pendingtree * pPendingTree,
						   const char * pszGid,
						   const char * pszPathDir,
						   const char * pszReason)
{
	SG_vhash * pvhItem = NULL;

	SG_NULLARGCHECK_RETURN(pPendingTree);
	SG_NONEMPTYCHECK_RETURN(pszGid);
	SG_NONEMPTYCHECK_RETURN(pszPathDir);
	// pszReason is optional

	if (!pPendingTree->pvaActionLog)
		SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pPendingTree->pvaActionLog)  );

	SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvhItem)  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "action",        "remove"    )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "gid",           pszGid     )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "destination",   pszPathDir )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "reason",        pszReason  )  );

	SG_ERR_CHECK(  SG_varray__append__vhash(pCtx, pPendingTree->pvaActionLog, &pvhItem)  );

	return;

fail:
	SG_VHASH_NULLFREE(pCtx, pvhItem);
}

static void _pt_log__rmdir(SG_context * pCtx,
						   SG_pendingtree * pPendingTree,
						   const char * pszGid,
						   const char * pszPathDir,
						   const char * pszReason)
{
	SG_vhash * pvhItem = NULL;

	SG_NULLARGCHECK_RETURN(pPendingTree);
	SG_NONEMPTYCHECK_RETURN(pszGid);
	SG_NONEMPTYCHECK_RETURN(pszPathDir);
	// pszReason is optional

	if (!pPendingTree->pvaActionLog)
		SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pPendingTree->pvaActionLog)  );

	SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvhItem)  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "action",        "rmdir"    )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "gid",           pszGid     )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "destination",   pszPathDir )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "reason",        pszReason  )  );

	SG_ERR_CHECK(  SG_varray__append__vhash(pCtx, pPendingTree->pvaActionLog, &pvhItem)  );

	return;

fail:
	SG_VHASH_NULLFREE(pCtx, pvhItem);
}

static void _pt_log__attrbits(SG_context * pCtx,
							  SG_pendingtree * pPendingTree,
							  const char * pszGid,
							  const char * pszPath,
							  SG_int64 attrBits,
							  const char * pszReason)
{
	SG_vhash * pvhItem = NULL;

	SG_NULLARGCHECK_RETURN(pPendingTree);
	SG_NONEMPTYCHECK_RETURN(pszGid);
	SG_NONEMPTYCHECK_RETURN(pszPath);
	// pszReason is optional

	if (!pPendingTree->pvaActionLog)
		SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pPendingTree->pvaActionLog)  );

	// TODO right now we only have one bit defined for files.
	// TODO do we want to convert "attrbits" from SG_ATTRBITS_BIT__* to "+x" or "-x"
	// TODO or do we want to leave like this for now?

	SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvhItem)  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "action",        "attrbits" )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "gid",           pszGid     )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "destination",   pszPath    )  );
	SG_ERR_CHECK(  SG_vhash__add__int64(     pCtx, pvhItem, "attrbits",      attrBits   )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "reason",        pszReason  )  );

	SG_ERR_CHECK(  SG_varray__append__vhash(pCtx, pPendingTree->pvaActionLog, &pvhItem)  );

	return;

fail:
	SG_VHASH_NULLFREE(pCtx, pvhItem);
}

static void _pt_log__xattrs(SG_context * pCtx,
							SG_pendingtree * pPendingTree,
							const char * pszGid,
							const char * pszPath,
							const char * pszHidXAttrs,
							SG_vhash * pvhXAttrs,
							const char * pszReason)
{
	SG_vhash * pvhItem = NULL;
	SG_vhash * pvhXAttrsCopy = NULL;

	SG_NULLARGCHECK_RETURN(pPendingTree);
	SG_NONEMPTYCHECK_RETURN(pszGid);
	SG_NONEMPTYCHECK_RETURN(pszPath);
	// pszHidXAttrs is non-null when we setting XATTRs on a file; NULL when we are removing all XATTRs
	SG_ARGCHECK_RETURN(  ((pszHidXAttrs && *pszHidXAttrs) == (pvhXAttrs != NULL)),  pszHidXAttrs  );
	// pszReason is optional

	if (!pPendingTree->pvaActionLog)
		SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pPendingTree->pvaActionLog)  );

	if (pvhXAttrs)
		SG_ERR_CHECK(  SG_VHASH__ALLOC__COPY(pCtx,&pvhXAttrsCopy,pvhXAttrs)  );

	SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvhItem)  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "action",        "setxattrs"  )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "gid",           pszGid       )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "destination",   pszPath      )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "hidxattr",      pszHidXAttrs )  );
	SG_ERR_CHECK(  SG_vhash__add__vhash(     pCtx, pvhItem, "xattrs",       &pvhXAttrsCopy)  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "reason",        pszReason    )  );

	SG_ERR_CHECK(  SG_varray__append__vhash(pCtx, pPendingTree->pvaActionLog, &pvhItem)  );

	return;

fail:
	SG_VHASH_NULLFREE(pCtx, pvhItem);
	SG_VHASH_NULLFREE(pCtx, pvhXAttrsCopy);
}

static void _pt_log__restore_file(SG_context * pCtx,
								  SG_pendingtree * pPendingTree,
								  const char * pszGid,
								  const char * pszPath,
								  const char * pszHidContent,
								  const char * pszReason)
{
	SG_vhash * pvhItem = NULL;

	SG_NULLARGCHECK_RETURN(pPendingTree);
	SG_NONEMPTYCHECK_RETURN(pszGid);
	SG_NONEMPTYCHECK_RETURN(pszPath);
	SG_NONEMPTYCHECK_RETURN(pszHidContent);
	// pszReason is optional

	if (!pPendingTree->pvaActionLog)
		SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pPendingTree->pvaActionLog)  );

	SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvhItem)  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "action",        "restore_file")  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "gid",           pszGid        )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "destination",   pszPath       )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "hidcontent",    pszHidContent )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "reason",        pszReason     )  );

	SG_ERR_CHECK(  SG_varray__append__vhash(pCtx, pPendingTree->pvaActionLog, &pvhItem)  );

	return;

fail:
	SG_VHASH_NULLFREE(pCtx, pvhItem);
}

static void _pt_log__restore_symlink(SG_context * pCtx,
									 SG_pendingtree * pPendingTree,
									 const char * pszGid,
									 const char * pszPath,
									 const char * pszLink,
									 const char * pszReason)
{
	SG_vhash * pvhItem = NULL;

	SG_NULLARGCHECK_RETURN(pPendingTree);
	SG_NONEMPTYCHECK_RETURN(pszGid);
	SG_NONEMPTYCHECK_RETURN(pszPath);
	SG_NONEMPTYCHECK_RETURN(pszLink);
	// pszReason is optional

	if (!pPendingTree->pvaActionLog)
		SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pPendingTree->pvaActionLog)  );

	SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvhItem)  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "action",        "restore_symlink")  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "gid",           pszGid           )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "destination",   pszPath          )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "symlink",       pszLink          )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "reason",        pszReason        )  );

	SG_ERR_CHECK(  SG_varray__append__vhash(pCtx, pPendingTree->pvaActionLog, &pvhItem)  );

	return;

fail:
	SG_VHASH_NULLFREE(pCtx, pvhItem);
}

static void _pt_log__delete_file(SG_context * pCtx,
								 SG_pendingtree * pPendingTree,
								 const char * pszGid,
								 const char * pszPath,
								 const char * pszReason)
{
	SG_vhash * pvhItem = NULL;

	SG_NULLARGCHECK_RETURN(pPendingTree);
	SG_NONEMPTYCHECK_RETURN(pszGid);
	SG_NONEMPTYCHECK_RETURN(pszPath);
	// pszReason is optional

	if (!pPendingTree->pvaActionLog)
		SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pPendingTree->pvaActionLog)  );

	SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvhItem)  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "action",        "delete_file" )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "gid",           pszGid        )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "destination",   pszPath       )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "reason",        pszReason     )  );

	SG_ERR_CHECK(  SG_varray__append__vhash(pCtx, pPendingTree->pvaActionLog, &pvhItem)  );

	return;

fail:
	SG_VHASH_NULLFREE(pCtx, pvhItem);
}

static void _pt_log__delete_symlink(SG_context * pCtx,
									SG_pendingtree * pPendingTree,
									const char * pszGid,
									const char * pszPath,
									const char * pszReason)
{
	SG_vhash * pvhItem = NULL;

	SG_NULLARGCHECK_RETURN(pPendingTree);
	SG_NONEMPTYCHECK_RETURN(pszGid);
	SG_NONEMPTYCHECK_RETURN(pszPath);
	// pszReason is optional

	if (!pPendingTree->pvaActionLog)
		SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pPendingTree->pvaActionLog)  );

	SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvhItem)  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "action",        "delete_symlink" )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "gid",           pszGid           )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "destination",   pszPath          )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "reason",        pszReason        )  );

	SG_ERR_CHECK(  SG_varray__append__vhash(pCtx, pPendingTree->pvaActionLog, &pvhItem)  );

	return;

fail:
	SG_VHASH_NULLFREE(pCtx, pvhItem);
}

//////////////////////////////////////////////////////////////////

void SG_pendingtree__get_action_log(SG_context * pCtx,
									SG_pendingtree * pPendingTree,
									SG_varray ** ppResult)
{
	SG_NULLARGCHECK_RETURN(pPendingTree);
	SG_NULLARGCHECK_RETURN(ppResult);

    *ppResult = pPendingTree->pvaActionLog;
}

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_PENDINGTREE__PRIVATE_LOG_H
