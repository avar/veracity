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
 * sg_status_formatter.c
 *
 *  Created on: Jun 8, 2010
 *      Author: jeremy
 */


#include "sg.h"

void _status_append_all_entries(SG_context * pCtx, SG_uint32 nDiffSection, SG_vhash * pVHashIn, SG_string * pstrOutput)
{
	SG_varray * pCurrentVarray = NULL;
	SG_vhash * pCurrentVhash = NULL;
	const char * pszSectionName = NULL;
	const char * pszPath = NULL;
	SG_uint32 nEntriesCount = 0;
	SG_uint32 nEntriesIndex = 0;
	SG_int64 nMask = 0;
	SG_string * pstrFlavorText = NULL;
	SG_bool bFound = SG_FALSE;
	SG_bool bHasXAttrChange = SG_FALSE;
	SG_bool bHasAttrChange = SG_FALSE;

	pszSectionName = SG_treediff__get_status_section_name(nDiffSection);
	SG_ERR_CHECK(  SG_vhash__has(pCtx, pVHashIn, pszSectionName, &bFound)  );
	if (bFound == SG_FALSE)
		return;
	SG_vhash__get__varray(pCtx, pVHashIn, pszSectionName, &pCurrentVarray);
	SG_ERR_CHECK(  SG_varray__count(pCtx, pCurrentVarray, &nEntriesCount)  );
	for (nEntriesIndex = 0; nEntriesIndex < nEntriesCount; nEntriesIndex++)
	{
		SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pCurrentVarray, nEntriesIndex, &pCurrentVhash)  );
		SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pCurrentVhash, "path", &pszPath)  );
		SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstrFlavorText)  );
		if (nDiffSection == SG_DIFFSTATUS_FLAGS__MOVED)
		{
			const char * pszOldPath = NULL;
			SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pCurrentVhash, "from", &pszOldPath)  );
			SG_ERR_CHECK(  SG_string__append__format(pCtx, pstrFlavorText, "\n\t\t#was at %s", pszOldPath)  );
		}
		else if (nDiffSection == SG_DIFFSTATUS_FLAGS__RENAMED)
		{
			const char * pszOldName = NULL;
			SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pCurrentVhash, "oldname", &pszOldName)  );
			SG_ERR_CHECK(  SG_string__append__format(pCtx, pstrFlavorText, "\n\t\t#was %s", pszOldName)  );
		}

		SG_ERR_CHECK(  SG_vhash__get__int64(pCtx, pCurrentVhash, "flags", &nMask)  );
		SG_ERR_CHECK(  SG_vhash__has(pCtx, pCurrentVhash, "changed_attr", &bHasAttrChange)  );
		SG_ERR_CHECK(  SG_vhash__has(pCtx, pCurrentVhash, "changed_xattr", &bHasXAttrChange)  );
		if (bHasAttrChange || bHasXAttrChange)
		{
			SG_ERR_CHECK(  SG_string__append__format(pCtx, pstrFlavorText, "\n\t\t#changed attributes")  );
		}
		//Look for any leftover changes.  Always
		if ((nMask ^ nDiffSection) != 0)
		{
			if (nDiffSection == SG_DIFFSTATUS_FLAGS__MODIFIED)
			{
				SG_int64 tmpMask = nMask;
				if (bHasAttrChange)
					tmpMask = tmpMask ^ SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS;
				if (bHasXAttrChange)
					tmpMask = tmpMask ^ SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS;
				if ((tmpMask ^ nDiffSection) != 0)
					SG_ERR_CHECK(  SG_string__append__format(pCtx, pstrFlavorText, "\n\t\t#Multiple changes to this item")  );

			}
			else
			{
				//There are multiple changes to this item.
				SG_ERR_CHECK(  SG_string__append__format(pCtx, pstrFlavorText, "\n\t\t#Multiple changes to this item")  );
			}
		}
		SG_ERR_CHECK(  SG_string__append__format(pCtx, pstrOutput, "%8s:  %s%s\n", pszSectionName, pszPath, SG_string__sz(pstrFlavorText))  );
		SG_STRING_NULLFREE(pCtx, pstrFlavorText);
	}


fail:
	return;
}

void _status_append_all_unresolved_conflicts(SG_context * pCtx, SG_varray * pvaConflicts, SG_string * pstrOutput)
{
	SG_vhash * pCurrentVhash = NULL;
	const char * pszPath = NULL;
	SG_uint32 nEntriesCount = 0;
	SG_uint32 nUnresolvedEntriesCount = 0;
	SG_uint32 nEntriesIndex = 0;
	const char * pszFlavorText = NULL;
	SG_int64 conflictFlags = 0;
	SG_int64 status64 = 0;
	SG_pendingtree_wd_issue_status status;

	if (pvaConflicts == NULL)
		return;
	SG_ERR_CHECK(  SG_varray__count(pCtx, pvaConflicts, &nEntriesCount)  );
	if (nEntriesCount == 0)
		return;

	for (nEntriesIndex = 0; nEntriesIndex < nEntriesCount; nEntriesIndex++)
	{
		// TODO 2010/07/08 WARNING: The repo-path in the ISSUE is somewhat dated.  It is valid
		// TODO            at the time that the MERGE is performed and before any changes are
		// TODO            made to the WD.  It may still be valid, but then again a parent
		// TODO            directory could have been moved/renamed since the MERGE was applied.
		// TODO
		// TODO            The user could have moved/renamed this entry, too.
		// TODO
		// TODO            What we should really do is used the current pendingtree and compute
		// TODO            a fresh repo-path for this GID.

		SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pvaConflicts, nEntriesIndex, &pCurrentVhash)  );
		SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pCurrentVhash, "repopath", &pszPath)  );
		SG_ERR_CHECK(  SG_vhash__get__int64(pCtx, pCurrentVhash, "conflict_flags", &conflictFlags)  );
		SG_ERR_CHECK(  SG_vhash__get__int64(pCtx, pCurrentVhash, "status", &status64)  );
		status = (SG_pendingtree_wd_issue_status)status64;
		if (status & SG_ISSUE_STATUS__MARKED_RESOLVED)
		{
			continue;
		}
		nUnresolvedEntriesCount++;
		if ((conflictFlags & SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_FILE_EDIT__MASK__NOT_OK) != 0)
		{
			//it's a content conflict.

			// TODO Isn't the following just "if (conflictFlags & ~__MASK__NOT_OK) ..." ?

			conflictFlags |= SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_FILE_EDIT__MASK__NOT_OK;
			if ((conflictFlags ^ SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_FILE_EDIT__MASK__NOT_OK) != 0)  //Remove all the file edit masks and see if anything is left.
				pszFlavorText = "\n\t\t#namespace and content conflict";
			else
				pszFlavorText = "\n\t\t#content conflict";
		}
		else
			pszFlavorText = "\n\t\t#namespace conflict";
		SG_ERR_CHECK(  SG_string__append__format(pCtx, pstrOutput, "%8s:  %s%s\n", "Conflict", pszPath, pszFlavorText)  );
	}
	if (nUnresolvedEntriesCount > 0)
		SG_ERR_CHECK(  SG_string__append__sz(pCtx, pstrOutput, "\n\t\t# Use the 'vv resolve' command to view more details \n\t\t# about your conflicts, or to resolve them.\n")  );

fail:
	return;
}


void SG_status_formatter__vhash_to_string(SG_context * pCtx, SG_vhash * pvhTreediffs, SG_varray * pvaConflicts, SG_string ** ppstrOutput)
{

	SG_string * pStrOutputTemp = NULL;

	SG_UNUSED(pvaConflicts);
	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pStrOutputTemp)  );

	SG_ERR_CHECK(  _status_append_all_entries(pCtx, SG_DIFFSTATUS_FLAGS__ADDED, pvhTreediffs, pStrOutputTemp)  );
	SG_ERR_CHECK(  _status_append_all_entries(pCtx, SG_DIFFSTATUS_FLAGS__MODIFIED, pvhTreediffs, pStrOutputTemp)  );
	SG_ERR_CHECK(  _status_append_all_entries(pCtx, SG_DIFFSTATUS_FLAGS__DELETED, pvhTreediffs, pStrOutputTemp)  );
	SG_ERR_CHECK(  _status_append_all_entries(pCtx, SG_DIFFSTATUS_FLAGS__RENAMED, pvhTreediffs, pStrOutputTemp)  );
	SG_ERR_CHECK(  _status_append_all_entries(pCtx, SG_DIFFSTATUS_FLAGS__MOVED, pvhTreediffs, pStrOutputTemp)  );
	SG_ERR_CHECK(  _status_append_all_entries(pCtx, SG_DIFFSTATUS_FLAGS__LOST, pvhTreediffs, pStrOutputTemp)  );
	SG_ERR_CHECK(  _status_append_all_entries(pCtx, SG_DIFFSTATUS_FLAGS__FOUND, pvhTreediffs, pStrOutputTemp)  );

	SG_ERR_CHECK(  _status_append_all_unresolved_conflicts(pCtx, pvaConflicts, pStrOutputTemp)  );

	*ppstrOutput = pStrOutputTemp;
	return;
	
fail:
	SG_STRING_NULLFREE(pCtx, pStrOutputTemp);
}
