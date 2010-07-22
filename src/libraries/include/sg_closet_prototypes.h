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
 * @file sg_closet_prototypes.h
 *
 * @details The Closet is a private place where Sprawl can store
 * information that is specific to a single user.
 *
 * In the initial design, the Closet is a directory within the user's
 * home directory, and any database-oriented stuff in the closet is
 * intended to be in sqlite db files within that directory.
 *
 * But the Closet API is designed to hide the details of how/where the
 * Closet is stored.  This helps the notion of the Closet stay more
 * cross-platform, as the Closet will appear in a different place on a
 * Windows system than it does on a Linux system.
 *
 * In the future, we may decide to implement the Closet differently,
 * such as in a PostgreSQL database.  Hopefully this API will hide the
 * details.
 *
 * The Closet is the place to store the following things:
 *
 * Repo Instance Descriptors
 *
 * REPO instances
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_CLOSET_PROTOTYPES_H
#define H_SG_CLOSET_PROTOTYPES_H

BEGIN_EXTERN_C;

/**
 * Add (or overwrite an existing) repo instance descriptor.
 */
void SG_closet__descriptors__add(
	SG_context * pCtx,
	const char* pszName,
	const SG_vhash* pvhDescriptor
	);

/**
 * Get an existing repo instance descriptor by name.
 * Fetching a name that does not exist is an error.
 */
void SG_closet__descriptors__get(
	SG_context * pCtx,
	const char* pszName,
	SG_vhash** ppResult /**< Caller must free this */
	);

/**
 * Determine if a descriptor by the provided name exists.
 */
void SG_closet__descriptors__exists(SG_context* pCtx, const char* pszName, SG_bool* pbExists);

/**
 * Delete an existing repo instance descriptor by name.  If the given
 * name does not exist, this function will return SG_ERR_OK and
 * effectively do nothing.
 */
void SG_closet__descriptors__remove(
	SG_context * pCtx,
	const char* pszName
	);

/**
 * List all repo instance descriptors.  The list is returned as a
 * vhash.  The keys of the vhash are the names.  The value for each
 * key is another vhash containing that repo instance descriptor.
 */
void SG_closet__descriptors__list(
	SG_context * pCtx,
	SG_vhash** ppResult /**< Caller must free this */
	);

/**
 * The closet can contain repo instances.  Use this function to obtain
 * the partial repo instance descriptor necessary to create a repo
 * instance in the closet.
 *
 */
void SG_closet__get_partial_repo_instance_descriptor_for_new_local_repo(
	SG_context * pCtx,
	SG_vhash** ppvhDescriptor
	);

void SG_closet__get_localsettings(
	SG_context * pCtx,
	SG_jsondb** ppJsondb
    );

END_EXTERN_C;

#endif //H_SG_CLOSET_PROTOTYPES_H
