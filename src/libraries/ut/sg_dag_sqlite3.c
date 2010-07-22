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
 * @file sg_dag_sqlite3.c
 *
 * @details PRIVATE Routines for the SQLITE3 implementation of the DAG
 *
 * Note: dagnode generation is now SG_int32 rather than SG_int64.
 */

//////////////////////////////////////////////////////////////////

#include <sg.h>

// TODO move these to sg_dag_typedefs.h
typedef struct SG_dag SG_dag;
typedef struct SG_dn SG_dn;

// TODO move these to sg_dag_prototypes.h
void SG_dag_sqlite3__get_whole_dag(
	SG_context* pCtx,
    sqlite3* psql,
    SG_uint32 iDagNum,
	SG_dag** pp
    );
void SG_dag__free(SG_context* pCtx, SG_dag* pThis);

// TODO move this to sg_nullfree.h
#define SG_DAG_NULLFREE(pCtx,p)               SG_STATEMENT( SG_context__push_level(pCtx);              SG_dag__free(pCtx, p);    SG_ASSERT(!SG_context__has_err(pCtx)); SG_context__pop_level(pCtx); p=NULL; )

//////////////////////////////////////////////////////////////////
// sqlite3 has a way to spin and retry an operation if another
// process has a table/db locked.  we use the keep trying for
// upto n-milliseconds version.
//
// WARNING: As of 3.5.6 (at least), this only works some of the
// WARNING: time.  It seems to let COMMIT TRANSACTIONS routinely
// WARNING: work (during parallel runs of my
// WARNING:    testapps/dag_stress_1/add_nodes
// WARNING: test app.
// WARNING:
// WARNING: It *DID NOT* prevent the first INSERT (after the initial
// WARNING: BEGIN TRANSACTION) of the 2nd..nth parallel instance of the
// WARNING: test.  The builtin busy callback wasn't even used.
// WARNING:
// WARNING: The SQLITE3 docs say that statements can return SQLITE_BUSY
// WARNING: immediately (rather than waiting and retrying) *IF* they
// WARNING: can detect deadlock or other locking problems.  Installing
// WARNING: a busy callback reduces the odds of getting a BUSY, but
// WARNING: does not prevent it.

#define MY_BUSY_TIMEOUT_MS      (30000)

//////////////////////////////////////////////////////////////////

#define FAKE_PARENT			"*root*"

//////////////////////////////////////////////////////////////////


void SG_dag_sqlite3__create(SG_context* pCtx, const SG_pathname* pPathnameSqlDb, sqlite3** ppNew)
{
	sqlite3 * p;
	int rc;

    SG_ERR_CHECK(  sg_sqlite__create__pathname(pCtx, pPathnameSqlDb,&p)  );
    rc = sqlite3_busy_timeout(p,MY_BUSY_TIMEOUT_MS);
	if (rc)
		SG_ERR_THROW(SG_ERR_SQLITE(rc));

    SG_ERR_CHECK(  SG_dag_sqlite3__create_tables(pCtx, p)  );

    *ppNew = p;

	return;

fail:
    /* TODO delete the sqldb we just created?  Probably not */
    SG_ERR_IGNORE(  SG_dag_sqlite3__nullclose(pCtx, &p)  );
}

void SG_dag_sqlite3__open(SG_context* pCtx, const SG_pathname* pPathnameSqlDb, sqlite3** ppNew)
{
	sqlite3 * p;
	int rc;

    SG_ERR_CHECK(  sg_sqlite__open__pathname(pCtx, pPathnameSqlDb,&p)  );
	//SG_ERR_CHECK(  sg_sqlite__exec(p, "PRAGMA page_size=8192")  );
	//SG_ERR_CHECK(  sg_sqlite__exec(p, "PRAGMA synchronous=0")  );
	rc = sqlite3_busy_timeout(p,MY_BUSY_TIMEOUT_MS);
	if (rc)
		SG_ERR_THROW(SG_ERR_SQLITE(rc));

    *ppNew = p;

	return;

fail:
    SG_ERR_IGNORE(  SG_dag_sqlite3__nullclose(pCtx, &p)  );
}

//////////////////////////////////////////////////////////////////

void SG_dag_sqlite3__create_tables(SG_context* pCtx, sqlite3 * psql)
{
	// Create all of the tables and indexes that we need to represent the DAG.
	// This includes the list of edges and the list of leaf nodes.

	// The first table is the "dag_edges".  This will have one row for
	// each EDGE in the DAG.  These are child-->parent links.  Since
	// a child may have more than one parent (and likewise, a parent
	// may have more than one child), these individual columns are not
	// marked UNIQUE and are not the PRIMARY KEY.  Rather, we make the
	// PRIMARY KEY be the PAIR, so that each EDGE can only occur once.
	//
	// Naturally, the child_id must not be NULL because it represents
	// the HID of the CHANGESET/DAGNODE in question.  But the parent_id
	// *could* be NULL (because the initial checkin doesn't have a parent),
	// but we also make it NOT NULL because there are problems/disputes in
	// SQL with unique and null in multi-column constraints (see
	// http://www.sql.org/sql-database/postgresql/manual/ddl-constraints.html
	// sections 2.4.3 Unique Contraints and 2.4.4 Primary Keys) and will
	// have the C code use a special value to indicate no-parent.
	//
	// By creating an EDGE table with 2 columns (rather than a node table
	// with the parents packed in some kind of vector) we should be able to
	// search the DAG in both directions - find all parents of a node
	// and find all children of a node.  We currently believe that if
	// we always walk the graph starting at LEAF nodes, we'll only
	// need the former, but let's not rule out the latter.
	//
	// It is our current belief that creating a combo primary key does
	// not cause the DB to create indexes on the individual fields, so
	// we explicitly create them here.
	//
	// TODO See if these are really necessary and/or redundant.
	//
	// NOTE: This design was chosen to be as generic as possible so
	// NOTE: that we can add other SQL engines with as little rework
	// NOTE: as possible.  We don't want to go down the dark path
	// NOTE: and make SQL do lots of black magic on our behalf.

	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, psql, "BEGIN EXCLUSIVE TRANSACTION")  );

	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, psql, ("CREATE TABLE dag_edges"
												"  ("
												"    child_id VARCHAR NOT NULL,"
												"    parent_id VARCHAR NOT NULL,"
												"    dagnum INTEGER NOT NULL,"
												"    PRIMARY KEY ( child_id, parent_id, dagnum )"
												"  )"))  );
	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, psql, ("CREATE INDEX dag_edges_index_child_id ON dag_edges ( child_id )"))  );
	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, psql, ("CREATE INDEX dag_edges_index_parent_id ON dag_edges ( parent_id )"))  );
	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, psql, ("CREATE INDEX dag_edges_index_dagnum ON dag_edges ( dagnum )"))  );

	// The second table is the "dag_leaves".  This will contain a row
	// for each LEAF node in the DAG.
	//
	// TODO figure out how we want to give human readable names to leaves
	// TODO and/or branches.  We may want to add a name column to this
	// TODO table.  But first we need to study the differences between
	// TODO GIT and HG (and others).

	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, psql,
										 ("CREATE TABLE dag_leaves"
										  "  ("
										  "    child_id VARCHAR PRIMARY KEY,"
										  "    dagnum INTEGER NOT NULL"
										  "    )"))  );
	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, psql, ("CREATE INDEX dag_leaves_index_dagnum ON dag_leaves ( dagnum )"))  );

	// The third table is the "dag_info".  This will contain a row for
	// each DAGNODE and various information for it.

	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, psql,
										 ("CREATE TABLE dag_info"
										  "  ("
										  "    child_id VARCHAR PRIMARY KEY,"
										  "    dagnum INTEGER NOT NULL,"
										  "    generation INTEGER"
										  "  )"))  );
	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, psql, ("CREATE INDEX dag_info_index_dagnum ON dag_info ( dagnum )"))  );

	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, psql, "COMMIT TRANSACTION")  );

	return;

fail:
	SG_ERR_IGNORE(  sg_sqlite__exec(pCtx, psql, "ROLLBACK TRANSACTION")  );
	// TODO should we do anything if we can't create one of the tables?
}

//////////////////////////////////////////////////////////////////

void SG_dag_sqlite3__nullclose(SG_context* pCtx, sqlite3** ppsql)
{
    sqlite3* psql = NULL;

	SG_NULLARGCHECK_RETURN(ppsql);

    psql = *ppsql;
    if (!psql)
		return;

	sg_sqlite__close(pCtx, psql);

    *ppsql = NULL;

	SG_ERR_REPLACE(SG_ERR_SQLITE(SQLITE_BUSY), SG_ERR_DB_BUSY);
}


//////////////////////////////////////////////////////////////////

static void _my_is_known(SG_context* pCtx, sqlite3* psql, const char * szHid, SG_bool * pbIsKnown, SG_int32 * pGen)
{
	// search the DAG_INFO table and see if the given HID is known.
	// this should be equivalent to searching for HID in the child_id column of DAG_EDGES.

	sqlite3_stmt * pStmtInfo = NULL;
	SG_int32 gen = -1;
	SG_uint32 nrInfoRows = 0;
	int rc;

	SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, psql, &pStmtInfo,
									  "SELECT generation FROM dag_info WHERE child_id = ?")  );
	SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx, pStmtInfo,1,szHid)  );

	while ((rc=sqlite3_step(pStmtInfo)) == SQLITE_ROW)
	{
		if (nrInfoRows == 0)
		{
			gen = (SG_int32)sqlite3_column_int(pStmtInfo,0);
			nrInfoRows++;
		}
		else			// this can't happen because of primary key.
		{
			SG_ERR_THROW(SG_ERR_DAG_NOT_CONSISTENT);
		}
	}
	if (rc != SQLITE_DONE)
	{
		SG_ERR_THROW(SG_ERR_SQLITE(rc));
	}

	SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx, &pStmtInfo)  );

	*pbIsKnown = (nrInfoRows == 1);
	*pGen = gen;

	return;

fail:
	SG_ERR_IGNORE(  sg_sqlite__nullfinalize(pCtx, &pStmtInfo)  );
}

static void _my_mark_as_not_leaf(SG_context* pCtx, sqlite3* psql, const char* pszidHid)
{
	// delete HID from the DAG_LEAVES TABLE.  this marks a DAGNODE as NOT a LEAF.
	// WE TRUST THE CALLER TO KNOW WHAT THEY'RE DOING.
	//
	// if the HID is not present in the DAG_LEAVES TABLE, we silently ignore
	// the request.

	sqlite3_stmt * pStmt = NULL;

	SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, psql,&pStmt,
									  "DELETE FROM dag_leaves WHERE child_id = ?")  );
	SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx, pStmt,1,pszidHid)  );
	SG_ERR_CHECK(  sg_sqlite__step(pCtx, pStmt,SQLITE_DONE)  );
	SG_ERR_CHECK(  sg_sqlite__finalize(pCtx, pStmt)  );

	return;

fail:
	SG_ERR_IGNORE(  sg_sqlite__finalize(pCtx, pStmt)  );
}

static void _my_mark_as_leaf(SG_context* pCtx, sqlite3* psql, SG_uint32 iDagNum, const char* pszidHid)
{
	// insert HID as a row in DAG_LEAVES TABLE.  this marks the DAGNODE as a LEAF.
	// WE TRUST THE CALLER TO KNOW WHAT THEY'RE DOING.
	//
	// if the HID is already a LEAF, we silently ignore the request.

	sqlite3_stmt * pStmt = NULL;

	SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, psql,&pStmt,
									  "INSERT OR IGNORE INTO dag_leaves (child_id,dagnum) VALUES (?,?)")  );
	SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx, pStmt,1,pszidHid)  );
	SG_ERR_CHECK(  sg_sqlite__bind_int(pCtx, pStmt,2,(SG_int32) iDagNum)  );
	SG_ERR_CHECK(  sg_sqlite__step(pCtx, pStmt,SQLITE_DONE)  );
	SG_ERR_CHECK(  sg_sqlite__finalize(pCtx, pStmt)  );

	return;

fail:
	SG_ERR_IGNORE(  sg_sqlite__finalize(pCtx, pStmt)  );
}

static void _my_store_edge(SG_context* pCtx, sqlite3* psql, SG_uint32 iDagNum, const char * szHidChild, const char * szHidParent)
{
	sqlite3_stmt * pStmt = NULL;

	// insert edge.  we expect either SQLITE_DONE (mapped to SG_ERR_OK)
	// or an SQLITE_CONSTRAINT on a duplicate record.

	SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, psql, &pStmt,
									  "INSERT INTO dag_edges (child_id, parent_id, dagnum) VALUES (?, ?, ?)")  );
	SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx, pStmt,1,szHidChild)  );
	SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx, pStmt,2,szHidParent)  );
	SG_ERR_CHECK(  sg_sqlite__bind_int(pCtx, pStmt,3,iDagNum)  );
	SG_ERR_CHECK(  sg_sqlite__step(pCtx, pStmt,SQLITE_DONE)  );
	SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );

	return;

fail:
	SG_ERR_IGNORE(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );
}

static void _my_store_info(SG_context* pCtx, sqlite3* psql, SG_uint32 iDagNum, const SG_dagnode * pDagnode)
{
	// add record to DAG_INFO table.

	sqlite3_stmt * pStmt = NULL;
	char* pszidHidChild = NULL;
	SG_int32 generation;

	SG_ERR_CHECK(  SG_dagnode__get_id(pCtx, pDagnode,&pszidHidChild)  );
	SG_ERR_CHECK(  SG_dagnode__get_generation(pCtx, pDagnode,&generation)  );

	// insert info.  we expect either SQLITE_DONE (mapped to SG_ERR_OK)
	// or an SQLITE_CONTRAINT on a duplicate record.

	SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, psql, &pStmt,
									  "INSERT INTO dag_info (child_id, dagnum, generation) VALUES (?, ?, ?)")  );
	SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx, pStmt,1,pszidHidChild)  );
	SG_ERR_CHECK(  sg_sqlite__bind_int(pCtx, pStmt,2,iDagNum)  );
	SG_ERR_CHECK(  sg_sqlite__bind_int(pCtx, pStmt,3,generation)  );
	SG_ERR_CHECK(  sg_sqlite__step(pCtx, pStmt,SQLITE_DONE)  );
	SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );

	SG_NULLFREE(pCtx, pszidHidChild);
	return;

fail:
	SG_NULLFREE(pCtx, pszidHidChild);
	SG_ERR_IGNORE(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );
}

static void _my_store_initial_dagnode(SG_context* pCtx, sqlite3* psql, SG_uint32 iDagNum, const SG_dagnode * pDagnode)
{
	// special case for the initial CHANGESET/DAGNODE.
	//
	// create fake edges from this dagnode to the root.
	// make this dagnode as a leaf.
	//
	// we hold the TX open as we create the EDGES and update the LEAVES, so
	// order doesn't really matter.
	//
	// WARNING: Due to the distributed nature of the system we ***CANNOT***
	// WARNING: insist that this node be the SINGLE ABSOLUTE/UNIQUE initial checkin.
	// WARNING: That is, the only dagnode that has no parents.  We have to
	// WARNING: allow for the possibility that someone created a repository
	// WARNING: and cloned it BEFORE the initial checkin.  Each instance of
	// WARNING: the repository would allow an initial checkin.  A PUSH/PULL
	// WARNING: between them would then give us a disconnected DAG....
	//
	// we store the DAG in a DAG_EDGES table with one
	// row for each EDGE from the DAGNODE to a parent
	// DAGNODE.
	//
	// Since this DAGNODE is the initial checkin, there
	// are no parents, and hence, no edges from it.
	//
	// We create a special parent "*root*" and make a
	// fake edge to it.  This keeps us from having to
	// rely on an SQL NULL value (which may or may not
	// work as expected when in a KEY).

	const char * szHidChild;

	// try to add record to DAG_INFO table.

	_my_store_info(pCtx, psql,iDagNum,pDagnode);
	if (SG_context__err_equals(pCtx, SG_ERR_SQLITE(SQLITE_CONSTRAINT)))
	{
		// duplicate row constraint in DAG_INFO.  the DB already had this
		// record.
		//
		// we assume that it contains the DAG_EDGES records.
		//
		// we cannot make any statements about whether
		// or not this node is a leaf.
		//
		// return special already-exists error code; this error may
		// be ignored by caller.

		SG_ERR_RESET_THROW(SG_ERR_DAGNODE_ALREADY_EXISTS );
	}
	SG_ERR_CHECK_CURRENT;  // otherwise, we have an unknown SQL error.

	// we just added a *NEW* node to DAG_INFO, so we must have
	// a new (to us) dagnode.  continue adding stuff to the
	// other tables.

	SG_ERR_CHECK(  SG_dagnode__get_id_ref(pCtx, pDagnode,&szHidChild)  );

	// try to add record to DAG_EDGES table.  if this fails because
	// an actual error --or-- because of a constraint (duplicate key)
	// error, something is wrong and we just give up.

	SG_ERR_CHECK(  _my_store_edge(pCtx, psql,iDagNum,szHidChild,FAKE_PARENT)  );

	// we juat added a *NEW* node to the DAG and, by definition,
	// no other node in the graph references us, so therefore
	// it is a leaf.

	SG_ERR_CHECK(  _my_mark_as_leaf(pCtx, psql,iDagNum,szHidChild)  );

	return;

fail:
	return;
}

static void _my_store_dagnode_with_parents(SG_context* pCtx, sqlite3* psql, SG_uint32 iDagNum, const SG_dagnode * pDagnode)
{
	// store non-initial CHANGESET/DAGNODE.
	//
	// we store an EDGE from child to each parent.
	// we mark the parents as not leaves.
	// we mark the child as a leaf.
	//
	// we hold the TX open as we create the EDGES and update the LEAVES, so
	// order doesn't really matter.

	const char * szHidChild;
	SG_uint32 k, kLimit;
	const char** paParents = NULL;

	// try to add record to DAG_INFO table.

	_my_store_info(pCtx, psql,iDagNum,pDagnode);
	if (SG_context__err_equals(pCtx, SG_ERR_SQLITE(SQLITE_CONSTRAINT)))
	{
		// duplicate row constraint in DAG_INFO.  the DB already had this
		// record.
		//
		// we assume that it contains the DAG_EDGES records.
		//
		// we cannot make any statements about whether
		// or not this node is a leaf.
		//
		// return special already-exists error code; this error may
		// be ignored by caller.

		SG_ERR_RESET_THROW(SG_ERR_DAGNODE_ALREADY_EXISTS);
	}
	SG_ERR_CHECK_CURRENT;  // otherwise, we have an unknown SQL error.

	// we just added a *NEW* node to DAG_INFO, so we must have
	// a new (to us) dagnode.  continue adding stuff to the
	// other tables.

	SG_ERR_CHECK_RETURN(  SG_dagnode__get_parents(pCtx, pDagnode,&kLimit,&paParents)  );

	SG_ERR_CHECK(  SG_dagnode__get_id_ref(pCtx, pDagnode,&szHidChild)  );

	// We want to insert a unique <child,parent> pair into the
	// DAG_EDGES table for each parent.

	for (k=0; k<kLimit; k++)
	{
		SG_bool bIsParentKnown = SG_FALSE;
		SG_int32 genUnused;

		// Because the DAG is essentially append-only and we want to avoid
		// having a sparse DAG, let's require that all of the parents already
		// be in the DAG before we allow the dagnode to be added.

		SG_ERR_CHECK(  _my_is_known(pCtx, psql,paParents[k],&bIsParentKnown,&genUnused)  );
		if (!bIsParentKnown)
		{
			SG_ERR_THROW(  SG_ERR_CANNOT_CREATE_SPARSE_DAG  );
		}

		// try to add record to DAG_EDGES table.  if this fails because
		// an actual error --or-- because of a constraint (duplicate key)
		// error, something is wrong and we just give up.

		SG_ERR_CHECK(  _my_store_edge(pCtx, psql,iDagNum,szHidChild,paParents[k])  );

		// we just added a *NEW* edge from child->parent to the DAG and, by
		// definition, the parent node is no longer a leaf.

		SG_ERR_CHECK(  _my_mark_as_not_leaf(pCtx, psql,paParents[k])  );
	}

	// we just added a *NEW* node to the DAG and, by definition, no other node
	// in the graph references us, so therefore, it is a leaf.

	SG_ERR_CHECK(  _my_mark_as_leaf(pCtx, psql,iDagNum,szHidChild)  );

	SG_NULLFREE(pCtx, paParents);
	paParents = NULL;

	return;

fail:
	SG_NULLFREE(pCtx, paParents);
}

//////////////////////////////////////////////////////////////////

void SG_dag_sqlite3__store_dagnode(SG_context* pCtx, sqlite3* psql, SG_uint32 iDagNum, const SG_dagnode * pDagnode)
{
	// Callers should have an open sqlite transaction and rollback if any error is returned.
	// Ian TODO: Except SG_ERR_DAGNODE_ALREADY_EXISTS, which will go away?

	SG_uint32 nrParents;

	SG_NULLARGCHECK_RETURN(psql);
	SG_NULLARGCHECK_RETURN(pDagnode);

	SG_ERR_CHECK(  SG_dagnode__count_parents(pCtx, pDagnode,&nrParents)  );

	if (nrParents == 0)
    {
		SG_ERR_CHECK(  _my_store_initial_dagnode(pCtx, psql,iDagNum,pDagnode)  );
    }
	else
    {
		SG_ERR_CHECK(  _my_store_dagnode_with_parents(pCtx, psql,iDagNum,pDagnode)  );
    }

	return;

fail:
	SG_ERR_REPLACE(SG_ERR_SQLITE(SQLITE_BUSY), SG_ERR_DB_BUSY);
}

//////////////////////////////////////////////////////////////////

void SG_dag_sqlite3__fetch_dagnode(SG_context* pCtx, sqlite3* psql, const char* pszidHidChangeset, SG_dagnode ** ppNewDagnode)
{
	// fetch all rows in the DAG_EDGE table that have the
	// given HID as the child_id field.  package up all
	// of the parent HIDs into a freshly allocate DAGNODE.

	// since dagnodes are constant once created (immutable), we
	// don't really have to worry about transaction consistency
	// when the caller is trying to read more than one dagnode
	// into memory.  (each fetch_dagnode can be in its own TX.)
	//
	// HOWEVER, we do need a TX to make sure that that we a
	// consistent view of the EDGES (all or none) (i think).

	int rc;
	SG_dagnode * pNewDagnode = NULL;
	sqlite3_stmt * pStmtEdge = NULL;
	SG_bool bHasFakeParent = SG_FALSE;
	SG_bool bIsKnownInfo = SG_FALSE;
	SG_uint32 sumParents = 0;
	SG_int32 generation = -1;

	SG_NULLARGCHECK_RETURN(psql);
	SG_NULLARGCHECK_RETURN(pszidHidChangeset);
	SG_NULLARGCHECK_RETURN(ppNewDagnode);

	//////////////////////////////////////////////////////////////////
	// begin transaction for doing a consistent SELECT.
	// we only look at the DAG_EDGES table.
	//////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////
	// fetch info from the DAG_INFO table.

	SG_ERR_CHECK(  _my_is_known(pCtx, psql,pszidHidChangeset,&bIsKnownInfo,&generation)  );
	if (!bIsKnownInfo)
	{
		// we didn't find a row in the dag_info table.
		// perhaps they gave us a bogus Changeset HID.
		// or perhaps we have a problem in the DAG DB.

		SG_ERR_THROW(SG_ERR_NOT_FOUND);
	}

	SG_ERR_CHECK(  SG_dagnode__alloc(pCtx, &pNewDagnode,pszidHidChangeset,generation)  );

	//////////////////////////////////////////////////////////////////
	// fetch all DAG_EDGE rows with having this child_id.
	//
	// we don't need to worry about having SQL sort the edges because
	// we now use a rbtree to store the list of parents in memory and
	// it will always be sorted.

	SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, psql, &pStmtEdge,
									  "SELECT parent_id FROM dag_edges WHERE child_id = ?")  );
	SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx, pStmtEdge,1,pszidHidChangeset)  );

	// read each parent row.

	while ((rc=sqlite3_step(pStmtEdge)) == SQLITE_ROW)
	{
		const char * szHidParent = (const char *)sqlite3_column_text(pStmtEdge,0);

		// if parent_id is the FAKE_PARENT then we think that this child_id
		// is the initial checkin/commit -- BUT WE SHOULD VERIFY THIS.  we
		// let the loop continue and see if there are any other parents in
		// the SELECT result set.

		if (strcmp(szHidParent,FAKE_PARENT) == 0)
			bHasFakeParent = SG_TRUE;
		else
		{
			SG_ERR_CHECK(  SG_dagnode__add_parent(pCtx, pNewDagnode, szHidParent)  );
			sumParents++;
		}
	}
	if (rc != SQLITE_DONE)
	{
		SG_ERR_THROW(  SG_ERR_SQLITE(rc)  );
	}

	SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx, &pStmtEdge)  );

	SG_ERR_CHECK(  SG_dagnode__freeze(pCtx, pNewDagnode)  );

#if 1
	//////////////////////////////////////////////////////////////////
	// begin consistency checks
	//////////////////////////////////////////////////////////////////
	{
		SG_uint32 nrParents;

#ifdef DEBUG
		if (bHasFakeParent)
			SG_ASSERT( (sumParents == 0)  &&  "Cannot have FAKE_PARENT and real parents." );
		else
			SG_ASSERT( (sumParents > 0)  && "Must have either FAKE_PARENT or at least one real parent.");
#endif

		SG_ERR_CHECK(  SG_dagnode__count_parents(pCtx, pNewDagnode,&nrParents)  );

		// We did a unique insert of each parent HID into the DAGNODEs idset.
		// So if everything is in order, the DAGNODE should have the
		// same number of parents as we found in the DB.  Since we have
		// a PRIMARY KEY on the <child,parent> pair on the disk, this shouldn't happen.

		SG_ASSERT( (nrParents == sumParents) && "Possible duplicate in list of real parents.");
	}

	//////////////////////////////////////////////////////////////////
	// end of consistency checks
	//////////////////////////////////////////////////////////////////
#endif

	*ppNewDagnode = pNewDagnode;
	return;

fail:
	SG_DAGNODE_NULLFREE(pCtx, pNewDagnode);
	SG_ERR_IGNORE(  sg_sqlite__finalize(pCtx, pStmtEdge)  );
	SG_ERR_REPLACE(SG_ERR_SQLITE(SQLITE_BUSY),SG_ERR_DB_BUSY);
}

//////////////////////////////////////////////////////////////////

static void _my_fetch_leaves(SG_context* pCtx, sqlite3* psql, SG_uint32 iDagNum, SG_rbtree ** ppIdsetReturned)
{
	// do the actual fetch_leaves assuming that a TX is already open.

	int rc;
	sqlite3_stmt * pStmt = NULL;
	SG_rbtree * pIdset = NULL;

	SG_NULLARGCHECK_RETURN(psql);
	SG_NULLARGCHECK_RETURN(ppIdsetReturned);

	SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &pIdset)  );

	// prepare SELECT to fetch all rows from DAG_LEAVES.
	//
	// we don't need to worry about having SQL sort the leaves because
	// we now use a rbtree to store the list in memory.

	SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, psql, &pStmt,
									  "SELECT child_id FROM dag_leaves WHERE dagnum=%d", iDagNum)  );

	// read each leaf row.

	while ((rc=sqlite3_step(pStmt)) == SQLITE_ROW)
	{
		const char * szHid = (const char *)sqlite3_column_text(pStmt,0);

		SG_ERR_CHECK(  SG_rbtree__add(pCtx, pIdset,szHid)  );
	}
	if (rc != SQLITE_DONE)
	{
		SG_ERR_THROW(  SG_ERR_SQLITE(rc)  );
	}

	SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );

#if 1
	//////////////////////////////////////////////////////////////////
	// begin consistency checks
	//////////////////////////////////////////////////////////////////
	{
		// TODO what kind of asserts should we put on the data that we found?
		// TODO should we verify the leaf count is > 0 when there are edges present?
	}
	//////////////////////////////////////////////////////////////////
	// end consistency checks
	//////////////////////////////////////////////////////////////////
#endif

	*ppIdsetReturned = pIdset;
	return;

fail:
	SG_RBTREE_NULLFREE(pCtx, pIdset);
	SG_ERR_IGNORE(  sg_sqlite__finalize(pCtx, pStmt)  );
	SG_ERR_REPLACE(SG_ERR_SQLITE(SQLITE_BUSY), SG_ERR_DB_BUSY);
}

void SG_dag_sqlite3__find_by_prefix(SG_context* pCtx, sqlite3* psql, SG_uint32 iDagNum, const char* psz_hid_prefix, SG_rbtree** pprb_results)
{
	int rc;
	sqlite3_stmt * pStmt = NULL;
    SG_rbtree* prb = NULL;

	SG_NULLARGCHECK_RETURN(psql);
	SG_NULLARGCHECK_RETURN(psz_hid_prefix);
	SG_NULLARGCHECK_RETURN(pprb_results);

    // TODO sqlite cannot use an INDEX for a LIKE query.
    // http://web.utk.edu/~jplyon/sqlite/SQLite_optimization_FAQ.html
	SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, psql, &pStmt,
									  "SELECT child_id FROM dag_info WHERE dagnum=%d AND child_id LIKE '%s%%'", iDagNum, psz_hid_prefix)  );

    SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb)  );
    //
	// read each row.

	while ((rc=sqlite3_step(pStmt)) == SQLITE_ROW)
	{
		const char * szHid = (const char *)sqlite3_column_text(pStmt,0);

		SG_ERR_CHECK(  SG_rbtree__add(pCtx, prb,szHid)  );
	}
	if (rc != SQLITE_DONE)
	{
		SG_ERR_THROW(  SG_ERR_SQLITE(rc)  );
	}

	SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );

    *pprb_results = prb;

	return;

fail:
    SG_RBTREE_NULLFREE(pCtx, prb);
	SG_ERR_REPLACE(SG_ERR_SQLITE(SQLITE_BUSY), SG_ERR_DB_BUSY);
}

void SG_dag_sqlite3__list_dags(SG_context* pCtx, sqlite3* psql, SG_uint32* piCount, SG_uint32** paDagNums)
{
	int rc;
	sqlite3_stmt * pStmt = NULL;
    SG_uint32 space = 0;
    SG_uint32* pResults = NULL;
    SG_uint32* pGrowResults = NULL;
    SG_uint32 count = 0;

	SG_NULLARGCHECK_RETURN(psql);
	SG_NULLARGCHECK_RETURN(piCount);
	SG_NULLARGCHECK_RETURN(paDagNums);

	SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, psql, &pStmt,
									  "SELECT DISTINCT dagnum FROM dag_info ORDER BY dagnum ASC")  );

    space = 32;
	SG_ERR_CHECK(  SG_allocN(pCtx,space,pResults)  );

	while ((rc=sqlite3_step(pStmt)) == SQLITE_ROW)
	{
		SG_uint32 d = sqlite3_column_int(pStmt,0);

        if ((count + 1) > space)
        {
            space *= 2;
			SG_ERR_CHECK(  SG_allocN(pCtx,space,pGrowResults)  );

            memcpy(pGrowResults, pResults, count * sizeof(SG_uint32));
            SG_NULLFREE(pCtx, pResults);
            pResults = pGrowResults;
            pGrowResults = NULL;
        }

        pResults[count++] = d;
	}
	if (rc != SQLITE_DONE)
	{
		SG_ERR_THROW(  SG_ERR_SQLITE(rc)  );
	}

	SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );

    *piCount = count;
    *paDagNums = pResults;

	return;

fail:
    SG_NULLFREE(pCtx, pResults);
    SG_NULLFREE(pCtx, pGrowResults);
	SG_ERR_REPLACE(SG_ERR_SQLITE(SQLITE_BUSY), SG_ERR_DB_BUSY);
}

void SG_dag_sqlite3__fetch_leaves(SG_context* pCtx, sqlite3* psql, SG_uint32 iDagNum, SG_rbtree ** ppIdsetReturned)
{
	// fetch HIDs of all LEAVES from the DAG_LEAVES table
	// and assemble them into a freshly allocated IDSET.

	// because leaves get added and deleted as nodes are added,
	// we hold a TX to make sure that we get a consistent view
	// of the leaves.
	//
	// let's agree here that we will give the caller a consistent
	// view of the leaves as of a particular point in time.  they
	// should start with this list and reference dagnodes and edges
	// with confidence -- a hid in the set of leaves implies that
	// it is already present (and complete) it the edges.
	//
	// if another process is adding nodes (and updating the leaves),
	// it won't affect this process - until we are called again to
	// get a new copy of the leaves.

	SG_NULLARGCHECK_RETURN(psql);
	SG_NULLARGCHECK_RETURN(ppIdsetReturned);

	//////////////////////////////////////////////////////////////////
	// begin transaction for doing a consistent SELECT.
	// we only look at the DAG_EDGES table.
	//////////////////////////////////////////////////////////////////

	SG_ERR_CHECK(  _my_fetch_leaves(pCtx, psql,iDagNum,ppIdsetReturned)  );

	return;

fail:
	SG_ERR_REPLACE(SG_ERR_SQLITE(SQLITE_BUSY), SG_ERR_DB_BUSY);
}

void SG_dag_sqlite3__fetch_dagnode_children(SG_context* pCtx, sqlite3* psql, SG_uint32 iDagNum, const char * pszDagnodeID, SG_rbtree ** ppIdsetReturned)
{
	int rc;
	sqlite3_stmt * pStmt = NULL;
	SG_rbtree * pIdset = NULL;

	// fetch HIDs of all LEAVES from the DAG_LEAVES table
	// and assemble them into a freshly allocated IDSET.

	// because children get added and deleted as nodes are added,
	// we hold a TX to make sure that we get a consistent view
	// of the leaves.
	//
	// let's agree here that we will give the caller a consistent
	// view of the children as of a particular point in time.  they
	// should start with this list and reference dagnodes and edges
	// with confidence -- a hid in the set of children implies that
	// it is already present (and complete) it the edges.
	//
	// if another process is adding nodes (and updating the children),
	// it won't affect this process - until we are called again to
	// get a new copy of the leaves.

	SG_NULLARGCHECK_RETURN(psql);
	SG_NULLARGCHECK_RETURN(ppIdsetReturned);

	//////////////////////////////////////////////////////////////////
	// begin transaction for doing a consistent SELECT.
	// we only look at the DAG_EDGES table.
	//////////////////////////////////////////////////////////////////

	// do the actual fetch_children assuming that a TX is already open.

	SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &pIdset)  );

	// prepare SELECT to fetch all rows from DAG_LEAVES.
	//
	// we don't need to worry about having SQL sort the leaves because
	// we now use a rbtree to store the list in memory.

	SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, psql, &pStmt,
									  "SELECT child_id FROM dag_edges WHERE dagnum=%d AND parent_id = ?", iDagNum)  );
	SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx, pStmt,1,pszDagnodeID)  );
	// read each row.

	while ((rc=sqlite3_step(pStmt)) == SQLITE_ROW)
	{
		const char * szHid = (const char *)sqlite3_column_text(pStmt,0);

		SG_ERR_CHECK(  SG_rbtree__add(pCtx, pIdset,szHid)  );
	}
	if (rc != SQLITE_DONE)
	{
		SG_ERR_THROW(  SG_ERR_SQLITE(rc)  );
	}

	SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );

#if 1
	//////////////////////////////////////////////////////////////////
	// begin consistency checks
	//////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////
	// end consistency checks
	//////////////////////////////////////////////////////////////////
#endif

	*ppIdsetReturned = pIdset;
	return;

fail:
	SG_RBTREE_NULLFREE(pCtx, pIdset);
	SG_ERR_IGNORE(  sg_sqlite__finalize(pCtx, pStmt)  );
	SG_ERR_REPLACE(SG_ERR_SQLITE(SQLITE_BUSY), SG_ERR_DB_BUSY);
return;

}

//////////////////////////////////////////////////////////////////

static void _my_compute_leaves(SG_context* pCtx, sqlite3* psql, SG_uint32 iDagNum, SG_rbtree ** ppIdsetReturned)
{
	// let SQL compute the set of leaves using some fancy/expensive black magic.
	// the caller must free the idset returned.
	//
	// scan the DAG_EDGES TABLE and find all the child_id's that do not
	// appear in the parent_id column.  these are effectively leaves.
	//
	// we require the caller to be in a transaction so that our results
	// come from the same consistent view of the DB as their other results.

	int rc;
	sqlite3_stmt * pStmt = NULL;
	SG_rbtree * pIdset = NULL;

	SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &pIdset)  );

	SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, psql, &pStmt,
									  "SELECT DISTINCT child_id FROM dag_edges"
									  "    WHERE dagnum=%d AND child_id NOT IN"
									  "        (SELECT DISTINCT parent_id FROM dag_edges)", iDagNum)  );

	while ((rc=sqlite3_step(pStmt)) == SQLITE_ROW)
	{
		const char * szHid = (const char *)sqlite3_column_text(pStmt,0);

		SG_ERR_CHECK(  SG_rbtree__add(pCtx, pIdset,szHid)  );
	}
	if (rc != SQLITE_DONE)
	{
		SG_ERR_THROW(  SG_ERR_SQLITE(rc)  );
	}

	SG_ERR_CHECK(  sg_sqlite__finalize(pCtx, pStmt)  );

	*ppIdsetReturned = pIdset;
	return;

fail:
	SG_RBTREE_NULLFREE(pCtx, pIdset);
	SG_ERR_IGNORE(  sg_sqlite__finalize(pCtx,pStmt)  );
	SG_ERR_REPLACE(SG_ERR_SQLITE(SQLITE_BUSY),SG_ERR_DB_BUSY);
}

static void _my_is_sparse(SG_context* pCtx, sqlite3* psql, SG_uint32 iDagNum, SG_bool * pbIsSparse)
{
	// let SQL compute whether the DAG is SPARSE using some fancy/expensive black magic.
	//
	// sparse - a DAG is SPARSE if there are nodes missing between
	//          known nodes and the ROOT.  assuming edges point from
	//          child to parent.
	//
	//          if it is SPARSE, then we cannot walk from every LEAF
	//          all the way to the ROOT.
	//
	// note that this computation only reflects the current snapshot
	// of the DAG that we have.  since we don't know the complete
	// graph or if we know the complete graph, we see if we have a
	// node that has a parent that we don't know about.
	//
	// that is, scan the DAG_EDGES TABLE and see if there is a referenced parent
	// that does not appear as a child.
	//
	// we require the caller to be in a transaction so that our results
	// come from the same consistent view of the DB as their other results.

	sqlite3_stmt * pStmt = NULL;
	SG_int32 result;

	SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, psql,&pStmt,
									  "SELECT EXISTS"
									  "    (SELECT parent_id FROM dag_edges"
									  "         WHERE parent_id != ?"
									  "           AND dagnum = ?"
									  "           AND parent_id NOT IN"
									  "                   (SELECT DISTINCT child_id FROM dag_edges)"
									  "         LIMIT 1)")  );
	SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx, pStmt,1,FAKE_PARENT)  );
	SG_ERR_CHECK(  sg_sqlite__bind_int(pCtx, pStmt,2,iDagNum)  );
	SG_ERR_CHECK(  sg_sqlite__step(pCtx, pStmt,SQLITE_ROW)  );

	result = (SG_int32)sqlite3_column_int(pStmt,0);
	*pbIsSparse = (result == 1);

	SG_ERR_CHECK(  sg_sqlite__finalize(pCtx, pStmt)  );

	return;

fail:
	SG_ERR_IGNORE(  sg_sqlite__finalize(pCtx,pStmt)  );
	SG_ERR_REPLACE(SG_ERR_SQLITE(SQLITE_BUSY),SG_ERR_DB_BUSY);
}

void SG_dag_sqlite3__check_consistency(SG_context* pCtx, sqlite3* psql, SG_uint32 iDagNum)
{
	// I consider this to be a DEBUG routine.  But I'm putting it
	// in the VTABLE, so I'm not #ifdef'ing it because I want
	// the size of the VTABLE to be constant.
	//
	// Run a series of consistency checks on the DAG DB.
	// We do this in our own transaction so that all queries are
	// consistent.
	//
	// we do a rollback when we're done because we don't want to
	// actually make any changes.

	SG_rbtree * pIdsetAsStored = NULL;
	SG_rbtree * pIdsetAsComputed = NULL;
	SG_bool bAreEqual;
	SG_bool bIsSparse = SG_TRUE;

	//////////////////////////////////////////////////////////////////
	// begin transaction for doing a consistent SELECT.
	//////////////////////////////////////////////////////////////////

	// Test #1:
	// [] Read the set of leaves as stored in the DAG_LEAVES table.
	// [] Have SQL compute the set of leaves from the DAG_EDGES table
	//    (this may be O(N) or O(N^2) expensive).
	// [] Compare the 2 sets.  This can be efficient since both rbtrees
	//    are sorted, but is still expensive.

	SG_ERR_CHECK(  _my_fetch_leaves(pCtx, psql,iDagNum,&pIdsetAsStored)  );
	SG_ERR_CHECK(  _my_compute_leaves(pCtx, psql,iDagNum,&pIdsetAsComputed)  );
	SG_ERR_CHECK(  SG_rbtree__compare__keys_only(pCtx, pIdsetAsStored,pIdsetAsComputed,&bAreEqual,NULL,NULL,NULL)  );
	if (!bAreEqual)
	{
		SG_ERR_THROW(  SG_ERR_DAG_NOT_CONSISTENT  );
	}

	// Test #2:
	// [] Verify that the DAG is not sparse.

	SG_ERR_CHECK(  _my_is_sparse(pCtx, psql,iDagNum,&bIsSparse)  );
	if (bIsSparse)
	{
		SG_ERR_THROW(  SG_ERR_DAG_NOT_CONSISTENT  );
	}

	// TODO have a test that verifies no loops in the dag using the generation
	// TODO field.  it ought to be possible to write an SQL statement that does
	// TODO something like:
	// TODO
	// TODO     for each edge in dag_edges ( child_id, parent_id )
	// TODO        join with dag_info on child_id, giving ( child_generation )
	// TODO        join with dag_info on parent_id, giving ( parent_generation )
	// TODO        count rows with ( child_generation <= parent_generation )
	// TODO
	// TODO any such rows would indicate a possible loop in the dag.

	// TODO Any other tests...?

	SG_RBTREE_NULLFREE(pCtx, pIdsetAsStored);
	SG_RBTREE_NULLFREE(pCtx, pIdsetAsComputed);
	return;

fail:
	SG_RBTREE_NULLFREE(pCtx, pIdsetAsStored);
	SG_RBTREE_NULLFREE(pCtx, pIdsetAsComputed);
	SG_ERR_REPLACE(SG_ERR_SQLITE(SQLITE_BUSY),SG_ERR_DB_BUSY);
}

//////////////////////////////////////////////////////////////////

struct _state_is_connected
{
	sqlite3* psql;
    SG_uint32 iDagNum;
	SG_bool bConnected;
	SG_rbtree ** pprbMissing;
};

static void _my_foreach_end_fringe_callback__sqlite3(SG_context* pCtx, const char * szHid, void * pStateVoid)
{
	// a SG_dagfrag__foreach_end_fringe_callback.

	SG_int32 genFetched;
	SG_bool bIsKnown;
	struct _state_is_connected * pStateIsConnected = (struct _state_is_connected *)pStateVoid;

	SG_ERR_CHECK_RETURN(  _my_is_known(pCtx, pStateIsConnected->psql,szHid,&bIsKnown,&genFetched)  );
	if (bIsKnown)
		return;

	// our dag does not know about this HID.
	pStateIsConnected->bConnected = SG_FALSE;

	if (!pStateIsConnected->pprbMissing)		// they don't care about which node is unknown
		return;

	if (!*pStateIsConnected->pprbMissing)		// we deferred creating the rbtree until we actually needed it.
		SG_ERR_CHECK_RETURN(  SG_RBTREE__ALLOC(pCtx, pStateIsConnected->pprbMissing)  );

	// add this HID to the list and return OK so that the iterator does the whole set.

	SG_rbtree__add(pCtx, *pStateIsConnected->pprbMissing,szHid);
}

static void _my_is_connected(SG_context* pCtx,
							 sqlite3* psql,
							 SG_uint32 iDagNum,
							 const SG_dagfrag * pFrag,
							 SG_bool* pbConnected,
							 SG_rbtree ** ppIdsetReturned)
{
	// iterate over the set of dagnodes in the end-fringe in the fragment
	// and see if we already know about all of them.

	SG_rbtree * prbMissing = NULL;		// defer allocating it until we actually need it and if requested.
	struct _state_is_connected state_is_connected;

	SG_NULLARGCHECK_RETURN(psql);
	SG_NULLARGCHECK_RETURN(pFrag);
	SG_NULLARGCHECK_RETURN(pbConnected);

	//////////////////////////////////////////////////////////////////
	// begin transaction for doing a consistent SELECT.
	// we only look at the DAG_INFO table.
	//
	// TODO There's no reason why we need to do all of the lookups in a
	// single transaction (or probably at all), but it I thought it
	// would be faster to have one TX around the whole iterator than
	// it would within iterator callback.  Investigate this later.
	//////////////////////////////////////////////////////////////////

	state_is_connected.psql = psql;
    state_is_connected.iDagNum = iDagNum;
	state_is_connected.pprbMissing = &prbMissing;
	state_is_connected.bConnected = SG_TRUE; // Default to true. The callback only changes this to false
										   // because a single unknown/disconnected node makes the whole
										   // frag disconnected.

	// iterate over all end-fringe nodes in the fragment.  optionally build a list
	// of unknown nodes.

	SG_ERR_CHECK(  SG_dagfrag__foreach_end_fringe(pCtx, pFrag,_my_foreach_end_fringe_callback__sqlite3,&state_is_connected)  );

	if (!state_is_connected.bConnected)			// callback allocated a list to put at least one item
	{
		SG_ASSERT(prbMissing);
        if (ppIdsetReturned)
        {
            *ppIdsetReturned = prbMissing;	// caller now owns the list
        }
        else
        {
            SG_RBTREE_NULLFREE(pCtx, prbMissing);
        }
		*pbConnected = SG_FALSE;
	}
	else
	{
		// all nodes were found, so we are connectable.
		*pbConnected = SG_TRUE;
	}

	return;

fail:
	SG_RBTREE_NULLFREE(pCtx, prbMissing);
}

struct _state_member
{
	sqlite3* psql;
    SG_uint32 iDagNum;
	SG_stringarray ** ppsa_new;
    SG_bool b_not_really;
	SG_rbtree * prbLeaves;
};

static void _my_insert_member_callback__sqlite3(SG_context* pCtx,
												const SG_dagnode * pDagnode,
												SG_dagfrag_state qs,
												void * pCtxVoid)
{
	// a SG_dagfrag__foreach_member_callback

	struct _state_member * pCtxMember = (struct _state_member *)pCtxVoid;
	const char * szHid;
	SG_bool bIsKnownInfo = SG_FALSE;
	SG_int32 generation = -1;
	SG_rbtree_iterator* pit = NULL;

	SG_UNUSED(qs);

	SG_ERR_CHECK_RETURN(  SG_dagnode__get_id_ref(pCtx, pDagnode,&szHid)  );

    if (pCtxMember->b_not_really)
    {
        SG_ERR_CHECK_RETURN(  _my_is_known(pCtx, pCtxMember->psql,szHid,&bIsKnownInfo,&generation)  );
        if (bIsKnownInfo)
        {
            // we didn't find a row in the dag_info table.

            return;
        }
    }
    else
    {
        SG_dag_sqlite3__store_dagnode(pCtx, pCtxMember->psql,pCtxMember->iDagNum,pDagnode);
        if (SG_context__err_equals(pCtx, SG_ERR_DAGNODE_ALREADY_EXISTS))
        {
            // we cannot precompute the amount of overlap between the frag and our dag,
            // so we expect to get duplicates.  silently ignore them.
			SG_context__err_reset(pCtx);
            return;
        }

        if (SG_context__err_equals(pCtx, SG_ERR_CANNOT_CREATE_SPARSE_DAG))
        {
            // this should not happen because we already checked the end-fringe and we
            // assumed that we have a connected graph before we started.  therefore, we
            // must be missing a node in the middle of the graph.
            //
            // return this error code to avoid confusing caller.
            //
            // TODO would it be helpful to also return the conflicting HID?
			SG_ERR_RESET_THROW_RETURN(SG_ERR_DAG_NOT_CONSISTENT);
        }

        if (SG_context__has_err(pCtx)) return;
    }

	// we added a "new to us" node.

	if (!*pCtxMember->ppsa_new)			// we deferred creating the rbtree until we actually needed it.
    {
		SG_ERR_CHECK_RETURN(  SG_STRINGARRAY__ALLOC(pCtx, pCtxMember->ppsa_new, 10)  );
    }

	SG_ERR_CHECK_RETURN(  SG_stringarray__add(pCtx, *pCtxMember->ppsa_new,szHid)  );

	if (pCtxMember->prbLeaves)
	{
		SG_rbtree* prbParents;
		const char* pszParentHid;
		SG_ERR_CHECK_RETURN(  SG_dagnode__get_parents__rbtree_ref(pCtx, pDagnode, &prbParents)  );
		if (prbParents)
		{
			SG_bool found;
			SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, prbParents, &found, &pszParentHid, NULL)  );
			while (found)
			{
				SG_bool has;
				// TODO: When rbtree__remove is fixed, handle the error so we can do just one rbtree lookup.
				SG_ERR_CHECK_RETURN(  SG_rbtree__find(pCtx, pCtxMember->prbLeaves, pszParentHid, &has, NULL)  );
				if (has)
					SG_ERR_CHECK_RETURN(  SG_rbtree__remove(pCtx, pCtxMember->prbLeaves, pszParentHid)  );
				SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &found, &pszParentHid, NULL)  );
			}
			SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);
		}
		if (!bIsKnownInfo)
			SG_ERR_CHECK(  SG_rbtree__add(pCtx, pCtxMember->prbLeaves, szHid)  );
	}

	return;

fail:
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);
}

//////////////////////////////////////////////////////////////////

void SG_dag_sqlite3__check_frag(SG_context* pCtx,
								sqlite3* psql,
								SG_dagfrag * pFrag,
								SG_bool * pbConnected,
								SG_rbtree ** ppIdsetMissing,
								SG_stringarray ** ppsa_new,
								SG_rbtree ** pprbLeaves)
{
	struct _state_member ctx_member;
    SG_uint32 iDagNum = 0;

	SG_NULLARGCHECK_RETURN(psql);
	SG_NULLARGCHECK_RETURN(pFrag);
	SG_NULLARGCHECK_RETURN(pbConnected);
	SG_NULLARGCHECK_RETURN(ppIdsetMissing);
	SG_NULLARGCHECK_RETURN(ppsa_new);

	*ppIdsetMissing = NULL;
	*ppsa_new = NULL;

    SG_ERR_CHECK_RETURN(  SG_dagfrag__get_dagnum(pCtx, pFrag, &iDagNum)  );

	SG_ERR_CHECK_RETURN(  _my_is_connected(pCtx, psql, iDagNum, pFrag, pbConnected, ppIdsetMissing)  );
	if (SG_FALSE == *pbConnected)
		return;

	ctx_member.psql = psql;
    ctx_member.iDagNum = iDagNum;
	ctx_member.ppsa_new = ppsa_new;
    ctx_member.b_not_really = SG_TRUE;
	ctx_member.prbLeaves = NULL;

	if (pprbLeaves)
	{
		SG_uint32 count;
		SG_ERR_CHECK(  _my_fetch_leaves(pCtx, psql, iDagNum, &ctx_member.prbLeaves)  );
		SG_ERR_CHECK(  SG_rbtree__count(pCtx, ctx_member.prbLeaves, &count)  );
		if (!count)
			SG_RBTREE_NULLFREE(pCtx, ctx_member.prbLeaves);
	}

	SG_ERR_CHECK(  SG_dagfrag__foreach_member(pCtx, pFrag,_my_insert_member_callback__sqlite3,(void *)&ctx_member)  );

	if (pprbLeaves)
		*pprbLeaves = ctx_member.prbLeaves;

	return;

fail:
	SG_RBTREE_NULLFREE(pCtx, ctx_member.prbLeaves);
}

void SG_dag_sqlite3__insert_frag(SG_context* pCtx,
								 sqlite3* psql,
								 SG_uint32 iDagNum,
								 SG_dagfrag * pFrag,
								 SG_rbtree ** ppIdsetMissing,
								 SG_stringarray ** ppsa_new)
{
	// try to insert the contents of this fragment into our dag.
	// this takes 2 steps:
	// [1] determine if fragment is "connected" to our dag.  that is,
	//     if we already know about everything in the end-fringe so
	//     that the resulting graph is completely connected (and we
	//     assume that the current graph is completely connected before
	//     we start).
	// [2] add individual dagnodes in ancestor order.  these are done
	//     with one TX for each dagnode so that we don't get funky
	//     conflicts with other concurrent processes.
	//
	// if [1] reports a disconnected graph, we return SG_ERR_CANNOT_CREATE_SPARSE_DAG
	// and an rbtree of the nodes we are missing.
	//
	// during [2] we accumulate the HID of nodes that were "new to us"
	// and return that.
	//
	// if [2] fails, we stop wherever we are and return the actual error.
	// because of the individual add TX's, you can get an error with a
	// partial result.
	//
	// the caller must free both idsets.

	struct _state_member state_member;
	SG_bool bConnected = SG_FALSE;

	SG_NULLARGCHECK_RETURN(psql);
	SG_NULLARGCHECK_RETURN(pFrag);
	SG_NULLARGCHECK_RETURN(ppIdsetMissing);
	SG_NULLARGCHECK_RETURN(ppsa_new);

	*ppIdsetMissing = NULL;
	*ppsa_new = NULL;

	SG_ERR_CHECK_RETURN(  _my_is_connected(pCtx, psql,iDagNum,pFrag,&bConnected,ppIdsetMissing)  );
	if (!bConnected)
		SG_ERR_THROW_RETURN(SG_ERR_CANNOT_CREATE_SPARSE_DAG);

	state_member.psql = psql;
    state_member.iDagNum = iDagNum;
	state_member.ppsa_new = ppsa_new;
    state_member.b_not_really = SG_FALSE;
	state_member.prbLeaves = NULL;

	SG_dagfrag__foreach_member(pCtx, pFrag,_my_insert_member_callback__sqlite3,(void *)&state_member);
}

//////////////////////////////////////////////////////////////////

static FN__sg_fetch_dagnode my_fetch_dagnodes;

static void my_fetch_dagnodes(SG_context* pCtx, void * pCtxFetchDagnode, const char * szHidDagnode, SG_dagnode ** ppDagnode)
{
	SG_dag_sqlite3__fetch_dagnode(pCtx, (sqlite3 *)pCtxFetchDagnode,szHidDagnode,ppDagnode);
}

void SG_dag_sqlite3__get_lca(
	SG_context* pCtx,
    sqlite3* psql,
    SG_uint32 iDagNum,
    const SG_rbtree* prbNodes,
	SG_daglca ** ppDagLca
    )
{
	SG_daglca * pDagLca = NULL;

	SG_ERR_CHECK(  SG_daglca__alloc(pCtx, &pDagLca,iDagNum,my_fetch_dagnodes,psql)  );
	SG_ERR_CHECK(  SG_daglca__add_leaves(pCtx, pDagLca,prbNodes)  );
	SG_ERR_CHECK(  SG_daglca__compute_lca(pCtx, pDagLca)  );

	*ppDagLca = pDagLca;

    return;

fail:
	SG_DAGLCA_NULLFREE(pCtx, pDagLca);
}

// The following code is a sample for how we might store an entire dag
// into a reasonably memory efficient structure.  This is just the
// sqlite version, which will work for the fs2 and sqlite implementations
// of the repo API.
//
// This code grabs the whole dag.  Modifying it to grab only a range of
// generations would be fairly straightforward, as long as a generation
// number is available on all tables.  A join would work, but would be
// more expensive.
//
// If this becomes real code, it will need to be properly added to
// the repo API, which would touch a whole bunch of files, so I'm
// just doing it here as a localized change for now.
//

// TODO move the structure definitions below here to sg_dag.c, probably

struct SG_dn
{
    SG_uint32 num_parents;

    const char** parents;  // points into the pool

    SG_int32 generation;
};

struct SG_dag
{
    SG_rbtree* prb_id;
    SG_rbtree* prb_user;
    SG_uint32 count_leaves;
    const char** leaves;
    SG_strpool* pool;

    // the rbtree uses the pool for all its keys
    //
    // in the rbtree the assoc is an SG_dn*, which is allocated in the pool
    //
    // SG_dn.parents is a buffer allocated in the pool.  The individual pts go
    // into the pool as well.
};

// TODO add other SG_dag__ functions, like accessor calls to actually
// retrieve the info from the dag, etc.

void SG_dag__free(SG_context* pCtx, SG_dag* pThis)
{
    if (!pThis)
    {
        return;
    }

    SG_STRPOOL_NULLFREE(pCtx, pThis->pool);
    SG_RBTREE_NULLFREE(pCtx, pThis->prb_id);
    SG_RBTREE_NULLFREE(pCtx, pThis->prb_user);
    SG_NULLFREE(pCtx, pThis);
}

void SG_dag_sqlite3__get_whole_dag(
	SG_context* pCtx,
    sqlite3* psql,
    SG_uint32 iDagNum,
	SG_dag** pp
    )
{
    SG_dag* pd = NULL;
	sqlite3_stmt * pStmt = NULL;
    int rc;
    SG_int32 count_dagnodes = 0;

    // first count how many dagnodes we're going to have.  we can
    // use this as an estimate for the size of the rbtree.

    // TODO I'm not 100% sure this COUNT(*) call is worth the
    // trouble.  It's expensive on sqlite.  Basically, it allows
    // us to pass an accurate guess into the rbtree allocator,
    // which means rbtree shouldn't need to grow its internal
    // node pool.  But the cost of the COUNT(*) might be
    // worse than just letting rbtree cope.
    //
	SG_ERR_CHECK(  sg_sqlite__exec__va__int32(pCtx, psql, &count_dagnodes, "SELECT COUNT(*) FROM dag_info WHERE dagnum=%d", iDagNum)  );

    // allocate our data structures
    SG_ERR_CHECK(  SG_alloc1(pCtx, pd)  );
    SG_ERR_CHECK(  SG_STRPOOL__ALLOC(pCtx, &pd->pool, 32768)  );

    // the main rbtree will use the pool we just created.  in fact,
    // everything will go into that pool.

    SG_ERR_CHECK(  SG_rbtree__alloc__params(pCtx, &pd->prb_id, count_dagnodes, pd->pool)  );

    // we also want an rbtree which indexes everything by user.  same
    // pool.

    SG_ERR_CHECK(  SG_rbtree__alloc__params(pCtx, &pd->prb_user, 100, pd->pool)  );

    // first, let's setup SG_dn structs for all the dagnodes.  might
    // as well fill in the generation value for each one too.
	SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, psql, &pStmt,
									  "SELECT child_id, generation FROM dag_info WHERE dagnum = ?")  );
	SG_ERR_CHECK(  sg_sqlite__bind_int(pCtx, pStmt, 1, iDagNum)  );

	while ((rc=sqlite3_step(pStmt)) == SQLITE_ROW)
	{
		const char * psz_id = (const char *)sqlite3_column_text(pStmt,0);
        SG_int32 gen = (SG_int32)sqlite3_column_int(pStmt,1);
        SG_dn* pdn = NULL;

        SG_ERR_CHECK(  SG_strpool__add__len(pCtx, pd->pool, sizeof(SG_dn), (const char**) &pdn)  );
        pdn->generation = gen;
        SG_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, pd->prb_id, psz_id, pdn)  );
	}
	if (rc != SQLITE_DONE)
	{
		SG_ERR_THROW(SG_ERR_SQLITE(rc));
	}
	SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );

    // now we grab all the parents.
	SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, psql, &pStmt,
									  "SELECT child_id, parent_id FROM dag_edges WHERE dagnum = ?")  );
	SG_ERR_CHECK(  sg_sqlite__bind_int(pCtx, pStmt, 1, iDagNum)  );

	while ((rc=sqlite3_step(pStmt)) == SQLITE_ROW)
	{
		const char * psz_node_id = (const char *)sqlite3_column_text(pStmt,0);
		const char * psz_parent_id = (const char *)sqlite3_column_text(pStmt,1);

        if (0 != strcmp(psz_parent_id, FAKE_PARENT))
        {
            SG_dn* pdn = NULL;
            const char** pnew = NULL;
            SG_uint16 cur_num_parents = 0;
            const char* pooled_parent_id = NULL;

            SG_ERR_CHECK(  SG_rbtree__find(pCtx, pd->prb_id, psz_node_id, NULL, (void**) &pdn)  );

            // we wish we could efficiently
            // know how many parents each node will have in advance, but
            // we can't.  So each dagnode's parents array is going to
            // grow one parent at a time.  and the previous version of
            // the array becomes wasted memory, which is exactly what
            // we're trying to avoid.  we accept this for now because
            // we believe the case of a dagnode with multiple parents
            // is relatively rare.  the alternative would be to walk
            // every row of this table twice, once to get accurate counts,
            // and once to actually store all the parents info.

            cur_num_parents = (SG_uint16) pdn->num_parents;

            SG_ERR_CHECK(  SG_strpool__add__len(pCtx, pd->pool, sizeof(char*) * ( cur_num_parents + 1), (const char**) &pnew)  );
            if (cur_num_parents)
            {
                memcpy((void*) pnew, pdn->parents, cur_num_parents * sizeof(char*));
            }

            SG_ERR_CHECK(  SG_rbtree__key(pCtx, pd->prb_id, psz_parent_id, &pooled_parent_id)  );
            SG_ASSERT(pooled_parent_id);

            pnew[cur_num_parents] = pooled_parent_id;
            pdn->parents = pnew;
            cur_num_parents++;
            pdn->num_parents = cur_num_parents;
        }
	}
	if (rc != SQLITE_DONE)
	{
		SG_ERR_THROW(SG_ERR_SQLITE(rc));
	}
	SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );

    *pp = pd;
    pd = NULL;

fail:
    SG_DAG_NULLFREE(pCtx, pd);
	sg_sqlite__nullfinalize(pCtx, &pStmt);
}


