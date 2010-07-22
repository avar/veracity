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
 * @file sg_portability.c
 *
 * We use the PORTABILITY code to detect POTENTIAL and ACTUAL problems with ENTRYNAMES and PATHNAMES.
 *
 * Detect POTENTIAL Problems:
 * ==========================
 *
 * Because we want to be able to fetch and populate a WORKING DIRECTORY on all platforms regardless
 * of where the original version of the tree was created, we need to be aware of various POTENTIAL
 * problems.  For example, a Linux user can create "README" and "ReadMe" in a directory and ADD and
 * COMMIT them; no problem.  But when a Windows or Mac user tries to CLONE/FETCH a view of the tree,
 * we won't be able to properly populate the WORKING DIRECTORY (because those platforms are not case-
 * sensitive).  In this example, it would be nice if we were able to warn the Linux user that they
 * are about to cause problems for users on other platforms and give a chance to do something different.
 * [We still allow them to do it, but we give them a chance to be nice.]
 *
 * The code in this file tries to PREDICT/GUESS these types of POTENTIAL problems without actually
 * touching the filesystem.  In a sense, we combine all of the various restictions on all of the
 * various platforms that we support and inspect the ENTRYNAMES/PATHNAMES that we are given.  This
 * tends to give warnings that are a superset of what any individual user would actually see in
 * practice when creating files and directories.  [This makes it a little difficult to determine
 * when we issue a true false-positive and something that is just not applicable to this particular
 * operating system/filesystem.]
 *
 * There are 2 types of issues:
 *
 * [1] INDIVIDUAL warnings.  These have to do with an individual ENTRYNAME or PATHNAME in isolation.
 *     For example, the ENTRYNAME "abc:def" is not valid on Windows.
 *
 * [2] COLLISION warnings.  These have to do with multiple, distinct sequences of characters being
 *     treated the same by the filesystem.
 *     For example, the ENTRYNAMES "README" and "ReadMe" on Windows and Mac systems.
 *
 * When our computation generates a warning, we record with it the reason for the warning:
 *
 * In [1] this is fairly straight-forward.  For example, an ENTRYNAME that contains a non-printable
 *        control character, such as 0x01 control-A, which isn't valid on a Windows filesystem.
 *
 * In [2] this might appear somewhat contrived and confusing.  For example, consider the ENTRYNAMES
 *        "readme", "README" and "ReadMe.".  Note the final dot on the latter.
 *        We would report that:
 *        () "readme"   (the "ORIGINAL", as given name) might collide with
 *        () "README"   when a filesystem ignores/strips "CASE" and with
 *        () "ReadMe."  when a filesystem ignores/strips "CASE" and "FINAL-DOTS".
 *
 * Most uses of the PORTABILITY code have us scan the contents of directories on the local system
 * that were independently created by the user and/or command line arguments provided by the user.
 * These names will be added to the repository in some way.  [A one-way flow of information into
 * the repository starting with something that is perfectly legal on the current system.  And where
 * problems won't be noticed until they are retrieved on another type of system.]
 *
 * We also recognize that we shouldn't badger the user about things too often.  So in the above
 * example, the user who tries to ADD "ReadMe" to the tree when "README" is already present should
 * get the warning -- right then at the time of the ADD.  Subsequent COMMITS or changes in the
 * directory should not complain about it again.  So we allow each ENTRYNAME to be marked as
 * "CHECK" or "NO-CHECK".  CHECK means that the caller wants to know about any problems with the
 * name.  NO-CHECK means that they don't *BUT* they still must provide it so that we can search
 * for POTENTIAL COLLISIONS with other (CHECKED) names.
 *
 * Since each installation may have a unique mix of system types, we allow users to turn off
 * specific warnings for things that they won't ever see.  For example, if a site will never have
 * Windows machines on the project, they can turn off the warnings about MS-DOS devices, such as COM1.
 * These are given in a MASK.
 *
 *
#if 0 // TODO
 * Detect ACTUAL Problems:
 * =======================
 *
 * Another use of the PORTABILITY is to detect ACTUAL problems with a given set of ENTRYNAMES.
 *
 * There are times when we need to restore entries into a directory from the repository, such
 * as on a "REVERT" command where we restore files that were DELETED from the WORKING DIRECTORY
 * but not COMMITTED.  Because of other changes in the directory, such as ADDED/RENAMED files
 * that are not part of the REVERT), we may not be able to complete the operation.  We would
 * like to be able to PREDICT these problems BEFORE we start modifying the WORKING DIRECTORY
 * (rather than failing half-way through the REVERT and leaving a mess for the user to clean up).
 * [This doesn't guaranteed that we won't have problems, but if we can avoid the cases where we
 * know there is a problem, the user is better off.]
 *
 * Note that these problems can only be tested using the actual filesystem supporting the
 * user's WORKING DIRECTORY.  For example, "README" and "ReadMe" don't collide on Linux systems,
 * but if the Linux user's WORKING DIRECTORY resides on a mounted network share from a Windows
 * system, then they probably do.
#endif
 *
 */

//////////////////////////////////////////////////////////////////

/*
 * TODO We need to store the user's PORTABILITY MASK settings in the LOCAL SETTINGS.
 *
 *
 * TODO See SPRAWL-54:
 * TODO
 * TODO The 8/13/2009 version of http://msdn.microsoft.com/en-us/library/aa365247%28VS.85,loband%29.aspx
 * TODO includes new information that I didn't see before.  This looks like an update for Vista and Win 7.
 * TODO
 * TODO There is discussion of \\.\ prefix to let you get into a different namespace (as in \\.\PhysicalDisk0)
 * TODO
 * TODO There is also discussion of \\?\GLOBALROOT in later versions that might need looking at.
 * TODO
 * TODO There is a comment (in the Q/A at the bottom) about leading spaces in file/directory names causing
 * TODO problems for (Vista/Win7) Windows Explorer (or rather it not respecting them when moving files).
 * TODO
 * TODO
 * TODO In http://msdn.microsoft.com/en-us/library/aa363866%28VS.85%29.aspx, it talks about how Win 7 and
 * TODO Server 2008 now have symlinks.  Don't know how they compare to Mac/Linux.
 * TODO
 * TODO
 * TODO Do we care about hard-links?
 * TODO
 * TODO In http://msdn.microsoft.com/en-us/library/aa363860%28VS.85%29.aspx, it talks about how Win 2000
 * TODO has hard-links.  So the problem is not limited to Linux/Mac.
 */

//////////////////////////////////////////////////////////////////

#include <sg.h>

//////////////////////////////////////////////////////////////////

struct _sg_portability_item
{
	SG_portability_flags	portWarnings;				// tests that hit

	char *					szRelativePath;				// repo-relative-pathname as given (should be UTF-8)
	SG_utf8_converter *		pConverterRepoCharSet;		// a CharSet<-->UTF-8 converter for the charset configured with the repo. (we do not own this).

	char *					szUtf8EntryName;			// UTF-8 version of as-given entryname
	SG_int32 *				pUtf32EntryName;			// UTF-32 version of entryname

	char *					szCharSetEntryName;			// charset-based entry (when needed)
	char *					szUtf8EntryNameAppleNFD;	// UTF-8 in apple-nfd (when needed)
	char *					szUtf8EntryNameAppleNFC;	// UTF-8 in apple-nfc (when needed)

	SG_uint32				lenEntryNameChars;			// length of entryname in code-points/characters (independent of encoding)
	SG_uint32				lenEntryNameUtf8Bytes;		// strlen(szUtf8EntryName) in bytes
	SG_uint32				lenEntryNameCharSetBytes;	// strlen(szCharSetEntryName) in bytes when needed)
	SG_uint32				lenEntryNameAppleNFDBytes;	// strlen(szUtf8EntryNameAppleNFD) in bytes when needed
	SG_uint32				lenEntryNameAppleNFCBytes;	// strlen(szUtf8EntryNameAppleNFC) in bytes when needed

	SG_bool					bCheckMyName_Indiv;			// do we want reporting on _INDIV issues?
	SG_bool					bCheckMyName_Collision;		// do we want reporting on _COLLISION issues?

	SG_bool					b7BitClean;					// true if US-ASCII [0x00..0x7f] only
	SG_bool					bHasDecomposition;			// true if NFC != NFD (when needed)
	SG_bool					bIsProperlyComposed;		// true if given == NFC (when needed)
	SG_bool					bIsProperlyDecomposed;		// true if given == NFD (when needed)

	void *					pVoidAssocData;				// optional user-defined data to be associated with this item.  (we do not own this.)
	SG_varray *				pvaLog;				    	// optional, cummulative log of problems for this item; we don't own this

	char					bufGid_Entry[SG_GID_BUFFER_LENGTH];		// optional, GID of entry for logging
};

struct _sg_portability_dir
{
	SG_portability_flags	portLogMask;			// bits we are interested in for logging
	SG_portability_flags	portWarningsAll;		// tests that hit - union over all items
	SG_portability_flags	portWarningsChecked;	// tests that hit - union over 'checked' items.

	char *					szRelativePath;			// utf-8 repo-relative-pathname
	SG_utf8_converter *		pConverterRepoCharSet;	// a CharSet<-->UTF-8 converter for the charset configured with the repo. (we do not own this).

	SG_rbtree *				prbCollider;			// rbtree<sz,collider_data> assoc-array of normalized entry names
	SG_rbtree *				prbItems;				// rbtree<sz,item> assoc-array of items

	SG_varray *				pvaLog;				    // optional, cummulative log of problems for everything in directory; we don't own this
};

struct _my_collider_assoc_data
{
	SG_portability_flags	portFlagsWhy;			// __PAIRWISE__ bit for why we added this entry to pDir->prbCollider
	SG_portability_item *	pItem;					// link to item data; we do not own this.
};

//////////////////////////////////////////////////////////////////

void _sg_portability_item__free(SG_context* pCtx, void * pVoidItem)
{
	SG_portability_item * pItem = (SG_portability_item *)pVoidItem;

	if (!pItem)
		return;

	SG_NULLFREE(pCtx, pItem->szRelativePath);
	// we do not own pItem->pConverterRepoCharSet

	SG_NULLFREE(pCtx, pItem->szUtf8EntryName);
	SG_NULLFREE(pCtx, pItem->pUtf32EntryName);

	SG_NULLFREE(pCtx, pItem->szCharSetEntryName);
	SG_NULLFREE(pCtx, pItem->szUtf8EntryNameAppleNFD);
	SG_NULLFREE(pCtx, pItem->szUtf8EntryNameAppleNFC);

	SG_NULLFREE(pCtx, pItem);
}

/**
 * Allocate a SG_portability_item structure to provide workspace
 * and results for evaluating the portability of an individual
 * file/directory name.
 *
 * szEntryName is the original entryname that we are given.
 * It *MUST* be in UTF-8.
 *
 * pVoidAssocData  is optional and for our callers use only;
 * we do not look at it or do anything with it.
 */
void _sg_portability_item__alloc(SG_context * pCtx,
								 const char * pszGid_Entry,			// optional, for logging.
								 const char * szRelativePath,
								 const char * szEntryNameAsGiven,
								 SG_utf8_converter * pConverterRepoCharSet,
								 SG_bool bCheckMyName_Indiv, SG_bool bCheckMyName_Collision,
								 void * pVoidAssocData, SG_varray * pvaLog,
								 SG_portability_item ** ppItem)
{
	SG_portability_item * pItem = NULL;
	SG_uint32 lenPath;
	SG_uint32 lenEntryNameAsGivenBytes;

	SG_NULLARGCHECK_RETURN(ppItem);
	SG_NONEMPTYCHECK_RETURN(szEntryNameAsGiven);

	SG_ERR_CHECK_RETURN(  SG_alloc1(pCtx, pItem)  );

	pItem->bCheckMyName_Indiv = bCheckMyName_Indiv;
	pItem->bCheckMyName_Collision = bCheckMyName_Collision;
	pItem->portWarnings = SG_PORT_FLAGS__NONE;
	pItem->pConverterRepoCharSet = pConverterRepoCharSet;
	pItem->pVoidAssocData = pVoidAssocData;
	pItem->pvaLog = pvaLog;

	if (pszGid_Entry && *pszGid_Entry)
		SG_ERR_CHECK(  SG_strcpy(pCtx, pItem->bufGid_Entry, SG_NrElements(pItem->bufGid_Entry), pszGid_Entry)  );

	lenEntryNameAsGivenBytes = (SG_uint32) strlen(szEntryNameAsGiven);

	// allocate/compose <relative-path-directory>/<entryname>
	// we do this with the original encoding as given.

	lenPath = (SG_uint32)(((szRelativePath) ? strlen(szRelativePath) : 0) + 1 + lenEntryNameAsGivenBytes + 1);
	SG_ERR_CHECK(  SG_allocN(pCtx, lenPath,pItem->szRelativePath)  );
	SG_ERR_CHECK(  SG_sprintf(pCtx,
							  pItem->szRelativePath,
							  lenPath,
							  "%s/%s",
							  ((szRelativePath)?szRelativePath:""),
							  szEntryNameAsGiven)  );

	// assume we were given a UTF-8 entryname and try to create a UTF-32 copy.
	// this will fail with SG_ERR_UTF8INVALID otherwise.

	SG_ERR_CHECK(  SG_utf8__to_utf32__sz(pCtx, szEntryNameAsGiven,&pItem->pUtf32EntryName,&pItem->lenEntryNameChars)  );

	// we were given a valid UTF-8 entryname.  make a copy of it.

	SG_ERR_CHECK(  SG_STRDUP(pCtx, szEntryNameAsGiven,&pItem->szUtf8EntryName)  );
	pItem->lenEntryNameUtf8Bytes = (SG_uint32) strlen(pItem->szUtf8EntryName);

	// we have US-ASCII-only string if there were no multi-byte UTF-8 characters.

	pItem->b7BitClean = (pItem->lenEntryNameUtf8Bytes == pItem->lenEntryNameChars);

	if (!pItem->b7BitClean)
	{
		// we now have UTF-8 data with at least one character >0x7f.
		//
		// The MAC stores filenames in canonical NFD (and rearranges the
		// accent characters as necessary).
		//
		// compute apple-style NFD and NFC from the UTF-8 input.  we seem to get different
		// results if we do:
		//    nfd := NFD(orig)
		//    nfc := NFC(orig)
		// and
		//    nfd := NFD(orig)
		//    nfc := NFC(nfd)
		//
		// we choose the latter because that's probably how the MAC filesystem works.
		//
		// we ***DO NOT*** change case nor strip ignorables nor convert any SFM characters.

		SG_bool bIsCharSetUtf8;

		SG_ERR_CHECK(  SG_apple_unicode__compute_nfd__utf8(pCtx,
														   pItem->szUtf8EntryName,          pItem->lenEntryNameUtf8Bytes,
														   &pItem->szUtf8EntryNameAppleNFD, &pItem->lenEntryNameAppleNFDBytes)  );

		SG_ERR_CHECK(  SG_apple_unicode__compute_nfc__utf8(pCtx,
														   pItem->szUtf8EntryNameAppleNFD,  pItem->lenEntryNameAppleNFDBytes,
														   &pItem->szUtf8EntryNameAppleNFC, &pItem->lenEntryNameAppleNFCBytes)  );

		pItem->bHasDecomposition     = (strcmp(pItem->szUtf8EntryNameAppleNFC,pItem->szUtf8EntryNameAppleNFD) != 0);
		pItem->bIsProperlyComposed   = (strcmp(pItem->szUtf8EntryName,        pItem->szUtf8EntryNameAppleNFC) == 0);
		pItem->bIsProperlyDecomposed = (strcmp(pItem->szUtf8EntryName,        pItem->szUtf8EntryNameAppleNFD) == 0);

		// if the original input contained partially decomposed characters or
		// if the decomposed accent characters were in a random order, we flag it.

		if (!pItem->bIsProperlyComposed && !pItem->bIsProperlyDecomposed)
			pItem->portWarnings |= SG_PORT_FLAGS__INDIV__NON_CANONICAL_NFD;

		SG_ERR_CHECK(  SG_utf8_converter__is_charset_utf8(pCtx, pItem->pConverterRepoCharSet,&bIsCharSetUtf8)  );
		if (!bIsCharSetUtf8)
		{
			// The repo has defined a non-UTF-8 CharSet in the configuration.  This means
			// that users are allowed to use the repo when they have that their LOCALE CHARSET
			// set to that (and only that) value.   This means that we might have to convert
			// entrynames from UTF-8 (as we store them internally) to that charset when we
			// are working on a Linux system, for example.
			//
			// Note: They are always free to set their LOCALE to UTF-8.)
			//
			// So here, we check for entrynames that cannot be converted to that charset.
			// As they would cause problems for Linux users later.  Later we will run the
			// charset-based names thru the collider.
			//
			//
			// try to convert the original name to a CHARSET-based string without using
			// substitution/slug characters.  this should fail, rather than substitute,
			// if there are characters that are not in this character set.  we want to
			// know how this entryname will look in the filesystem on a Linux system.
			// if it cannot be cleanly converted, Linux users will have problems with
			// this entry.  We ***ASSUME*** that all Linux-based users of this repo are
			// using the same CHARSET.
			//
			// TODO I'm assuming here that when populating a working directory that we
			// TODO also fail rather than substitute.

			SG_utf8_converter__to_charset__sz(pCtx,
											  pItem->pConverterRepoCharSet,
											  szEntryNameAsGiven,
											  &pItem->szCharSetEntryName);
			if (SG_context__has_err(pCtx))
			{
				pItem->portWarnings |= SG_PORT_FLAGS__INDIV__CHARSET;
				SG_context__err_reset(pCtx);
			}
			else
			{
				pItem->lenEntryNameCharSetBytes = (SG_uint32)strlen(pItem->szCharSetEntryName);
			}
		}
	}

	SG_ASSERT(  (pItem->szUtf8EntryName && pItem->pUtf32EntryName)  &&  "Coding Error."  );
	SG_ASSERT(  (pItem->lenEntryNameUtf8Bytes >= pItem->lenEntryNameChars)  &&  "Coding Error."  );
	SG_ASSERT(  (pItem->lenEntryNameChars > 0)  &&  "Zero length entryname??"  );

	*ppItem = pItem;
	return;

fail:
	SG_ERR_IGNORE(  _sg_portability_item__free(pCtx, pItem)  );
}

//////////////////////////////////////////////////////////////////

void _my_collider_assoc_data__free(SG_context * pCtx, void * pVoidData)
{
	struct _my_collider_assoc_data * pData = (struct _my_collider_assoc_data *)pVoidData;

	if (!pData)
		return;

	// we *DO NOT* own the pData->pItem.

	SG_free(pCtx, pData);
}

void _my_collider_assoc_data__alloc(SG_context * pCtx,
									SG_portability_flags why, SG_portability_item * pItem,
									struct _my_collider_assoc_data ** ppData)
{
	struct _my_collider_assoc_data * pData = NULL;

	SG_ERR_CHECK_RETURN(  SG_alloc1(pCtx, pData)  );

	pData->portFlagsWhy = why;
	pData->pItem = pItem;

	*ppData = pData;
}

//////////////////////////////////////////////////////////////////

void SG_portability_dir__free(SG_context * pCtx, SG_portability_dir * pDir)
{
	if (!pDir)
		return;

	SG_RBTREE_NULLFREE_WITH_ASSOC(pCtx, pDir->prbCollider,_my_collider_assoc_data__free);
	SG_RBTREE_NULLFREE_WITH_ASSOC(pCtx, pDir->prbItems,_sg_portability_item__free);
	SG_NULLFREE(pCtx, pDir->szRelativePath);
	// we do not own pDir->pConverterRepoCharSet
	// we do not own pDir->pvaLog

	SG_free(pCtx, pDir);
}

void SG_portability_dir__alloc(SG_context * pCtx,
							   SG_portability_flags portLogMask,
							   const char * szRelativePath,
							   SG_utf8_converter * pConverterRepoCharSet,
							   SG_varray * pvaLog,
							   SG_portability_dir ** ppDir)
{
	SG_portability_dir * pDir = NULL;

	SG_NULLARGCHECK(ppDir);

	SG_ERR_CHECK_RETURN(  SG_alloc1(pCtx, pDir)  );

	pDir->portLogMask = portLogMask;
	pDir->pvaLog = pvaLog;

	SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &pDir->prbCollider)  );
	SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &pDir->prbItems)  );

	if (szRelativePath)
		SG_ERR_CHECK(  SG_STRDUP(pCtx, szRelativePath,&pDir->szRelativePath)  );

	pDir->pConverterRepoCharSet = pConverterRepoCharSet;	// we don't own this

	*ppDir = pDir;
	return;

fail:
	SG_PORTABILITY_DIR_NULLFREE(pCtx, pDir);
}

void _sg_portability_dir__add_item_to_list(SG_context * pCtx,
										   SG_portability_dir * pDir, SG_portability_item * pItem)
{
	// add this item to assoc-array of items.
	// this assoc-array is keyed off of the original entryname that we were given.
	//
	// we return SG_ERR_RBTREE_DUPLICATEKEY if we already have data for this entryname.
	// (in the case of a trivial collision).

	SG_NULLARGCHECK_RETURN(pDir);
	SG_NULLARGCHECK_RETURN(pItem);

	SG_ERR_CHECK_RETURN(  SG_rbtree__add__with_assoc(pCtx, pDir->prbItems,pItem->szUtf8EntryName,(void *)pItem)  );
}

//////////////////////////////////////////////////////////////////

static void _indiv__test_invalid_or_restricted_00_7f(SG_UNUSED_PARAM(SG_context * pCtx),
													 SG_portability_item * pItem)
{
	// see if entryname has an invalid or restricted character in the
	// range [0x00 .. 0x7f].
	//
	// We know about the following INVALID characters:
	//
	// [] On Windows, the following are invalid:
	//    {} the control characters 0x00 .. 0x1f
	//    {} the characters  < > : " / \ | ? *
	//
	// [] Everywhere / is invalid.
	//
	// http://msdn.microsoft.com/en-us/library/aa365247.aspx
	//
	// We also define a set of RESTRICTED characters in the [0x00 .. 0x7f]
	// range that are known to cause problems in some combinations of OS
	// and filesystems.
	//
	// [] On Mac OS 10.5 to thumbdrive (with vfat), 0x7f DEL is invalid.
	// [] On some combinations of a Linux VM mounting an XP NTFS using
	//    VMware's vmhgfs, '%' get url-encoded giving "%25".  That is,
	//    "foo%bar.txt" gets converted to "foo%25bar.txt".
	//
	// We set INVALID if any of the CI characters are found.
	// We set RESTRICTED if any of the CR characters are found.

	const char * sz;
	SG_bool bHaveCI = SG_FALSE;
	SG_bool bHaveCR = SG_FALSE;
	SG_byte b;
	SG_byte key;

	// The following characters are either invalid or otherwise problematic
	// on at least one OS or filesystem.

#define CR			0x01	// in __INDIV__RESTRICTED_00_7F
#define CI			0x02	// in __INDIV__INVALID_00_7F

	static SG_byte sa_00_7F[0x80] = { CI, CI, CI, CI,   CI, CI, CI, CI, 	// 0x00 .. 0x07 control chars
									  CI, CI, CI, CI,   CI, CI, CI, CI, 	// 0x08 .. 0x0f control chars
									  CI, CI, CI, CI,   CI, CI, CI, CI, 	// 0x10 .. 0x17 control chars
									  CI, CI, CI, CI,   CI, CI, CI, CI, 	// 0x18 .. 0x1f control chars
									  0,  0,  CI, 0,    0,  CR, 0,  0,		// 0x20 .. 0x27 " %
									  0,  0,  CI, 0,    0,  0,  0,  CI,		// 0x28 .. 0x2f * /
									  0,  0,  0,  0,    0,  0,  0,  0,		// 0x30 .. 0x37
									  0,  0,  CI, 0,    CI, 0,  CI, CI,		// 0x38 .. 0x3f : < > ?
									  0,  0,  0,  0,    0,  0,  0,  0,		// 0x40 .. 0x47
									  0,  0,  0,  0,    0,  0,  0,  0,		// 0x48 .. 0x4f
									  0,  0,  0,  0,    0,  0,  0,  0,		// 0x50 .. 0x57
									  0,  0,  0,  0,    CI, 0,  0,  0,		// 0x58 .. 0x5f backslash
									  0,  0,  0,  0,    0,  0,  0,  0,		// 0x60 .. 0x67
									  0,  0,  0,  0,    0,  0,  0,  0,		// 0x68 .. 0x6f
									  0,  0,  0,  0,    0,  0,  0,  0,		// 0x70 .. 0x77
									  0,  0,  0,  0,    CI, 0,  0,  CR,		// 0x78 .. 0x7f | DEL
	};

	SG_UNUSED(pCtx);

	for (sz=pItem->szUtf8EntryName; (*sz); sz++)
	{
		b = (SG_byte)*sz;

		if (b >= 0x80)
			continue;

		key = sa_00_7F[b];
		if (key == 0)		// a normal, valid character
			continue;

		if (key & CI)
		{
			bHaveCI = SG_TRUE;
			if (bHaveCR)
				break;		// found both, so we can stop looking
		}

		if (key & CR)
		{
			bHaveCR = SG_TRUE;
			if (bHaveCI)
				break;		// found both, so we can stop looking
		}
	}

	if (bHaveCR)
		pItem->portWarnings |= SG_PORT_FLAGS__INDIV__RESTRICTED_00_7F;

	if (bHaveCI)
		pItem->portWarnings |= SG_PORT_FLAGS__INDIV__INVALID_00_7F;

#undef CR
#undef CI
}

static void _indiv__test_name_length(SG_context * pCtx,
									 SG_portability_item * pItem)
{
	// see if entryname or repo-relative-pathname exceed length limits on any OS/FS.
	//
	// For entrynames, there are several OS- and filesystem-specific issues here:
	//
	// [] The total number of bytes that can be used for an entryname.
	// [] The on-disk encoding of the entryname (8-bit, 16-bit, and/or UTF-x); that is,
    //    is there a "character" based limit or a "code-point" based limit.
	// [] Whether entryname length limit is independent of pathname limit.
	//
	// Since we can't know all combinations of limits and since we need to make
	// this the minimum value, I'm going to arbitrarily make this a 255 byte limit
	// because Linux has 255 BYTE entryname limits.  Other platforms may be more
	// or in code-points/characters.
	//
	//
	// For pathnames, there are several issues here:
	//
	// [] On Windows (without the \\?\ prefix) pathnames are limited to 260 CHARACTERS.
	// [] On Windows (with the \\?\ prefix) that limit goes to either 32k or 64k (i've
	//    seen both in the docs (TODO discover if this is 32k characters/code-points
	//    which consume 64k bytes)).
	// [] Some platforms have no overall limit.
	//
	// Windows has the 260 for paths, but we always use the \\?\ prefix, so
	// we won't hit it, but the user might not be able to access a deeply nested
	// file with their editor.
	//
	// If there is an overall OS- or filesystem-specific pathname length limit, it
	// will be hit in terms of the working folder.  And we cannot tell where they
	// might put that.  So we're limited to complaining if the repo-relative path
	// hits the a limit.
	//
	// Since we're looking for a minimum, I'm going with the 256 because on Windows,
	// the 260 maps into: 256 for entryname + 3 for "C:\" + 1 for terminating NUL.
	// So with a relative path, 256 would be the longest allowable in 260.

	SG_uint32 lenPathNameChars;

	// we count BYTES not CODE-POINTS because Linux has byte-based limits.
	// Linux has the smallest limit for the entryname (255 bytes).
	// Mac and Winodws allow 255 or more characters because they use some
	// variation of UTF-16 internally.
	//
	// if a Linux user is allowed to use a non-UTF-8 LOCALE, we check the
	// length of the charset-based version.
	//
	// we always check the UTF-8-based length because Linux users are allowed
	// to have the LOCALE set to either.

	if (   ((pItem->szCharSetEntryName) && (pItem->lenEntryNameCharSetBytes > 255))
		|| (pItem->lenEntryNameUtf8Bytes > 255))
		pItem->portWarnings |= SG_PORT_FLAGS__INDIV__ENTRYNAME_LENGTH;

	// we count CODE-POINTS in the relative pathname.
	// Windows has the 256 character limit (when not using the "\\?\" prefix).
	// Linux and Mac seem to allow an unlimited overall pathname.

	SG_ERR_CHECK(  SG_utf8__length_in_characters__sz(pCtx, pItem->szRelativePath,&lenPathNameChars)  );
	if (lenPathNameChars > 256)
		pItem->portWarnings |= SG_PORT_FLAGS__INDIV__PATHNAME_LENGTH;

	return;

fail:
	return;
}

static void _indiv__test_final_dot_space(SG_UNUSED_PARAM(SG_context * pCtx),
										 SG_portability_item * pItem)
{
	// see if entryname has a final SPACE and/or PERIOD.
	//
	// The Windows (Win32 layer) subsystem does not allow filenames that end
	// with a final space or dot.  It silently strips them out.  This is a
	// backwards-compatibility issue with MSDOS.
	//
	// The manuals say *A* final space or dot is silently trimmed, but testing
	// shows that *ANY NUMBER* of final spaces, final dots, or any combination of
	// the two are trimmed.
	//
	// QUESTION: can I create a file named "......" on Windows?  or will this
	// always collide with ".." because of the trimming?
	//
	// This is not a restriction imposed by NTFS, but rather by the Windows subsystem
	// on top of NTFS.  You can create them from a DOS-box if you use a \\?\ style
	// pathname.  And you can remotely create such a file from Linux onto a SAMBA
	// mounted partition from Windows XP; Windows can see the files, but cannot
	// access/delete them (unless you use a \\?\ style path).
	//
	// Final spaces and dots are not a problem for other platforms.

	char ch;

	SG_UNUSED(pCtx);

	if (pItem->szUtf8EntryName[0] == '.')
	{
		SG_bool bDot = (pItem->lenEntryNameUtf8Bytes == 1);
		SG_bool bDotDot = ((pItem->lenEntryNameUtf8Bytes == 2) && (pItem->szUtf8EntryName[1] == '.'));

		SG_ASSERT( !bDot && !bDotDot && "Should not get Dot or DotDot??" );
		if (bDot || bDotDot)
			return;
	}

	ch = pItem->szUtf8EntryName[pItem->lenEntryNameUtf8Bytes-1];
	if ((ch==' ') || (ch=='.'))
		pItem->portWarnings |= SG_PORT_FLAGS__INDIV__FINAL_DOT_SPACE;
}

#ifdef DEBUG
static void _indiv__test_debug_bad_name(SG_UNUSED_PARAM(SG_context * pCtx),
										SG_portability_item * pItem)
{
	size_t len = strlen(SG_PORT_DEBUG_BAD_NAME);

	SG_UNUSED(pCtx);

	// allow any entryname that begins with the DEBUG_BAD_NAME.

    if (0 == strncmp(pItem->szUtf8EntryName, SG_PORT_DEBUG_BAD_NAME, len))
    {
	    pItem->portWarnings |= SG_PORT_FLAGS__INDIV__DEBUG_BAD_NAME;
    }
}
#endif

static void _indiv__test_msdos_device(SG_UNUSED_PARAM(SG_context * pCtx),
									  SG_portability_item * pItem)
{
	// see if the entryname is a reserved MSDOS device name, such as "COM1".
	//
	// [] Windows doesn't like files with a reserved device name, like "COM1".
	// [] Some Windows apps also have problems with those names with a suffix,
	//    like "COM1.txt".
	//
	// http://msdn.microsoft.com/en-us/library/aa365247.aspx

	char * sz;

	SG_UNUSED(pCtx);

	//static const char * sa_devices[] =
	//	{
	//		"aux",
	//		"com1", "com2", "com3", "com4", "com5", "com6", "com7", "com8", "com9",
	//		"con",
	//		"lpt1", "lpt2", "lpt3", "lpt4", "lpt5", "lpt6", "lpt7", "lpt8", "lpt9",
	//		"nul",
	//		"prn",
	//	};

#define CH(ch,l,u)	(((ch)==(l)) || ((ch)==(u)))
#define C19(ch)		(((ch)>='1') && ((ch)<='9'))
#define CDOTNUL(ch)	(((ch)=='.') || ((ch)==0))

	sz = pItem->szUtf8EntryName;
	switch (sz[0])
	{
	case 'a':
	case 'A':
		if (CH(sz[1],'u','U')  &&  CH(sz[2],'x','X')  &&  CDOTNUL(sz[3]))
			goto found_match;
		return;

	case 'c':
	case 'C':
		if (CH(sz[1],'o','O'))
		{
			if (CH(sz[2],'m','M')  &&  C19(sz[3])  &&  CDOTNUL(sz[4]))
				goto found_match;
			if (CH(sz[2],'n','N')  &&  CDOTNUL(sz[3]))
				goto found_match;
		}
		return;

	case 'l':
	case 'L':
		if (CH(sz[1],'p','P')  &&  CH(sz[2],'t','T')  &&  C19(sz[3])  &&  CDOTNUL(sz[4]))
			goto found_match;
		return;

	case 'n':
	case 'N':
		if (CH(sz[1],'u','U')  &&  CH(sz[2],'l','L')  &&  CDOTNUL(sz[3]))
			goto found_match;
		return;

	case 'p':
	case 'P':
		if (CH(sz[1],'r','R')  &&  CH(sz[2],'n','N')  &&  CDOTNUL(sz[3]))
			goto found_match;
		return;

	default:
		return;
	}

#undef CH
#undef C19
#undef CDOTNUL

found_match:
	pItem->portWarnings |= SG_PORT_FLAGS__INDIV__MSDOS_DEVICE;
}

static void _indiv__test_nonbmp(SG_UNUSED_PARAM(SG_context * pCtx),
								SG_portability_item * pItem)
{
	// see if the entryname contains a non-bmp character ( > U+FFFF ).
	//
	// This can be problematic because some platforms:
	//
	// [] support Non-BMP characters in entrynames (either because they
	//    (actively/blindly) allow arbitrary UTF-8 sequences or (actively/
	//     blindly) allow the multi-word UTF-16 sequences).
	//
	// [] don't support Non-BMP characters (because they use UCS-2 or
	//    Unicode 2.0 internally).
	//
	// [] actively (and silently) REPLACE Non-BMP characters with a Unicode
	//    replacement character.  We've observed U+FFFD REPLACEMENT CHARACTER
	//    being used for this.   So, for example, create() may succeed but
	//    readdir() won't find it because of a character substitution.

	SG_uint32 k;

	SG_UNUSED(pCtx);

	if (pItem->b7BitClean)
		return;

	for (k=0; k<pItem->lenEntryNameChars; k++)
	{
		if (pItem->pUtf32EntryName[k] > 0x0000ffff)
		{
			pItem->portWarnings |= SG_PORT_FLAGS__INDIV__NONBMP;
			return;
		}
	}
}

static void _indiv__test_ignorable(SG_context * pCtx,
								   SG_portability_item * pItem)
{
	// see if entryname contains Unicode "Ignorable" characters.
	//
	// Various systems differ on how "Ignorable" characters are handled:
	//
	// [] significant - preserved on create, significant during lookups.
	// [] ignored - preserved on create, ignoed on lookups.
	//
	// HFS+ (and HFSX in case-insensitive mode) allow files to be created
	// with "ignorable" characters.  When you create a filename containing
	// ignorable characters, you can see the ignorables with readdir().
	//
	// But you can stat() the file using a filename that contains:
	// {} the ignorables,
	// {} one that omits the ignorables,
	// {} one that has DIFFERENT ignorables in it.
	//
	// Basically, they are omitted from the lookup comparison.
	//
	// http://developer.apple.com/technotes/tn/tn1150.html#UnicodeSubtleties
	// See also Apple's FastUnicodeCompare.c

	SG_bool bContainsIgnorables;

	if (pItem->b7BitClean)
		return;

	SG_ERR_CHECK_RETURN(  SG_apple_unicode__contains_ignorables__utf32(pCtx,
																	   pItem->pUtf32EntryName,
																	   &bContainsIgnorables)  );
	if (bContainsIgnorables)
		pItem->portWarnings |= SG_PORT_FLAGS__INDIV__IGNORABLE;
}

static void _indiv__test_shortnames(SG_UNUSED_PARAM(SG_context * pCtx),
									SG_portability_item * pItem)
{
	// On Windows, files/directories can have 2 names:
	// [] the long name
	// [] the somewhat hidden 8.3 short name
	//
	// Normally, we only deal with the long names.  But if someone on
	// Linux creates both "Program Files" and "PROGRA~1" as 2 separate
	// directories and then tries to move the tree to Windows, they may
	// have problems.
	//
	// Since the short name is auto-generated by Windows (a variable
	// length prefix and a sequence number), we cannot tell what it
	// will be for any given long name.
	//
	// Therefore, we complain if the given name ***LOOKS*** like it
	// might be a short name.
	//
	// http://msdn.microsoft.com/en-us/library/aa365247.aspx
	// http://en.wikipedia.org/wiki/8.3_filename
	//
	// based upon the Wikipedia article and some experimentation,
	// shortnames look like:
	//
	//    01234567
	// [] XXXXXX~1.XXX
	// [] XXhhhh~1.XXX
	// [] Xhhhh~1.XXX

	SG_int32 * p32;

	SG_UNUSED(pCtx);

#define C09(ch)		(((ch)>='0') && ((ch)<='9'))
#define CAF(ch)		(((ch)>='A') && ((ch)<='F'))
#define Caf(ch)		(((ch)>='a') && ((ch)<='f'))
#define CHEX(ch)	(C09(ch) || CAF(ch) || Caf(ch))
#define CDOTNUL(ch)	(((ch)=='.') || ((ch)==0))
#define CTILDA(ch)	((ch)=='~')

	p32 = pItem->pUtf32EntryName;

	if (pItem->lenEntryNameChars >= 7)
	{
		if (CTILDA(p32[5]) && C09(p32[6]) && CDOTNUL(p32[7]))
			if (CHEX(p32[1]) && CHEX(p32[2]) && CHEX(p32[3]) && CHEX(p32[4]))
				goto found_match;
	}
	if (pItem->lenEntryNameChars >= 8)
	{
		if (CTILDA(p32[6]) && C09(p32[7]) && CDOTNUL(p32[8]))
			goto found_match;
	}

	return;

found_match:
	pItem->portWarnings |= SG_PORT_FLAGS__INDIV__SHORTNAMES;

#undef C09
#undef CAF
#undef Caf
#undef CHEX
#undef CDOTNUL
#undef CTILDA
}

static void _indiv__test_sfm_ntfs(SG_context * pCtx,
								  SG_portability_item * pItem)
{
	// The Mac has something called "Services for Macintosh" that
	// allows the mac to use NTFS filesystems.  It maps the various
	// invalid ntfs characters 0x00..0x1f, '"', '*', '/', ':', and
	// etc ***as well as*** final spaces and dots into Unicode
	// Private Use characters in the range [u+f001 .. u+f029].
	//
	// i've confirmed that this happens between Mac OS X 10.5.5
	// and a remote NTFS partition on XP.
	//
	// i have not tested whether it happens on directly mounted
	// NTFS partitions, but i'm not sure it matters.  the important
	// thing is that somebody does it, so we need to test for it.
	//
	// The exact remapping is described in:
	//
	// http://support.microsoft.com/kb/q117258/
	//
	// ***NOTE*** This is documented as if it were a Windows feature
	// and something that NT 3.5 and 4.0 exported to MACs.  ***BUT***
	// I found evidence in the mac source that they''ve got code for
	// doing it too.  ***And*** the mac version substitues ':' for '/'
	// in the table because they changed pathname separators between
	// OS 9 and OS X.
	//
	// http://www.opensource.apple.com/darwinsource/10.5.6/xnu-1228.9.59/bsd/vfs/vfs_utfconv.c

	SG_bool bContainsSFM;

	if (pItem->b7BitClean)
		return;

	SG_ERR_CHECK_RETURN(  SG_apple_unicode__contains_sfm_chars__utf32(pCtx,
																	  pItem->pUtf32EntryName,
																	  &bContainsSFM)  );
	if (bContainsSFM)
		pItem->portWarnings |= SG_PORT_FLAGS__INDIV__SFM_NTFS;
}

static void _indiv__test_leading_dash(SG_UNUSED_PARAM(SG_context * pCtx),
									  SG_portability_item * pItem)
{
	// check for a leading dash in the filename.  this can cause problems
	// if a user on Windows creates a file with a leading dash (where the
	// forward slash is used to denote command line arguments) and then is
	// used on Linux (where the dash is used to denote command line arguments).

	SG_UNUSED(pCtx);

	if (pItem->szUtf8EntryName[0] == '-')
		pItem->portWarnings |= SG_PORT_FLAGS__INDIV__LEADING_DASH;
}

//////////////////////////////////////////////////////////////////

/**
 * Log a message for any potential problems.
 *
 */
void _sg_portability_dir__log_indiv(SG_context * pCtx,
									SG_portability_dir * pDir, SG_portability_item * pItem, SG_varray * pvaLog)
{
	SG_portability_flags wf = pItem->portWarnings & SG_PORT_FLAGS__MASK_INDIV;
	SG_portability_flags m  = pDir->portLogMask & SG_PORT_FLAGS__MASK_INDIV;
    SG_vhash* pvhItem = NULL;
    SG_varray* pvaWhy = NULL;

	SG_NULLARGCHECK_RETURN(pDir);
	SG_NULLARGCHECK_RETURN(pItem);
	SG_NULLARGCHECK_RETURN(pvaLog);

    /* alloc a vhash for our warning item */
	SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvhItem)  );
    SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "type", "indiv")  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "path", pItem->szRelativePath)  );
	SG_ERR_CHECK(  SG_vhash__add__int64(pCtx, pvhItem, "why_bits", ((wf) & (m)))  );

	if (pItem->bufGid_Entry[0])
		SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "gid", pItem->bufGid_Entry)  );

    /* all the reasons why are going into an array */
    SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pvaWhy)  );

#define M(wf,m,bit,msg)	SG_STATEMENT( if ((wf) & (m) & (bit)) SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pvaWhy, (msg))  ); )

	M(wf,m,SG_PORT_FLAGS__INDIV__INVALID_00_7F,			"INVALID_CHARS");
	M(wf,m,SG_PORT_FLAGS__INDIV__RESTRICTED_00_7F,		"RESTRICTED_CHARS");
	M(wf,m,SG_PORT_FLAGS__INDIV__ENTRYNAME_LENGTH,		"ENTRYNAME_LENGTH");
	M(wf,m,SG_PORT_FLAGS__INDIV__PATHNAME_LENGTH,		"PATHNAME_LENGTH");
	M(wf,m,SG_PORT_FLAGS__INDIV__FINAL_DOT_SPACE,		"FINAL_DOT_SPACE");
	M(wf,m,SG_PORT_FLAGS__INDIV__MSDOS_DEVICE,			"MSDOS_DEVICE");
	M(wf,m,SG_PORT_FLAGS__INDIV__NONBMP,				"UNICODE_NON_BMP");
	M(wf,m,SG_PORT_FLAGS__INDIV__IGNORABLE,				"UNICODE_IGNORABLE");
	M(wf,m,SG_PORT_FLAGS__INDIV__CHARSET,				"CHARSET");
	M(wf,m,SG_PORT_FLAGS__INDIV__SHORTNAMES,			"MSDOS_SHORTNAME");
	M(wf,m,SG_PORT_FLAGS__INDIV__SFM_NTFS,				"APPLE_SFM_NTFS");
	M(wf,m,SG_PORT_FLAGS__INDIV__NON_CANONICAL_NFD,		"UNICODE_NON_CANONICAL");
#ifdef DEBUG
	M(wf,m,SG_PORT_FLAGS__INDIV__DEBUG_BAD_NAME,		"DEBUG_BAD_NAME");
#endif
	M(wf,m,SG_PORT_FLAGS__INDIV__LEADING_DASH,			"LEADING_DASH");

#undef M

    SG_ERR_CHECK(  SG_vhash__add__varray(pCtx, pvhItem, "why", &pvaWhy)  );

    /* add our warning to the running log */
    SG_ERR_CHECK(  SG_varray__append__vhash(pCtx, pvaLog, &pvhItem)  );

    return;

fail:
    SG_VARRAY_NULLFREE(pCtx, pvaWhy);
    SG_VHASH_NULLFREE(pCtx, pvhItem);
}

/**
 * Inspect the given file/directory entryname (which is a part of the
 * container's relative pathname) and see if it has any problematic
 * chars and add it to our container so that we can check for collisions.
 *
 * If we find a problem, we update the pItem->portWarnings.
 *
 * We try to be as verbose as possible and completely list warnings
 * rather than stopping after the first one.
 */
void _sg_portability_dir__inspect_item(SG_context * pCtx,
									   SG_portability_dir * pDir, SG_portability_item * pItem)
{
	// run all the individual inspections on the given entryname and
	// repo-relative pathname and set any __INDIV__ bits that apply.

	SG_NULLARGCHECK_RETURN(pItem);

#ifdef DEBUG
	SG_ERR_CHECK(  _indiv__test_debug_bad_name(pCtx, pItem)  );
#endif
	SG_ERR_CHECK(  _indiv__test_invalid_or_restricted_00_7f(pCtx, pItem)  );
	SG_ERR_CHECK(  _indiv__test_name_length(pCtx, pItem)  );
	SG_ERR_CHECK(  _indiv__test_final_dot_space(pCtx, pItem)  );
	SG_ERR_CHECK(  _indiv__test_msdos_device(pCtx, pItem)  );
	SG_ERR_CHECK(  _indiv__test_nonbmp(pCtx, pItem)  );
	SG_ERR_CHECK(  _indiv__test_ignorable(pCtx, pItem)  );
	SG_ERR_CHECK(  _indiv__test_shortnames(pCtx, pItem)  );
	SG_ERR_CHECK(  _indiv__test_sfm_ntfs(pCtx, pItem)  );
	SG_ERR_CHECK(  _indiv__test_leading_dash(pCtx, pItem)  );

	// update the union of all warning flags.  this only includes the
	// __INDIV__ bits and thus is possibly incomplete because we haven't
	// computed the potential collisions yet.

	pDir->portWarningsAll |= pItem->portWarnings;
	if (pItem->bCheckMyName_Indiv)
		pDir->portWarningsChecked |= pItem->portWarnings;

	// if the item is new and it has one or more potential probmes that
	// we care about, then give a detailed description.

	if (pItem->bCheckMyName_Indiv)
	{
		if (pItem->portWarnings & pDir->portLogMask & SG_PORT_FLAGS__MASK_INDIV)
		{
			if (pDir->pvaLog)
				SG_ERR_CHECK(  _sg_portability_dir__log_indiv(pCtx, pDir,pItem,pDir->pvaLog)  );
			if (pItem->pvaLog)
				SG_ERR_CHECK(  _sg_portability_dir__log_indiv(pCtx, pDir,pItem,pItem->pvaLog)  );
		}
	}

	return;

fail:
	return;
}

//////////////////////////////////////////////////////////////////

void _append_collision_msg(SG_context * pCtx,
						   SG_varray * pva, SG_portability_flags why)
{
	// append a series of descriptions for what was done to the entryname
	// to make it collide with something else.  we do not filter out steps
	// using the portLogMask because it may have taken multiple why-bits to get
	// this version of the entryname.

#define M(wf,bit,msg)	SG_STATEMENT( if ((wf) & (bit)) SG_ERR_CHECK_RETURN(  SG_varray__append__string__sz(pCtx, pva,(msg))  ); )

	M(why,SG_PORT_FLAGS__COLLISION__ORIGINAL,			"ORIGINAL");
	M(why,SG_PORT_FLAGS__COLLISION__CASE,				"CASE");
	M(why,SG_PORT_FLAGS__COLLISION__FINAL_DOT_SPACE,	"FINAL_DOT_SPACE");
	M(why,SG_PORT_FLAGS__COLLISION__PERCENT25,			"PERCENT25");
	M(why,SG_PORT_FLAGS__COLLISION__CHARSET,			"CHARSET");
	M(why,SG_PORT_FLAGS__COLLISION__SFM_NTFS,			"APPLE_SFM_NTFS");
	M(why,SG_PORT_FLAGS__COLLISION__NONBMP,				"UNICODE_NON_BMP");
	M(why,SG_PORT_FLAGS__COLLISION__IGNORABLE,			"UNICODE_IGNORABLE");
	M(why,SG_PORT_FLAGS__COLLISION__NFC,				"UNICODE_NFC");
#ifdef DEBUG
	M(why,SG_PORT_FLAGS__COLLISION__DEBUG_BAD_NAME,		"DEBUG_BAD_NAME");
#endif

	// TODO more...

#undef M
}

void _sg_portability_dir__log_collision(SG_context * pCtx,
										SG_portability_dir * pDir, const char * szUtf8Key,
										SG_portability_item * pItem1, SG_portability_flags why1,
										SG_portability_item * pItem2, SG_portability_flags why2,
										SG_varray * pvaLog)
{
    SG_vhash* pvhItem = NULL;
    SG_varray* pvaWhy1 = NULL;
    SG_varray* pvaWhy2 = NULL;

	SG_NULLARGCHECK_RETURN(pDir);
	SG_NONEMPTYCHECK_RETURN(szUtf8Key);
	SG_NULLARGCHECK_RETURN(pItem1);
	SG_NULLARGCHECK_RETURN(pItem2);
	SG_NULLARGCHECK_RETURN(pvaLog);

    /* allocate a vhash for our warning item */
	SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvhItem)  );
    SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "type", "collision")  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "key", szUtf8Key)  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "path1", pItem1->szRelativePath)  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "path2", pItem2->szRelativePath)  );
	SG_ERR_CHECK(  SG_vhash__add__int64(pCtx, pvhItem, "why1_bits", why1)  );
	SG_ERR_CHECK(  SG_vhash__add__int64(pCtx, pvhItem, "why2_bits", why2)  );

	if (pItem1->bufGid_Entry[0])
		SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "gid1", pItem1->bufGid_Entry)  );
	if (pItem2->bufGid_Entry[0])
		SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "gid2", pItem2->bufGid_Entry)  );

    /* the reasons why go into varrays */
    SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pvaWhy1)  );
    SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pvaWhy2)  );
	SG_ERR_CHECK(  _append_collision_msg(pCtx, pvaWhy1,why1)  );
	SG_ERR_CHECK(  _append_collision_msg(pCtx, pvaWhy2,why2)  );
    SG_ERR_CHECK(  SG_vhash__add__varray(pCtx, pvhItem, "why1", &pvaWhy1)  );
    SG_ERR_CHECK(  SG_vhash__add__varray(pCtx, pvhItem, "why2", &pvaWhy2)  );

    /* add the warning item to the running log */
	SG_ERR_CHECK(  SG_varray__append__vhash(pCtx, pvaLog, &pvhItem)  );

    return;

fail:
    SG_VHASH_NULLFREE(pCtx, pvhItem);
    SG_VARRAY_NULLFREE(pCtx, pvaWhy1);
    SG_VARRAY_NULLFREE(pCtx, pvaWhy2);
}

/**
 * add an individual name mangling to our container.
 * if we get a collision, log it and continue.
 *
 */
void _sg_portability_dir__add_mangling(SG_context * pCtx,
									   SG_portability_dir * pDir, const char * szUtf8Key,
									   SG_portability_item * pItem, SG_portability_flags portWhy)
{
	struct _my_collider_assoc_data * pData = NULL;
	struct _my_collider_assoc_data * pDataCollision;
	SG_bool bDuplicateKey;

	// portWhy gives us the SG_PORT_FLAGS__PAIRWISE__ bit describing why we are
	// adding this entry to prbCollider.  For example, this can be because the key
	// is the original entry name or because we are adding the lowercase version
	// and etc.  This will be used when we have a collision and generate a better
	// warning.  For example on a Linux system, you might get something like:
	//      the lowercase("FOO") collided with the lowercase("Foo")

	SG_ERR_CHECK_RETURN(  _my_collider_assoc_data__alloc(pCtx, portWhy,pItem,&pData)  );

	// try to add (key,(why,item)) to rbtree.  if this succeeds, we did not
	// collide with anything and can just return.

	SG_rbtree__add__with_assoc(pCtx, pDir->prbCollider,szUtf8Key,pData);
	if (SG_context__has_err(pCtx) == SG_FALSE)
		return;

	bDuplicateKey = SG_context__err_equals(pCtx,SG_ERR_RBTREE_DUPLICATEKEY);

	// we could not add (key,pData) to the rbtree, so free pData before we
	// go any further.

	SG_ERR_IGNORE(  _my_collider_assoc_data__free(pCtx, pData)  );
	pData = NULL;

	if (!bDuplicateKey)
		SG_ERR_RETHROW_RETURN;

	SG_context__err_reset(pCtx);

	// we have a collision.  make sure it isn't us.  (this could happen if
	// 2 different manglings just happen to produce the same result (such
	// as when we are intially given a completely lowercase filename and our
	// normalization computes a matching one).)

	SG_ERR_CHECK_RETURN(  SG_rbtree__find(pCtx, pDir->prbCollider,szUtf8Key,NULL,(void **)&pDataCollision)  );
	if (pDataCollision->pItem == pItem)
		return;

	// we have a real collision.  add the WHY bits to the actual warnings
	// on these items.

	pItem->portWarnings |= portWhy;
	pDataCollision->pItem->portWarnings |= pDataCollision->portFlagsWhy;

	// update the union of all warning flags on the entire directory to include
	// the WHY bits here.  note that the directory warnings bits are possibly
	// still incomplete because we haven't completed computing the collisions yet.

	pDir->portWarningsAll |= portWhy | pDataCollision->portFlagsWhy;
	if (pItem->bCheckMyName_Collision)
		pDir->portWarningsChecked |= portWhy;
	if (pDataCollision->pItem->bCheckMyName_Collision)
		pDir->portWarningsChecked |= pDataCollision->portFlagsWhy;

	// if one or both are new items and caller cares about this type of problem,
	// then we should give a detailed collision description.

	if (pItem->bCheckMyName_Collision || pDataCollision->pItem->bCheckMyName_Collision)
	{
		if ((portWhy | pDataCollision->portFlagsWhy) & pDir->portLogMask & SG_PORT_FLAGS__MASK_COLLISION)
		{
			if (pDir->pvaLog)
				SG_ERR_CHECK_RETURN(  _sg_portability_dir__log_collision(pCtx,
																		 pDir,szUtf8Key,
																		 pItem,portWhy,
																		 pDataCollision->pItem,pDataCollision->portFlagsWhy,
																		 pDir->pvaLog)  );
			if (pItem->pvaLog)
				SG_ERR_CHECK_RETURN(  _sg_portability_dir__log_collision(pCtx,
																		 pDir,szUtf8Key,
																		 pItem,portWhy,
																		 pDataCollision->pItem,pDataCollision->portFlagsWhy,
																		 pItem->pvaLog)  );
			if (pDataCollision->pItem->pvaLog)
				SG_ERR_CHECK_RETURN(  _sg_portability_dir__log_collision(pCtx,
																		 pDir,szUtf8Key,
																		 pDataCollision->pItem,pDataCollision->portFlagsWhy,
																		 pItem,portWhy,
																		 pDataCollision->pItem->pvaLog)  );
		}
	}
}

void _sg_portability_dir__collide_item_7bit_clean(SG_context * pCtx,
												  SG_portability_dir * pDir,
												  SG_portability_item * pItem)
{
	// do 7-bit clean portion of collide_item using the given entryname
	// and flags as a starting point.
	//
	// that is, we compute an "effectively equivalent" entryname; one that
	// or more actual filesystems may interpret/confuse with the given entryname.
	// this is not an exact match -- this is an equivalence class.  that is, i'm
	// not going to individually compute all the possible permutations of confusion
	// (such as only folding case for the mac and then separately stripping final
	// whitespace for windows).  instead, we apply all the transformations and
	// compute the base that all members of this class will map to.

	char * szCopy = NULL;
	char * p;
	const char * szEntryNameStart = pItem->szUtf8EntryName;
	SG_portability_flags portWhyStart = SG_PORT_FLAGS__NONE;
	SG_portability_flags portWhyComputed = portWhyStart;

	// create a copy of the original entryname so that we can trash/munge it.

	SG_ERR_CHECK(  SG_STRDUP(pCtx, szEntryNameStart,&szCopy)  );

	if (pItem->portWarnings & SG_PORT_FLAGS__INDIV__RESTRICTED_00_7F)
	{
		// Whenn using VMWare's host-guest filesystem (vmhgfs), we sometimes get
		// filenames with '%' mapped to "%25".  I've seen this running an Ubunutu 8.04
		// VM inside VMWare 6 on XP with an NTFS directory shared into the VM.
		//
		// TODO determine if this was a bug in VMWare or if they want this feature.
		//
		// TODO determine if this happens recursively -- that is, if "foo%.txt" get
		// TODO mapped to "foo%25.txt", can that get mapped to "foo%%2525.txt"?
		//
		// Since the point here is to determine *potential* collisions (and we can't
		// tell if the user actually created the file with this name or if VMHGFS
		// munged it), collapse all "%25" to "%" and don't worry about which one we
		// actually had.

		p = szCopy;
		while (*p)
		{
			if ((p[0]=='%')  &&  (p[1]=='2')  && (p[2]=='5'))
			{
				memmove(&p[1],&p[3],strlen(&p[3])+1);
				portWhyComputed |= SG_PORT_FLAGS__COLLISION__PERCENT25;

				// restart with this '%' so that we collapse "%%2525" to "%%".

				continue;
			}

			p++;
		}
	}

	if (pItem->portWarnings & SG_PORT_FLAGS__INDIV__FINAL_DOT_SPACE)
	{
		// on a Windows/NTFS filesystem, final space and dot normally get stripped.
		// the docs only talk about the final character, but testing shows that all
		// trailing space/dot get stripped.

		p = szCopy + strlen(szCopy) - 1;
		while (p > szCopy)
		{
			if ((*p != ' ') && (*p != '.'))
				break;

			*p-- = 0;
			portWhyComputed |= SG_PORT_FLAGS__COLLISION__FINAL_DOT_SPACE;
		}
	}

	// both Mac and Windows are case-preserving, but not case-sensitive.  so
	// we convert the entryname to lowercase to simulate this.  for US-ASCII,
	// both platforms use the same case-conversion rules.

	for (p=szCopy; (*p); p++)
	{
		if ((*p >= 'A') && (*p <= 'Z'))
		{
			*p = 'a' + (*p - 'A');
			portWhyComputed |= SG_PORT_FLAGS__COLLISION__CASE;
		}
	}

	// if we added something to the flags, add our string to the collider.

	if (portWhyComputed != portWhyStart)
	{
		SG_ERR_CHECK(  _sg_portability_dir__add_mangling(pCtx, pDir,szCopy,pItem,portWhyComputed)  );
	}
	else
	{
		// this is a coding error, but it is safe to let this continue.
		// our collision detector will just be missing an entry.
		SG_ASSERT( (strcmp(szEntryNameStart,szCopy) == 0)  &&  "Filename munged but no bits set." );
	}

	// fall-thru to common cleanup.

fail:
	SG_NULLFREE(pCtx, szCopy);
}

void _sg_portability_dir__collide_item_32bit(SG_context * pCtx,
											 SG_portability_dir * pDir,
											 SG_portability_item * pItem)
{
	// do full collision detection assuming we have UTF-32 data that
	// contains some non-US-ASCII characters.
	//
	// again, we want to compute the base entryname for the equivalence class
	// rather than all possible permutations.

	SG_int32 * pUtf32Copy = NULL;
	SG_int32 * pUtf32CopyNFC = NULL;
	char * pUtf8Copy = NULL;
	char * pUtf8CopyNFC = NULL;
	char * pUtf8CopyNFD = NULL;
	SG_uint32 k;
	SG_uint32 lenCopyChars;
	SG_bool bHadFinalDotSpaceSFM = SG_FALSE;
	SG_portability_flags portWhyStart = SG_PORT_FLAGS__NONE;
	SG_portability_flags portWhyComputed = portWhyStart;

	// create a copy of the UTF-32 entryname so that we can trash/munge it.

	SG_ERR_CHECK(  SG_allocN(pCtx, pItem->lenEntryNameChars+1,pUtf32Copy)  );

	memcpy(pUtf32Copy,pItem->pUtf32EntryName,pItem->lenEntryNameChars*sizeof(SG_int32));
	lenCopyChars = pItem->lenEntryNameChars;

	if (pItem->portWarnings & SG_PORT_FLAGS__INDIV__NONBMP)
	{
		// convert all non-bmp (those > u+ffff) characters into a
		// U+FFFD REPLACEMENT CHARACTER.  some systems support and
		// respect non-bmp characters and some platforms fold them
		// all to this replacement character.

		for (k=0; k<lenCopyChars; k++)
		{
			if (pUtf32Copy[k] > 0x00ffff)
			{
				pUtf32Copy[k] = 0x00fffd;
				portWhyComputed |= SG_PORT_FLAGS__COLLISION__NONBMP;
			}
		}
	}

	if (pItem->portWarnings & SG_PORT_FLAGS__INDIV__RESTRICTED_00_7F)
	{
		// see note in 7bit version about "%25" on VMHGFS.

		k = 0;
		while (k < lenCopyChars-2)
		{
			if ((pUtf32Copy[k] == '%')  &&  (pUtf32Copy[k+1] == '2')  &&  (pUtf32Copy[k+2] == '5'))
			{
				// strip out "25" in "%25".  we add 1 to bring along the trailing null.
				// then restart loop with this '%' so that we collapse "%%2525" to "%%".

				memmove(&pUtf32Copy[k+1],&pUtf32Copy[k+3],
					   sizeof(SG_int32)*(lenCopyChars - (k+3) + 1));
				lenCopyChars -= 2;

				portWhyComputed |= SG_PORT_FLAGS__COLLISION__PERCENT25;
			}
			else
			{
				k++;
			}
		}
	}

	if (pItem->portWarnings & SG_PORT_FLAGS__INDIV__SFM_NTFS)
	{
		// go thru the string and convert any SFM characters to the US-ASCII equivalents.
		// (we have to do this before we do the final_dot_space checks.)

		for (k=0; k<lenCopyChars; k++)
		{
			SG_int32 ch32;

			SG_ERR_CHECK(  SG_apple_unicode__map_sfm_char_to_ascii__utf32(pCtx, pUtf32Copy[k],&ch32)  );
			if (ch32 != pUtf32Copy[k])
			{
				pUtf32Copy[k] = ch32;
				portWhyComputed |= SG_PORT_FLAGS__COLLISION__SFM_NTFS;

				if ((ch32 == ' ') || (ch32 == '.'))
					bHadFinalDotSpaceSFM = SG_TRUE;
			}
		}
	}

	if ((pItem->portWarnings & SG_PORT_FLAGS__INDIV__FINAL_DOT_SPACE)  ||  bHadFinalDotSpaceSFM)
	{
		// see note in 7bit version for final dot and space.

		while (lenCopyChars > 0)
		{
			SG_int32 ch32 = pUtf32Copy[lenCopyChars-1];

			if ((ch32 != '.') && (ch32 != ' '))
				break;

			pUtf32Copy[--lenCopyChars] = 0;
			portWhyComputed |= SG_PORT_FLAGS__COLLISION__FINAL_DOT_SPACE;
		}
	}

	if (pItem->bHasDecomposition)
	{
		// the NFC and NFD forms of the entryname are different.
		// we (arbitrarily) pick NFC as the base case for the collider
		// and convert everything to that.
		//
		// we have to do this before we strip ignorables so that we don't
		// accidentally combine or permute 2 accent characters that were
		// separated by an ignorable.

		if (!pItem->bIsProperlyComposed)
		{
			// <rant>I'm sick of these Unicode routines (ICU's, Apple's,
			// and my wrappers for them).  Some want UTF-8, some want UTF-16,
			// and some want UTF-32.  Apple's routines take UTF-8 and secretely
			// convert to UTF-16 to do the work and then convert the result
			// back to UTF-8.  So I have to convert from UTF-32 to UTF-8 just
			// so that I can call it.</rant>

			SG_uint32 lenUtf8Copy, lenUtf8CopyNFC, lenUtf8CopyNFD, lenUtf32CopyNFC;

			// convert our working UTF-32 buffer to UTF-8

			SG_ERR_CHECK(  SG_utf8__from_utf32(pCtx, pUtf32Copy,&pUtf8Copy,&lenUtf8Copy)  );
			if (pItem->bIsProperlyDecomposed)
			{
				// if original is canonical NFD, we can directly compute NFC from it.
				SG_ERR_CHECK(  SG_apple_unicode__compute_nfc__utf8(pCtx, pUtf8Copy,lenUtf8Copy, &pUtf8CopyNFC, &lenUtf8CopyNFC)  );
			}
			else
			{
				// if original is non-canonical NFD, we first have to make it canonical NFD
				// and then convert that to NFC.  (yes, we get different answers.)
				SG_ERR_CHECK(  SG_apple_unicode__compute_nfd__utf8(pCtx, pUtf8Copy,   lenUtf8Copy,    &pUtf8CopyNFD, &lenUtf8CopyNFD)  );
				SG_ERR_CHECK(  SG_apple_unicode__compute_nfc__utf8(pCtx, pUtf8CopyNFD,lenUtf8CopyNFD, &pUtf8CopyNFC, &lenUtf8CopyNFC)  );
			}

			// convert our UTF-8 NFC result back to UTF-32 and replace our working UTF-32 buffer.

			SG_ERR_CHECK(  SG_utf8__to_utf32__buflen(pCtx, pUtf8CopyNFC,lenUtf8CopyNFC, &pUtf32CopyNFC,&lenUtf32CopyNFC)  );

			SG_NULLFREE(pCtx, pUtf8Copy);
			SG_NULLFREE(pCtx, pUtf8CopyNFC);

			SG_free(pCtx, pUtf32Copy);
			pUtf32Copy = pUtf32CopyNFC;
			pUtf32CopyNFC = NULL;

			lenCopyChars = lenUtf32CopyNFC;

			portWhyComputed |= SG_PORT_FLAGS__COLLISION__NFC;
		}
	}

	if (pItem->portWarnings & SG_PORT_FLAGS__INDIV__IGNORABLE)
	{
		// apple ignores various characters in filenames (they are preserved
		// in the name but are ignored on lookups).  so we strip them out to
		// catch any ambituities/collisions.

		SG_bool bIgnorable;

		k = 0;
		while (k < lenCopyChars)
		{
			SG_ERR_CHECK(  SG_apple_unicode__is_ignorable_char__utf32(pCtx, pUtf32Copy[k],&bIgnorable)  );

			if (bIgnorable)
			{
				memmove(&pUtf32Copy[k],&pUtf32Copy[k+1],
					   sizeof(SG_int32)*(lenCopyChars-k));
				lenCopyChars -= 1;

				portWhyComputed |= SG_PORT_FLAGS__COLLISION__IGNORABLE;
			}
			else
			{
				k++;
			}
		}
	}

	// convert entryname to lowercase for the collider.  since apple and windows
	// are not case sensitive, we make lowercase the base class for the collider.

	for (k=0; k<lenCopyChars; k++)
	{
		SG_int32 ch32Apple;
		SG_int32 ch32AppleICU;
		SG_int32 ch32ICU;

		// Apple's lowercase routines are based upon "their interpretation"
		// of Unicode 3.2.  This is what they have hard-coded in their filesystem.
		// So our collider needs to fold case the way that they do.
		//
		// BUT, Windows/NTFS has their own set of rules hard-coded in their filesystem.
		// So our collider needs to fold case the way that they do.
		// TODO I haven't found a reference for this yet.
		//
		// We have the Apple code and we have ICU (based upon Unicode 5.1).
		// So let's run it thru both and see what happens.  Again we're only
		// trying compute the base of an equivalence class -- not the various
		// permutation members.  This may cause some suble false-positives,
		// but I don't care.

		SG_ERR_CHECK(  SG_apple_unicode__to_lower_char__utf32(pCtx, pUtf32Copy[k],&ch32Apple)  );
		SG_ERR_CHECK(  SG_utf32__to_lower_char(pCtx, ch32Apple,&ch32AppleICU)  );

		SG_ERR_CHECK(  SG_utf32__to_lower_char(pCtx, pUtf32Copy[k],&ch32ICU)  );

		SG_ASSERT(  (ch32ICU == ch32AppleICU)  &&  "Inconsistent lowercase."  );

		if (ch32AppleICU != pUtf32Copy[k])
		{
			pUtf32Copy[k] = ch32AppleICU;

			portWhyComputed |= SG_PORT_FLAGS__COLLISION__CASE;
		}
	}

	// if we added something to the flags, add our string to the collider.

	if (portWhyComputed != portWhyStart)
	{
		SG_uint32 lenUtf8Copy;

		SG_ERR_CHECK(  SG_utf8__from_utf32(pCtx, pUtf32Copy,&pUtf8Copy,&lenUtf8Copy)  );
		SG_ERR_CHECK(  _sg_portability_dir__add_mangling(pCtx, pDir,pUtf8Copy,pItem,portWhyComputed)  );
		SG_NULLFREE(pCtx, pUtf8Copy);
	}
	else
	{
		// this is a coding error, but it is safe to let this continue.
		// our collision detector will just be missing an entry.
		SG_ASSERT( (lenCopyChars == pItem->lenEntryNameChars)
				&& (memcmp(pItem->pUtf32EntryName,pUtf32Copy,lenCopyChars) == 0)
				&& "UTF-32 Filename munged but no bits set." );
	}

	// fall-thru to common cleanup.

fail:
	SG_NULLFREE(pCtx, pUtf32Copy);
	SG_NULLFREE(pCtx, pUtf32CopyNFC);
	SG_NULLFREE(pCtx, pUtf8Copy);
	SG_NULLFREE(pCtx, pUtf8CopyNFC);
	SG_NULLFREE(pCtx, pUtf8CopyNFD);
}

#ifdef DEBUG
static void _sg_portability_dir__collide_debug_bad_name(SG_context * pCtx,
														SG_portability_dir * pDir,
														SG_portability_item * pItem)
{
	// we define that all entrynames that begin with <debug_bad_name> collide.
	// strip off anything after the fixed prefix and add that to the collider.
	// for these tests we ***DO NOT*** both folding case and all the other stuff.
	// we only ***EXACT MATCH*** on the prefix.

	char * szCopy = NULL;
	const char * szEntryNameStart = pItem->szUtf8EntryName;
	SG_portability_flags portWhyStart = SG_PORT_FLAGS__NONE;
	SG_portability_flags portWhyComputed = portWhyStart;

	if (pItem->portWarnings & SG_PORT_FLAGS__INDIV__DEBUG_BAD_NAME)
	{
		size_t lenPrefix = strlen(SG_PORT_DEBUG_BAD_NAME);

		if (pItem->lenEntryNameUtf8Bytes > lenPrefix)
		{
			SG_ERR_CHECK(  SG_strdup(pCtx, szEntryNameStart,&szCopy)  );

			szCopy[lenPrefix] = 0;
			portWhyComputed |= SG_PORT_FLAGS__COLLISION__DEBUG_BAD_NAME;

			SG_ERR_CHECK(  _sg_portability_dir__add_mangling(pCtx, pDir,szCopy,pItem,portWhyComputed)  );
		}
	}

	// fall thru

fail:
	SG_NULLFREE(pCtx, szCopy);
}
#endif

/**
 * Compute the various possible name manglings and accumulate
 * them in our container and look for potential collisions.
 */
void _sg_portability_dir__collide_item(SG_context * pCtx,
									   SG_portability_dir * pDir, SG_portability_item * pItem)
{
	// add the original/unmodified UTF-8 entryname and if necessary
	// the CHARSET-based version to the collider.

	SG_ERR_CHECK(  _sg_portability_dir__add_mangling(pCtx,
													 pDir,pItem->szUtf8EntryName,
													 pItem,SG_PORT_FLAGS__COLLISION__ORIGINAL)  );
	if (pItem->szCharSetEntryName)
		SG_ERR_CHECK(  _sg_portability_dir__add_mangling(pCtx,
														 pDir,pItem->szCharSetEntryName,
														 pItem,SG_PORT_FLAGS__COLLISION__CHARSET)  );

#ifdef DEBUG
	SG_ERR_CHECK(  _sg_portability_dir__collide_debug_bad_name(pCtx,pDir,pItem)  );
#endif

	if (pItem->b7BitClean)
	{
		// we have US-ASCII only data.  we can take some shortcuts because none
		// of the Unicode stuff is necessary.
		//
		// Use the UTF-8 entryname and do all the case folding and other such
		// tricks to compute a "base" value for the equivalence class that this
		// entryname is a member of.

		SG_ERR_CHECK(  _sg_portability_dir__collide_item_7bit_clean(pCtx,pDir,pItem)  );
	}
	else
	{
		// we have UTF-8 with some non-US-ASCII characters so we have to do it
		// the hard way.

		SG_ERR_CHECK(  _sg_portability_dir__collide_item_32bit(pCtx,pDir,pItem)  );
	}

	// TODO detect potential collisions items that are "shortnames"
	// TODO (pItem->portWarnings & SG_PORT_FLAGS__INDIV__SHORTNAMES)
	// TODO with other items non-shortname items.  this might require
	// TODO computing a wildcard or psuedo-shortname for all non-shortname
	// TODO items and adding them to a short-name-collider and then seeing
	// TODO if the given shortname matches the pattern.  This will likely
	// TODO have lots of false positives.  Perhaps the __INDIV__SHORTNAMES
	// TODO warning is sufficient.

	// fall-thru to common cleanup.

fail:
	return;
}

//////////////////////////////////////////////////////////////////

void SG_portability_dir__add_item(SG_context * pCtx,
								  SG_portability_dir * pDir,
								  const char * pszGid_Entry,
								  const char * szEntryName,
								  SG_bool bCheckMyName_Indiv, SG_bool bCheckMyName_Collision,
								  SG_bool * pbIsDuplicate)
{
	SG_ERR_CHECK_RETURN(  SG_portability_dir__add_item__with_assoc(pCtx,
																   pDir,
																   pszGid_Entry,
																   szEntryName,
																   bCheckMyName_Indiv,bCheckMyName_Collision,
																   NULL,NULL,
																   pbIsDuplicate,NULL)  );
}

void SG_portability_dir__add_item__with_assoc(SG_context * pCtx,
											  SG_portability_dir * pDir,
											  const char * pszGid_Entry,
											  const char * szEntryName,
											  SG_bool bCheckMyName_Indiv, SG_bool bCheckMyName_Collision,
											  void * pVoidAssocData, SG_varray * pvaLog,
											  SG_bool * pbIsDuplicate, void ** ppVoidAssocData_Original)
{
	SG_portability_item * pItemAllocated = NULL;
	SG_portability_item * pItem;

	SG_NULLARGCHECK_RETURN(pDir);
	SG_NULLARGCHECK_RETURN(pbIsDuplicate);
	// ppVoidAssocData_Original is optional

	// allocate an Item for this entryname and add it to our assoc-array of items.
	// if the entryname exactly matches something that we have already processed,
	// we have a trivial collision.

	SG_ERR_CHECK(  _sg_portability_item__alloc(pCtx,
											   pszGid_Entry,
											   pDir->szRelativePath,szEntryName,
											   pDir->pConverterRepoCharSet,
											   bCheckMyName_Indiv,bCheckMyName_Collision,
											   pVoidAssocData,pvaLog,
											   &pItemAllocated)  );

	_sg_portability_dir__add_item_to_list(pCtx,pDir,pItemAllocated);
	if (SG_context__has_err(pCtx))
	{
		if (SG_context__err_equals(pCtx,SG_ERR_RBTREE_DUPLICATEKEY))
			goto report_duplicate;

		SG_ERR_RETHROW;
	}

	pItem = pItemAllocated;		// we can still reference the item,
	pItemAllocated = NULL;		// but we no longer own it.

	SG_ERR_CHECK(  _sg_portability_dir__inspect_item(pCtx,pDir,pItem)  );
	SG_ERR_CHECK(  _sg_portability_dir__collide_item(pCtx,pDir,pItem)  );
	*pbIsDuplicate = SG_FALSE;

	return;

report_duplicate:
	SG_context__err_reset(pCtx);
	*pbIsDuplicate = SG_TRUE;
	if (ppVoidAssocData_Original)
		SG_ERR_CHECK(  SG_portability_dir__get_item_result_flags__with_assoc(pCtx,pDir,szEntryName,
																			 NULL,NULL,
																			 ppVoidAssocData_Original)  );
fail:
	SG_ERR_IGNORE(  _sg_portability_item__free(pCtx, pItemAllocated)  );
}

//////////////////////////////////////////////////////////////////

void SG_portability_dir__get_result_flags(SG_context * pCtx,
										  SG_portability_dir * pDir,
										  SG_portability_flags * pFlagsWarningsAll,
										  SG_portability_flags * pFlagsWarningsChecked,
										  SG_portability_flags * pFlagsWarningsLogged)
{
	SG_NULLARGCHECK_RETURN(pDir);

	if (pFlagsWarningsAll)
		*pFlagsWarningsAll = pDir->portWarningsAll;

	if (pFlagsWarningsChecked)
		*pFlagsWarningsChecked = pDir->portWarningsChecked;

	if (pFlagsWarningsLogged)
		*pFlagsWarningsLogged = (pDir->portWarningsChecked & pDir->portLogMask);
}

void SG_portability_dir__get_item_result_flags(SG_context * pCtx,
											   SG_portability_dir * pDir, const char * szEntryName,
											   SG_portability_flags * pFlagsWarnings,
											   SG_portability_flags * pFlagsLogged)
{
	SG_ERR_CHECK_RETURN(  SG_portability_dir__get_item_result_flags__with_assoc(pCtx,
																				pDir,
																				szEntryName,
																				pFlagsWarnings,
																				pFlagsLogged,
																				NULL)  );
}

void SG_portability_dir__get_item_result_flags__with_assoc(SG_context * pCtx,
														   SG_portability_dir * pDir,
														   const char * szEntryName,
														   SG_portability_flags * pFlagsWarnings,
														   SG_portability_flags * pFlagsLogged,
														   void ** ppVoidAssocData)
{
	SG_portability_item * pItem;

	SG_NULLARGCHECK_RETURN(pDir);

	SG_ERR_CHECK_RETURN(  SG_rbtree__find(pCtx, pDir->prbItems,szEntryName,NULL,(void **)&pItem)  );

	if (pFlagsWarnings)
		*pFlagsWarnings = pItem->portWarnings;

	if (pFlagsLogged)
		*pFlagsLogged = (pItem->portWarnings & pDir->portLogMask);

	if (ppVoidAssocData)
		*ppVoidAssocData = pItem->pVoidAssocData;
}

//////////////////////////////////////////////////////////////////

void SG_portability_dir__foreach_with_issues(SG_context * pCtx,
											 SG_portability_dir * pDir,
											 SG_bool bAnyWarnings,
											 SG_portability_dir_foreach_callback * pfn,
											 void * pVoidCallerData)
{
	SG_rbtree_iterator * pIter = NULL;
	SG_portability_item * pItem;
	const char * pszEntryName;
	SG_bool bFound;

	SG_NULLARGCHECK_RETURN(pDir);
	SG_NULLARGCHECK_RETURN(pfn);

	SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pIter,pDir->prbItems,&bFound,&pszEntryName,(void **)&pItem)  );
	while (bFound)
	{
		// if they want "any warnings" notify them of anything with a flag set.
		// otherwise, they only want notifications for things we logged.

		if ((bAnyWarnings && pItem->portWarnings) || (pItem->portWarnings & pDir->portLogMask))
			SG_ERR_CHECK(  (*pfn)(pCtx,
								  pszEntryName,
								  pItem->portWarnings,
								  (pItem->portWarnings & pDir->portLogMask),
								  pItem->pVoidAssocData,
								  pVoidCallerData)  );

		SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pIter,&bFound,&pszEntryName,(void **)&pItem)  );
	}

	// fall thru

fail:
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIter);
}
