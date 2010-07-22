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
 * @file sg_mrg__private_cset_stats.h
 *
 * @details
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_MRG__PRIVATE_CSET_STATS_H
#define H_SG_MRG__PRIVATE_CSET_STATS_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

void SG_mrg_cset_stats__free(SG_context * pCtx, SG_mrg_cset_stats * pMrgCSetStats)
{
	SG_NULLFREE(pCtx, pMrgCSetStats);
}

void SG_mrg_cset_stats__alloc(SG_context * pCtx, SG_mrg_cset_stats ** ppMrgCSetStats)
{
	SG_mrg_cset_stats * pMrgCSetStats = NULL;

	SG_NULLARGCHECK_RETURN(ppMrgCSetStats);

	SG_ERR_CHECK_RETURN(  SG_alloc1(pCtx,pMrgCSetStats)  );

	*ppMrgCSetStats = pMrgCSetStats;
}

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_MRG__PRIVATE_CSET_STATS_H
