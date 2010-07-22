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
 * @file sg_pendingtree__private_resolve_issues.h
 *
 * @details 
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_PENDINGTREE__PRIVATE_RESOLVE_ISSUES_H
#define H_SG_PENDINGTREE__PRIVATE_RESOLVE_ISSUES_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

void SG_pendingtree__format_issue(SG_context * pCtx,
								  SG_pendingtree * pPendingTree,
								  const SG_vhash * pvhIssue,
								  SG_string * pStrOutput,
								  SG_string ** ppStrRepoPath_Returned)
{
	SG_string * pStrRepoPath = NULL;
	SG_string * pStrRepoPath_k = NULL;
	const char * pszGid;
	SG_int64 i64;
	SG_pendingtree_wd_issue_status status;
	SG_mrg_cset_entry_conflict_flags conflict_flags;
	SG_portability_flags portability_flags;
	SG_bool bResolved;
	SG_bool bCollisions;
	SG_bool bMadeUnique;

	SG_NULLARGCHECK_RETURN( pPendingTree );
	SG_NULLARGCHECK_RETURN( pvhIssue );
	SG_NULLARGCHECK_RETURN( pStrOutput );
	// ppStrRepoPath_Returned is optional

	SG_ERR_CHECK_RETURN(  SG_vhash__get__int64(pCtx, pvhIssue, "status", &i64)  );
	status = (SG_pendingtree_wd_issue_status)i64;
	bResolved = ((status & SG_ISSUE_STATUS__MARKED_RESOLVED) == SG_ISSUE_STATUS__MARKED_RESOLVED);

	SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvhIssue, "gid", &pszGid)  );
#if 0 && defined(DEBUG)
	SG_ERR_IGNORE(  SG_vhash_debug__dump_to_console__named(pCtx, pvhIssue, pszGid)  );
	SG_ERR_IGNORE(  SG_pendingtree_debug__dump_existing_to_console(pCtx, pPendingTree)  );
#endif
	SG_ERR_CHECK(  SG_pendingtree__find_repo_path_by_gid(pCtx, pPendingTree, pszGid, &pStrRepoPath)  );

	SG_ERR_CHECK(  SG_string__append__format(pCtx, pStrOutput,
											 "%s  %s\n",
											 ((bResolved) ? "Resolved:  " : "Unresolved:"),
											 SG_string__sz(pStrRepoPath))  );

	SG_ERR_CHECK(  SG_vhash__get__bool(pCtx, pvhIssue, "made_unique_entryname", &bMadeUnique)  );
	if (bMadeUnique)
	{
		// because of a conflicts, collisions, or potential collisions we had to temporarily
		// rename the entry with a unique name.

		SG_ERR_CHECK(  SG_string__append__format(pCtx, pStrOutput,
												 "\t\t# The entry was given a temporary name pending resolution.\n")  );
	}

	SG_ERR_CHECK(  SG_vhash__get__int64(pCtx, pvhIssue, "conflict_flags", &i64)  );
	conflict_flags = (SG_mrg_cset_entry_conflict_flags)i64;
	if (conflict_flags)
	{
		// TODO 2010/07/10 Decide how we want to nicely explain the conflict.
		// TODO            For now just dump the (debug) symbols stored in the
		// TODO            issue.  Later we may want to use the actual flags and
		// TODO            make a tighter message.  For example, instead of just
		// TODO            listing: REMOVE-vs-MOVE and REMOVE-vs-RENAME on 2 lines,
		// TODO            we could use the flags and the "conflict_deletes" info
		// TODO            to format something like:
		// TODO                 "%s was removed in the baseline but moved/renamed
		// TODO                  to %s in the other branch ..."
		// TODO            and then include the new destination pathname (dynamically
		// TODO            computed).  Note that there may be a lack of symmetry in
		// TODO            the message because when we have a conflict like this, we
		// TODO            try to leave the entry where it was in the baseline.

		const SG_varray * pvaConflictReasons;
		SG_uint32 k, count;

		SG_ERR_CHECK(  SG_string__append__format(pCtx, pStrOutput,
												 "\t\t# Conflicting changes were made to the entry:\n")  );

		SG_ERR_CHECK(  SG_vhash__get__varray(pCtx, pvhIssue, "conflict_reasons", (SG_varray **)&pvaConflictReasons)  );
		SG_ERR_CHECK(  SG_varray__count(pCtx, pvaConflictReasons, &count)  );
		for (k=0; k<count; k++)
		{
			const char * psz_k;

			SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pvaConflictReasons, k, &psz_k)  );
			SG_ERR_CHECK(  SG_string__append__format(pCtx, pStrOutput,
													 "\t\t#\t%s\n", psz_k)  );
		}
	}

	SG_ERR_CHECK(  SG_vhash__get__bool(pCtx, pvhIssue, "collision_flags", &bCollisions)  );
	if (bCollisions)
	{
		// An exact match entryname collision with one or more other entries.
		// We don't really need to list the other entrynames because they're
		// all the same (and they will each appear in their own issue), but
		// we do so that we can show them the make-unique name that we gave it.

		const SG_varray * pvaCollisions;
		SG_uint32 k, count;

		SG_ERR_CHECK(  SG_vhash__get__varray(pCtx, pvhIssue, "collisions", (SG_varray **)&pvaCollisions)  );
		SG_ERR_CHECK(  SG_varray__count(pCtx, pvaCollisions, &count)  );

		SG_ERR_CHECK(  SG_string__append__format(pCtx, pStrOutput,
												 "\t\t# The entry collided with:\n")  );
		for (k=0; k<count; k++)
		{
			const char * pszGid_k;

			SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pvaCollisions, k, &pszGid_k)  );
			if (strcmp(pszGid_k, pszGid) != 0)
			{
				SG_ERR_CHECK(  SG_pendingtree__find_repo_path_by_gid(pCtx, pPendingTree, pszGid_k, &pStrRepoPath_k)  );
				SG_ERR_CHECK(  SG_string__append__format(pCtx, pStrOutput,
														 "\t\t#\t%s\n",
														 SG_string__sz(pStrRepoPath_k))  );
				SG_STRING_NULLFREE(pCtx, pStrRepoPath_k);
			}
		}
	}

	SG_ERR_CHECK(  SG_vhash__get__int64(pCtx, pvhIssue, "portability_flags", &i64)  );
	portability_flags = (SG_portability_flags)i64;
	if (portability_flags)
	{
		// There were POTENTIAL entryname collisions with one or more other entries.

		const SG_varray * pvaPortabilityDetails;
		SG_uint32 k, count;

		SG_ERR_CHECK(  SG_vhash__get__varray(pCtx, pvhIssue, "portability_details", (SG_varray **)&pvaPortabilityDetails)  );
		SG_ERR_CHECK(  SG_varray__count(pCtx, pvaPortabilityDetails, &count)  );

		SG_ERR_CHECK(  SG_string__append__format(pCtx, pStrOutput,
												 "\t\t# The entry potentially collided with:\n")  );
		for (k=0; k<count; k++)
		{
			const SG_vhash * pvh_port_detail_k;
			const char * pszGid_k;

			SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pvaPortabilityDetails, k, (SG_vhash **)&pvh_port_detail_k)  );

			SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_port_detail_k, "gid1", &pszGid_k)  );
			if (strcmp(pszGid_k, pszGid) != 0)
			{
				SG_ERR_CHECK(  SG_pendingtree__find_repo_path_by_gid(pCtx, pPendingTree, pszGid_k, &pStrRepoPath_k)  );
				SG_ERR_CHECK(  SG_string__append__format(pCtx, pStrOutput,
														 "\t\t#\t%s\n",
														 SG_string__sz(pStrRepoPath_k))  );
				SG_STRING_NULLFREE(pCtx, pStrRepoPath_k);
			}
			SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_port_detail_k, "gid2", &pszGid_k)  );
			if (strcmp(pszGid_k, pszGid) != 0)
			{
				SG_ERR_CHECK(  SG_pendingtree__find_repo_path_by_gid(pCtx, pPendingTree, pszGid_k, &pStrRepoPath_k)  );
				SG_ERR_CHECK(  SG_string__append__format(pCtx, pStrOutput,
														 "\t\t#\t%s\n",
														 SG_string__sz(pStrRepoPath_k))  );
				SG_STRING_NULLFREE(pCtx, pStrRepoPath_k);
			}
		}
	}

	if (ppStrRepoPath_Returned)
	{
		*ppStrRepoPath_Returned = pStrRepoPath;
		pStrRepoPath = NULL;
	}

fail:
	SG_STRING_NULLFREE(pCtx, pStrRepoPath);
	SG_STRING_NULLFREE(pCtx, pStrRepoPath_k);
}



//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_PENDINGTREE__PRIVATE_RESOLVE_ISSUES_H
