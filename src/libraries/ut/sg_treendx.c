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

#include <sg.h>

/* TODO handle SQLITE_BUSY */

struct _SG_treendx
{
	SG_repo* pRepo;
    SG_uint32 iDagNum;

	SG_pathname* pPath_db;  /* the sqlite db file */

    sqlite3* psql;
	SG_bool			bInTransaction;		// TODO sqlite3 does not allow nested transactions, right?

    // TODO bQueryOnly isn't used
    SG_bool bQueryOnly;
};

static void sg_treendx__create_db(SG_context* pCtx, SG_treendx* pTreeNdx)
{
	/*
	 * The treendx table is used to store a gid and all of
	 * the paths that it has ever been at.  There is no attempt
	 * to store which changesets in which a particular gid/path
	 * combination is valid.  The approach is that if we store
	 * all possible paths, they can be quickly narrowed down by
	 * the SG_treendx__find_path_in_changeset.
	 * */
	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pTreeNdx->psql, "CREATE TABLE treendx (gid VARCHAR NOT NULL, strpath VARCHAR NOT NULL, CONSTRAINT treendx_uniq UNIQUE (gid, strpath));")  );

    SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pTreeNdx->psql, "CREATE INDEX IF NOT EXISTS treendx_gid ON treendx (gid)")  );

	return;
fail:
	return;
}

void SG_treendx__open(
	SG_context* pCtx,
	SG_repo* pRepo,
	SG_uint32 iDagNum,
	SG_pathname* pPath,
	SG_bool bQueryOnly,
	SG_treendx** ppNew
	)
{
	SG_treendx* pTreeNdx = NULL;
    SG_bool b_exists = SG_FALSE;

	SG_ERR_CHECK(  SG_alloc1(pCtx, pTreeNdx)  );

    pTreeNdx->pRepo = pRepo;
    pTreeNdx->pPath_db = pPath;
    pPath = NULL;
    pTreeNdx->iDagNum = iDagNum;
    pTreeNdx->bQueryOnly = bQueryOnly;
	pTreeNdx->bInTransaction = SG_FALSE;

    SG_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pTreeNdx->pPath_db, &b_exists, NULL, NULL)  );

    if (b_exists)
    {
        SG_ERR_CHECK(  sg_sqlite__open__pathname(pCtx, pTreeNdx->pPath_db, &pTreeNdx->psql)  );
    }
    else
    {
        SG_ERR_CHECK(  sg_sqlite__create__pathname(pCtx, pTreeNdx->pPath_db,&pTreeNdx->psql)  );

        SG_ERR_CHECK(  sg_treendx__create_db(pCtx, pTreeNdx)  );
    }

	*ppNew = pTreeNdx;

	return;
fail:
    SG_PATHNAME_NULLFREE(pCtx, pPath);
    SG_TREENDX_NULLFREE(pCtx, pTreeNdx);
}

// TODO consider the possible perf benefits of changing this routine
// to accept lots of changeset ids instead of just one, so it
// can handle them all at once.
void SG_treendx__update__multiple(
        SG_context* pCtx,
        SG_treendx* pTreeNdx,
        SG_stringarray* psa
        )
{
    SG_changeset* pcs = NULL;
	sqlite3_stmt* pStmt = NULL;
    SG_vhash* pvh_treepaths = NULL;
    SG_uint32 count_treepaths = 0;
    SG_uint32 count_changesets = 0;
    SG_uint32 ics = 0;

	SG_NULLARGCHECK_RETURN(psa);
	SG_NULLARGCHECK_RETURN(pTreeNdx);

    SG_ERR_CHECK(  SG_stringarray__count(pCtx, psa, &count_changesets)  );

    SG_ERR_CHECK(  sg_sqlite__exec__va(pCtx, pTreeNdx->psql, "BEGIN TRANSACTION; ")  );
    SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, pTreeNdx->psql, &pStmt, "INSERT OR IGNORE INTO treendx (gid, strpath) VALUES (?, ?)")  );
    for (ics=0; ics<count_changesets; ics++)
    {
        const char* psz_hid = NULL;
        SG_uint32 i = 0;

        SG_ERR_CHECK(  SG_stringarray__get_nth(pCtx, psa, ics, &psz_hid)  );
        SG_ERR_CHECK(  SG_changeset__load_from_repo(pCtx, pTreeNdx->pRepo, psz_hid, &pcs)  );
        SG_ERR_CHECK(  SG_changeset__get_treepaths(pCtx, pcs, &pvh_treepaths)  );

        if (pvh_treepaths)
        {
            SG_ERR_CHECK(  SG_vhash__count(pCtx, pvh_treepaths, &count_treepaths)  );


            for (i=0; i<count_treepaths; i++)
            {
                const char* psz_gid = NULL;
                const SG_variant* pv = NULL;
                const char* psz_path = NULL;

                SG_ERR_CHECK(  SG_vhash__get_nth_pair(pCtx, pvh_treepaths, i, &psz_gid, &pv)  );
                SG_ERR_CHECK(  SG_variant__get__sz(pCtx, pv, &psz_path)  );

                SG_ERR_CHECK(  sg_sqlite__reset(pCtx, pStmt)  );
                SG_ERR_CHECK(  sg_sqlite__clear_bindings(pCtx, pStmt)  );
                SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx, pStmt, 1, psz_gid)  );
                SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx, pStmt, 2, psz_path)  );
                SG_ERR_CHECK(  sg_sqlite__step(pCtx, pStmt, SQLITE_DONE)  );
            }
        }
        SG_CHANGESET_NULLFREE(pCtx, pcs);
    }
    SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );
    SG_ERR_CHECK(  sg_sqlite__exec__va(pCtx, pTreeNdx->psql, "COMMIT TRANSACTION; ")  );

fail:
    SG_CHANGESET_NULLFREE(pCtx, pcs);
}

void SG_treendx__get_path_in_dagnode(SG_context* pCtx, SG_treendx* pTreeNdx, const char* psz_search_item_gid, const char* psz_changeset, SG_treenode_entry ** ppTreeNodeEntry)
{
	SG_rbtree_iterator * rb_it = NULL;
	const char * pPath = NULL;
	SG_changeset * pChangeset = NULL;
	SG_stringarray * pPaths = NULL;
	const char* pszHidTreeNode = NULL;
	SG_treenode * pTreenodeRoot = NULL;
	char* pszReturnedGID = NULL;
	SG_uint32 i = 0;
	SG_uint32 count = 0;

	SG_ERR_CHECK_RETURN(  SG_gid__argcheck(pCtx, psz_search_item_gid)  );

	SG_ERR_CHECK(  SG_treendx__get_all_paths(pCtx, pTreeNdx, psz_search_item_gid, &pPaths)   );
	*ppTreeNodeEntry = NULL;
	SG_ERR_CHECK(  SG_changeset__load_from_repo(pCtx, pTreeNdx->pRepo, psz_changeset, &pChangeset)  );
	SG_ERR_CHECK(  SG_changeset__get_root(pCtx, pChangeset, &pszHidTreeNode) );
	SG_ERR_CHECK(  SG_treenode__load_from_repo(pCtx, pTreeNdx->pRepo, pszHidTreeNode, &pTreenodeRoot)  );
	SG_ERR_CHECK(  SG_stringarray__count(pCtx, pPaths, &count )  );
	for (i = 0; i < count; i++)
	{
		SG_ERR_CHECK(  SG_stringarray__get_nth(pCtx, pPaths, i, &pPath)  );
		SG_ERR_CHECK(  SG_treenode__find_treenodeentry_by_path(pCtx, pTreeNdx->pRepo, pTreenodeRoot, pPath, &pszReturnedGID, ppTreeNodeEntry)  );
		if (*ppTreeNodeEntry != NULL && strcmp(pszReturnedGID, psz_search_item_gid) == 0)
		{
			break;
		}
		else if (*ppTreeNodeEntry != NULL)
		{
			SG_TREENODE_ENTRY_NULLFREE(pCtx, *ppTreeNodeEntry);
			*ppTreeNodeEntry = NULL; //It's not the right GID, even though it's in the right spot.
		}
	}

	SG_NULLFREE(pCtx, pszReturnedGID);
	SG_CHANGESET_NULLFREE(pCtx, pChangeset);
	SG_TREENODE_NULLFREE(pCtx, pTreenodeRoot);
	SG_STRINGARRAY_NULLFREE(pCtx, pPaths);
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, rb_it);
	return;
fail:
	SG_NULLFREE(pCtx, pszReturnedGID);
	SG_CHANGESET_NULLFREE(pCtx, pChangeset);
	SG_STRINGARRAY_NULLFREE(pCtx, pPaths);
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, rb_it);
	return;

}

void SG_treendx__get_all_paths(SG_context* pCtx, SG_treendx* pTreeNdx, const char* psz_gid, SG_stringarray ** ppResults)
{
	sqlite3_stmt* pStmt = NULL;
	SG_stringarray * pResults = NULL;
	int rc;

	SG_ERR_CHECK_RETURN(  SG_gid__argcheck(pCtx, psz_gid)  );

	SG_ERR_CHECK(  SG_STRINGARRAY__ALLOC(pCtx,&pResults, 1)  );
	SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, pTreeNdx->psql, &pStmt, "SELECT strpath FROM treendx WHERE gid='%s' ORDER BY strpath;", psz_gid)  );
	while ((rc = sqlite3_step(pStmt)) == SQLITE_ROW)
	{
		const char* pszPath = (const char*) sqlite3_column_text(pStmt, 0);
		SG_ERR_CHECK(  SG_stringarray__add(pCtx, pResults, pszPath)   );
	}
	if (rc != SQLITE_DONE)
	{
		SG_ERR_THROW(SG_ERR_SQLITE(rc));
	}

	SG_ERR_CHECK(  sg_sqlite__finalize(pCtx, pStmt)  );
	*ppResults = pResults;

	return;
fail:
	if (pStmt)
	{
		SG_ERR_IGNORE(  sg_sqlite__finalize(pCtx, pStmt)  );
	}

}

void SG_treendx__free(SG_context* pCtx, SG_treendx* pTreeNdx)
{
	if (!pTreeNdx)
		return;

	SG_ERR_IGNORE(  sg_sqlite__close(pCtx, pTreeNdx->psql)  );

	SG_PATHNAME_NULLFREE(pCtx, pTreeNdx->pPath_db);
	SG_NULLFREE(pCtx, pTreeNdx);
}


