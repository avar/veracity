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
 * @file sg_jsondb.c
 *
 * @details Implementation for storing json objects in a sql store for speedier access to elements.
 *
 */
#include <sg.h>

#define MY_BUSY_TIMEOUT_MS      (30000)

typedef struct
{
	sqlite3* psql;
	const char* pszObjectName;
	SG_int64 objectId;
} _jsondb_handle;

struct _add_foreach_state
{
	_jsondb_handle* pJsonDb;
	const char* pszParentPath;
	SG_uint32 iVarrayIdxOffset;
};
typedef struct _add_foreach_state add_foreach_state;

//////////////////////////////////////////////////////////////////////////

#define STACK_SIZE 1024
struct _subTreeStack
{
	void* stack[STACK_SIZE];
	SG_bool isVhash[STACK_SIZE];
	SG_uint32 ndx;
};
typedef struct _subTreeStack subTreeStack;

static void _push(SG_context* pCtx, subTreeStack* pStack, SG_bool isVhash, void* p)
{
	if (pStack->ndx == (STACK_SIZE - 1))
		SG_ERR_THROW_RETURN(SG_ERR_LIMIT_EXCEEDED);

	pStack->stack[++pStack->ndx] = p;
	pStack->isVhash[pStack->ndx] = isVhash;
}

static void _pop(SG_context* pCtx, subTreeStack* pStack, SG_bool* pIsVhash, void** pp)
{
	if (pStack->ndx == 0)
		SG_ERR_THROW_RETURN(SG_ERR_LIMIT_EXCEEDED);

	*pIsVhash = pStack->isVhash[pStack->ndx];
	*pp = pStack->stack[pStack->ndx--];
}

//////////////////////////////////////////////////////////////////////////

static void _insertVariant(
	SG_context* pCtx,
	_jsondb_handle* pMe,
	const char* pszNewNodePath,
	SG_bool bAddRecursive,
	SG_bool bVariantIndexOk,
	const SG_variant* pv);

/**
 * Don't call this on the root path.  It won't work.
 */
static void _pathHelper(
	SG_context* pCtx,
	const char* pszInPath,
	SG_uint32 lenPath,
	char** ppszScrubbedPath,
	char** ppszParentPath,
	char** ppszLeafName)
{
	SG_uint32 i;
	char* pszScrubbedPath = NULL;
	char* pszParentPath = NULL;
	char* pszLeafName = NULL;

	if (!lenPath)
		lenPath = strlen(pszInPath);
	SG_ASSERT(lenPath > 1);

	// Look for double-slashes
	for (i = 1; i < lenPath; i++)
	{
		if ( (pszInPath[i] == '/') && (pszInPath[i-1] == '/') )
			SG_ERR_THROW_RETURN(SG_ERR_JSONDB_INVALID_PATH);
	}

	// Ignore trailing slash.
	if (pszInPath[lenPath - 1] == '/')
	{
		// As long as it's not escaped.
		if ( (lenPath < 2) || (pszInPath[lenPath-2] != '\\') )
			lenPath--;
	}

	if (ppszScrubbedPath)
	{
		SG_ERR_CHECK(  SG_allocN(pCtx,lenPath+1,pszScrubbedPath)  );
		memcpy(pszScrubbedPath, pszInPath, lenPath);
		pszScrubbedPath[lenPath] = 0;
	}

	for (i = lenPath - 1; SG_TRUE; i--)
	{
		if ( (pszInPath[i] == '/') && ((i < 2) || (pszInPath[i-1] != '\\')) )
		{
			if (ppszParentPath)
			{
				// Remove the trailing slash except when it's the root node.
				SG_uint32 lenDestBuf = (i ? i+1 : 2);

				SG_ERR_CHECK(  SG_allocN(pCtx, lenDestBuf, pszParentPath)  );

				// We expect strcpy to truncate the child path to create the parent path.
				SG_strcpy(pCtx, pszParentPath, lenDestBuf, pszInPath);
				SG_ASSERT(SG_context__err_equals(pCtx, SG_ERR_BUFFERTOOSMALL));
				SG_ERR_CHECK_CURRENT_DISREGARD(SG_ERR_BUFFERTOOSMALL);
			}

			if (ppszLeafName)
			{
				const char* pszLeaf = &pszInPath[i] + 1;
				SG_uint32 len = strlen(pszLeaf);

				// remove trailing slash
				if (pszLeaf[len-1] == '/')
				{
					// As long as it's not escaped.
					if ( (len < 2) || (pszLeaf[len-2] != '\\') )
						len--;
				}

				pszLeafName = (char*)SG_malloc((SG_uint32)(len+1));
				if (!pszLeafName)
					SG_ERR_THROW_RETURN(SG_ERR_MALLOCFAILED);

				memcpy(pszLeafName,pszLeaf,len);
				pszLeafName[len] = 0;
			}

			break;
		}

		if (0 == i)
			SG_ERR_THROW(SG_ERR_JSONDB_INVALID_PATH);
	}

	SG_RETURN_AND_NULL(pszScrubbedPath, ppszScrubbedPath);
	SG_RETURN_AND_NULL(pszParentPath, ppszParentPath);
	SG_RETURN_AND_NULL(pszLeafName, ppszLeafName);

	/* fall through */
fail:
	SG_NULLFREE(pCtx, pszScrubbedPath);
	SG_NULLFREE(pCtx, pszParentPath);
	SG_NULLFREE(pCtx, pszLeafName);
}

/**
 * Caller must manage sqlite tx.
 */
static void _insertNode(
	SG_context* pCtx,
	_jsondb_handle* pMe,
	const char* pszNewNodePathGiven,
	SG_bool bAddRecursive,
	SG_uint16 type,
	SG_bool bVariantIndexOk,
	const char* pszVal,
	SG_uint32* piNewNodeRgt,
	char** ppszNewNodePath)
{
	SG_uint32 lenPath = strlen(pszNewNodePathGiven);
	char* pszNewNodePathForVarray = NULL;
	char* pszNewNodePath = NULL;
	char* pszParentPath = NULL;
	char* pszYoungestVarraySiblingLeafName = NULL;
	char* pszNodeLeafName = NULL;
	SG_uint32 iParentNodeRgt;
	SG_bool bValidVarrayPath = SG_FALSE;
	SG_uint16 parentType;
	const char* pszYoungestVarraySiblingPath = NULL;
	char buf[11];


	sqlite3_stmt* pStmt = NULL;
#ifdef DEBUG
	SG_uint32 nrNodesUpdated;
#endif

	if (!pMe->objectId)
		SG_ERR_THROW_RETURN(  SG_ERR_JSONDB_NO_CURRENT_OBJECT  );

	if (pszNewNodePathGiven[0] != '/')
		SG_ERR_THROW_RETURN(SG_ERR_JSONDB_INVALID_PATH); // path not rooted

	if (lenPath > 1)
	{
		// Not the root node, so we need to find/create/update ancestors

		// Get parent path
		SG_ERR_CHECK(  _pathHelper(pCtx, pszNewNodePathGiven, lenPath, &pszNewNodePath, &pszParentPath, &pszNodeLeafName)  );

		// When adding an element to a varray, the node name should be "#", e.g. "/my_varray/#"
		// to signify "add this element at the end of the array".
		if ( (pszNodeLeafName[0] == '#') && (strlen(pszNodeLeafName) == 1) )
			bValidVarrayPath = SG_TRUE;

		// Get parent
		SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, pMe->psql, &pStmt,
			"SELECT type, rgt FROM nodes "
			"WHERE json_object_id = ? AND full_path = ? LIMIT 1")  );

		SG_ERR_CHECK(  sg_sqlite__bind_int64(pCtx, pStmt, 1, pMe->objectId)  );
		SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx, pStmt, 2, pszParentPath)  );
		sg_sqlite__step(pCtx, pStmt, SQLITE_ROW);
		if (SG_context__has_err(pCtx))
		{
			// The parent node doesn't already exist.
			if (bAddRecursive)
			{
				// Recursively add parents, get parent rgt.

				SG_ERR_CHECK_CURRENT_DISREGARD(SG_ERR_SQLITE(SQLITE_DONE));
				SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );

				// Add the impliied parent node.  It will be a vhash unless '#'
				// was given for its name, in which case it will be a varray.
				if (bValidVarrayPath)
				{
					char* pTmp = NULL;
					parentType = SG_VARIANT_TYPE_VARRAY;

					// When adding to a varray, the path changes: the #'s get replaced with numbers.
					// So we have to re-set the actual parent path here.
					SG_ERR_CHECK(  _insertNode(pCtx, pMe, pszParentPath, bAddRecursive, parentType, SG_FALSE,
						NULL, &iParentNodeRgt, &pTmp)  );
					SG_NULLFREE(pCtx, pszParentPath);
					pszParentPath = pTmp;
				}
				else
				{
					parentType = SG_VARIANT_TYPE_VHASH;
					SG_ERR_CHECK(  _insertNode(pCtx, pMe, pszParentPath, bAddRecursive, parentType, SG_FALSE,
						NULL, &iParentNodeRgt, NULL)  );
				}
			}
			else
			{
				SG_ERR_REPLACE(SG_ERR_SQLITE(SQLITE_DONE), SG_ERR_JSONDB_PARENT_DOESNT_EXIST);
				SG_context__err_set_description(pCtx, pszParentPath);
				SG_ERR_RETHROW;
			}
		}
		else
		{
			// The parent node exists, but it has to be a vhash or varray to have have descendants.
			parentType = (SG_uint16)sqlite3_column_int(pStmt, 0);
			if (parentType == SG_VARIANT_TYPE_VHASH)
			{
				// A vhash is always an acceptable parent.  All is well, do nothing.
			}
			else if (parentType == SG_VARIANT_TYPE_VARRAY)
			{
				// A varray is an acceptable parent if this node's key is "#", which we determined above.
				if (!bValidVarrayPath && !bVariantIndexOk)
					SG_ERR_THROW2(SG_ERR_JSONDB_INVALID_PATH, (pCtx, pszNewNodePathGiven));
			}
			else
				SG_ERR_THROW2(SG_ERR_JSONDB_NON_CONTAINER_ANCESTOR_EXISTS, (pCtx, pszParentPath));

			// The parent exists and is a valid container type. Grab its rgt.
			iParentNodeRgt = sqlite3_column_int(pStmt, 1);
			SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );
		}

		// If we're inserting a varray element, we need to determine its index.
		if (parentType == SG_VARIANT_TYPE_VARRAY && !bVariantIndexOk)
		{
			SG_uint32 index;
			SG_uint32 lenNewPath;

			SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, pMe->psql, &pStmt,
				"SELECT child.full_path "
				"FROM nodes AS parent, nodes AS child "
				"WHERE parent.json_object_id = ?1 AND child.json_object_id = ?1 "
				"  AND parent.rgt = ?2 AND child.rgt = parent.rgt - 1 "
				"LIMIT 1;")  );
			SG_ERR_CHECK(  sg_sqlite__bind_int64(pCtx, pStmt, 1, pMe->objectId)  );
			SG_ERR_CHECK(  sg_sqlite__bind_int(pCtx, pStmt, 2, iParentNodeRgt)  );
			sg_sqlite__step(pCtx, pStmt, SQLITE_ROW);
			if (SG_context__err_equals(pCtx, SG_ERR_SQLITE(SQLITE_DONE)))
			{
				SG_ERR_DISCARD;
				// The parent varray is empty.  New node's index should be 0.
				index = 0;
			}
			else
			{
				SG_ERR_CHECK_CURRENT;

				// The parent varray is not empty.  Get next index.
				pszYoungestVarraySiblingPath = (const char*)sqlite3_column_text(pStmt, 0);
				SG_ERR_CHECK(  _pathHelper(pCtx, pszYoungestVarraySiblingPath, 0, NULL, NULL, &pszYoungestVarraySiblingLeafName)  );

				SG_ERR_CHECK(  SG_uint32__parse(pCtx, &index, pszYoungestVarraySiblingLeafName)  );
				index++;
			}
			SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );

			// Set path with correct index
			SG_ERR_CHECK(  SG_sprintf(pCtx, buf, 10, "%d", index)  );
			if (strcmp(pszParentPath, "/") == 0)
			{
				lenNewPath = strlen(pszParentPath) + strlen(buf) + 1;
				SG_ERR_CHECK(  SG_allocN(pCtx, lenNewPath, pszNewNodePathForVarray)  );
				SG_ERR_CHECK(  SG_strcpy(pCtx, pszNewNodePathForVarray, lenNewPath, pszParentPath)  );
			}
			else
			{
				lenNewPath = strlen(pszParentPath) + strlen(buf) + 2;
				SG_ERR_CHECK(  SG_allocN(pCtx, lenNewPath, pszNewNodePathForVarray)  );
				SG_ERR_CHECK(  SG_strcpy(pCtx, pszNewNodePathForVarray, lenNewPath, pszParentPath)  );
				SG_ERR_CHECK(  SG_strcat(pCtx, pszNewNodePathForVarray, lenNewPath, "/")  );
			}
			SG_ERR_CHECK(  SG_strcat(pCtx, pszNewNodePathForVarray, lenNewPath, buf)  );
			SG_NULLFREE(pCtx, pszNodeLeafName);
			pszNodeLeafName = buf;
		}

		// Update ancestor lft/rgt values
		SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, pMe->psql, &pStmt,
			"UPDATE nodes "
			"  SET lft = CASE WHEN lft > ?1 THEN lft + 2 ELSE lft END, "
			"      rgt = CASE when rgt >= ?1 THEN rgt + 2 ELSE rgt END "
			"WHERE json_object_id = ?2 AND rgt >= ?1;")  );
		SG_ERR_CHECK(  sg_sqlite__bind_int(pCtx, pStmt, 1, iParentNodeRgt)  );
		SG_ERR_CHECK(  sg_sqlite__bind_int64(pCtx, pStmt, 2, pMe->objectId)  );
		SG_ERR_CHECK(  sg_sqlite__step(pCtx, pStmt, SQLITE_DONE)  );
		SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );
#ifdef DEBUG
		nrNodesUpdated = 0;
		SG_ERR_CHECK(  sg_sqlite__num_changes(pCtx, pMe->psql, &nrNodesUpdated)  );
		SG_ASSERT(nrNodesUpdated);
#endif
	} // (lenPath > 1)
	else
	{
		// Inserting root node.
		SG_ERR_CHECK(  SG_strdup(pCtx, pszNewNodePathGiven, &pszNewNodePath)  );
		iParentNodeRgt = 1;
	}

	// Insert new node
	{
		const char* pszActualPath = (pszNewNodePathForVarray ? pszNewNodePathForVarray : pszNewNodePath);

		SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, pMe->psql, &pStmt,
			"INSERT into nodes "
			"  (json_object_id, full_path, leaf_name, type, val, lft, rgt) VALUES "
			"  (?, ?, ?, ?, ?, ?, ?);")  );
		SG_ERR_CHECK(  sg_sqlite__bind_int64(pCtx, pStmt, 1, pMe->objectId)  );
		SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx, pStmt, 2, pszActualPath)  );
		if (pszNodeLeafName)
			SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx, pStmt, 3, pszNodeLeafName)  );
		else
			SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx, pStmt, 3, "**ROOT**")  );
		SG_ERR_CHECK(  sg_sqlite__bind_int(pCtx, pStmt, 4, type)  );
		SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx, pStmt, 5, pszVal)  );
		SG_ERR_CHECK(  sg_sqlite__bind_int(pCtx, pStmt, 6, iParentNodeRgt)  );
		SG_ERR_CHECK(  sg_sqlite__bind_int(pCtx, pStmt, 7, iParentNodeRgt + 1)  );

		sg_sqlite__step(pCtx, pStmt, SQLITE_DONE);
		if (SG_context__err_equals(pCtx, SG_ERR_SQLITE(SQLITE_CONSTRAINT)))
			SG_ERR_RESET_THROW2(SG_ERR_JSONDB_OBJECT_ALREADY_EXISTS, (pCtx, pszActualPath));
		SG_ERR_CHECK_CURRENT;

		SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );
	}

#ifdef DEBUG
	nrNodesUpdated = 0;
	SG_ERR_CHECK(  sg_sqlite__num_changes(pCtx, pMe->psql, &nrNodesUpdated)  );
	SG_ASSERT(nrNodesUpdated == 1);
#endif

	if (ppszNewNodePath)
	{
		if (pszNewNodePathForVarray)
			SG_RETURN_AND_NULL(pszNewNodePathForVarray, ppszNewNodePath);
		else
			SG_RETURN_AND_NULL(pszNewNodePath, ppszNewNodePath);
	}

	if (piNewNodeRgt)
		*piNewNodeRgt = iParentNodeRgt + 1;

	/* fall through */
fail:
	SG_NULLFREE(pCtx, pszNewNodePath);
	SG_NULLFREE(pCtx, pszParentPath);
	if (pszNodeLeafName != buf)
		SG_NULLFREE(pCtx, pszNodeLeafName);
	SG_NULLFREE(pCtx, pszYoungestVarraySiblingLeafName);
	SG_NULLFREE(pCtx, pszNewNodePathForVarray);
	SG_ERR_IGNORE(  sg_sqlite__finalize(pCtx, pStmt)  );
}

/**
 * Caller must manage sqlite tx.
 */
static void _vhash_add_foreach_cb(
	SG_context* pCtx,
	void* ctx,
	const SG_vhash* pvh,
	const char* putf8Key,
	const SG_variant* pv)
{
	add_foreach_state* pState = (add_foreach_state*)ctx;
	SG_uint32 len;
	char* pszNewNodePath = NULL;

	SG_UNUSED(pvh);

	len = strlen(pState->pszParentPath) + strlen(putf8Key) + 2;
	SG_ERR_CHECK(  SG_allocN(pCtx, len, pszNewNodePath)  );
	SG_ERR_CHECK(  SG_strcpy(pCtx, pszNewNodePath, len, pState->pszParentPath)  );
	SG_ERR_CHECK(  SG_strcat(pCtx, pszNewNodePath, len, "/")  );
	SG_ERR_CHECK(  SG_strcat(pCtx, pszNewNodePath, len, putf8Key)  );

	SG_ERR_CHECK(  _insertVariant(pCtx, pState->pJsonDb, pszNewNodePath, SG_FALSE, SG_FALSE, pv)  );

	/* fall through */
fail:
	SG_NULLFREE(pCtx, pszNewNodePath);
}

static void _varray_add_foreach_cb(
	SG_context* pCtx,
	void* pVoidData,
	const SG_varray* pva,
	SG_uint32 ndx,
	const SG_variant* pv)
{
	add_foreach_state* pState = (add_foreach_state*)pVoidData;
	SG_uint32 len;
	char* pszNewNodePath = NULL;
	char buf[11];

	SG_UNUSED(pva);

	ndx += pState->iVarrayIdxOffset;
	SG_ERR_CHECK(  SG_sprintf(pCtx, buf, 10, "%d", ndx)  );

	len = strlen(pState->pszParentPath) + strlen(buf) + 2;
	SG_ERR_CHECK(  SG_allocN(pCtx, len, pszNewNodePath)  );
	SG_ERR_CHECK(  SG_strcpy(pCtx, pszNewNodePath, len, pState->pszParentPath)  );
	SG_ERR_CHECK(  SG_strcat(pCtx, pszNewNodePath, len, "/")  );
	SG_ERR_CHECK(  SG_strcat(pCtx, pszNewNodePath, len, buf)  );

	SG_ERR_CHECK(  _insertVariant(pCtx, pState->pJsonDb, pszNewNodePath, SG_FALSE, SG_TRUE, pv)  );

	/* fall through */
fail:
	SG_NULLFREE(pCtx, pszNewNodePath);
}

static void _insertVariant(
	SG_context* pCtx,
	_jsondb_handle* pMe,
	const char* pszNewNodePath,
	SG_bool bAddRecursive,
	SG_bool bVariantIndexOk,
	const SG_variant* pv)
{
	char buf[256];
	add_foreach_state state;
	char* pszParentPath = NULL;


	switch (pv->type)
	{
	case SG_VARIANT_TYPE_NULL:
		SG_ERR_CHECK(  _insertNode(pCtx, pMe, pszNewNodePath, bAddRecursive, pv->type,
			bVariantIndexOk, NULL, NULL, NULL)  );
		break;
	case SG_VARIANT_TYPE_INT64:
		SG_int64_to_sz(pv->v.val_int64, buf);
		SG_ERR_CHECK(  _insertNode(pCtx, pMe, pszNewNodePath, bAddRecursive, pv->type,
			bVariantIndexOk, buf, NULL, NULL)  );
		break;
	case SG_VARIANT_TYPE_DOUBLE:
		SG_ERR_CHECK(  SG_sprintf(pCtx, buf, 255, "%f", pv->v.val_double)  );
		SG_ERR_CHECK(  _insertNode(pCtx, pMe, pszNewNodePath, bAddRecursive, pv->type,
			bVariantIndexOk, buf, NULL, NULL)  );
		break;
	case SG_VARIANT_TYPE_BOOL:
		SG_ERR_CHECK(  _insertNode(pCtx, pMe, pszNewNodePath, bAddRecursive, pv->type,
			bVariantIndexOk, pv->v.val_bool ? "1" : "0", NULL, NULL)  );
		break;
	case SG_VARIANT_TYPE_SZ:
		SG_ERR_CHECK(  _insertNode(pCtx, pMe, pszNewNodePath, bAddRecursive, pv->type,
			bVariantIndexOk, pv->v.val_sz, NULL, NULL)  );
		break;
	case SG_VARIANT_TYPE_VHASH:
		SG_ERR_CHECK(  _insertNode(pCtx, pMe, pszNewNodePath, bAddRecursive, pv->type,
			bVariantIndexOk, NULL, NULL, NULL)  );
		state.pJsonDb = pMe;
		if (0 == strcmp(pszNewNodePath, "/"))
			state.pszParentPath = "";
		else
			state.pszParentPath = pszNewNodePath;
		SG_ERR_CHECK(  SG_vhash__foreach(pCtx, pv->v.val_vhash, _vhash_add_foreach_cb, &state)  );
		break;
	case SG_VARIANT_TYPE_VARRAY:
		SG_ERR_CHECK(  _insertNode(pCtx, pMe, pszNewNodePath, bAddRecursive, pv->type,
			bVariantIndexOk, NULL, NULL, &pszParentPath)  );
		state.pJsonDb = pMe;
		state.iVarrayIdxOffset = 0;
		if (0 == strcmp(pszParentPath, "/"))
			state.pszParentPath = "";
		else
			state.pszParentPath = pszParentPath;
		SG_ERR_CHECK(  SG_varray__foreach(pCtx, pv->v.val_varray, _varray_add_foreach_cb, &state)  );
		SG_NULLFREE(pCtx, pszParentPath);
		break;
	default:
		SG_ERR_THROW(SG_ERR_NOTIMPLEMENTED);
	}

	return;

fail:
	SG_NULLFREE(pCtx, pszParentPath);
}

static void _removeNode(
	SG_context* pCtx,
	_jsondb_handle* pMe,
	SG_bool bDescendantsOnly, // used by update, delete subtree but not the node itself
	SG_int64 deleteNodeId,
	SG_uint32 deleteNodeLft,
	SG_uint32 deleteNodeRgt)
{
	sqlite3_stmt* pStmt = NULL;
	sqlite3_stmt* pStmtSibs = NULL;
	SG_uint32 newLftRgt;
	char* pszParentNodeFullPath = NULL;
	char* pszNewFullPath = NULL;

	// If the deleted node's parent is a varray, and the deleted node has younger
	// siblings, we need to update their paths and the paths of their descendants.
	if (!bDescendantsOnly)
	{
		// Get parent node
		SG_int64 parentId = 0;
		SG_uint32 parentLft = 0, parentRgt = 0;
		SG_uint16 parentType = 0;

		newLftRgt = deleteNodeRgt - deleteNodeLft + 1;

		SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, pMe->psql, &pStmt,
			"SELECT parent.id, parent.full_path, parent.lft, parent.rgt, parent.type "
			"FROM nodes child, nodes parent "
			"WHERE parent.lft < child.lft AND parent.rgt > child.rgt "
			"  AND child.id= ? "
			"  AND parent.json_object_id = ? "
			"ORDER BY parent.rgt-child.rgt ASC "
			"LIMIT 1;")  );
		SG_ERR_CHECK(  sg_sqlite__bind_int64(pCtx, pStmt, 1, deleteNodeId)  );
		SG_ERR_CHECK(  sg_sqlite__bind_int64(pCtx, pStmt, 2, pMe->objectId)  );
		sg_sqlite__step(pCtx, pStmt, SQLITE_ROW);
		if (!SG_context__has_err(pCtx))
		{
			parentId = sqlite3_column_int64(pStmt, 0);
			SG_ERR_CHECK(  SG_strdup(pCtx, (const char*)sqlite3_column_text(pStmt, 1), &pszParentNodeFullPath)  );
			parentLft = sqlite3_column_int(pStmt, 2);
			parentRgt = sqlite3_column_int(pStmt, 3);
			parentType = (SG_uint16)sqlite3_column_int(pStmt, 4);
		}
		else
		{
			// The root node has no parent, so this is ok.
			SG_ERR_CHECK_CURRENT_DISREGARD(SG_ERR_SQLITE(SQLITE_DONE));
		}

		SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );

		// Delete the node and and all its descendants
		SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, pMe->psql, &pStmt,
			"DELETE FROM nodes "
			"WHERE json_object_id = ?1 "
			"  AND lft >= ?2 AND rgt <= ?3")  );
		SG_ERR_CHECK(  sg_sqlite__bind_int64(pCtx, pStmt, 1, pMe->objectId)  );
		SG_ERR_CHECK(  sg_sqlite__bind_int(pCtx, pStmt, 2, deleteNodeLft)  );
		SG_ERR_CHECK(  sg_sqlite__bind_int(pCtx, pStmt, 3, deleteNodeRgt)  );
		SG_ERR_CHECK(  sg_sqlite__step(pCtx, pStmt, SQLITE_DONE)  );
		SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );

		if (parentType == SG_VARIANT_TYPE_VARRAY)
		{
			// The parent's a varray, so we'll update the path of any younger
			// siblings and their descendants.

			SG_int32 rc;
			SG_uint32 lenBufNewFullPath = strlen(pszParentNodeFullPath) + 12;

			SG_ERR_CHECK(  SG_allocN(pCtx, lenBufNewFullPath, pszNewFullPath)  );

			// Get the deleted node's younger siblings.
			SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, pMe->psql, &pStmt,
				"SELECT child.id, child.leaf_name, child.lft, child.rgt "
				"FROM nodes child, nodes parent "
				"WHERE child.lft BETWEEN parent.lft AND parent.rgt "
				"  AND parent.id = "
				"      (SELECT MAX(s.id) FROM nodes s "
				"      WHERE s.lft < child.lft AND s.rgt > child.rgt) "
				"  AND parent.id = ? "
				"  AND child.json_object_id = ? "
				"  AND child.lft > ?")  );
			SG_ERR_CHECK(  sg_sqlite__bind_int64(pCtx, pStmt, 1, parentId)  );
			SG_ERR_CHECK(  sg_sqlite__bind_int64(pCtx, pStmt, 2, pMe->objectId)  );
			SG_ERR_CHECK(  sg_sqlite__bind_int(pCtx, pStmt, 3, deleteNodeRgt)  );

			while ((rc=sqlite3_step(pStmt)) == SQLITE_ROW)
			{
				// Update the younger sibling's path and leaf name.

				SG_uint32 newLeafVal = sqlite3_column_int(pStmt, 1) - 1;
				SG_uint32 lenActualNewFullPath;

				char newLeafName[11];
				SG_ERR_CHECK(  SG_sprintf(pCtx, newLeafName, 11, "%d", newLeafVal)  );

				SG_ERR_CHECK(  SG_strcpy(pCtx, pszNewFullPath, lenBufNewFullPath, pszParentNodeFullPath)  );
				if (pszNewFullPath[1] != 0)
					SG_ERR_CHECK(  SG_strcat(pCtx, pszNewFullPath, lenBufNewFullPath, "/")  );
				SG_ERR_CHECK(  SG_strcat(pCtx, pszNewFullPath, lenBufNewFullPath, newLeafName)  );
				lenActualNewFullPath = strlen(pszNewFullPath);

				SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, pMe->psql, &pStmtSibs,
					"UPDATE nodes "
					"SET leaf_name = ?, full_path = ? "
					"WHERE id = ?")  );
				SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx, pStmtSibs, 1, newLeafName)  );
				SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx, pStmtSibs, 2, pszNewFullPath)  );
				SG_ERR_CHECK(  sg_sqlite__bind_int64(pCtx, pStmtSibs, 3, sqlite3_column_int64(pStmt, 0))  );
				SG_ERR_CHECK(  sg_sqlite__step(pCtx, pStmtSibs, SQLITE_DONE)  );
				SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx, &pStmtSibs)  );

				// Update the younger sibling's descendants' paths.
				SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, pMe->psql, &pStmtSibs,
					"UPDATE nodes "
					"SET full_path = ? || substr(full_path, ?) "
					"WHERE json_object_id = ? "
					"  AND lft > ? AND rgt < ?")  );
				SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx, pStmtSibs, 1, pszNewFullPath)  );
				SG_ERR_CHECK(  sg_sqlite__bind_int(pCtx, pStmtSibs, 2, lenActualNewFullPath + 1)  );
				SG_ERR_CHECK(  sg_sqlite__bind_int64(pCtx, pStmtSibs, 3, pMe->objectId)  );
				SG_ERR_CHECK(  sg_sqlite__bind_int(pCtx, pStmtSibs, 4, sqlite3_column_int(pStmt, 2))  );
				SG_ERR_CHECK(  sg_sqlite__bind_int(pCtx, pStmtSibs, 5, sqlite3_column_int(pStmt, 3))  );
				SG_ERR_CHECK(  sg_sqlite__step(pCtx, pStmtSibs, SQLITE_DONE)  );
				SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx, &pStmtSibs)  );
			}
			if (rc != SQLITE_DONE)
				SG_ERR_THROW(  SG_ERR_SQLITE(rc)  );

			SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );
		}
	}
	else
	{
		newLftRgt = deleteNodeRgt - deleteNodeLft - 1;

		// Deleting only the descendants of the specified node, not the node itself.
		// Delete the node and and all its descendants
		SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, pMe->psql, &pStmt,
			"DELETE FROM nodes "
			"WHERE json_object_id = ?1 "
			"  AND lft > ?2 AND rgt < ?3")  );
		SG_ERR_CHECK(  sg_sqlite__bind_int64(pCtx, pStmt, 1, pMe->objectId)  );
		SG_ERR_CHECK(  sg_sqlite__bind_int(pCtx, pStmt, 2, deleteNodeLft)  );
		SG_ERR_CHECK(  sg_sqlite__bind_int(pCtx, pStmt, 3, deleteNodeRgt)  );
		SG_ERR_CHECK(  sg_sqlite__step(pCtx, pStmt, SQLITE_DONE)  );
		SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );
	}

	// Close the gap left by the deleted subtree
	SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, pMe->psql, &pStmt,
		"UPDATE nodes "
		"SET lft = CASE "
		"  WHEN lft > ?1 THEN lft - ?4 "
		"  ELSE lft END, "
		" rgt = CASE "
		"  WHEN rgt > ?1 THEN rgt - ?4 "
		"  ELSE rgt END "
		"WHERE json_object_id = ?3 "
		"  AND (lft > ?1 OR rgt > ?1)")  );
	SG_ERR_CHECK(  sg_sqlite__bind_int(pCtx, pStmt, 1, deleteNodeLft)  );
	SG_ERR_CHECK(  sg_sqlite__bind_int(pCtx, pStmt, 2, deleteNodeRgt)  );
	SG_ERR_CHECK(  sg_sqlite__bind_int64(pCtx, pStmt, 3, pMe->objectId)  );
	SG_ERR_CHECK(  sg_sqlite__bind_int(pCtx, pStmt, 4, newLftRgt)  );
	SG_ERR_CHECK(  sg_sqlite__step(pCtx, pStmt, SQLITE_DONE)  );
	SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );

	/* fall through */
fail:
	SG_ERR_IGNORE(  sg_sqlite__finalize(pCtx, pStmt)  );
	SG_ERR_IGNORE(  sg_sqlite__finalize(pCtx, pStmtSibs)  );
	SG_NULLFREE(pCtx, pszParentNodeFullPath);
	SG_NULLFREE(pCtx, pszNewFullPath);
}

//////////////////////////////////////////////////////////////////////////

void SG_jsondb__close_free(SG_context* pCtx, SG_jsondb* pThis)
{
	_jsondb_handle* pMe = (_jsondb_handle*)pThis;

	if (pMe)
	{
		SG_ERR_CHECK_RETURN(  sg_sqlite__close(pCtx, pMe->psql)  );
		SG_NULLFREE(pCtx, pMe->pszObjectName);
		SG_NULLFREE(pCtx, pMe);
	}
}

//////////////////////////////////////////////////////////////////////////

void SG_jsondb__create(SG_context* pCtx, const SG_pathname* pPathDbFile, SG_jsondb** ppJsondb)
{
	_jsondb_handle* pMe = NULL;
	SG_jsondb* pJsondb = NULL;
	SG_bool bCreated = SG_FALSE;
	SG_bool bInTx = SG_FALSE;

	SG_ERR_CHECK(  SG_alloc1(pCtx, pMe)  );
	pJsondb = (SG_jsondb*)pMe;

	SG_ERR_CHECK(  sg_sqlite__create__pathname(pCtx, pPathDbFile, &pMe->psql)  );
	bCreated = SG_TRUE;
	sqlite3_busy_timeout(pMe->psql, MY_BUSY_TIMEOUT_MS);

	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pMe->psql, "BEGIN TRANSACTION;")  );
	bInTx = SG_TRUE;

	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pMe->psql,
		"CREATE TABLE json_objects"
		"   ("
		"     id INTEGER PRIMARY KEY,"
		"     name VARCHAR NULL"
		"   )")  );

	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pMe->psql,
		"CREATE TABLE nodes"
		"   ("
		"     id INTEGER PRIMARY KEY,"
		"     json_object_id INTEGER NOT NULL,"
		"     full_path VARCHAR NOT NULL,"
		"     leaf_name VARCHAR NOT NULL,"
		"     type INTEGER NOT NULL,"
		"     val VARCHAR NULL,"
		"     lft INTEGER NOT NULL,"
		"     rgt INTEGER NOT NULL"
		"   )")  );

	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pMe->psql,
		"CREATE UNIQUE INDEX json_objects_name on json_objects ( name )")  );

	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pMe->psql,
		"CREATE UNIQUE INDEX nodes_json_object_id_full_path on nodes ( json_object_id, full_path )")  );

	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pMe->psql,
		"CREATE INDEX nodes_json_object_id_lft_rgt on nodes ( json_object_id, lft, rgt )")  );
	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pMe->psql,
		"CREATE INDEX nodes_json_object_id_rgt on nodes ( json_object_id, rgt )")  );

	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pMe->psql, "COMMIT TRANSACTION;")  );
	bInTx = SG_FALSE;

	SG_RETURN_AND_NULL(pJsondb, ppJsondb);

	/* fall through */
fail:
	if (bInTx && SG_context__has_err(pCtx) && pMe && pMe->psql)
		SG_ERR_IGNORE(  sg_sqlite__exec(pCtx, pMe->psql, "ROLLBACK TRANSACTION;")  );
	SG_JSONDB_NULLFREE(pCtx, pJsondb);
	if (bCreated && SG_context__has_err(pCtx)) // If we created the database but we're failing, clean it up.
		SG_ERR_IGNORE(  SG_fsobj__rmdir__pathname(pCtx, pPathDbFile)  );
}

void SG_jsondb__open(
	SG_context* pCtx,
	const SG_pathname* pPathDbFile,
	const char* pszObjectName,
	SG_jsondb** ppJsondb)
{
	_jsondb_handle* pMe = NULL;
	SG_jsondb* pJsondb = NULL;

	SG_ERR_CHECK(  SG_alloc1(pCtx, pMe)  );
	pJsondb = (SG_jsondb*)pMe;

	SG_ERR_CHECK(  sg_sqlite__open__pathname(pCtx, pPathDbFile, &pMe->psql)  );
	sqlite3_busy_timeout(pMe->psql, MY_BUSY_TIMEOUT_MS);

	SG_ERR_CHECK(  SG_jsondb__set_current_object(pCtx, pJsondb, pszObjectName)  );

	SG_RETURN_AND_NULL(pJsondb, ppJsondb);

	/* fall through */
fail:
	SG_JSONDB_NULLFREE(pCtx, pJsondb);
}

void SG_jsondb__create_or_open(
	SG_context* pCtx,
	const SG_pathname* pPathDbFile,
	const char* pszObjectName,
	SG_jsondb** ppJsondb)
{
	SG_bool exists;
	SG_jsondb* pDb = NULL;
	SG_bool bCreatedFile = SG_FALSE;

	SG_NULLARGCHECK_RETURN(pszObjectName);

	SG_ERR_CHECK_RETURN(  SG_fsobj__exists__pathname(pCtx, pPathDbFile, &exists, NULL, NULL)  );
	if (exists)
	{
		SG_ERR_CHECK(  SG_jsondb__open(pCtx, pPathDbFile, NULL, &pDb)  );
		SG_jsondb__set_current_object(pCtx, pDb, pszObjectName);
		if (SG_context__err_equals(pCtx, SG_ERR_NOT_FOUND))
		{
			SG_ERR_DISCARD;
			SG_ERR_CHECK(  SG_jsondb__add__object(pCtx, pDb, pszObjectName, NULL)  );
		}
		else
		{
			SG_ERR_CHECK_CURRENT;
		}
	}
	else
	{
		SG_ERR_CHECK(  SG_jsondb__create(pCtx, pPathDbFile, &pDb)  );
		bCreatedFile = SG_TRUE;
		SG_ERR_CHECK(  SG_jsondb__add__object(pCtx, pDb, pszObjectName, NULL)  );
	}

	SG_RETURN_AND_NULL(pDb, ppJsondb);

	return;

fail:
	SG_JSONDB_NULLFREE(pCtx, pDb);
	if (bCreatedFile)
		SG_ERR_IGNORE(  SG_fsobj__remove__pathname(pCtx, pPathDbFile)  );
}

//////////////////////////////////////////////////////////////////////////

void SG_jsondb__get_current_object_name(
	SG_context* pCtx,
	SG_jsondb* pThis,
	const char** ppszObjectName)
{
	_jsondb_handle* pMe = (_jsondb_handle*)pThis;

	SG_NULLARGCHECK_RETURN(pThis);
	SG_NULLARGCHECK_RETURN(ppszObjectName);

	*ppszObjectName = pMe->pszObjectName;
}

void SG_jsondb__set_current_object(
	SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszObjectName)
{
	_jsondb_handle* pMe = (_jsondb_handle*)pThis;
	const char* pszOldObjectName = NULL;
	sqlite3_stmt* pStmt = NULL;

	SG_NULLARGCHECK_RETURN(pThis);

	if (!pszObjectName)
	{
		pMe->objectId = 0;
		SG_NULLFREE(pCtx, pMe->pszObjectName);
		return;
	}

	SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, pMe->psql, &pStmt,
		"SELECT id FROM json_objects WHERE name = ? LIMIT 1")  );
	SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx, pStmt, 1, pszObjectName)  );
	sg_sqlite__step(pCtx, pStmt, SQLITE_ROW);
	if (SG_context__has_err(pCtx))
	{
		SG_ERR_REPLACE(SG_ERR_SQLITE(SQLITE_DONE), SG_ERR_NOT_FOUND);
		SG_ERR_RETHROW;
	}

	pMe->objectId = (SG_int32)sqlite3_column_int(pStmt, 0);

	pszOldObjectName = pMe->pszObjectName;
	SG_ERR_CHECK(  SG_strdup(pCtx, pszObjectName, (char**)&pMe->pszObjectName)  );

	SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );

	/* fall through */
fail:
	SG_ERR_IGNORE(  sg_sqlite__finalize(pCtx, pStmt)  );
	SG_NULLFREE(pCtx, pszOldObjectName);
}

void SG_jsondb__get__object(
	SG_context* pCtx,
	SG_jsondb* pThis,
	SG_variant** ppvhObject)
{
	SG_jsondb__get__variant(pCtx, pThis, "/", ppvhObject);
}

void SG_jsondb__add__object(
	SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszObjectName,
	SG_variant* pvRootObject)
{
	_jsondb_handle* pMe = (_jsondb_handle*)pThis;
	_jsondb_handle tempMe;
	sqlite3_stmt* pStmt = NULL;
	SG_int64 lNewObjectId;
	SG_bool bInTx = SG_FALSE;

	SG_NULLARGCHECK_RETURN(pThis);
	SG_NULLARGCHECK_RETURN(pszObjectName);

	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pMe->psql, "BEGIN TRANSACTION;")  );
	bInTx = SG_TRUE;

	// Insert object
	SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, pMe->psql, &pStmt,
		"INSERT INTO json_objects (name) VALUES (?)")  );
	SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx, pStmt, 1, pszObjectName)  );
	sg_sqlite__step(pCtx, pStmt, SQLITE_DONE);
	SG_ERR_REPLACE(SG_ERR_SQLITE(SQLITE_CONSTRAINT), SG_ERR_JSONDB_OBJECT_ALREADY_EXISTS);
	SG_ERR_CHECK_CURRENT;
	SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );

	SG_ERR_CHECK(  sg_sqlite__last_insert_rowid(pCtx, pMe->psql, &lNewObjectId)  );

	if (pvRootObject)
	{
		tempMe.objectId = lNewObjectId;
		tempMe.psql = pMe->psql;
		tempMe.pszObjectName = pszObjectName;
		SG_ERR_CHECK(  _insertVariant(pCtx, &tempMe, "/", SG_FALSE, SG_FALSE, pvRootObject)  );
	}

	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pMe->psql, "COMMIT TRANSACTION;")  );
	bInTx = SG_FALSE;

	// Now that everything has succeeded, set the new current object.
	pMe->objectId = lNewObjectId;
	SG_NULLFREE(pCtx, pMe->pszObjectName);
	SG_ERR_CHECK(  SG_strdup(pCtx, pszObjectName, (char**)&pMe->pszObjectName)  );

	return;

fail:
	SG_ERR_IGNORE(sg_sqlite__finalize(pCtx, pStmt));
	if (bInTx && pMe && pMe->psql)
		SG_ERR_IGNORE(  sg_sqlite__exec(pCtx, pMe->psql, "ROLLBACK TRANSACTION;")  );
}

void SG_jsondb__remove__object(
	SG_context* pCtx,
	SG_jsondb* pThis)
{
	_jsondb_handle* pMe = (_jsondb_handle*)pThis;
	sqlite3_stmt* pStmt = NULL;
	SG_bool bInTx = SG_FALSE;

	SG_NULLARGCHECK_RETURN(pThis);

	if (!pMe->objectId)
		SG_ERR_THROW_RETURN(SG_ERR_JSONDB_NO_CURRENT_OBJECT);

	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pMe->psql, "BEGIN TRANSACTION;")  );
	bInTx = SG_TRUE;

	// delete object
	SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, pMe->psql, &pStmt,
		"DELETE FROM nodes WHERE json_object_id = ?;")  );
	SG_ERR_CHECK(  sg_sqlite__bind_int64(pCtx, pStmt, 1, pMe->objectId)  );
	SG_ERR_CHECK(  sg_sqlite__step(pCtx, pStmt, SQLITE_DONE)  );
	SG_ERR_CHECK(  sg_sqlite__finalize(pCtx, pStmt)  );

	SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, pMe->psql, &pStmt,
		"DELETE FROM json_objects WHERE id = ?;")  );
	SG_ERR_CHECK(  sg_sqlite__bind_int64(pCtx, pStmt, 1, pMe->objectId)  );
	SG_ERR_CHECK(  sg_sqlite__step(pCtx, pStmt, SQLITE_DONE)  );
#ifdef DEBUG
	{
		SG_uint32 nrNodesUpdated = 0;
		SG_ERR_CHECK(  sg_sqlite__num_changes(pCtx, pMe->psql, &nrNodesUpdated)  );
		SG_ASSERT(nrNodesUpdated == 1);
	}
#endif

	SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );

	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pMe->psql, "COMMIT TRANSACTION;")  );
	bInTx = SG_FALSE;

	// Now that everything has succeeded, set the new current object.
	pMe->objectId = 0;
	SG_NULLFREE(pCtx, pMe->pszObjectName);

	return;

fail:
	SG_ERR_IGNORE(sg_sqlite__finalize(pCtx, pStmt));
	if (bInTx && pMe && pMe->psql)
		SG_ERR_IGNORE(  sg_sqlite__exec(pCtx, pMe->psql, "ROLLBACK TRANSACTION;")  );
}

//////////////////////////////////////////////////////////////////////////

/**
 * pStmt should have these columns in this order:
 * leaf_name, type, val, lft, rgt
 *
 * Note that ppszVal is pStmt's copy.  You have to copy it if you want it after
 * the statement is finalized.
 */
static void _getSingleNode(
	SG_context* pCtx,
	sqlite3_stmt* pStmt,
	SG_uint16 expectedType,
	SG_uint16* pActualType,
	SG_int64* plVal,
	double* pDblVal,
	SG_bool* pbVal,
	const char** ppszVal,
	char** ppszLeafName,
	SG_uint32* piLft,
	SG_uint32* piRgt)
{
	SG_uint16 fetchedType;

	fetchedType = (SG_uint16)sqlite3_column_int(pStmt, 1);
	if ((fetchedType & expectedType) != fetchedType)
		SG_ERR_THROW_RETURN(SG_ERR_VARIANT_INVALIDTYPE);

	switch (fetchedType)
	{
	case SG_VARIANT_TYPE_NULL:
		// nothing to do
		break;
	case SG_VARIANT_TYPE_INT64:
		if (plVal)
			*plVal = sqlite3_column_int64(pStmt, 2);
		break;
	case SG_VARIANT_TYPE_DOUBLE:
		if (pDblVal)
			*pDblVal = sqlite3_column_double(pStmt, 2);
		break;
	case SG_VARIANT_TYPE_BOOL:
		if (pbVal)
			*pbVal = sqlite3_column_int(pStmt, 2);
		break;
	case SG_VARIANT_TYPE_SZ:
		if (ppszVal)
			*ppszVal = (const char*)sqlite3_column_text(pStmt, 2);
		break;
	case SG_VARIANT_TYPE_VHASH:
		// nothing to do
		break;
	case SG_VARIANT_TYPE_VARRAY:
		// nothing to do
		break;
	default:
		SG_ERR_THROW_RETURN(SG_ERR_NOTIMPLEMENTED);
	}

	if (piLft)
		*piLft = sqlite3_column_int(pStmt, 3);
	if (piRgt)
		*piRgt = sqlite3_column_int(pStmt, 4);

	if (pActualType)
		*pActualType = fetchedType;

	if (ppszLeafName)
	{
		SG_ERR_CHECK_RETURN(  SG_jsondb__unescape_keyname(pCtx, (const char*)sqlite3_column_text(pStmt, 0), ppszLeafName)  );
	}
}

static void _getSubTree(
	SG_context* pCtx,
	_jsondb_handle* pMe,
	SG_uint32 root_lft,
	SG_uint32 root_rgt,
	SG_vhash* root_pvh,
	SG_varray* root_pva)
{
	sqlite3_stmt* pStmt = NULL;
	SG_int32 rc;
	SG_vhash* current_pvh = root_pvh;
	SG_varray* current_pva = root_pva;
	SG_vhash* pvh_temp = NULL; // We alloc this and therefore own it
    SG_vhash* pvh_temp_ref = NULL; // We need to refer to pvh_temp after losing ownership
	SG_varray* pva_temp = NULL; // We alloc this and therefore own it
    SG_varray* pva_temp_ref = NULL; // We need to refer to pva_temp after losing ownership
	SG_uint32 last_rgt = root_lft;
	char* pszLeafName = NULL;

	subTreeStack stack;
	stack.ndx = 0;

	SG_ASSERT( (root_pvh == NULL) ^ (root_pva == NULL));

	SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, pMe->psql, &pStmt,
		"SELECT leaf_name, type, val, lft, rgt FROM nodes "
		"WHERE json_object_id = ? "
		"  AND lft > ? AND rgt < ? "
		"ORDER BY lft;")  );
	SG_ERR_CHECK(  sg_sqlite__bind_int64(pCtx, pStmt, 1, pMe->objectId)  );
	SG_ERR_CHECK(  sg_sqlite__bind_int(pCtx, pStmt, 2, root_lft)  );
	SG_ERR_CHECK(  sg_sqlite__bind_int(pCtx, pStmt, 3, root_rgt)  );

	while ((rc=sqlite3_step(pStmt)) == SQLITE_ROW)
	{
		SG_uint16 type;
		SG_int64 lVal;
		double dblVal;
		SG_bool bVal;
		const char* pszVal;

		SG_uint32 lft, rgt;

		SG_ERR_CHECK(  _getSingleNode(pCtx, pStmt, SG_UINT16_MAX, &type,
			&lVal, &dblVal, &bVal, &pszVal, &pszLeafName, &lft, &rgt)  );

		for (; last_rgt+1 < lft; last_rgt++)
		{
			SG_bool isVhash = SG_FALSE;
			void* p = NULL;
			SG_ERR_CHECK(  _pop(pCtx, &stack, &isVhash, &p)  );

			if (isVhash)
			{
				current_pvh = (SG_vhash*)p;
				current_pva = NULL;
			}
			else
			{
				current_pvh = NULL;
				current_pva = (SG_varray*)p;
			}
		}

		switch (type)
		{
		case SG_VARIANT_TYPE_NULL:
			SG_ASSERT(rgt - lft == 1);
			if (current_pvh)
				SG_ERR_CHECK(  SG_vhash__add__null(pCtx, current_pvh, pszLeafName)  );
			else
				SG_ERR_CHECK(  SG_varray__append__null(pCtx, current_pva)  );
			break;

		case SG_VARIANT_TYPE_INT64:
			SG_ASSERT(rgt - lft == 1);
			if (current_pvh)
				SG_ERR_CHECK(  SG_vhash__add__int64(pCtx, current_pvh, pszLeafName, lVal)  );
			else
				SG_ERR_CHECK(  SG_varray__append__int64(pCtx, current_pva, lVal)  );
			break;

		case SG_VARIANT_TYPE_DOUBLE:
			SG_ASSERT(rgt - lft == 1);
			if (current_pvh)
				SG_ERR_CHECK(  SG_vhash__add__double(pCtx, current_pvh, pszLeafName, dblVal)  );
			else
				SG_ERR_CHECK(  SG_varray__append__double(pCtx, current_pva, dblVal)  );
			break;

		case SG_VARIANT_TYPE_BOOL:
			SG_ASSERT(rgt - lft == 1);
			if (current_pvh)
				SG_ERR_CHECK(  SG_vhash__add__bool(pCtx, current_pvh, pszLeafName, bVal)  );
			else
				SG_ERR_CHECK(  SG_varray__append__bool(pCtx, current_pva, bVal)  );
			break;

		case SG_VARIANT_TYPE_SZ:
			SG_ASSERT(rgt - lft == 1);
			if (current_pvh)
				SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, current_pvh, pszLeafName, pszVal)  );
			else
				SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, current_pva, pszVal)  );
			break;

		case SG_VARIANT_TYPE_VHASH:

			SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvh_temp)  );
			pvh_temp_ref = pvh_temp;
			pva_temp_ref = NULL;
			if (current_pvh)
				SG_ERR_CHECK(  SG_vhash__add__vhash(pCtx, current_pvh, pszLeafName, &pvh_temp)  );
			else
				SG_ERR_CHECK(  SG_varray__append__vhash(pCtx, current_pva, &pvh_temp)  );
			break;

		case SG_VARIANT_TYPE_VARRAY:

			SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pva_temp)  );
			pva_temp_ref = pva_temp;
			pvh_temp_ref = NULL;
			if (current_pvh)
				SG_ERR_CHECK(  SG_vhash__add__varray(pCtx, current_pvh, pszLeafName, &pva_temp)  );
			else
				SG_ERR_CHECK(  SG_varray__append__varray(pCtx, current_pva, &pva_temp)  );
			break;

		default:
			SG_ERR_THROW(SG_ERR_NOTIMPLEMENTED);
		}

		if (rgt - lft > 1)
		{
			if (current_pvh)
				SG_ERR_CHECK(  _push(pCtx, &stack, SG_TRUE, current_pvh)  );
			else
				SG_ERR_CHECK(  _push(pCtx, &stack, SG_FALSE, current_pva)  );

			current_pvh = pvh_temp_ref;
			current_pva = pva_temp_ref;
		}

		last_rgt = rgt;
		SG_NULLFREE(pCtx, pszLeafName);

	} // while (rc == SQLITE_ROW)

	if (rc != SQLITE_DONE)
		SG_ERR_THROW(  SG_ERR_SQLITE(rc)  );

	/* fall through */
fail:
	SG_ERR_IGNORE(  sg_sqlite__finalize(pCtx, pStmt)  );
	SG_VHASH_NULLFREE(pCtx, pvh_temp);
	SG_VARRAY_NULLFREE(pCtx, pva_temp);
	SG_NULLFREE(pCtx, pszLeafName);
}

static void _getByPath(
	SG_context* pCtx,
	_jsondb_handle* pMe,
	const char* pszPath,
	SG_uint16 expectedType,
	SG_uint16* pActualType,
	SG_int64* plVal,
	double* pDblVal,
	SG_bool* pbVal,
	char** ppszVal,
	SG_vhash** ppvhVal,
	SG_varray** ppvaVal)
{
	SG_uint32 lenPath;
	char* pszScrubbedPath = NULL;
	sqlite3_stmt* pStmt = NULL;
	SG_uint16 actualType;
	const char* pszVal = NULL;
	SG_uint32 lft, rgt;
	SG_vhash* pvh = NULL;
	SG_varray* pva = NULL;

	if (!pMe->objectId)
		SG_ERR_THROW_RETURN(SG_ERR_JSONDB_NO_CURRENT_OBJECT);

	if (pszPath[0] != '/')
		SG_ERR_THROW_RETURN(SG_ERR_JSONDB_INVALID_PATH);

	lenPath = strlen(pszPath);
	if (lenPath > 1)
		SG_ERR_CHECK(  _pathHelper(pCtx, pszPath, lenPath, &pszScrubbedPath, NULL, NULL)  );
	else
		SG_ERR_CHECK(  SG_strdup(pCtx, pszPath, &pszScrubbedPath)  );

	SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, pMe->psql, &pStmt,
		"SELECT leaf_name, type, val, lft, rgt "
		"FROM nodes "
		"WHERE json_object_id = ? AND full_path = ? "
		"LIMIT 1")  );
	SG_ERR_CHECK(  sg_sqlite__bind_int64(pCtx, pStmt, 1, pMe->objectId)  );
	SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx, pStmt, 2, pszScrubbedPath)  );

	sg_sqlite__step(pCtx, pStmt, SQLITE_ROW);
	SG_ERR_REPLACE(SG_ERR_SQLITE(SQLITE_DONE), SG_ERR_NOT_FOUND);
	SG_ERR_CHECK_CURRENT;

	SG_ERR_CHECK(  _getSingleNode(pCtx, pStmt, expectedType, &actualType,
		plVal, pDblVal, pbVal, &pszVal, NULL, &lft, &rgt)  );

	if ( (actualType == SG_VARIANT_TYPE_VHASH) && ppvhVal )
	{
		SG_VHASH__ALLOC(pCtx, &pvh);
		SG_ERR_CHECK(  _getSubTree(pCtx, pMe, lft, rgt, pvh, NULL)  );
		*ppvhVal = pvh;
		pvh = NULL;
	}
	else if ( (actualType == SG_VARIANT_TYPE_VARRAY) && ppvaVal )
	{
		SG_VARRAY__ALLOC(pCtx, &pva);
		SG_ERR_CHECK(  _getSubTree(pCtx, pMe, lft, rgt, NULL, pva)  );
		*ppvaVal = pva;
		pva = NULL;
	}

	if (pActualType)
		*pActualType = actualType;

	if (ppszVal && pszVal)
		SG_ERR_CHECK(  SG_strdup(pCtx, pszVal, ppszVal)  );


	SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );
	SG_NULLFREE(pCtx, pszScrubbedPath);

	return;

fail:
	SG_ERR_IGNORE(  sg_sqlite__finalize(pCtx, pStmt)  );
	SG_NULLFREE(pCtx, pszScrubbedPath);
	SG_VHASH_NULLFREE(pCtx, pvh);
	SG_VARRAY_NULLFREE(pCtx, pva);
}

#define PRE_GET		_jsondb_handle* pMe = (_jsondb_handle*)pThis;									\
					SG_bool bInTx = SG_FALSE;														\
					SG_NULLARGCHECK_RETURN(pThis);													\
					SG_NULLARGCHECK_RETURN(pszPath);												\
					SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pMe->psql, "BEGIN TRANSACTION;")  );		\
					bInTx = SG_TRUE

#define POST_GET	fail:																			\
					if (bInTx && pMe && pMe->psql)													\
						SG_ERR_IGNORE(  sg_sqlite__exec(pCtx, pMe->psql, "ROLLBACK TRANSACTION;")  )

void SG_jsondb__has(
	SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszPath,
	SG_bool* pbResult)
{
	SG_bool bExists;

	PRE_GET;

	SG_NULLARGCHECK(pbResult);

	_getByPath(pCtx, pMe, pszPath, SG_UINT16_MAX, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
	if (SG_context__err_equals(pCtx, SG_ERR_NOT_FOUND))
	{
		SG_ERR_DISCARD;
		bExists = SG_FALSE;
	}
	else
	{
		bExists = SG_TRUE;
	}
	SG_ERR_CHECK_CURRENT;

	*pbResult = bExists;

	POST_GET;
}

void SG_jsondb__typeof(
	SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszPath,
	SG_uint16* pResult)
{
	PRE_GET;

	SG_NULLARGCHECK(pResult);
	SG_ERR_CHECK(  _getByPath(pCtx, pMe, pszPath, SG_UINT16_MAX, pResult, NULL, NULL, NULL, NULL, NULL, NULL)  );

	POST_GET;
}

void SG_jsondb__count(
	SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszPath,
	SG_uint32* piResult)
{
	sqlite3_stmt* pStmt = NULL;
	SG_int64 id;
	SG_uint32 lft, rgt, count;
	PRE_GET;

	SG_NULLARGCHECK(piResult);

	if (!pMe->objectId)
		SG_ERR_THROW_RETURN(SG_ERR_JSONDB_NO_CURRENT_OBJECT);

	if (pszPath[0] != '/')
		SG_ERR_THROW_RETURN(SG_ERR_JSONDB_INVALID_PATH);

	// Check that an object exists at the specified path.
	SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, pMe->psql, &pStmt,
		"SELECT id, lft, rgt "
		"FROM nodes "
		"WHERE json_object_id = ? AND full_path = ? "
		"LIMIT 1")  );
	SG_ERR_CHECK(  sg_sqlite__bind_int64(pCtx, pStmt, 1, pMe->objectId)  );
	SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx, pStmt, 2, pszPath)  );

	sg_sqlite__step(pCtx, pStmt, SQLITE_ROW);
	SG_ERR_REPLACE(SG_ERR_SQLITE(SQLITE_DONE), SG_ERR_NOT_FOUND);
	SG_ERR_CHECK_CURRENT;
	id = sqlite3_column_int64(pStmt, 0);
	lft = sqlite3_column_int(pStmt, 1);
	rgt = sqlite3_column_int(pStmt, 2);
	SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );

	// If the node's a leaf, we don't need to bother with the second query.
	if (!(rgt-lft-1))
	{
		*piResult = 0;
	}
	else
	{
		// The node's not a leaf so we have to count the edges.
		SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, pMe->psql, &pStmt,
			"SELECT COUNT(child.id) "
			"FROM nodes child, nodes parent "
			"WHERE parent.id = ? "
			"AND child.lft BETWEEN parent.lft AND parent.rgt "
			"AND parent.id = (SELECT MAX(s.id) FROM nodes s WHERE s.json_object_id = ? AND s.lft < child.lft AND s.rgt > child.rgt) "
			"AND parent.json_object_id = child.json_object_id")  );
		SG_ERR_CHECK(  sg_sqlite__bind_int64(pCtx, pStmt, 1, id)  );
		SG_ERR_CHECK(  sg_sqlite__bind_int64(pCtx, pStmt, 2, pMe->objectId)  );
		SG_ERR_CHECK(  sg_sqlite__step(pCtx, pStmt, SQLITE_ROW)  );
		count = sqlite3_column_int(pStmt, 0);
		SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );

		*piResult = count;
	}

	POST_GET;
	SG_ERR_IGNORE(  sg_sqlite__finalize(pCtx, pStmt)  );
}

void SG_jsondb__get__variant(
	SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszPath,
	SG_variant** ppResult)
{
	SG_variant* pResult = NULL;

	PRE_GET;

	SG_NULLARGCHECK(ppResult);

	SG_ERR_CHECK(  SG_alloc1(pCtx, pResult)  );

	SG_ERR_CHECK(  _getByPath(pCtx, pMe, pszPath, SG_UINT16_MAX, &pResult->type, &pResult->v.val_int64,
		&pResult->v.val_double, &pResult->v.val_bool, (char**)&pResult->v.val_sz, &pResult->v.val_vhash, &pResult->v.val_varray)  );

	*ppResult = pResult;
	pResult = NULL;

	POST_GET;
	SG_NULLFREE(pCtx, pResult);
}

void SG_jsondb__get__sz(
	SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszPath,
	char** ppszValue)
{
	PRE_GET;
	SG_ERR_CHECK(  _getByPath(pCtx, pMe, pszPath, SG_VARIANT_TYPE_SZ, NULL, NULL, NULL, NULL, ppszValue, NULL, NULL)  );
	POST_GET;
}

void SG_jsondb__get__vhash(
	SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszPath,
	SG_vhash** ppvhObject)
{
	PRE_GET;
	SG_ERR_CHECK(  _getByPath(pCtx, pMe, pszPath, SG_VARIANT_TYPE_VHASH, NULL, NULL, NULL, NULL, NULL, ppvhObject, NULL)  );
	POST_GET;
}

void SG_jsondb__check__sz(
	SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszPath,
	SG_bool* pbExists,
	char** ppszValue)
{
	PRE_GET;

	SG_NULLARGCHECK(pbExists);

	_getByPath(pCtx, pMe, pszPath, SG_VARIANT_TYPE_SZ, NULL, NULL, NULL, NULL, ppszValue, NULL, NULL);
	if (SG_context__err_equals(pCtx, SG_ERR_NOT_FOUND))
	{
		SG_ERR_DISCARD;
		*pbExists = SG_FALSE;
	}
	else
	{
		SG_ERR_CHECK_CURRENT;
		*pbExists = SG_TRUE;
	}

	POST_GET;
}

void SG_jsondb__get__double(
	SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszPath,
	double* pResult)
{
	PRE_GET;
	SG_ERR_CHECK(  _getByPath(pCtx, pMe, pszPath, SG_VARIANT_TYPE_DOUBLE, NULL, NULL, pResult, NULL, NULL, NULL, NULL)  );
	POST_GET;
}

void SG_jsondb__get__int64(
	SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszPath,
	SG_int64* pResult)
{
	PRE_GET;
	SG_ERR_CHECK(  _getByPath(pCtx, pMe, pszPath, SG_VARIANT_TYPE_INT64, NULL, pResult, NULL, NULL, NULL, NULL, NULL)  );
	POST_GET;
}

void SG_jsondb__get__int64_or_double(
	SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszPath,
	SG_int64* pResult)
{
	SG_int64 i64 = 0;
	double dbl = 0;

	PRE_GET;

	SG_NULLARGCHECK(pResult);

	SG_ERR_CHECK(  _getByPath(pCtx, pMe, pszPath,
		SG_VARIANT_TYPE_DOUBLE | SG_VARIANT_TYPE_INT64, NULL, &i64, &dbl, NULL, NULL, NULL, NULL)  );

	if (pResult)
	{
		if (dbl)
		{
			if (SG_double__fits_in_int64(dbl))
				*pResult = (SG_int64)dbl;
			else
				SG_ERR_THROW(  SG_ERR_VARIANT_INVALIDTYPE  );
		}
		else
			*pResult = i64;
	}

	POST_GET;
}

void SG_jsondb__get__uint32(
	SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszPath,
	SG_uint32* pResult)
{
	SG_int64 i64;

	PRE_GET;

	SG_ERR_CHECK(  _getByPath(pCtx, pMe, pszPath, SG_VARIANT_TYPE_INT64, NULL, &i64, NULL, NULL, NULL, NULL, NULL)  );
	if (SG_int64__fits_in_uint32(i64))
		*pResult = (SG_uint32) i64;
	else
		SG_ERR_THROW(  SG_ERR_INTEGER_OVERFLOW  );

	POST_GET;
}

void SG_jsondb__get__bool(
	SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszPath,
	SG_bool* pResult)
{
	PRE_GET;
	SG_ERR_CHECK(  _getByPath(pCtx, pMe, pszPath, SG_VARIANT_TYPE_BOOL, NULL, NULL, NULL, pResult, NULL, NULL, NULL)  );
	POST_GET;
}

void SG_jsondb__get__varray(
	SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszPath,
	SG_varray** ppResult)
{
	PRE_GET;
	SG_ERR_CHECK(  _getByPath(pCtx, pMe, pszPath, SG_VARIANT_TYPE_VARRAY, NULL, NULL, NULL, NULL, NULL, NULL, ppResult)  );
	POST_GET;
}
//////////////////////////////////////////////////////////////////////////

#define PRE_ADD		_jsondb_handle* pMe = (_jsondb_handle*)pThis;									\
					SG_bool bInTx = SG_FALSE;														\
					SG_NULLARGCHECK_RETURN(pThis);													\
					SG_NULLARGCHECK_RETURN(pszPath);												\
					SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pMe->psql, "BEGIN TRANSACTION;")  );		\
					bInTx = SG_TRUE

#define POST_ADD	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pMe->psql, "COMMIT TRANSACTION;")  );		\
					bInTx = SG_FALSE;																\
				fail:																				\
					if (bInTx && pMe && pMe->psql)													\
						SG_ERR_IGNORE(  sg_sqlite__exec(pCtx, pMe->psql, "ROLLBACK TRANSACTION;")  )


void SG_jsondb__add__string__sz(
	SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszPath,
	SG_bool bAddRecursive,
	const char* pszValue)
{
	PRE_ADD;
	SG_ERR_CHECK(  _insertNode(pCtx, pMe, pszPath, bAddRecursive, SG_VARIANT_TYPE_SZ,
		SG_FALSE, pszValue, NULL, NULL)  );
	POST_ADD;
}

void SG_jsondb__add__string__buflen(
	SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszPath,
	SG_bool bAddRecursive,
	const char* pszValue,
	SG_uint32 len)
{
	char* pszToStore = NULL;
	PRE_ADD;

	if (len == 0 || strlen(pszValue) < len)
	{
		SG_ERR_CHECK(  _insertNode(pCtx, pMe, pszPath, bAddRecursive, SG_VARIANT_TYPE_SZ,
			SG_FALSE, pszValue, NULL, NULL)  );
	}
	else
	{
		SG_ERR_CHECK(  SG_allocN(pCtx, len+1, pszToStore)  );
		SG_strcpy(pCtx, pszToStore, len+1, pszValue);
		SG_ERR_CHECK_CURRENT_DISREGARD(SG_ERR_BUFFERTOOSMALL);
		SG_ERR_CHECK(  _insertNode(pCtx, pMe, pszPath, bAddRecursive, SG_VARIANT_TYPE_SZ,
			SG_FALSE, pszToStore, NULL, NULL)  );
	}

	POST_ADD;
	SG_NULLFREE(pCtx, pszToStore);
}

void SG_jsondb__add__int64(
	SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszPath,
	SG_bool bAddRecursive,
	SG_int64 intValue)
{
	SG_int_to_string_buffer tmp;

	PRE_ADD;

	SG_int64_to_sz(intValue, tmp);
	SG_ERR_CHECK(  _insertNode(pCtx, pMe, pszPath, bAddRecursive, SG_VARIANT_TYPE_INT64,
		SG_FALSE, tmp, NULL, NULL)  );

	POST_ADD;
}

void SG_jsondb__add__double(
	SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszPath,
	SG_bool bAddRecursive,
	double fv)
{
	char buf[256];

	PRE_ADD;

	SG_ERR_CHECK(  SG_sprintf(pCtx, buf, 255, "%f", fv)  );
	SG_ERR_CHECK(  _insertNode(pCtx, pMe, pszPath, bAddRecursive, SG_VARIANT_TYPE_DOUBLE,
		SG_FALSE, buf, NULL, NULL)  );

	POST_ADD;
}

void SG_jsondb__add__null(
	SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszPath,
	SG_bool bAddRecursive)
{
	PRE_ADD;
	SG_ERR_CHECK(  _insertNode(pCtx, pMe, pszPath, bAddRecursive, SG_VARIANT_TYPE_NULL,
		SG_FALSE, NULL, NULL, NULL)  );
	POST_ADD;
}

void SG_jsondb__add__bool(
	SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszPath,
	SG_bool bAddRecursive,
	SG_bool b)
{
	PRE_ADD;
	SG_ERR_CHECK(  _insertNode(pCtx, pMe, pszPath, bAddRecursive, SG_VARIANT_TYPE_BOOL,
		SG_FALSE, b ? "1" : "0", NULL, NULL)  );
	POST_ADD;
}

void SG_jsondb__add__vhash(
	SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszPath,
	SG_bool bAddRecursive,
	const SG_vhash* pHashValue)
{
	struct _add_foreach_state state;

	PRE_ADD;

	SG_ERR_CHECK(  _insertNode(pCtx, pMe, pszPath, bAddRecursive, SG_VARIANT_TYPE_VHASH,
		SG_FALSE, NULL, NULL, NULL)  );

	if (pHashValue)
	{
		state.pJsonDb = pMe;
 		if (0 == strcmp(pszPath, "/"))
 			state.pszParentPath = "";
 		else
			state.pszParentPath = pszPath;
		SG_ERR_CHECK(  SG_vhash__foreach(pCtx, pHashValue, _vhash_add_foreach_cb, &state)  );
	}

	POST_ADD;
}

void SG_jsondb__add__varray(
	SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszPath,
	SG_bool bAddRecursive,
	const SG_varray* pva)
{
	struct _add_foreach_state state;
	char* pszParentPath = NULL;

	PRE_ADD;

	SG_ERR_CHECK(  _insertNode(pCtx, pMe, pszPath, bAddRecursive, SG_VARIANT_TYPE_VARRAY, SG_FALSE, NULL, NULL, &pszParentPath)  );

	if (pva)
	{
		state.pJsonDb = pMe;
		state.iVarrayIdxOffset = 0;
		if (0 == strcmp(pszParentPath, "/"))
			state.pszParentPath = "";
		else
			state.pszParentPath = pszParentPath;
		SG_ERR_CHECK(  SG_varray__foreach(pCtx, pva, _varray_add_foreach_cb, &state)  );
	}

	POST_ADD;

	SG_NULLFREE(pCtx, pszParentPath);
}

//////////////////////////////////////////////////////////////////////////

static void _updateNode(
	SG_context* pCtx,
	_jsondb_handle* pMe,
	const char* pszPath,
	SG_bool bAddRecursive,
	SG_uint16 type,
	SG_bool bVariantIndexOk,
	const char* pszVal,
	const SG_vhash* pvhValue,
	const SG_varray* pvaValue)
{
	sqlite3_stmt* pStmt = NULL;
	SG_int64 existingNodeId;
	SG_uint32 existingNodeLft, existingNodeRgt;
	SG_int32 rc;
	struct _add_foreach_state state;

	// Look up the provided path.
	SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, pMe->psql, &pStmt,
		"SELECT id, lft, rgt "
		"FROM nodes "
		"WHERE json_object_id = ? AND full_path = ? "
		"LIMIT 1")  );
	SG_ERR_CHECK(  sg_sqlite__bind_int64(pCtx, pStmt, 1, pMe->objectId)  );
	SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx, pStmt, 2, pszPath)  );
	rc = sqlite3_step(pStmt);
	if (rc == SQLITE_ROW)
	{
		// The node exists.

		existingNodeId = sqlite3_column_int64(pStmt, 0);
		existingNodeLft = sqlite3_column_int(pStmt, 1);
		existingNodeRgt = sqlite3_column_int(pStmt, 2);
		SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );

		if ((existingNodeRgt - existingNodeLft) > 1)
			SG_ERR_CHECK(  _removeNode(pCtx, pMe, SG_TRUE, existingNodeId, existingNodeLft, existingNodeRgt)  );

		SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, pMe->psql, &pStmt,
			"UPDATE nodes "
			"SET type = ?, val = ? "
			"WHERE id = ?")  );
		SG_ERR_CHECK(  sg_sqlite__bind_int(pCtx, pStmt, 1, type)  );
		SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx, pStmt, 2, pszVal)  );
		SG_ERR_CHECK(  sg_sqlite__bind_int64(pCtx, pStmt, 3, existingNodeId)  );
		SG_ERR_CHECK(  sg_sqlite__step(pCtx, pStmt, SQLITE_DONE)  );
		SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );
#ifdef DEBUG
		{
			SG_uint32 nrRowsUpdated = 0;
			SG_ERR_CHECK(  sg_sqlite__num_changes(pCtx, pMe->psql, &nrRowsUpdated)  );
			SG_ASSERT(nrRowsUpdated == 1);
		}
#endif
	}
	else if (rc == SQLITE_DONE)
	{
		// The node doesn't already exist.
		SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );
		SG_ERR_CHECK(  _insertNode(pCtx, pMe, pszPath, bAddRecursive, type, bVariantIndexOk, pszVal, NULL, NULL)  );
	}
	else
		SG_ERR_THROW(SG_ERR_SQLITE(rc));

	// The node itself has now been updated or inserted.  If it was a container type, fill it in.
	if ((type == SG_VARIANT_TYPE_VHASH) && pvhValue)
	{
		state.pJsonDb = pMe;
		if (0 == strcmp(pszPath, "/"))
			state.pszParentPath = "";
		else
			state.pszParentPath = pszPath;
		SG_ERR_CHECK(  SG_vhash__foreach(pCtx, pvhValue, _vhash_add_foreach_cb, &state)  );
	}
	else if ((type == SG_VARIANT_TYPE_VARRAY) && pvaValue)
	{
		state.pJsonDb = pMe;
		if (0 == strcmp(pszPath, "/"))
			state.pszParentPath = "";
		else
			state.pszParentPath = pszPath;
		state.iVarrayIdxOffset = 0;
		SG_ERR_CHECK(  SG_varray__foreach(pCtx, pvaValue, _varray_add_foreach_cb, &state)  );
	}

	return;

fail:
	SG_ERR_IGNORE(  sg_sqlite__finalize(pCtx, pStmt)  );
}

void SG_jsondb__update__string__sz(
	SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszPath,
	SG_bool bAddRecursive,
	const char* pszValue)
{
	PRE_ADD;
	SG_ERR_CHECK(  _updateNode(pCtx, pMe, pszPath, bAddRecursive, SG_VARIANT_TYPE_SZ,
		SG_FALSE, pszValue, NULL, NULL)  );
	POST_ADD;
}

void SG_jsondb__update__string__buflen(
	SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszPath,
	SG_bool bAddRecursive,
	const char* pszValue, // We'll make our own copy of this.  The caller still owns it.
	SG_uint32 len)
{
	char* pszToStore = NULL;
	PRE_ADD;

	if (len == 0 || strlen(pszValue) < len)
	{
		SG_ERR_CHECK(  _updateNode(pCtx, pMe, pszPath, bAddRecursive, SG_VARIANT_TYPE_SZ,
			SG_FALSE, pszValue, NULL, NULL)  );
	}
	else
	{
		SG_ERR_CHECK(  SG_allocN(pCtx, len+1, pszToStore)  );
		SG_strcpy(pCtx, pszToStore, len+1, pszValue);
		SG_ERR_CHECK_CURRENT_DISREGARD(SG_ERR_BUFFERTOOSMALL);
		SG_ERR_CHECK(  _updateNode(pCtx, pMe, pszPath, bAddRecursive, SG_VARIANT_TYPE_SZ,
			SG_FALSE, pszToStore, NULL, NULL)  );
	}

	POST_ADD;
	SG_NULLFREE(pCtx, pszToStore);
}

void SG_jsondb__update__int64(
	SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszPath,
	SG_bool bAddRecursive,
	SG_int64 intValue)
{
	SG_int_to_string_buffer tmp;

	PRE_ADD;

	SG_int64_to_sz(intValue, tmp);
	SG_ERR_CHECK(  _updateNode(pCtx, pMe, pszPath, bAddRecursive, SG_VARIANT_TYPE_INT64,
		SG_FALSE, tmp, NULL, NULL)  );

	POST_ADD;
}

void SG_jsondb__update__double(
	SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszPath,
	SG_bool bAddRecursive,
	double fv)
{
	char buf[256];

	PRE_ADD;

	SG_ERR_CHECK(  SG_sprintf(pCtx, buf, 255, "%f", fv)  );
	SG_ERR_CHECK(  _updateNode(pCtx, pMe, pszPath, bAddRecursive, SG_VARIANT_TYPE_DOUBLE,
		SG_FALSE, buf, NULL, NULL)  );

	POST_ADD;
}

void SG_jsondb__update__null(
	SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszPath,
	SG_bool bAddRecursive)
{
	PRE_ADD;
	SG_ERR_CHECK(  _updateNode(pCtx, pMe, pszPath, bAddRecursive, SG_VARIANT_TYPE_NULL,
		SG_FALSE, NULL, NULL, NULL)  );
	POST_ADD;
}

void SG_jsondb__update__bool(
	SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszPath,
	SG_bool bAddRecursive,
	SG_bool b)
{
	PRE_ADD;
	SG_ERR_CHECK(  _updateNode(pCtx, pMe, pszPath, bAddRecursive, SG_VARIANT_TYPE_BOOL,
		SG_FALSE, b ? "1" : "0", NULL, NULL)  );
	POST_ADD;
}

void SG_jsondb__update__vhash(
	SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszPath,
	SG_bool bAddRecursive,
	const SG_vhash* pHashValue)
{
	PRE_ADD;
	SG_ERR_CHECK(  _updateNode(pCtx, pMe, pszPath, bAddRecursive, SG_VARIANT_TYPE_VHASH,
		SG_FALSE, NULL, pHashValue, NULL)  );
	POST_ADD;
}

void SG_jsondb__update__varray(
	SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszPath,
	SG_bool bAddRecursive,
	const SG_varray* pva)
{
	PRE_ADD;
	SG_ERR_CHECK(  _updateNode(pCtx, pMe, pszPath, bAddRecursive, SG_VARIANT_TYPE_VARRAY,
		SG_FALSE, NULL, NULL, pva)  );
	POST_ADD;
}
//////////////////////////////////////////////////////////////////////////

void SG_jsondb__remove(
	SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszPath)
{
	_jsondb_handle* pMe = (_jsondb_handle*)pThis;
	sqlite3_stmt* pStmt = NULL;
	SG_int64 deleteNodeId;
	SG_uint32 deleteNodeLft, deleteNodeRgt;
	SG_bool bInTx = SG_FALSE;

	SG_NULLARGCHECK_RETURN(pThis);
	SG_NULLARGCHECK_RETURN(pszPath);

	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pMe->psql, "BEGIN TRANSACTION;")  );
	bInTx = SG_TRUE;

	// Find the node in question
	SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, pMe->psql, &pStmt,
		"SELECT id, lft, rgt "
		"FROM nodes "
		"WHERE json_object_id = ? AND full_path = ? "
		"LIMIT 1")  );
	SG_ERR_CHECK(  sg_sqlite__bind_int64(pCtx, pStmt, 1, pMe->objectId)  );
	SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx, pStmt, 2, pszPath)  );
	sg_sqlite__step(pCtx, pStmt, SQLITE_ROW);
	SG_ERR_REPLACE(SG_ERR_SQLITE(SQLITE_DONE), SG_ERR_NOT_FOUND);
	SG_ERR_CHECK_CURRENT;
	deleteNodeId = sqlite3_column_int64(pStmt, 0);
	deleteNodeLft = sqlite3_column_int(pStmt, 1);
	deleteNodeRgt = sqlite3_column_int(pStmt, 2);
	SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );

	SG_ERR_CHECK(  _removeNode(pCtx, pMe, SG_FALSE, deleteNodeId, deleteNodeLft, deleteNodeRgt)  );

	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pMe->psql, "COMMIT TRANSACTION;")  );
	bInTx = SG_FALSE;

	return;

fail:
	SG_ERR_IGNORE(  sg_sqlite__finalize(pCtx, pStmt)  );
	if (bInTx && pMe && pMe->psql)
		SG_ERR_IGNORE(  sg_sqlite__exec(pCtx, pMe->psql, "ROLLBACK TRANSACTION;")  );
}

void SG_jsondb__escape_keyname(
	SG_context* pCtx,
	const char* pszKeyname,
	char** ppszEscapedKeyname)
{
	const char* p = pszKeyname;
	SG_uint32 padCount = 0;
	char* pszEscapedKeyname = NULL;

	SG_NONEMPTYCHECK_RETURN(pszKeyname);
	SG_NULLARGCHECK_RETURN(ppszEscapedKeyname);

	while (*p)
	{
		unsigned char c = (unsigned char)*p;
		if (c == '/' || c == '\\')
			padCount++;

		p++;
	}

	if (padCount)
	{
		char* ep;
		SG_ERR_CHECK(  SG_allocN(pCtx, (p - pszKeyname + padCount + 1), pszEscapedKeyname)  );
		ep = pszEscapedKeyname;
		p = pszKeyname;
		while (*p)
		{
			unsigned char c = (unsigned char)*p;

			if (c == '/' || c == '\\')
			{
				*ep = '\\';
				ep++;
			}

			*ep = c;

			ep++;
			p++;
		}
	}
	else
	{
		SG_ERR_CHECK(  SG_strdup(pCtx, pszKeyname, &pszEscapedKeyname)  );
	}

	*ppszEscapedKeyname = pszEscapedKeyname;

	return;

fail:
	SG_NULLFREE(pCtx, pszEscapedKeyname);
}


void SG_jsondb__unescape_keyname(
	SG_context* pCtx,
	const char* pszKeyname,
	char** ppszUnescapedKeyname)
{
	const char* p = pszKeyname;
	SG_uint32 padCount = 0;
	char* pszUnescapedKeyname = NULL;

	SG_NONEMPTYCHECK_RETURN(pszKeyname);

	while (*p)
	{
		unsigned char c = (unsigned char)*p;
		if (c == '\\')
		{
			p++;
			padCount++;
		}

		p++;
	}

	if (padCount)
	{
		char* up;
		SG_ERR_CHECK(  SG_allocN(pCtx, (p - pszKeyname - padCount + 1), pszUnescapedKeyname)  );
		up = pszUnescapedKeyname;
		p = pszKeyname;
		while (*p)
		{
			unsigned char c = *p;

			if (c == '\\')
				c = *++p;

			*up = c;

			up++;
			p++;
		}
	}
	else
	{
		SG_ERR_CHECK(  SG_strdup(pCtx, pszKeyname, &pszUnescapedKeyname)  );
	}

	*ppszUnescapedKeyname = pszUnescapedKeyname;

	return;

fail:
	SG_NULLFREE(pCtx, pszUnescapedKeyname);
}


#if defined(DEBUG)
void SG_jsondb_debug__verify_tree(
	SG_context* pCtx,
	SG_jsondb* pThis)
{
	_jsondb_handle* pMe = (_jsondb_handle*)pThis;
	sqlite3_stmt* pStmt = NULL;
	SG_int32 rc;
	SG_uint32 nodeCount = 0;
	SG_uint32 rootRgt = 0;
	char* pszParsedLeafName = NULL;
	char* pszParsedParentPath = NULL;
	SG_uint32 last_rgt = 1;

	subTreeStack stack;
	stack.ndx = 0;

	SG_NULLARGCHECK_RETURN(pThis);

	SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, pMe->psql, &pStmt,
		"SELECT full_path, leaf_name, type, lft, rgt FROM nodes "
		"WHERE json_object_id = ? "
		"ORDER BY lft;")  );
	SG_ERR_CHECK(  sg_sqlite__bind_int64(pCtx, pStmt, 1, pMe->objectId)  );

	while ((rc=sqlite3_step(pStmt)) == SQLITE_ROW)
	{
		const char* pszFullPath = (const char*)sqlite3_column_text(pStmt, 0);
		SG_uint32 lenPath = strlen(pszFullPath);
		SG_uint32 lft = sqlite3_column_int(pStmt, 3);
		SG_uint32 rgt = sqlite3_column_int(pStmt, 4);

		//SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDERR, "[jsondb verify tree] Verifying %s\n", pszFullPath)  );

		nodeCount++;

		for (; last_rgt+1 < lft; last_rgt++)
		{
			void* p = NULL;
			SG_bool dontCare;

			SG_ERR_CHECK(  _pop(pCtx, &stack, &dontCare, &p)  );
			SG_NULLFREE(pCtx, p);
		}

		if (lenPath == 1)
		{
			if (pszFullPath[0] != '/')
				SG_ERR_THROW(SG_ERR_JSONDB_INVALID_PATH);
			if (strcmp("**ROOT**", (const char*)sqlite3_column_text(pStmt, 1)) != 0)
				SG_ERR_THROW(SG_ERR_JSONDB_INVALID_PATH);
			if (stack.ndx != 0)
				SG_ERR_THROW(SG_ERR_JSONDB_INVALID_PATH);
			if (lft != 1)
				SG_ERR_THROW(SG_ERR_JSONDB_INVALID_PATH);

			rootRgt = rgt;
		}
		else
		{
			char* pszParentPath;

			SG_ERR_CHECK(  _pathHelper(pCtx, pszFullPath, lenPath, NULL, &pszParsedParentPath, &pszParsedLeafName)  );

			if (stack.ndx == 0)
				SG_ERR_THROW(SG_ERR_JSONDB_INVALID_PATH);
			if (strcmp(pszParsedLeafName, (const char*)sqlite3_column_text(pStmt, 1)) != 0)
				SG_ERR_THROW(SG_ERR_JSONDB_INVALID_PATH);

			pszParentPath = (char*)stack.stack[stack.ndx];

			if (strcmp(pszParsedParentPath, pszParentPath) != 0)
				SG_ERR_THROW(SG_ERR_JSONDB_INVALID_PATH);
		}

		if (rgt - lft > 1)
		{
			char* pszPath;
			SG_ERR_CHECK(  SG_strdup(pCtx, pszFullPath, &pszPath)  );
			SG_ERR_CHECK(  _push(pCtx, &stack, SG_FALSE, pszPath)  );
		}

		last_rgt = rgt;

		SG_NULLFREE(pCtx, pszParsedParentPath);
		SG_NULLFREE(pCtx, pszParsedLeafName);

	}
	if (rc != SQLITE_DONE)
		SG_ERR_THROW(  SG_ERR_SQLITE(rc)  );

	if (!rootRgt)
		SG_ERR_THROW2(SG_ERR_UNSPECIFIED, (pCtx, "No root node."));
	if ((rootRgt / 2) != nodeCount)
		SG_ERR_THROW2(SG_ERR_UNSPECIFIED, (pCtx, "Root lft/rgt and node count disagree."));


	SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );

	/* fall through */
fail:
	while (stack.ndx)
	{
		void* p = NULL;
		SG_bool dontCare;

		SG_ERR_CHECK(  _pop(pCtx, &stack, &dontCare, &p)  );
		SG_NULLFREE(pCtx, p);
	}
	SG_ERR_IGNORE(  sg_sqlite__finalize(pCtx, pStmt)  );
	SG_NULLFREE(pCtx, pszParsedParentPath);
	SG_NULLFREE(pCtx, pszParsedLeafName);
}
#else
void SG_jsondb_debug__verify_tree(
	SG_UNUSED_PARAM(SG_context* pCtx),
	SG_UNUSED_PARAM(SG_jsondb* pThis))
{
	SG_UNUSED(pCtx);
	SG_UNUSED(pThis);
}
#endif
