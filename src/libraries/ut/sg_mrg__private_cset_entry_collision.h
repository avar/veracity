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
 * @file sg_mrg_cset_entry_collision.h
 *
 * @details
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_MRG_CSET_ENTRY_COLLISION_H
#define H_SG_MRG_CSET_ENTRY_COLLISION_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

void SG_mrg_cset_entry_collision__alloc(SG_context * pCtx,
										SG_mrg_cset_entry_collision ** ppMrgCSetEntryCollision)
{
	SG_mrg_cset_entry_collision * pMrgCSetEntryCollision = NULL;

	SG_NULLARGCHECK_RETURN(ppMrgCSetEntryCollision);

	SG_ERR_CHECK_RETURN(  SG_alloc1(pCtx,pMrgCSetEntryCollision)  );
	SG_ERR_CHECK(  SG_vector__alloc(pCtx,&pMrgCSetEntryCollision->pVec_MrgCSetEntry,2)  );

	*ppMrgCSetEntryCollision = pMrgCSetEntryCollision;
	return;

fail:
	SG_MRG_CSET_ENTRY_COLLISION_NULLFREE(pCtx,pMrgCSetEntryCollision);
}

void SG_mrg_cset_entry_collision__free(SG_context * pCtx,
									   SG_mrg_cset_entry_collision * pMrgCSetEntryCollision)
{
	if (!pMrgCSetEntryCollision)
		return;

	SG_VECTOR_NULLFREE(pCtx, pMrgCSetEntryCollision->pVec_MrgCSetEntry);		// we do not own the pointers within
	SG_NULLFREE(pCtx, pMrgCSetEntryCollision);
}

//////////////////////////////////////////////////////////////////

void SG_mrg_cset_entry_collision__add_entry(SG_context * pCtx,
											SG_mrg_cset_entry_collision * pMrgCSetEntryCollision,
											SG_mrg_cset_entry * pMrgCSetEntry)
{
	SG_NULLARGCHECK_RETURN(pMrgCSetEntryCollision);
	SG_NULLARGCHECK_RETURN(pMrgCSetEntry);

	SG_ASSERT(  (pMrgCSetEntry->pMrgCSetEntryCollision == NULL)  );

	// TODO consider debug code to ASSERT that this entry is not already in the vector.

	SG_ERR_CHECK_RETURN(  SG_vector__append(pCtx,pMrgCSetEntryCollision->pVec_MrgCSetEntry,pMrgCSetEntry,NULL)  );

	pMrgCSetEntry->pMrgCSetEntryCollision = pMrgCSetEntryCollision;
}

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_MRG_CSET_ENTRY_COLLISION_H
