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
 * @file sg_typedefs.h
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_TYPEDEFS_H
#define H_SG_TYPEDEFS_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

struct _sg_rev_tag
{
	SG_bool bRev;
	char * pszRevTag;
};

typedef struct _sg_rev_tag SG_rev_tag_obj;

struct _sg_option_state
{
	SG_int32* paRecvdOpts;
	SG_uint32 count_options;

    const char * const * ppExcludes;
	SG_uint32 iCountExcludes;
    const char * const * ppIncludes;
	SG_uint32 iCountIncludes;
	SG_uint32 iCountRevs;
	SG_uint32 iCountTags;
	SG_int32 iPort;
	SG_int32 iSPort;
	SG_int32 iUnified;
	SG_int32 iMaxHistoryResults;

	char* psz_cert;
	char* psz_changeset;
	char* psz_comment;
	SG_varray* pva_dagnums;
	char* psz_dest_repo;
	SG_stringarray* psa_excludes;
	SG_stringarray* psa_includes;
	char* psz_message;
	char* psz_password;
	char* psz_src_repo;
	char* psz_repo;
	char* psz_username;
	char* psz_from_date;
	char* psz_to_date;
	SG_stringarray* psa_stamps;

	SG_vector* pvec_rev_tags;

	SG_bool bAll;
	SG_bool bAllBut;
	SG_bool bClean;
	SG_bool bForce;
	SG_bool bGlobal;
	SG_bool bIgnoreWarnings;
	SG_bool bIgnoreIgnores;
	SG_bool bList;
	SG_bool bListAll;
	SG_bool bMark;
	SG_bool bMarkAll;
	SG_bool bNoPlain;
	SG_bool bPromptForDescription;
	SG_bool bRecursive;
	SG_bool bRemove;
	SG_bool bAdd;
    SG_bool bSubgroups;
	SG_bool bShowAll;
	SG_bool bTest;
//	SG_bool bUnmark;
//	SG_bool bUnmarkAll;
	SG_bool bUpdate;
	SG_bool bVerbose;
	SG_bool bLeaves;

	SG_stringarray* psa_assocs;
    const char* const* ppAssocs;
	SG_uint32 iCountAssocs;
};

typedef struct _sg_option_state SG_option_state;

//////////////////////////////////////////////////////////////////

#define CMD_FUNC_PARAMETERS SG_context * pCtx,const char* pszAppName,const char* pszCommandName,SG_getopt* pGetopt,SG_option_state* pOptSt,SG_bool *pbUsageError

typedef void cmdfunc(CMD_FUNC_PARAMETERS);

#define DECLARE_CMD_FUNC(name) void cmd_##name(CMD_FUNC_PARAMETERS)

//////////////////////////////////////////////////////////////////

/**
 * An arbitrary value.  No individual command should define more arg flags than this.
 * This only covers how many unique flags can be defined for a command; it does not
 * refer to any limits on the number of flags that may be typed on a command line
 * (because some flags (like --include) can be repeated without limit).
 */
#define SG_CMDINFO_OPTION_LIMIT		50

struct cmdinfo
{
	const char* name;
	const char* alias1;
	const char* alias2;
	const char* alias3;
	cmdfunc* fn;
	const char* desc;
	const char *extraHelp; /**< additional text to display after "usage: vv cmdname " */

	SG_bool advanced; /* command shows up only in --all command lists */

	/* TODO maybe this is the place to put full usage info? */

	SG_int32 valid_options[SG_CMDINFO_OPTION_LIMIT];

	/** A list of option help descriptions, keyed by the option unique enum
	* (the 2nd field in sg_getopt_option), which override the generic
	* descriptions given in an sg_getopt_option on a per-command basis.
	*/
	struct { int optch; const char *desc; } desc_overrides[SG_CMDINFO_OPTION_LIMIT];
};

typedef struct cmdinfo cmdinfo;


//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_TYPEDEFS_H
