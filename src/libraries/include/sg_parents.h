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

/*
 * sg_parents.h
 *
 *  Created on: Apr 19, 2010
 *      Author: jeremy
 */

#ifndef SG_PARENTS_H_
#define SG_PARENTS_H_

/**
 * List the parents of a given revision.  It is possible to pass NULL in for the pending tree.  If one is needed, it will be allocated and disposed of.
 *
 * The objects in the pvec_changesets_or_tags are SG_rev_tag_obj.  If multiple are supplied, the parents of each will be
 *
 */
void SG_parents__list(SG_context * pCtx, SG_repo * pRepo, SG_pendingtree * pPendingTree,
		const char * psz_rev_or_tag, SG_bool bRev, const char* pszFilePath, SG_stringarray ** pStringArrayResults);

#endif /* SG_PARENTS_H_ */
