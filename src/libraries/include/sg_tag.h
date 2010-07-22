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
 * sg_tag.h
 *
 *  Created on: May 26, 2010
 *      Author: jeremy
 */

#ifndef SG_TAG_H_
#define SG_TAG_H_


void SG_tag__add_tags(SG_context * pCtx, SG_repo * pRepo, SG_pendingtree * pPendingTree, const char* psz_spec_cs, SG_bool bRev, SG_bool bForce, const char** ppszTags, SG_uint32 count_args);

void SG_tag__remove(SG_context * pCtx, SG_repo * pRepo, const char* pszRev, SG_bool bRev, SG_uint32 count_args, const char** paszArgs);

#endif /* SG_TAG_H_ */
