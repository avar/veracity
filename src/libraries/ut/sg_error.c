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

// src/libraries/ut/sg_error.c
// Error message-related functions.
//////////////////////////////////////////////////////////////////

#include <sg.h>

//////////////////////////////////////////////////////////////////

void SG_error__get_message(SG_error err,
							 char * pBuf,
							 SG_uint32 lenBuf)
{
	// fetch error message for given error code.
	//
	// we try to be absolutely minimalist so that have high probability
	// of success (in a low-free-memory environment, for example).  therefore,
	// we only copy (with truncatation) into the buffer provided and don't
	// do a lot of fancy string tricks.
	//
	// we return a pointer to the buffer supplied (so caller can call us
	// in the arg list of a printf, for example).

	if (!pBuf || (lenBuf==0))  // if no buffer given, or zero length
		return;                // we can't do anything.

	memset(pBuf,0,lenBuf*sizeof(*pBuf));

	if (SG_IS_OK(err))			// TODO should we return "No error."
		return;

	// we have multiple ranges of error codes and each gets its message
	// text from a different source.  see sg_error.h

	switch (SG_ERR_TYPE(err))
	{
	case __SG_ERR__ERRNO__:			// an errno-based system error.
		{
			char bufErrnoMessage[SG_ERROR_BUFFER_SIZE+1];
			int errCode;

			errCode = SG_ERR_ERRNO_VALUE(err);

#if defined(WINDOWS)
			strerror_s(bufErrnoMessage,SG_ERROR_BUFFER_SIZE+1,errCode);	// use secure-crt-save version
			strncpy_s(pBuf,lenBuf,bufErrnoMessage,_TRUNCATE);
#endif
#if defined(MAC)
			strerror_r(errCode,bufErrnoMessage,SG_ERROR_BUFFER_SIZE+1);	// use thread-safe version
			strncpy(pBuf,bufErrnoMessage,lenBuf-1);
#endif
#if defined(LINUX)
#  if defined(__USE_GNU)
			// we get the non-standard GNU version
			strncpy(pBuf,
					strerror_r(errCode,bufErrnoMessage,SG_ERROR_BUFFER_SIZE+1),
					lenBuf-1);
#  else
			// otherwise, we get the XSI version
			strerror_r(errCode,bufErrnoMessage,SG_ERROR_BUFFER_SIZE+1);	// use thread-safe version
			strncpy(pBuf,bufErrnoMessage,lenBuf-1);
#  endif
#endif
			return;
		}

#if defined(WINDOWS)
	case __SG_ERR__GETLASTERROR__:	// a GetLastError() based system error
		{
			DWORD errCode;

			errCode = SG_ERR_GETLASTERROR_VALUE(err);

			FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK,
						   NULL,errCode,0,pBuf,lenBuf-1,NULL);

			return;
		}

	//case __SG_ERR__COM__:
	// TODO...
#endif

	default:
		SG_ASSERT(0 && err);			// should not happen (quiets compiler)

	case __SG_ERR__ZLIB__:
#if defined(WINDOWS)
		_snprintf_s(pBuf,lenBuf,_TRUNCATE,"Error %d (zlib)",SG_ERR_ZLIB_VALUE(err));
#else
		snprintf(pBuf,(size_t)lenBuf,"Error %d (zlib)",SG_ERR_ZLIB_VALUE(err));
#endif
		return;

	case __SG_ERR__SQLITE__:		// an error code from sqlite
#if defined(WINDOWS)
		_snprintf_s(pBuf,lenBuf,_TRUNCATE,"Error %d (sqlite)",SG_ERR_SQLITE_VALUE(err));
#else
		snprintf(pBuf,(size_t)lenBuf,"Error %d (sqlite)",SG_ERR_SQLITE_VALUE(err));
#endif
		return;

	case __SG_ERR__ICU__:			// an ICU error code
#if defined(WINDOWS)
		_snprintf_s(pBuf,lenBuf,_TRUNCATE,"Error %d (icu): %s",
				  SG_ERR_ICU_VALUE(err),
				  SG_utf8__get_icu_error_msg(err));
#else
		snprintf(pBuf,(size_t)lenBuf,"Error %d (icu): %s",
				 SG_ERR_ICU_VALUE(err),
				 SG_utf8__get_icu_error_msg(err));
#endif
		return;

	case __SG_ERR__LIBCURL__:		// a libcurl error code
#if defined(WINDOWS)
		_snprintf_s(pBuf,lenBuf,_TRUNCATE,"Error %d (curl): %s",
			SG_ERR_LIBCURL_VALUE(err),
			curl_easy_strerror(SG_ERR_LIBCURL_VALUE(err)));
#else
		snprintf(pBuf,(size_t)lenBuf,"Error %d (curl): %s",
			SG_ERR_LIBCURL_VALUE(err),
			curl_easy_strerror(SG_ERR_LIBCURL_VALUE(err)));
#endif
		return;



	case __SG_ERR__SG_LIBRARY__:	// a library error
		switch (err)
		{
#if defined(WINDOWS)
#define E(e,m)	case e: _snprintf_s(pBuf,lenBuf,_TRUNCATE,"Error %d (sglib): " m,SG_ERR_SG_LIBRARY_VALUE(e)); return;
#else
#define E(e,m)	case e: snprintf(pBuf,(size_t)lenBuf,"Error %d (sglib): " m,SG_ERR_SG_LIBRARY_VALUE(e)); return;
#endif

			E(SG_ERR_UNSPECIFIED,                                   "Unspecified error");
			E(SG_ERR_INVALIDARG,                                    "Invalid argument");
			E(SG_ERR_NOTIMPLEMENTED,                                "Not implemented");
			E(SG_ERR_UNINITIALIZED,                                 "Uninitialized");
			E(SG_ERR_MALLOCFAILED,                                  "Memory allocation failed");
			E(SG_ERR_INCOMPLETEWRITE,                               "Incomplete write");
			E(SG_ERR_REPO_ALREADY_OPEN,                             "Repository already open");
			E(SG_ERR_NOTAREPOSITORY,                                "Not a repository");
			E(SG_ERR_NOTAFILE,                                      "Not a file");
			E(SG_ERR_COLLECTIONFULL,                                "Collection full");
			E(SG_ERR_USERNOTFOUND,                                  "User not found");
			E(SG_ERR_INVALIDHOMEDIRECTORY,                          "Invalid home directory");
			E(SG_ERR_CANNOTTRIMROOTDIRECTORY,                       "Cannot get parent directory of root directory");
			E(SG_ERR_LIMIT_EXCEEDED,                                "A fixed limit has been exceeded");
			E(SG_ERR_OVERLAPPINGBUFFERS,                            "Overlapping buffers in memory copy");
			E(SG_ERR_REPO_NOT_OPEN,                                 "Repository not open");
			E(SG_ERR_NOMOREFILES,                                   "No more files in directory");
			E(SG_ERR_BUFFERTOOSMALL,                                "Buffer too small");
			E(SG_ERR_INVALIDBLOBOPERATION,                          "Invalid blob operation");
			E(SG_ERR_BLOBVERIFYFAILED,                              "Blob verification failed");
			E(SG_ERR_DBRECORDFIELDNAMEDUPLICATE,                    "Field name already exists");
			E(SG_ERR_DBRECORDFIELDNAMEINVALID,                      "Field name invalid");
			E(SG_ERR_INDEXOUTOFRANGE,                               "Index out of range");
			E(SG_ERR_DBRECORDFIELDNAMENOTFOUND,                     "Field name not found");
			E(SG_ERR_UTF8INVALID,                                   "UTF-8 string invalid");
			E(SG_ERR_XMLWRITERNOTWELLFORMED,                        "This operation would result in non-well-formed XML");
			E(SG_ERR_EOF,                                           "End of File");
			E(SG_ERR_INCOMPLETEREAD,                                "Incomplete read");
			E(SG_ERR_VCDIFF_NUMBER_ENCODING,                        "Invalid vcdiff number encoding");
			E(SG_ERR_VCDIFF_UNSUPPORTED,                            "Unsupported vcdiff feature");
			E(SG_ERR_VCDIFF_INVALID_FORMAT,                         "Invalid vcdiff file");
			E(SG_ERR_DAGLCA_LEAF_IS_ANCESTOR,                       "One of the given leaves is an ancestor of another"); // TODO Jeff: is there a better message here?
			E(SG_ERR_BLOBFILEALREADYEXISTS,                         "Blob file already exists");
			E(SG_ERR_ASSERT,                                        "Assert failed");
			E(SG_ERR_CHANGESETDIRALREADYEXISTS,                     "Changeset directory already exists");
			E(SG_ERR_INVALID_BLOB_DIRECTORY,                        "Invalid blob directory");
			E(SG_ERR_COMPRESSION_NOT_HELPFUL,                       "Compression not helpful");
			E(SG_ERR_INVALID_BLOB_HEADER,                           "The blob header is invalid");
			E(SG_ERR_UNSUPPORTED_BLOB_VERSION,                      "Unsupported Blob Version");
			E(SG_ERR_BLOB_NOT_VERIFIED_INCOMPLETE,                  "The blob could not be verified because it is incomplete");
			E(SG_ERR_BLOB_NOT_VERIFIED_MISMATCH,                    "The blob could not be verified: the calculated HID does not match the stored HID");
			E(SG_ERR_JSONWRITERNOTWELLFORMED,                       "The JSON is not well-formed");
			E(SG_ERR_JSONPARSER_SYNTAX,                             "JSON syntax error in parser");
			E(SG_ERR_VHASH_DUPLICATEKEY,                            "The vhash key already exists");
			E(SG_ERR_VHASH_KEYNOTFOUND,                             "vhash key could not be found");
			E(SG_ERR_VARIANT_INVALIDTYPE,                           "Invalid variant type");
			E(SG_ERR_NOT_A_DIRECTORY,                               "A file was found where a directory was expected");
			E(SG_ERR_VARRAY_INDEX_OUT_OF_RANGE,                     "Varray index out of bounds");
			E(SG_ERR_TREENODE_ENTRY_VALIDATION_FAILED,              "Treenode entry validation failed"); // TODO: better error message?
			E(SG_ERR_BLOB_NOT_FOUND,                                "Blob not found");
			E(SG_ERR_INVALID_WHILE_FROZEN,                          "The requested operation cannot be performed because the object is frozen");
			E(SG_ERR_INVALID_UNLESS_FROZEN,                         "The requested operation cannot be performed unless the object is frozen");
			E(SG_ERR_ARGUMENT_OUT_OF_RANGE,                         "An argument is outside the valid range");
			E(SG_ERR_CHANGESET_VALIDATION_FAILED,                   "Changeset validation failed");
			E(SG_ERR_TREENODE_VALIDATION_FAILED,                    "Treenode validation failed");
			E(SG_ERR_TRIENODE_VALIDATION_FAILED,                    "Trienode validation failed");
			E(SG_ERR_INVALID_DBCRITERIA,                            "Invalid database criteria");  // TODO: better error message?
			E(SG_ERR_DBRECORD_VALIDATION_FAILED,                    "Database record validation failed");
			E(SG_ERR_DAGNODE_ALREADY_EXISTS,                        "The DAG node already exists");
			E(SG_ERR_NOT_FOUND,                                     "The requested object could not be found");
			E(SG_ERR_CANNOT_NEST_DB_TX,                             "Cannot nest database transactions");
			E(SG_ERR_NOT_IN_DB_TX,                                  "The operation cannot be performed outside the scope of a database transaction");
			E(SG_ERR_VTABLE_NOT_BOUND,                              "The vtable is not bound");
			E(SG_ERR_REPO_DB_NOT_OPEN,                              "The repository database is not open");
			E(SG_ERR_DAG_NOT_CONSISTENT,                            "The DAG is inconsistent");
			E(SG_ERR_DB_BUSY,                                       "The repository database is busy");
			E(SG_ERR_CANNOT_CREATE_SPARSE_DAG,                      "Cannot create sparse DAG");  // TODO: change this to refer to unknown parents rather than sparse dags?
			E(SG_ERR_RBTREE_DUPLICATEKEY,                           "The rbtree key already exists");
			E(SG_ERR_RESTART_FOREACH,                               "Restarting foreach"); // TODO: Remove this.  Pretty sure it's an error that shouldn't be.
			E(SG_ERR_SPLIT_MOVE,                                    "Split move");
			E(SG_ERR_PENDINGTREE_OBJECT_ALREADY_EXISTS,             "The file/folder already exists");
			E(SG_ERR_FILE_LOCK_FAILED,                              "File lock failed");
			E(SG_ERR_SYMLINK_NOT_SUPPORTED,                         "Symbolic links are unsupported on this platform");
			E(SG_ERR_USAGE,                                         "Usage error");
			E(SG_ERR_UNMAPPABLE_UNICODE_CHAR,                       "Unmappable unicode character"); // TODO: better error message?
			E(SG_ERR_ILLEGAL_CHARSET_CHAR,                          "Illegal charset character"); // TODO: better error message?
			E(SG_ERR_INVALID_PENDINGTREE_OBJECT_TYPE,               "Invalid pending tree object type");
			E(SG_ERR_PORTABILITY_WARNINGS,                          "There are portability warnings");
			E(SG_ERR_CANNOT_REMOVE_SAFELY,                          "The object cannot be safely removed");
			E(SG_ERR_TOO_MANY_BACKUP_FILES,                         "The number of supported backup files has been exceeded");
			E(SG_ERR_TAG_ALREADY_EXISTS,                            "The tag already exists");
			E(SG_ERR_TAG_CONFLICT,                                  "The tag conflicts with an existing tag");
			E(SG_ERR_AMBIGUOUS_ID_PREFIX,                           "The HID prefix does not uniquely identify a dagnode or blob");
			E(SG_ERR_UNKNOWN_STORAGE_IMPLEMENTATION,                "Unknown repository storage implementation");
			E(SG_ERR_DBNDX_ALLOWS_QUERY_ONLY,                       "The database index is in a query-only state");
			E(SG_ERR_ZIP_BAD_FILE,                                  "Bad zip file");
			E(SG_ERR_ZIP_CRC,                                       "Zip file CRC mismatch");
			E(SG_ERR_DAGFRAG_DESERIALIZATION_VERSION,               "Serialized DAG fragment has an invalid version");
			E(SG_ERR_DAGNODE_DESERIALIZATION_VERSION,               "Serialized DAG node has an invalid version");
			E(SG_ERR_FRAGBALL_INVALID_VERSION,                      "Invalid fragball version");
			E(SG_ERR_FRAGBALL_BLOB_HASH_MISMATCH,                   "Fragball blob hash mismatch");
			E(SG_ERR_MALFORMED_SUPERROOT_TREENODE,                  "The super-root tree node is malformed");
			E(SG_ERR_ABNORMAL_TERMINATION,                          "A spawned process terminated abnormally");
			E(SG_ERR_EXTERNAL_TOOL_ERROR,                           "An external tool reported an error");
			E(SG_ERR_FRAGBALL_INVALID_OP,                           "Invalid op in fragball");
			E(SG_ERR_INCOMPLETE_BLOB_IN_REPO_TX,                    "The pending repository transaction includes an incomplete blob");
			E(SG_ERR_REPO_MISMATCH,                                 "Repository mismatch: the repositories have no common history");
			E(SG_ERR_UNKNOWN_CLIENT_PROTOCOL,                       "Unknown client protocol");
			E(SG_ERR_SAFEPTR_WRONG_TYPE,                            "Safeptr type mismatch");
			E(SG_ERR_SAFEPTR_NULL,                                  "Null safeptr");
			E(SG_ERR_JS,                                            "JavaScript error");
			E(SG_ERR_INTEGER_OVERFLOW,                              "Integer overflow");
			E(SG_ERR_JS_DBRECORD_FIELD_VALUES_MUST_BE_STRINGS,      "Database record fields must be strings"); // TODO: Better error message?
			E(SG_ERR_ALREADY_INITIALIZED,                           "Already initialized");
			E(SG_ERR_LOCAL_SETTING_DOES_NOT_EXIST,                  "Local setting does not exist");
			E(SG_ERR_INVALID_VALUE_FOR_LOCAL_SETTING,               "Invalid value for local setting");
			E(SG_ERR_LOCAL_SETTING_IS_NOT_OF_THE_REQUESTED_TYPE,    "Local setting is not of the requested type");
			E(SG_ERR_CTX_HAS_ERR,                                   "The context object already indicates an error state");
			E(SG_ERR_CTX_NEEDS_ERR,                                 "The context object does not indicate an error state");
			E(SG_ERR_LOGGING_MODULE_IN_USE,                         "Logging module already in use");
			E(SG_ERR_EXEC_FAILED,                                   "Failed to spawn external process");
			E(SG_ERR_MERGE_REQUIRES_UNIQUE_CSETS,                   "Merge requires unique change sets");
			E(SG_ERR_MERGE_REQUESTED_CLEAN_WD,                      "This type of merge requires a clean working directory");
			E(SG_ERR_CANNOT_MOVE_INTO_CURRENT_PARENT,                "You cannot move something into the folder it is already in");
			E(SG_ERR_MERGE_BASELINE_IS_IMPLIED,						"Merge input includes Baseline -- Merge implictly uses baseline");
			E(SG_ERR_URI_HTTP_400_BAD_REQUEST,                      "400 Bad Request");
			E(SG_ERR_URI_HTTP_401_UNAUTHORIZED,                     "401 Unauthorized");
			E(SG_ERR_URI_HTTP_404_NOT_FOUND,                        "404 Not Found");
			E(SG_ERR_URI_HTTP_405_METHOD_NOT_ALLOWED,               "405 Method Not Allowed");
			E(SG_ERR_URI_HTTP_413_REQUEST_ENTITY_TOO_LARGE,         "413 Request Entity Too Large");
			E(SG_ERR_URI_HTTP_500_INTERNAL_SERVER_ERROR,            "500 Internal Server Error");
			E(SG_ERR_URI_POSTDATA_MISSING,                          "POST request needs a message body (aka \"post data\")");
			E(SG_ERR_CANNOT_MOVE_INTO_SUBFOLDER,                    "You cannot move an object into its own subfolder");
			E(SG_ERR_DIR_ALREADY_EXISTS,                            "Directory already exists");
			E(SG_ERR_DIR_PATH_NOT_FOUND,                            "A parent directory in the pathname does not exist");
			E(SG_ERR_WORKING_DIRECTORY_IN_USE,						"The current directory is already a working folder");

			E(SG_ERR_PENDINGTREE_PARTIAL_CHANGE_COLLISION,          "Entryname conflict in partial change");	// TODO give this a better message.
			E(SG_ERR_REPO_BUSY,                                     "Repo too busy");
			E(SG_ERR_PENDINGTREE_PARENT_DIR_MISSING_OR_INACTIVE,    "Parent directory missing or inactive");
			E(SG_ERR_ITEM_NOT_UNDER_VERSION_CONTROL,                "Attempt to operate on an item which is not under version control");

			E(SG_ERR_UNKNOWN_BLOB_TYPE,								"Unknown blob type");

			E(SG_ERR_GETOPT_NO_MORE_OPTIONS,						"Reached end of options");
			E(SG_ERR_GETOPT_BAD_ARG,								"Bad argument");

			E(SG_ERR_GID_FORMAT_INVALID,							"Format of GID not valid");		// GID string has invalid format; does not mean it wasn't found
			E(SG_ERR_UNKNOWN_HASH_METHOD,                           "Unknown hash-method");
			E(SG_ERR_TRIVIAL_HASH_DIFFERENT,                        "Trivial hash different");
			E(SG_ERR_DAGNODES_UNRELATED,							"Changesets are unrelated");
			E(SG_ERR_MULTIPLE_HEADS_FROM_DAGNODE,					"There are multiple heads from this changeset");
			E(SG_ERR_DATE_PARSING_ERROR,							"The date could not be parsed.  Please provide either \"YYYY-MM-DD\" or \"YYYY-MM-DD HH:MM:SS\"");
			E(SG_ERR_INT_PARSE,							            "Invalid integer string");
			E(SG_ERR_TAG_NOT_FOUND,							        "Tag not found");

			E(SG_ERR_STAGING_AREA_MISSING,							"Couldn't find staging area");
			E(SG_ERR_NOT_A_WORKING_COPY,							"There is no working copy here");
			E(SG_ERR_REPO_ID_MISMATCH,								"The repositories are not clones");
			E(SG_ERR_REPO_HASH_METHOD_MISMATCH,						"The repositories don't use the same hash");
			E(SG_ERR_SERVER_HTTP_ERROR,								"The server returned an HTTP error");

			E(SG_ERR_JSONDB_NO_CURRENT_OBJECT,						"No current jsondb object is set");
			E(SG_ERR_JSONDB_INVALID_PATH,							"The jsondb element path is invalid");
			E(SG_ERR_JSONDB_OBJECT_ALREADY_EXISTS,					"The jsondb object already exists");
			E(SG_ERR_JSONDB_PARENT_DOESNT_EXIST,					"The parent object doesn't exist");
			E(SG_ERR_JSONDB_NON_CONTAINER_ANCESTOR_EXISTS,			"Add not possible because a non-container ancestor exists");

			E(SG_ERR_SYNC_ADDS_HEADS,								"The push would increase the number of heads");
			E(SG_ERR_CANNOT_MERGE_SYMLINK_VALUES,					"UPDATE cannot merge symlink values.  Consider COMMIT followed by MERGE");

			E(SG_ERR_PULL_INVALID_FRAGBALL_REQUEST,					"Invalid pull request");
			E(SG_ERR_NO_SUCH_DAG,									"The requested DAG wasn't found");
			E(SG_ERR_BASELINE_NOT_HEAD,								"Baseline not a HEAD");
			E(SG_ERR_INVALID_REPO_NAME,								"The repo name provided is invalid");

			E(SG_ERR_UPDATE_CONFLICT,								"Conflicts prevented UPDATE.  Consider COMMIT followed by MERGE");
			E(SG_ERR_ENTRYNAME_CONFLICT,							"Operation would produce an entryname conflict in result");
			E(SG_ERR_CANNOT_MAKE_RELATIVE_PATH,						"Cannot make a relative path using the given pathnames");
			E(SG_ERR_CANNOT_DO_WHILE_UNCOMMITTED_MERGE,				"Cannot perform operation with an uncommitted MERGE");
			E(SG_ERR_INVALID_REPO_PATH,								"Invalid REPO-PATH");
			E(SG_ERR_MERGE_NEEDS_ANOTHER_CSET,						"MERGE requires at least one changeset to merge with the baseline");
			E(SG_ERR_NOTHING_TO_COMMIT,								"Nothing to COMMIT");
			E(SG_ERR_REPO_FEATURE_NOT_SUPPORTED,					"This storage implementation does not support that feature");
			E(SG_ERR_ISSUE_NOT_FOUND,                               "Issue not found");
			E(SG_ERR_CANNOT_COMMIT_WITH_UNRESOLVED_ISSUES,          "Cannot commit with unresolved merge issues");
			E(SG_ERR_CANNOT_PARTIAL_COMMIT_AFTER_MERGE,             "Cannot do partial commit after merge");
			E(SG_ERR_CANNOT_PARTIAL_REVERT_AFTER_MERGE,             "Cannot do partial revert after merge");
			E(SG_ERR_DUPLICATE_ISSUE,                               "Issue duplicated")
			E(SG_ERR_NO_MERGE_TOOL_CONFIGURED,						"No external merge tool has been configured to handle files like this");
			

			E(SG_ERR_USER_NOT_FOUND,                                "User not found");
			E(SG_ERR_ZING_NO_RECID,                                 "Can't ask for history unless the record has a recid");
			E(SG_ERR_ZING_LINK_MUST_BE_SINGLE,                      "Link must be a single variety");
			E(SG_ERR_ZING_INVALID_RECTYPE_FOR_LINK,		       		"Invalid rectype for link");
			E(SG_ERR_ZING_TYPE_MISMATCH,                            "Type mismatch");
			E(SG_ERR_ZING_LINK_MUST_NOT_BE_SINGLE,		       		"Link must be a collection");
			E(SG_ERR_ZING_INVALID_TEMPLATE,                         "Invalid template");
			E(SG_ERR_ZING_UNKNOWN_ACTION_TYPE,                      "Unknown action type in template");
			E(SG_ERR_ZING_UNKNOWN_BUILTIN_ACTION,		       		"Unknown builtin action");
			E(SG_ERR_ZING_DIRECTED_LINK_CYCLE,                      "Illegal cycle in directed link");
			E(SG_ERR_ZING_CANNOT_DELETE_A_RECORD_WITH_LINKS_TO_IT,  "Cannot delete a record with links to it");
			E(SG_ERR_ZING_ILLEGAL_DURING_COMMIT,		       		"This operation is illegal during a commit");
			E(SG_ERR_ZING_TEMPLATE_NOT_FOUND,                       "Template not found");
			E(SG_ERR_ZING_ONE_TEMPLATE,                             "Only one template allowed");
			E(SG_ERR_ZING_RECORD_NOT_FOUND,                         "Record not found");
			E(SG_ERR_ZING_CONSTRAINT,                               "Constraint violation");
			E(SG_ERR_ZING_FIELD_NOT_FOUND,                          "Field not found");
			E(SG_ERR_ZING_LINK_NOT_FOUND,                           "Link not found");
			E(SG_ERR_ZING_MERGE_CONFLICT,                           "Merge conflict");
			E(SG_ERR_ZING_WHERE_PARSE_ERROR,                        "Syntax error in where");
			E(SG_ERR_ZING_SORT_PARSE_ERROR,                         "Syntax error in sort");
			E(SG_ERR_ZING_RECTYPE_NOT_FOUND,                        "Record type not found");
			E(SG_ERR_NO_CURRENT_WHOAMI,                             "No current user set");
			E(SG_ERR_ZING_NEEDS_MERGE,                              "A database dag has two heads");
			E(SG_ERR_ZING_NO_ANCESTOR,                              "No ancestor found");
			E(SG_ERR_INVALID_USERID,                                "Invalid userid");
			E(SG_ERR_WRONG_DAG_TYPE,                                "Wrong DAG type");

			// Add new error messages above this comment... Also, note: When
			// SG_context__err_to_string constructs a more specific/complete report,
			// it will append either a period or a colon to the end of the message
			// (depending on whether more details follow). For that reason, we put
			// no punctuation at the end here (test u0000_error_message even checks
			// for this).
#undef E

		default:
#if defined(WINDOWS)
			_snprintf_s(pBuf,lenBuf,_TRUNCATE,"Error %d (sglib)",(int)err);
#else
			snprintf(pBuf,(size_t)lenBuf,"Error %d (sglib)",(int)err);
#endif
			return;
		}
	}
}

//////////////////////////////////////////////////////////////////
// SG_assert__release() is used to generate an SG_ERR_ASSERT and
// let the program continue.  It ***DOES NOT*** call the CRT abort()
// or assert() routines/macros.
//
// These are defined in both DEBUG and RELEASE builds and always do
// the same thing.

#if defined(MAC)

void SG_assert__release(SG_context* pCtx, const char * szExpr, const char * szFile, int line)
{
	SG_context__err(pCtx, SG_ERR_ASSERT, szFile, line, szExpr); // set a breakpoint on this to catch assertions.
}

#elif defined(LINUX)

void SG_assert__release(SG_context* pCtx, const char * szExpr, const char * szFile, int line)
{
	SG_context__err(pCtx, SG_ERR_ASSERT, szFile, line, szExpr); // set a breakpoint on this to catch assertions.
}

#elif defined(WINDOWS)

void SG_assert__release(SG_context* pCtx, const char * szExpr, const char * szFile, int line)
{
	SG_context__err(pCtx, SG_ERR_ASSERT, szFile, line, szExpr); // set a breakpoint on this to catch assertions.
}

#endif // WINDOWS

//////////////////////////////////////////////////////////////////
// SG_assert__debug() is used ***IN DEBUG BUILDS ONLY*** to wrap the
// the call to abort() and/or assert().  We do this so that we don't
// get bogus false-positives on code coverage throughout the code.
//
// The problem is that assert() usually has a test within it and gcov/lcov/zcov
// sees it and reports that we never took the assert and skews our numbers.
// We use this function to hide that so that the code doing the SG_ASSERT()
// doesn't get dinged.
#if defined(DEBUG)
void SG_assert__debug(SG_bool bExpr, const char * pszExpr, const char * pszFile, int line)
{
	if (bExpr)
		return;

	fprintf(stderr,"SG_assrt__debug: [%s] on %s:%d\n",pszExpr,pszFile,line);

	// let platform assert() take care of hooking into the debugger and/or stopping the program.

#if defined(MAC) || defined(LINUX)
	assert(bExpr);
#elif defined(WINDOWS)
	_ASSERTE(bExpr);
#if defined(SG_REAL_BUILD)
	/* On the Windows build machine, assert failures don't raise a dialog.
	 * See the calls to _CrtSetReportMode in SG_lib__global_initialize for
	 * where this behavior is set up.
	 *
	 * We abort here to ensure the build reports failure. */
	abort();
#endif
#endif
}
#endif
