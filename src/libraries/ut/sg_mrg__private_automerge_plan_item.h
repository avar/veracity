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
 * @file sg_mrg__private_automerge_plan_item.h
 *
 * @details
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_MRG__PRIVATE_AUTOMERGE_PLAN_ITEM_H
#define H_SG_MRG__PRIVATE_AUTOMERGE_PLAN_ITEM_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

/**
 * build the temp-file pathname for the ancestor version to be used
 * in this step in the plan.
 *
 * since ancestors are ***ALWAYS*** found in the repo, we will always
 * have a blob-hid for this.
 *
 * allocate a pathname for it in the plan.
 */
void SG_mrg_automerge_plan_item__set_ancestor(SG_context * pCtx,
											  SG_mrg_automerge_plan_item * pItem)
{
	SG_NULLARGCHECK_RETURN(pItem);
	SG_ARGCHECK_RETURN((pItem->pPath_Ancestor==NULL),pItem->pPath_Ancestor);

	SG_ERR_CHECK_RETURN(  SG_mrg__make_automerge_temp_file_pathname(pCtx,pItem,"A",&pItem->pPath_Ancestor)  );
}

/**
 * create a temp-file pathname for fetching the blob for the "mine" version of
 * the file.  remember the blob-hid for later fetching as the plan is executed.
 */
void SG_mrg_automerge_plan_item__set_mine_from_repo(SG_context * pCtx,
													SG_mrg_automerge_plan_item * pItem,
													SG_uint32 kLabel,
													const char * pszHidBlob)
{
	char bufLabelSuffix[100];

	SG_NULLARGCHECK_RETURN(pItem);
	SG_NONEMPTYCHECK_RETURN(pszHidBlob);
	SG_ARGCHECK_RETURN((pItem->pPath_Mine==NULL),pItem->pPath_Mine);

	SG_ERR_CHECK_RETURN(  SG_sprintf(pCtx,bufLabelSuffix,SG_NrElements(bufLabelSuffix),"U%d",kLabel)  );
	SG_ERR_CHECK_RETURN(  SG_mrg__make_automerge_temp_file_pathname(pCtx,pItem,bufLabelSuffix,&pItem->pPath_Mine)  );
	SG_ERR_CHECK_RETURN(  SG_strcpy(pCtx,pItem->bufHidBlob_Mine,SG_NrElements(pItem->bufHidBlob_Mine),pszHidBlob)  );
}

/**
 * The input for the "mine" version in this step comes from a previous sub-merge.
 * For example, one of the "a0_M" files.
 *
 * For these, we just make a copy of the pathname -- and assume that the sub-merge
 * will create the file in a previous step in the plan **BEFORE** this step in the
 * plan is executed.
 */
void SG_mrg_automerge_plan_item__set_mine_from_submerge(SG_context * pCtx,
														SG_mrg_automerge_plan_item * pItem,
														const char * pszPath)
{
	SG_NULLARGCHECK_RETURN(pItem);
	SG_NONEMPTYCHECK_RETURN(pszPath);
	SG_ARGCHECK_RETURN((pItem->pPath_Mine==NULL),pItem->pPath_Mine);

	SG_ERR_CHECK_RETURN(  SG_PATHNAME__ALLOC__SZ(pCtx,&pItem->pPath_Mine,pszPath)  );
}

/**
 * remember the blob-hid for the "yours" version of the file and create a temp-pathname
 * for it and add it to the plan."
 */
void SG_mrg_automerge_plan_item__set_yours_from_repo(SG_context * pCtx,
													 SG_mrg_automerge_plan_item * pItem,
													 SG_uint32 kLabel,
													 const char * pszHidBlob)
{
	char bufLabelSuffix[100];

	SG_NULLARGCHECK_RETURN(pItem);
	SG_NONEMPTYCHECK_RETURN(pszHidBlob);
	SG_ARGCHECK_RETURN((pItem->pPath_Yours==NULL),pItem->pPath_Yours);

	SG_ERR_CHECK_RETURN(  SG_sprintf(pCtx,bufLabelSuffix,SG_NrElements(bufLabelSuffix),"U%d",kLabel)  );
	SG_ERR_CHECK_RETURN(  SG_mrg__make_automerge_temp_file_pathname(pCtx,pItem,bufLabelSuffix,&pItem->pPath_Yours)  );
	SG_ERR_CHECK_RETURN(  SG_strcpy(pCtx,pItem->bufHidBlob_Yours,SG_NrElements(pItem->bufHidBlob_Yours),pszHidBlob)  );
}

void SG_mrg_automerge_plan_item__set_yours_from_submerge(SG_context * pCtx,
														 SG_mrg_automerge_plan_item * pItem,
														 const char * pszPath)
{
	SG_NULLARGCHECK_RETURN(pItem);
	SG_NONEMPTYCHECK_RETURN(pszPath);
	SG_ARGCHECK_RETURN((pItem->pPath_Yours==NULL),pItem->pPath_Yours);

	SG_ERR_CHECK_RETURN(  SG_PATHNAME__ALLOC__SZ(pCtx,&pItem->pPath_Yours,pszPath)  );
}

/**
 * create a placeholder pathname for the auto-merge result for
 * this step in the plan.  the actual file won't be created
 * until if/when we execute the auto-merge plan.
 */
void SG_mrg_automerge_plan_item__set_result(SG_context * pCtx,
											SG_mrg_automerge_plan_item * pItem)
{
	SG_NULLARGCHECK_RETURN(pItem);
	SG_ARGCHECK_RETURN((pItem->pPath_Result==NULL),pItem->pPath_Result);

	SG_ERR_CHECK_RETURN(  SG_mrg__make_automerge_temp_file_pathname(pCtx,pItem,"M",&pItem->pPath_Result)  );
}

void SG_mrg_automerge_plan_item__set_interior_result(SG_context * pCtx,
													 SG_mrg_automerge_plan_item * pItem,
													 SG_uint32 kMerge)
{
	char bufLabelSuffix[100];

	SG_NULLARGCHECK_RETURN(pItem);
	SG_ARGCHECK_RETURN((pItem->pPath_Result==NULL),pItem->pPath_Result);

	SG_ERR_CHECK_RETURN(  SG_sprintf(pCtx,bufLabelSuffix,SG_NrElements(bufLabelSuffix),"m%d",kMerge)  );
	SG_ERR_CHECK_RETURN(  SG_mrg__make_automerge_temp_file_pathname(pCtx,pItem,bufLabelSuffix,&pItem->pPath_Result)  );
}

//////////////////////////////////////////////////////////////////

/**
 * actually fetch the contents of the ancestor/mine/yours blobs
 * from the repo *if* necessary.
 */
void SG_mrg_automerge_plan_item__fetch_files(SG_context * pCtx,
											 SG_mrg_automerge_plan_item * pItem)
{
	SG_ERR_CHECK_RETURN(  SG_mrg__export_to_temp_file(pCtx,pItem->pMrg,
													  pItem->pMrgCSetEntry_Ancestor->bufHid_Blob,
													  pItem->pPath_Ancestor)  );

	if (pItem->bufHidBlob_Mine[0])
		SG_ERR_CHECK_RETURN(  SG_mrg__export_to_temp_file(pCtx,pItem->pMrg,
														  pItem->bufHidBlob_Mine,
														  pItem->pPath_Mine)  );

	if (pItem->bufHidBlob_Yours[0])
		SG_ERR_CHECK_RETURN(  SG_mrg__export_to_temp_file(pCtx,pItem->pMrg,
														  pItem->bufHidBlob_Yours,
														  pItem->pPath_Yours)  );
}

//////////////////////////////////////////////////////////////////

/**
 * we do not remove the files from the disk;
 * we only free the memory.
 */
void SG_mrg_automerge_plan_item__free(SG_context * pCtx,
									  SG_mrg_automerge_plan_item * pItem)
{
	if (!pItem)
		return;

	// we do not own pItem->pMrg.
	// we do not own pItem->pMrgCSetEntry_Ancestor.

	SG_PATHNAME_NULLFREE(pCtx,pItem->pPath_Ancestor);
	SG_PATHNAME_NULLFREE(pCtx,pItem->pPath_Mine);
	SG_PATHNAME_NULLFREE(pCtx,pItem->pPath_Yours);
	SG_PATHNAME_NULLFREE(pCtx,pItem->pPath_Result);

	SG_NULLFREE(pCtx,pItem);
}

/**
 * remove the files from disk and free the item.
 */
void SG_mrg_automerge_plan_item__remove_and_free(SG_context * pCtx,
												 SG_mrg_automerge_plan_item * pItem)
{
	if (!pItem)
		return;

#if 0	// for testing.
	if (pItem->pPath_Ancestor) SG_ERR_IGNORE(  SG_fsobj__remove__pathname(pCtx,pItem->pPath_Ancestor)  );
	if (pItem->pPath_Mine)     SG_ERR_IGNORE(  SG_fsobj__remove__pathname(pCtx,pItem->pPath_Mine)      );
	if (pItem->pPath_Yours)    SG_ERR_IGNORE(  SG_fsobj__remove__pathname(pCtx,pItem->pPath_Yours)     );
	if (pItem->pPath_Result)   SG_ERR_IGNORE(  SG_fsobj__remove__pathname(pCtx,pItem->pPath_Result)    );
#endif

	SG_MRG_AUTOMERGE_PLAN_ITEM_NULLFREE(pCtx,pItem);
}

void SG_mrg_automerge_plan_item__alloc(SG_context * pCtx,
									   SG_mrg * pMrg,
									   SG_mrg_cset_entry * pMrgCSetEntry_Ancestor,
									   SG_mrg_automerge_plan_item ** ppItem)
{
	SG_mrg_automerge_plan_item * pItem = NULL;

	SG_ERR_CHECK_RETURN(  SG_alloc1(pCtx,pItem)  );

	pItem->pMrg = pMrg;
	pItem->pMrgCSetEntry_Ancestor = pMrgCSetEntry_Ancestor;
	pItem->result = SG_MRG_AUTOMERGE_RESULT__NOT_ATTEMPTED;

	SG_ERR_CHECK(  SG_mrg_automerge_plan_item__set_ancestor(pCtx,pItem)  );

	*ppItem = pItem;
	return;

fail:
	SG_MRG_AUTOMERGE_PLAN_ITEM_NULLFREE(pCtx,pItem);
}

//////////////////////////////////////////////////////////////////

#if TRACE_MRG
void SG_mrg_automerge_plan_item_debug__dump(SG_context * pCtx, const SG_mrg_automerge_plan_item * pItem, const char * pszLabel)
{
	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
							   ("AutoMergePlanItem: %s:\n"
								"    Ancestor:      %s\n"
								"        Mine:      %s\n"
								"        Yours:     %s\n"
								"    Result:        %s\n"
								"        Status:    %d\n"),
							   pszLabel,
							   ((pItem->pPath_Ancestor) ? SG_pathname__sz(pItem->pPath_Ancestor) : "(null)"),
							   ((pItem->pPath_Mine)     ? SG_pathname__sz(pItem->pPath_Mine)     : "(null)"),
							   ((pItem->pPath_Yours)    ? SG_pathname__sz(pItem->pPath_Yours)    : "(null)"),
							   ((pItem->pPath_Result)   ? SG_pathname__sz(pItem->pPath_Result)   : "(null)"),
							   pItem->result)  );
}

#endif

//////////////////////////////////////////////////////////////////

void SG_mrg_automerge_plan_item__copy(SG_context * pCtx,
									  SG_mrg_automerge_plan_item ** ppResult,
									  const SG_mrg_automerge_plan_item * pItemSrc)
{
	SG_mrg_automerge_plan_item * pItem_Allocated = NULL;

	SG_NULLARGCHECK_RETURN(pItemSrc);

	SG_ERR_CHECK(  SG_mrg_automerge_plan_item__alloc(pCtx,pItemSrc->pMrg,pItemSrc->pMrgCSetEntry_Ancestor,&pItem_Allocated)  );

	SG_ASSERT(  (pItemSrc->pPath_Ancestor)  );
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__COPY(pCtx,&pItem_Allocated->pPath_Ancestor,pItemSrc->pPath_Ancestor)  );

	SG_ASSERT(  (pItemSrc->pPath_Mine)  );
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__COPY(pCtx,&pItem_Allocated->pPath_Mine,pItemSrc->pPath_Mine)  );

	SG_ASSERT(  (pItemSrc->pPath_Yours)  );
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__COPY(pCtx,&pItem_Allocated->pPath_Yours,pItemSrc->pPath_Yours)  );

	SG_ASSERT(  (pItemSrc->pPath_Result)  );
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__COPY(pCtx,&pItem_Allocated->pPath_Result,pItemSrc->pPath_Result)  );

	pItem_Allocated->result = pItemSrc->result;

	if (pItemSrc->bufHidBlob_Mine[0])
		SG_ERR_CHECK(  SG_strcpy(pCtx,
								 pItem_Allocated->bufHidBlob_Mine,
								 SG_NrElements(pItem_Allocated->bufHidBlob_Mine),
								 pItemSrc->bufHidBlob_Mine)  );
	if (pItemSrc->bufHidBlob_Yours[0])
		SG_ERR_CHECK(  SG_strcpy(pCtx,
								 pItem_Allocated->bufHidBlob_Yours,
								 SG_NrElements(pItem_Allocated->bufHidBlob_Yours),
								 pItemSrc->bufHidBlob_Yours)  );

	*ppResult = pItem_Allocated;
	return;

fail:
	SG_MRG_AUTOMERGE_PLAN_ITEM_NULLFREE(pCtx,pItem_Allocated);
}

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_MRG__PRIVATE_AUTOMERGE_PLAN_ITEM_H
