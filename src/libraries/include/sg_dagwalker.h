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
 * sg_dagwalker.h
 *
 *  Created on: Mar 1, 2010
 *      Author: jeremy
 */

#ifndef SG_DAGWALKER_H_
#define SG_DAGWALKER_H_

typedef void (SG_dag_walk_callback)(SG_context* pCtx, SG_repo * pRepo, void * pVoidData, SG_dagnode * currentDagnode, SG_rbtree * pDagnodeCache, SG_bool * bContinue);

/**
 * Walk DAG from a single node.
 */
void SG_dagwalker__walk_dag_single(SG_context* pCtx,
								SG_repo* pRepo,
								const char* pszStartWithNodeHid,
								SG_dag_walk_callback* callbackFunction,
								void* callbackData);

/**
 * Walk DAG from a list of nodes.
 */
void SG_dagwalker__walk_dag(SG_context * pCtx, SG_repo * pRepo, SG_stringarray * pStringArrayStartNodes, SG_dag_walk_callback * callbackFunction, void * callbackData);

#endif /* SG_DAGWALKER_H_ */
