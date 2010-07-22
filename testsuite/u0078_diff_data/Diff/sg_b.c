/*
  vv
  
  The Sprawl command-line application
  Copyright 2008 SourceGear LLC
  All Rights Reserved
*/

#include <sg.h>
#include <signal.h>
#include "../libraries/consolelib/sg_getopt.h"
#include "sg_typedefs.h"
#include "sg_prototypes.h"
#include "../libraries/mongoose/sg_mongoose.h"

#define	WHATSMYNAME	"vv"

/* For every command, provide a forward declaration of the cmd func
 * here, so that its function pointer can be included in the lookup
 * table. */

DECLARE_CMD_FUNC(add);
DECLARE_CMD_FUNC(addremove);
DECLARE_CMD_FUNC(cat);
DECLARE_CMD_FUNC(checkout);
DECLARE_CMD_FUNC(clone);
DECLARE_CMD_FUNC(createuser);
DECLARE_CMD_FUNC(creategroup);
DECLARE_CMD_FUNC(users);
DECLARE_CMD_FUNC(groups);
DECLARE_CMD_FUNC(group);
DECLARE_CMD_FUNC(log);
DECLARE_CMD_FUNC(comment);
DECLARE_CMD_FUNC(commit);
DECLARE_CMD_FUNC(deleterepo);
DECLARE_CMD_FUNC(diff);
DECLARE_CMD_FUNC(heads);
DECLARE_CMD_FUNC(help);
DECLARE_CMD_FUNC(history);
DECLARE_CMD_FUNC(listrepos);
DECLARE_CMD_FUNC(localsetting);
DECLARE_CMD_FUNC(localsettings);
DECLARE_CMD_FUNC(merge);
DECLARE_CMD_FUNC(move);
DECLARE_CMD_FUNC(new);
DECLARE_CMD_FUNC(parents);
DECLARE_CMD_FUNC(pull);
DECLARE_CMD_FUNC(push);
DECLARE_CMD_FUNC(remove);
DECLARE_CMD_FUNC(rename);
DECLARE_CMD_FUNC(resolve);
DECLARE_CMD_FUNC(revert);
DECLARE_CMD_FUNC(scan);
DECLARE_CMD_FUNC(serve);
#if 0
DECLARE_CMD_FUNC(slurp);
#endif
DECLARE_CMD_FUNC(stamp);
DECLARE_CMD_FUNC(stamps);
DECLARE_CMD_FUNC(status);
DECLARE_CMD_FUNC(tag);
DECLARE_CMD_FUNC(tags);
DECLARE_CMD_FUNC(update);
DECLARE_CMD_FUNC(version);
DECLARE_CMD_FUNC(whoami);
#if 0
DECLARE_CMD_FUNC(fragball);
#endif

//advanced commands
DECLARE_CMD_FUNC(zingmerge);
DECLARE_CMD_FUNC(blobcount);
DECLARE_CMD_FUNC(dump_json);
DECLARE_CMD_FUNC(dump_dag);
DECLARE_CMD_FUNC(dump_lca);
#ifdef DEBUG
DECLARE_CMD_FUNC(dump_treenode);
#endif
DECLARE_CMD_FUNC(hid);
DECLARE_CMD_FUNC(unpack);
DECLARE_CMD_FUNC(vpack);
DECLARE_CMD_FUNC(zpack);
DECLARE_CMD_FUNC(vacuum);
DECLARE_CMD_FUNC(vcdiff);

// enum for options that only have a "long" form
// options with a short form will be identified by the single char
typedef enum
{
	opt_cert = 256,
	opt_changeset,
	opt_comment,
	opt_verbose,
	opt_exclude_from,
	opt_from,
	opt_global,
	opt_ignore_portability_warnings,
	opt_include_from,
	opt_leaves,
	opt_list,
	opt_list_all,
	opt_mark,
	opt_mark_all,
	opt_max,
	opt_no_plain,
	opt_port,
	opt_remove,
	opt_repo,
	opt_show_all,
	opt_sport,
	opt_stamp,
	opt_tag,
	opt_to,
	opt_unmark,
	opt_unmark_all,
	opt_all,
	opt_all_but,
    opt_add,
    opt_subgroups,
} sg_cl__long_option;

struct sg_getopt_option sg_cl_options[] =
{
    {"allcommands",  opt_show_all, 0, "Include normally hidden, debugging-oriented commands such as blobcount, etc."},
    {"add",          opt_add,      0, "Add"},
    {"all",          opt_all,      0, "Include all files and folders recursively."},
	{"all-but",      opt_all_but,  0, "Include all items except for those listed."},
    {"assoc",        'a',          1, "Associate work item or wiki page."},
    {"cert",         opt_cert,     1, "Full or relative path of certificate (.PEM) file to use in SSL encryption. Defaults to ./ssl_cert.pem"},
    {"changeset",    opt_changeset,1, "Changeset ID used to narrow down the file versions"},
    {"clean",        'C',          0, "Replace working folder contents with baseline repository contents. "},
    {"comment",      opt_comment,  1, "Comment"},
    {"exclude",      'X',          1, "Exclude files matching the given (extended glob) pattern. May be used multiple times."},
    {"exclude-from", opt_exclude_from, 1, ""},
    {"force",        'F',          0, "Force"},
    {"from",         opt_from,     1, "Earliest date included in the history query"},
    {"global",       opt_global,   0, "Apply change to the Machine Default of the setting (in addition to the current Working Folder, if applicable)."},
    {"ignore-portability-warnings", opt_ignore_portability_warnings, 0, "Ignore portability warnings"},
    {"include",      'I',          1, "Include only files matching the given (extended glob) pattern. May be used multiple times."},
    {"include-from", opt_include_from, 1, ""},
    {"leaves",       opt_leaves,   0, "Include history starting at all the leaf nodes in the dag."},
	{"list",         opt_list,     0, "List unresolved issues."},
	{"listall",      opt_list_all, 0, "List resolved and unresolved issues."},
	{"mark",         opt_mark,     1, "Manually mark issue as resolved."},
	{"markall",      opt_mark_all, 0, "Manually mark all issues as resolved."},
    {"message",      'm',          1, "Comment"},
    {"max",          opt_max,      1, "Maximum number of results to include in the history query."},
    {"nonrecursive", 'N',          0, "Do not recurse"},
    {"noplain",      opt_no_plain, 0, "Serve encrypted content only."}, 
    {"port",         opt_port,     1, "The port to use for plaintext content. Defaults to 8080"},
    {"remove",       opt_remove,   0, "Remove, rather than add, tags to files/folders/changesets."},
    {"repo",         opt_repo,     1, "The repo from which we will pull file data from"},
    {"rev",          'r',          1, "CHANGESETID to use when selecting file versions"},
    {"sport",        opt_sport,    1, "The port to use for encrypted content. Must not be the same as the implicit or explicit value of --port"},
    {"stamp",        opt_stamp,    1, "Stamp to add. May be used more than once. "},
    {"subgroups",    opt_subgroups,    0, "The members to be added are names of subgroups rather than names of users"},
    {"tag",          opt_tag,      1, "Pull data from files tagged with this TAGID. It is an error if one or more specified files is not tagged with this ID."},
    {"test",         'T',          0, "Don't actually perform the action, just show what would be done."},
    {"to",           opt_to,       1, "Latest date included in the history query"},
    {"unified",      'u',          1, "number of lines of context to print around diffs"},
	{"unmark",       opt_unmark,       1, "Manually mark issue as not resolved."},
	{"unmarkall",    opt_unmark_all,   0, "Manually mark all issues as not resolved."},
    {"user",         'U',          1, "Specify the user committing these changes. Without this parameter, " WHATSMYNAME " will fall back on the default user"},
    {"verbose",      opt_verbose,  0, "Show extra, low-level status details"},

    {0,               0, 0, 0},
};

/* Now the lookup table. */

struct cmdinfo commands[] =
{
	{
		"add", NULL, NULL, NULL, cmd_add, 
		"Add a file or directory", "", 
		SG_FALSE, {'N', 'T'/*, 'I', 'X', opt_include_from, opt_exclude_from*/}, { {0, ""} }
	},
	
	{
		"addremove", NULL, NULL, NULL, cmd_addremove, 
		"Scan for files that need to be added or removed", "", 
		SG_FALSE, {'N', 'T'/*, 'I', 'X', opt_include_from, opt_exclude_from*/}, { {0, ""} }
	},

	{
		"blobcount", NULL, NULL, NULL, cmd_blobcount, 
		"Show the number of blobs in a repo", "repo_name", 
		SG_TRUE, {0}, { {0, ""} }
	},

	{
		"cat", NULL, NULL, NULL, cmd_cat, 
		"Unix-style cat on given files", "[--repo=REPOSITORY] [--rev=CHANGESETID | --tag=TAG]  file [...]", 
		SG_FALSE, {opt_repo, 'r', opt_tag}, { {0, ""} }
	},

	{
		"checkout", NULL, NULL, NULL, cmd_checkout, 
		"Create a working directory for a repo and fill it up", "[--rev=CHANGESETID | --tag=TAGID] REPOSITORY path", 
		SG_FALSE, {'r', opt_tag}, { {0, ""} }  
	},

	{
		"clone", NULL, NULL, NULL, cmd_clone, 
		"Create a new repo with the same ID as an existing one", "existing_repo_name new_repo_name", 
		SG_FALSE, {0}, { {0, ""} }
	},
		
	{
		"createuser", NULL, NULL, NULL, cmd_createuser, 
		"Create a new user account", "[--repo=REPOSITORY] email_address", 
		SG_FALSE, {opt_repo}, { {0, ""} }
	},
		
	{
		"creategroup", NULL, NULL, NULL, cmd_creategroup, 
		"Create a new user group", "[--repo=REPOSITORY] group_name", 
		SG_FALSE, {opt_repo}, { {0, ""} }
	},
		
	{
		"comment", NULL, NULL, NULL, cmd_comment, 
		"Add a comment to a changeset", "(--rev=CHANGESET | --tag=TAG) [--message=message] [REPOSITORY]", 
		SG_FALSE, {'r', 'm', opt_tag}, { {0, ""} }
	},
		
	{
		"commit", NULL, NULL, NULL, cmd_commit, 
		"Commit the pending changeset", "[local paths of things to be committed]", 
		SG_FALSE, {'U', 'P', 'N', 'm'/*, 'I', 'X', opt_include_from, opt_exclude_from*/, opt_stamp, 'T', 'a'}, { {0, ""} }
	},
		
	{
		"diff", NULL, NULL, NULL, cmd_diff, 
		"Diff", "", 
		SG_FALSE, {'r', opt_tag, 'u'/*, 'I', 'X', opt_include_from, opt_exclude_from*/}, { {0, ""} }
	},
		
	{
		"deleterepo", "rmrepo", "removerepo", NULL, cmd_deleterepo, 
		"Delete a repository from your computer", "repo [repo2 ...]", 
		SG_FALSE, {opt_all_but, 'F'}, { {'F', "Do not prompt to confirm (only applicable when using '--all-but')."}, {0, ""} }
	},

	{
		"dump_json", NULL, NULL, NULL, cmd_dump_json, 
		"Write a repository blob to stdout in JSON format", "blobid", 
		SG_TRUE, {0}, { {0, ""} }
	},

	{
		"zingmerge", NULL, NULL, NULL, cmd_zingmerge, 
		"Attempt automatic merge of all zing dags", "[--repo=REPOSITORY]", 
		SG_TRUE, {opt_repo}, { {0, ""} }
	},

	{
		"dump_dag", NULL, NULL, NULL, cmd_dump_dag, 
		"Write the DAG to stdout in graphviz dot format", "", 
		SG_TRUE, {0}, { {0, ""} }
	},

	{
		"dump_lca", NULL, NULL, NULL, cmd_dump_lca, 
		"Write the DAG LCA to stdout in graphviz dot format", "", 
		SG_TRUE, {0}, { {0, ""} }
	},

#ifdef DEBUG
	{
		"dump_treenode", NULL, NULL, NULL, cmd_dump_treenode, 
		"Dump the contents of the treenode for the given changeset", "[changesetid]", 
		SG_TRUE, {'r', opt_tag}, { {0, ""} }
	},
#endif

	{
		"group", NULL, NULL, NULL, cmd_group, 
		"Manage membership of a group", "group_name [ --add|--remove [--subgroups] member_name+ ]", 
		SG_FALSE, {opt_repo, opt_add, opt_remove, opt_subgroups}, { {opt_add, "Add members to the specified group"}, {opt_remove, "Remove members from the specified group"} }
	},
		
	{
		"groups", NULL, NULL, NULL, cmd_groups, 
		"List groups", "[--repo=REPOSITORY]", 
		SG_FALSE, {opt_repo}, { {0, ""} }
	},

	{
		"heads", "leaves", "tips", NULL, cmd_heads, 
		"List HIDs of head/leaf CSETs", "", 
		SG_FALSE, {0}, { {0, ""} }
	},

	{
		"help", NULL, NULL, NULL, cmd_help, 
		"List all supported commands", "[command name]", 
		SG_FALSE, {opt_show_all}, { {0, ""} }
	},

	{
		"hid", NULL, NULL, NULL, cmd_hid, 
		"Print the hash of a file", "filename", 
		SG_TRUE , {0}, { {0, ""} }
	},

	{
		"history", "log", NULL, NULL, cmd_history, 
		"Show the history of the repository", "", 
		SG_FALSE, {'r', opt_tag, 'U', opt_max, opt_from, opt_to, opt_leaves, opt_stamp}, { {0, ""} }
	},

	{
        "listrepos", NULL, NULL, NULL, cmd_listrepos, 
		"Print a list of all local repositories", "", 
		SG_FALSE , {0}, { {0, ""} }
	},

	{
		"localsetting", "set", NULL, NULL, cmd_localsettings, 
		"View or edit a local setting", "", 
		SG_FALSE, { opt_verbose}, { {0, ""} }
	},

	{
		"localsettings", NULL, NULL, NULL, cmd_localsettings, 
		"Print a list of all local settings", "[setting_name [=setting_value | reset]]", 
		SG_FALSE, { opt_verbose}, { {0, ""} }
	},
	
	{
		"merge", NULL, NULL, NULL, cmd_merge, 
		"Merges branches", "", 
		SG_FALSE, {'r', opt_tag, 'T', opt_ignore_portability_warnings, opt_verbose}, { {0, ""} }
	},

	{
		"move", "mv", NULL, NULL, cmd_move, 
		"Move a file or directory", "item_to_be_moved new_directory", 
		SG_FALSE, {'F'}, { {0, ""} }
	},

	{
		"new", "init", NULL, NULL, cmd_new, 
		"Create a new repo with cwd as its working directory", "new_descriptor_name", 
		SG_FALSE, {0}, { {0, ""} }
	},

	{
		"parents", NULL, NULL, NULL, cmd_parents, 
		"List the working directory's parent changsets", "", 
		SG_FALSE, {'r', opt_tag}, { {0, ""} }
	},	

	{
		"pull", NULL, NULL, NULL, cmd_pull, 
		"Pull changes from another repository instance", "source_repo", 
		SG_FALSE, {0}, { {0, ""} }
	},

	{
		"push", NULL, NULL, NULL, cmd_push, 
		"Push committed changes to another repository instance", "destination", 
		SG_FALSE, {'r', opt_tag, 'F'}, { {0, ""} }
	},

	{
		"remove", "rm", "delete", "del", cmd_remove, 
		"Remove an item from the repository", "[local path of things to be removed]", 
		SG_FALSE, {'T'/*, 'I', 'X', opt_include_from, opt_exclude_from*/}, { {0, ""} }
	},
		
	{
		"rename", "ren", NULL, NULL, cmd_rename, 
		"Rename something in the pending tree", "[--force] item_to_be_renamed new_name", 
		SG_FALSE, {'F'}, { {0, ""} }
	},

	{
		"resolve", NULL, NULL, NULL, cmd_resolve,
		"Resolve merge conflicts.", "",
		SG_FALSE, {opt_list, opt_list_all, opt_mark, opt_mark_all, opt_unmark, opt_unmark_all}, { {0, ""} }
	},

	{
		"revert", NULL, NULL, NULL, cmd_revert, 
		"Revert", "[local paths of things to be reverted]", 
		SG_FALSE, {'N', 'T'/*, 'I', 'X', opt_include_from, opt_exclude_from*/, opt_all, opt_verbose}, { {0, ""} }
	},

	{
		"scan", NULL, NULL, NULL, cmd_scan, 
		"Scan the working directory for changed files", "", 
		SG_FALSE, {'F'}, { {0, ""} }
	},

	{
		"serve", "server", NULL, NULL, cmd_serve, 
		"Run as a server", "", 
		SG_FALSE, {opt_port, opt_sport, opt_no_plain, opt_cert}, { {0, ""} }
	},

#if 0
	{
		"slurp", NULL, NULL, NULL, cmd_slurp, 
		"Import a fragball file into a repo", "",
		SG_FALSE, {0}, { {0, ""} }
	},
#endif

	{
		"stamp", NULL, NULL, NULL, cmd_stamp, 
		"Add or remove stamps on changesets", "",
		SG_FALSE, { opt_remove, opt_stamp, opt_tag, 'r'}, { {0, ""} }
	},

	{
		"stamps", NULL, NULL, NULL, cmd_stamps,
		"List the stamps in a repository", "",
		SG_FALSE, {0 }, { {0, ""} }
	},

	{
		"status", "st", NULL, NULL, cmd_status, 
		"Show status of the working directory", "", 
		SG_FALSE, {'N'/*, 'I', 'X', opt_include_from, opt_exclude_from*/, opt_verbose}, { {0, ""} }
	},
		
	{
		"tag", "label", NULL, NULL, cmd_tag, 
		"Apply a symbolic name to a changeset", "[changesetid comment]", 
		SG_FALSE, {'F', 'r', opt_remove, opt_tag}, { {opt_tag, "Existing tag identifying a changeset to which the new tag will be added"} }
	},

	{
		"tags", NULL, NULL, NULL, cmd_tags, 
		"Lists tags in use in a repository instance", "", 
		SG_FALSE, {0}, { {0, ""} }
	},

	{
		"unpack", NULL, NULL, NULL, cmd_unpack, 
		"uncompress/undeltify blobs", "", 
		SG_TRUE, {0}, { {0, ""} }
	},

	{
		"update", NULL, NULL, NULL, cmd_update, 
		"Update the working directory contents to match a changeset in the repository", "", 
		SG_FALSE, {'r', opt_tag, 'F', 'T', opt_ignore_portability_warnings, opt_verbose }, { {0, ""} }
	},

	{
		"users", NULL, NULL, NULL, cmd_users, 
		"List user accounts", "[--repo=REPOSITORY]", 
		SG_FALSE, {opt_repo}, { {0, ""} }
	},

	{
		"vcdiff", NULL, NULL, NULL, cmd_vcdiff, 
		"Encode or decode binary file deltas", "deltify|undeltify basefile targetfile deltafile", 
		SG_TRUE, {0}, { {0, ""} }
	},

	{
		"version", NULL, NULL, NULL, cmd_version, 
		"Report the current version.", "", 
		SG_TRUE, {0}, { {0, ""} }
	},

	{
		"whoami", NULL, NULL, NULL, cmd_whoami, 
		"Set or print current user account", "[--repo=REPOSITORY] email_address", 
		SG_FALSE, {opt_repo}, { {0, ""} }
	},
		
	{
		"vpack", NULL, NULL, NULL, cmd_vpack, 
		"vcdiff a repo's blobs", "repo_name", 
		SG_TRUE, {0}, { {0, ""} }
	},

	{
		"vacuum", NULL, NULL, NULL, cmd_vacuum, 
		"vacuum", "repo_name", 
		SG_TRUE, {0}, { {0, ""} }
	},
		
#if 0
	{
		"fragball", NULL, NULL, NULL, cmd_fragball, 
		"Package a changeset as a fragball file", "", 
		SG_FALSE, {0}, { {0, ""} }
	},
#endif

	{
		"zpack", NULL, NULL, NULL, cmd_zpack, 
		"zlib a repo's blobs", "repo_name", 
		SG_TRUE, {0}, { {0, ""} }
	}


};

const SG_uint32 VERBCOUNT = (sizeof(commands) / sizeof(struct cmdinfo));


/* ---------------------------------------------------------------- */
/* Any functions which are needed to implement the commands go here */
/* ---------------------------------------------------------------- */

static void _get_descriptor_name_from_cwd(SG_context* pCtx, SG_string** ppstrDescriptorName, SG_pathname** ppPathCwd)
{
	SG_pathname* pPathCwd = NULL;
	SG_string* pstrRepoDescriptorName = NULL;

	SG_ERR_CHECK(  SG_PATHNAME__ALLOC(pCtx, &pPathCwd)  );
	SG_ERR_CHECK(  SG_pathname__set__from_cwd(pCtx, pPathCwd)  );

	SG_workingdir__find_mapping(pCtx, pPathCwd, NULL, &pstrRepoDescriptorName, NULL);
	SG_ERR_REPLACE(SG_ERR_NOT_FOUND, SG_ERR_NOT_A_WORKING_COPY);
	SG_ERR_CHECK_CURRENT;

	SG_RETURN_AND_NULL(pstrRepoDescriptorName, ppstrDescriptorName);
	SG_RETURN_AND_NULL(pPathCwd, ppPathCwd);

	/* fall through */
fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathCwd);
	SG_STRING_NULLFREE(pCtx, pstrRepoDescriptorName);
}

/**
 * "Throws" SG_ERR_NOT_A_WORKING_COPY if the cwd's not inside a working copy.
 */
static void _get_repo_from_cwd(SG_context* pCtx, SG_repo** ppRepo, SG_pathname** ppPathCwd)
{
	SG_string* pstrRepoDescriptorName = NULL;
	SG_vhash* pvhDescriptor = NULL;
	SG_repo* pRepo = NULL;

	SG_ERR_CHECK(  _get_descriptor_name_from_cwd(pCtx, &pstrRepoDescriptorName, ppPathCwd)  );
	SG_ERR_CHECK(  SG_closet__descriptors__get(pCtx, SG_string__sz(pstrRepoDescriptorName), &pvhDescriptor)  );
	SG_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, &pRepo, pvhDescriptor)  );

	SG_RETURN_AND_NULL(pRepo, ppRepo);

	/* fall through */
fail:
	SG_STRING_NULLFREE(pCtx, pstrRepoDescriptorName);
	SG_VHASH_NULLFREE(pCtx, pvhDescriptor);
	SG_REPO_NULLFREE(pCtx, pRepo);
}

static void _get_canonical_command(const char *cmd_name,
								   const cmdinfo** pCmdInfo)
{
	SG_uint32 i = 0;

	if (cmd_name == NULL)
		return;

	for ( i = 0; i < VERBCOUNT; ++i )
	{
		if ((strcmp(cmd_name, commands[i].name) == 0) ||
			(commands[i].alias1 && (strcmp(cmd_name, commands[i].alias1) == 0)) || 
			(commands[i].alias2 && (strcmp(cmd_name, commands[i].alias2) == 0)) || 
			(commands[i].alias3 && (strcmp(cmd_name, commands[i].alias3) == 0)) 
		   )
		{
			*pCmdInfo = commands + i;
			return;
		}
	}

	// TODO Review: so what happens here?  We return nothing or NULL?
	// TODO         is this case even possible? (have we already gone
	// TODO         a parser that did validate the name?)

  /* If we get here, there was no matching command name or alias. */
	return;
}

static void _parse_and_count_args(
	SG_context *pCtx,
	SG_getopt* os, 
	const char*** ppaszArgs,
	SG_uint32 expectedCount,
	SG_bool *pbUsageError)
{
	SG_uint32	count_args = 0;

	SG_ERR_CHECK_RETURN(  SG_getopt__parse_all_args(pCtx, os, ppaszArgs, &count_args)  );

	if (count_args != expectedCount)
		*pbUsageError = SG_TRUE;
}


static void do_cmd_help__list_all_commands(SG_context * pCtx, const char *pszAppName, SG_bool bShowAll)
{
	SG_uint32 i;
	size_t	maxWidth = 0;

	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "Usage: %s command [options]\n\nWhere 'command' is one of:\n\n", pszAppName)  );

	for (i=0; i<VERBCOUNT; i++)
	{
		if (strlen(commands[i].name) > maxWidth)
		{
			maxWidth = strlen(commands[i].name);
		}
	}

	for (i=0; i<VERBCOUNT; i++)
	{
		if (bShowAll || (! commands[i].advanced))
		{
			SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "%-*s    %s\n", (int)maxWidth, commands[i].name, commands[i].desc)  );
		}
	}

	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "\nsay \"%s help command\" for details on a particular command\n", pszAppName)  );

}

static void do_cmd_help__list_command_help(SG_context * pCtx, const char * szCommandName, SG_bool showCommandName)
{
	SG_uint32 j = 0;
	const cmdinfo *thiscmd = NULL;

	_get_canonical_command(szCommandName, &thiscmd);

	if (thiscmd)
	{
		if (showCommandName)
		{
			SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "%s\t", thiscmd->name)  );
		}

		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "%s\n", thiscmd->desc)  );
		if (thiscmd->valid_options[0])
			SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "\nPossible options:\n")  );
		while(thiscmd->valid_options[j])
		{
			SG_uint32 k;
			for (k = 0; k<(sizeof(sg_cl_options) / sizeof(struct sg_getopt_option)); k++)
			{
				if (thiscmd->valid_options[j] == sg_cl_options[k].optch)
				{
					SG_ERR_IGNORE(  SG_getopt__print_option(pCtx, &sg_cl_options[k], NULL)  );
					break;
				}
			}   
			j++;
		}
	}
	else
	{
		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "Unknown command: %s\n\n", szCommandName)  );
		SG_ERR_THROW_RETURN(  SG_ERR_USAGE  );
	}
}


void sg_report_portability_warnings(SG_context * pCtx, SG_pendingtree* pPendingTree)
{
    SG_varray* pva = NULL;
    SG_vhash* pvh = NULL;
    SG_string* pstr = NULL;
    SG_uint32 count;
    SG_uint32 i;

    SG_ERR_CHECK(  SG_pendingtree__get_warnings(pCtx, pPendingTree, &pva)  );
    SG_ERR_CHECK(  SG_varray__count(pCtx, pva, &count)  );
    
    /* TODO dumping these warnings as serialized JSON is a little unfriendly,
     * but it'll do for now. */

    SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstr)  );
    for (i=0; i<count; i++)
    {
        SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pva, i, &pvh)  );
        SG_ERR_CHECK(  SG_string__clear(pCtx, pstr)  );
        SG_ERR_CHECK(  SG_vhash__to_json(pCtx, pvh, pstr)  );
        SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR ,"%s\n", SG_string__sz(pstr))  );
    }
    SG_STRING_NULLFREE(pCtx, pstr);

    return;

fail:
    SG_STRING_NULLFREE(pCtx, pstr);
    return;
}

/**
 * Dump the "ActionLog" to the console.  This is for things that
 * requested "--test" or "--verbose".
 *
 * TODO Pretty this up; dumping JSON to the terminal is stupid.
 */
void sg_report_action_log(SG_context * pCtx, SG_pendingtree* pPendingTree)
{
    SG_varray* pva = NULL;				// we DO NOT own this
    SG_vhash* pvh = NULL;				// we DO NOT own this
    SG_string* pstr = NULL;
    SG_uint32 count;
    SG_uint32 i;

    SG_ERR_CHECK(  SG_pendingtree__get_action_log(pCtx, pPendingTree, &pva)  );
	if (pva)
	{
		SG_ERR_CHECK(  SG_varray__count(pCtx, pva, &count)  );
    
		SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstr)  );
		for (i=0; i<count; i++)
		{
			SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pva, i, &pvh)  );
			SG_ERR_CHECK(  SG_string__clear(pCtx, pstr)  );
			SG_ERR_CHECK(  SG_vhash__to_json(pCtx, pvh, pstr)  );
			SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR ,"%s\n", SG_string__sz(pstr))  );
		}
	}

fail:
    SG_STRING_NULLFREE(pCtx, pstr);
}

/* TODO we should ask ourselves if we need portability warnings on tag strings.
 * For now, I'm going to assume no. */


static SG_bool _command_takes_option(const cmdinfo* pCmdInfo, SG_int32 iOptionCode)
{
	SG_uint32 i;

	for (i = 0; i < SG_NrElements(pCmdInfo->valid_options); i++)
	{
		if (pCmdInfo->valid_options[i] == iOptionCode)
			return SG_TRUE;
		else if (pCmdInfo->valid_options[i] == 0) // no more valid options
			break;
	}

	return SG_FALSE;
}

static void _print_invalid_option(SG_context* pCtx, const char* pszAppName, const char* pszCommandName, SG_int32 iInvalidOpt)
{
	// TODO fix this to print a nicer message.
	SG_console(pCtx, SG_CS_STDERR, "Invalid option for '%s %s': ", pszAppName, pszCommandName);
	SG_getopt__print_invalid_option(pCtx, iInvalidOpt, sg_cl_options);
	SG_console(pCtx, SG_CS_STDERR, "\n");

}

static void _print_invalid_option_combination(SG_context * pCtx, const char * pszAppName, const char * pszCommandName,
											  SG_int32 iInvalidOpt_1,
											  SG_int32 iInvalidOpt_2)
{
	// TODO fix this to print a nicer message.
	SG_console(pCtx, SG_CS_STDERR, "Invalid option combination for '%s %s':\n", pszAppName, pszCommandName);
	SG_getopt__print_invalid_option(pCtx, iInvalidOpt_1, sg_cl_options);
	SG_getopt__print_invalid_option(pCtx, iInvalidOpt_2, sg_cl_options);
}


/**
 * Look at all of the -f or --flags given on the command line
 * and verify each is defined for this command.
 *
 * Print error message for each invalid flag and return bUsageError.
 * (We don't throw this because we are only part of the parse and
 * validation.  We want the caller to be able to use the existing
 * INVOKE() macro and be consistent with the other CMD_FUNCs (at
 * least for now).
 *
 * TODO convert all CMD_FUNCs that have a loop like the one in this
 * TODO function to use this function instead.
 */
static void _inspect_all_options(SG_context * pCtx,
								 const char * pszAppName,
								 const char * pszCommandName,
								 SG_option_state * pOptSt,
								 SG_bool * pbUsageError)
{
	const cmdinfo * thisCmd = NULL;
	SG_bool bUsageError = SG_FALSE;
	SG_uint32 k;

	_get_canonical_command(pszCommandName, &thisCmd);
	SG_ASSERT(  (thisCmd)  );

	for (k=0; k < pOptSt->count_options; k++)
	{
		if ( ! _command_takes_option(thisCmd, pOptSt->paRecvdOpts[k]))
		{
			SG_ERR_IGNORE(  _print_invalid_option(pCtx, pszAppName, pszCommandName, pOptSt->paRecvdOpts[k])  );
			bUsageError = SG_TRUE;
		}
	}

	*pbUsageError = bUsageError;
}

/**
 * Call this on commands that can utilize only *one* --rev
 * or --tag option at a time, but can accept both.  This method
 * will print an appropriate error message if iCountRevs +
 * iCountTags > 1 and return bUsageError.
 **/
static void _validate_rev_tag_options(SG_context* pCtx, 
									  const char* pszCommandName,
									  SG_option_state* pOptSt,
									  SG_bool* pbUsageError)
{
	SG_bool bUsageError = SG_FALSE;

	if (pOptSt->pvec_rev_tags)
	{
		if (pOptSt->iCountRevs > 0 && pOptSt->iCountTags > 0)
		{
			SG_console(pCtx, SG_CS_STDERR, "%s can only accept a single --rev value or a single --tag value, not both\n", pszCommandName);
            bUsageError = SG_TRUE;
		} 
		else if (pOptSt->iCountRevs > 1)
		{
			SG_console(pCtx, SG_CS_STDERR, "%s can only accept a single --rev value\n", pszCommandName);
			bUsageError = SG_TRUE;
		}
		else if (pOptSt->iCountTags > 1)
		{
			SG_console(pCtx, SG_CS_STDERR, "%s can only accept a single --tag value\n", pszCommandName);
	        bUsageError = SG_TRUE;
		}
	}	

	*pbUsageError = bUsageError;
}

//////////////////////////////////////////////////////////////////

void do_cmd_remove_tags(SG_context * pCtx, const char* pszRev, SG_bool bRev, SG_uint32 count_args, const char** paszArgs)
{
    SG_repo* pRepo = NULL;

    /* find the repo descriptor name at the top level of this working directory */
	SG_ERR_CHECK(  _get_repo_from_cwd(pCtx, &pRepo, NULL)  );

	SG_ERR_CHECK(  SG_tag__remove(pCtx, pRepo, pszRev, bRev, count_args, paszArgs)  );

fail:
	SG_REPO_NULLFREE(pCtx, pRepo);
}

/* TODO move this into the library as SG_log, returning a vhash. */
void my_dump_log(SG_context * pCtx, SG_repo* pRepo, const char* psz_hid_cs, SG_bool bRecurseToParents)
{
    SG_changeset* pcs = NULL;
    SG_uint32 count_parents = 0;
    const char** paszParents = NULL;
    SG_uint32 i;
    SG_varray* pva_comments = NULL;
    SG_varray* pva_tags = NULL;
    SG_varray* pva_stamps = NULL;
    SG_rbtree_iterator* pit = NULL;
    SG_varray* pva_audits = NULL;
    SG_uint32 count_audits = 0;

    SG_ERR_CHECK(  SG_changeset__load_from_repo(pCtx, pRepo, psz_hid_cs, &pcs)  );
    SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDOUT, "%s\n", psz_hid_cs)  );

    /* TODO throughout this function, deal with who */

    /* print the time stamps */
    SG_ERR_CHECK(  SG_audit__lookup(pCtx, pRepo, psz_hid_cs, SG_DAGNUM__VERSION_CONTROL, &pva_audits)  );
    SG_ERR_CHECK(  SG_varray__count(pCtx, pva_audits, &count_audits)  );
    for (i=0; i<count_audits; i++)
    {
        SG_vhash* pvh = NULL;
        SG_int64 itime = -1;
        const char* psz_userid = NULL;
        char buf_time_formatted[256];

        SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pva_audits, i, &pvh)  );
        SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh, "who", &psz_userid)  );
        SG_ERR_CHECK(  SG_vhash__get__int64(pCtx, pvh, "when", &itime)  );
        SG_ERR_CHECK(  SG_time__format_local__i64(pCtx, itime, buf_time_formatted, sizeof(buf_time_formatted))  );
        SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDOUT, "  When: %s\n", buf_time_formatted)  );
		SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDOUT, "  Who: %s\n", psz_userid)  );
    }
    SG_VARRAY_NULLFREE(pCtx, pva_audits);

    /* TODO maybe we should retrieve the entire tag list once and pass it
     * around instead of doing one query for each cs? */
    SG_ERR_CHECK(  SG_vc_tags__lookup(pCtx, pRepo, psz_hid_cs, &pva_tags)  );
    if (pva_tags)
    {
        SG_uint32 count = 0;
        SG_uint32 i = 0;

        SG_ERR_CHECK(  SG_varray__count(pCtx, pva_tags, &count)  );
        for (i=0; i<count; i++)
        {
            SG_vhash* pvh_tag = NULL;
            const char* psz_tag = NULL;

            SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pva_tags, i, &pvh_tag)  );
            SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_tag, "tag", &psz_tag)  );

            SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDOUT, "  Tag : %s\n", psz_tag)  );
        }
        SG_VARRAY_NULLFREE(pCtx, pva_tags);
    }

    SG_ERR_CHECK(  SG_vc_comments__lookup(pCtx, pRepo, psz_hid_cs, &pva_comments)  );
    if (pva_comments)
    {
        SG_uint32 count = 0;
        SG_uint32 i = 0;

        SG_ERR_CHECK(  SG_varray__count(pCtx, pva_comments, &count)  );
        for (i=0; i<count; i++)
        {
            SG_vhash* pvh_comment = NULL;
            const char* psz_comment = NULL;

            SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pva_comments, i, &pvh_comment)  );
            SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_comment, "text", &psz_comment)  );
            SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDOUT, "  Comment : %s\n", psz_comment)  );
        }
        SG_VARRAY_NULLFREE(pCtx, pva_comments);
    }

    SG_ERR_CHECK(  SG_vc_stamps__lookup(pCtx, pRepo, psz_hid_cs, &pva_stamps)  );
    if (pva_stamps)
    {
        SG_uint32 count = 0;
        SG_uint32 i = 0;

        SG_ERR_CHECK(  SG_varray__count(pCtx, pva_stamps, &count)  );
        for (i=0; i<count; i++)
        {
            SG_vhash* pvh_stamp = NULL;
            const char* psz_stamp = NULL;

            SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pva_stamps, i, &pvh_stamp)  );
            SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_stamp, "stamp", &psz_stamp)  );

            SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDOUT, "  Stamp : %s\n", psz_stamp)  );
        }
        SG_VARRAY_NULLFREE(pCtx, pva_stamps);
    }

    /* TODO who */
    /* TODO where? */

    /* Now recurse on each parent of this changeset */
    if (bRecurseToParents)
    {
        SG_ERR_CHECK(  SG_changeset__get_parents(pCtx, pcs, &count_parents, &paszParents)  );
        for (i=0; i<count_parents; i++)
        {
            SG_ERR_CHECK(  my_dump_log(pCtx, pRepo, paszParents[i], bRecurseToParents)  );
        }
        SG_NULLFREE(pCtx, paszParents);
    }

    SG_CHANGESET_NULLFREE(pCtx, pcs);

	return;

fail:
    SG_VARRAY_NULLFREE(pCtx, pva_audits);
	SG_VARRAY_NULLFREE(pCtx, pva_tags);
	SG_VARRAY_NULLFREE(pCtx, pva_stamps);
	SG_VARRAY_NULLFREE(pCtx, pva_comments);
    SG_CHANGESET_NULLFREE(pCtx, pcs);
	SG_NULLFREE(pCtx, paszParents);
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);
}

void do_cmd_history(SG_context * pCtx, SG_int32 iCountArgs, const char ** paszArgs, SG_uint32 iCountRevs, SG_vector * pvec_revs, const char* pszUser, const char * pszStamp, const char* pszFromDate, const char* pszToDate, SG_bool bLeaves, SG_uint32 iMaxResults)
{
	SG_varray * pVArrayResults = NULL;
    SG_int64 nFromDate = 0;
    SG_int64 nToDate = SG_INT64_MAX;
    SG_uint32 nChangesetCount = 0;
    const char ** pasz_changesets = NULL;
    SG_repo * pRepo = NULL;


	if (pszToDate != NULL)
		SG_ERR_CHECK(  SG_time__parse(pCtx, pszToDate, &nToDate, SG_TRUE)  );
	if (pszFromDate != NULL)
		SG_ERR_CHECK(  SG_time__parse(pCtx, pszFromDate, &nFromDate, SG_FALSE)  );

	SG_ERR_CHECK(  _get_repo_from_cwd(pCtx, &pRepo, NULL)  );

	if (pvec_revs != NULL)
	{
		SG_uint32 iLength = 0;
		SG_uint32 i = 0;
		SG_uint32 iCount = 0;
		char * psz_rev = NULL;
		if (iCountRevs > 0)
		{
			SG_ERR_CHECK(  SG_alloc(pCtx, iCountRevs, sizeof(const char *), &pasz_changesets)  );
			SG_ERR_CHECK(  SG_vector__length(pCtx, pvec_revs, &iLength)  );
			for(i =0; i < iLength; i++)
			{

				SG_rev_tag_obj* pRTobj = NULL;
				SG_ERR_CHECK(  SG_vector__get(pCtx, pvec_revs, i, (void**)&pRTobj)  );
				if (pRTobj->bRev)
				{
					SG_ERR_CHECK(  SG_repo__hidlookup__dagnode(pCtx, pRepo, SG_DAGNUM__VERSION_CONTROL, pRTobj->pszRevTag, &psz_rev)  );
					if (!psz_rev)
					{
						SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "Could not locate changeset:  %s\n", pRTobj->pszRevTag)  );
					}
					else
						pasz_changesets[iCount++] = psz_rev;
				}
				else
				{
					SG_ERR_CHECK(  SG_vc_tags__lookup__tag(pCtx, pRepo, pRTobj->pszRevTag, &psz_rev)  );
					if (psz_rev != NULL)
						pasz_changesets[iCount++] = psz_rev;
					else
					{
						SG_console(pCtx, SG_CS_STDERR, "Could not locate changeset for tag: %s\n", pRTobj->pszRevTag);
						iCountRevs--;
					}
				}
			}
			nChangesetCount = iCountRevs;
			if (nChangesetCount == 0)
				goto fail;
		}
	}
	SG_ERR_CHECK(  SG_history__query(pCtx, NULL, pRepo, iCountArgs, paszArgs, pasz_changesets, nChangesetCount, pszUser, pszStamp, iMaxResults, nFromDate, nToDate, bLeaves, SG_FALSE, &pVArrayResults)  );
    
	// fall thru to common cleanup
	if (pVArrayResults != NULL)
	{
		//TODO: Sort the VArray on date
		//print the information for each
		SG_uint32 resultsLength = 0;
		SG_uint32 i = 0;
		const char* currentInfoItem = NULL;
		SG_vhash * currentDagnode = NULL;
		SG_varray * pVArray = NULL;
		SG_vhash * pVHashCurrentThingy = NULL;
		SG_uint32 nCount = 0;
		SG_uint32 nIndex = 0;
		const char * pszUser = NULL;
		const char * pszTag = NULL;
		const char * pszComment = NULL;
		const char * pszStamp = NULL;
		const char * pszParent = NULL;

		SG_ERR_CHECK(  SG_varray__count(pCtx, pVArrayResults, &resultsLength)  );

		for (i = 0; i < resultsLength; i++)
		{
			SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pVArrayResults, i, &currentDagnode)  );
			SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, currentDagnode, "changeset_id", &currentInfoItem));
			SG_ERR_CHECK(  SG_vhash__get__varray(pCtx, currentDagnode, "audits", &pVArray)  );
			SG_ERR_CHECK(  SG_varray__count(pCtx, pVArray, &nCount)  );
			SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDOUT, "\nchangeset_id:  %s\n", currentInfoItem)  );
			for (nIndex = 0; nIndex < nCount; nIndex++)
			{
                SG_int64 itime = -1;
                char buf_time_formatted[256];

				SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pVArray, nIndex, &pVHashCurrentThingy)  );
				SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pVHashCurrentThingy, "who", &pszUser)  );
				SG_ERR_CHECK(  SG_vhash__get__int64(pCtx, pVHashCurrentThingy, "when", &itime)  );
                SG_ERR_CHECK(  SG_time__format_local__i64(pCtx, itime, buf_time_formatted, sizeof(buf_time_formatted))  );
				SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDOUT, "\t%7s:  %s\n", "who", pszUser)  );
				SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDOUT, "\t%7s:  %s\n", "when", buf_time_formatted)  );
			}

			SG_ERR_CHECK(  SG_vhash__get__varray(pCtx, currentDagnode, "tags", &pVArray)  );

			if (pVArray)
			{
				SG_ERR_CHECK(  SG_varray__count(pCtx, pVArray, &nCount)  );
				for (nIndex = 0; nIndex < nCount; nIndex++)
				{
					SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pVArray, nIndex, &pszTag)  );
					SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDOUT, "\t%7s:  %s\n", "tag", pszTag)  );
				}
			}

			SG_ERR_CHECK(  SG_vhash__get__varray(pCtx, currentDagnode, "comments", &pVArray)  );
            if (pVArray)
            {
                SG_ERR_CHECK(  SG_varray__count(pCtx, pVArray, &nCount)  );
                for (nIndex = 0; nIndex < nCount; nIndex++)
                {				
                    SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pVArray, nIndex, &pVHashCurrentThingy)  );
                    SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pVHashCurrentThingy, "text", &pszComment)  );
                    SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDOUT, "\t%7s:  %s\n", "comment", pszComment)  );
                }
            }
            SG_ERR_CHECK(  SG_vhash__get__varray(pCtx, currentDagnode, "stamps", &pVArray)  );
            if (pVArray)
            {
				SG_ERR_CHECK(  SG_varray__count(pCtx, pVArray, &nCount)  );
				for (nIndex = 0; nIndex < nCount; nIndex++)
				{
					SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pVArray, nIndex, &pszStamp)  );
					SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDOUT, "\t%7s:  %s\n", "stamp", pszStamp)  );
				}
            }
			SG_ERR_CHECK(  SG_vhash__get__varray(pCtx, currentDagnode, "parents", &pVArray)  );
			SG_ERR_CHECK(  SG_varray__count(pCtx, pVArray, &nCount)  );
			for (nIndex = 0; nIndex < nCount; nIndex++)
			{
				SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pVArray, nIndex, &pszParent)  );
				SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDOUT, "\t%7s:  %s\n", "parent", pszParent)  );
			}

		}
	}

fail:
	SG_ERR_IGNORE(  SG_freeStringList(pCtx, &pasz_changesets, nChangesetCount)  );
	SG_VARRAY_NULLFREE(pCtx, pVArrayResults);
	SG_REPO_NULLFREE(pCtx, pRepo);
}

void _vv__delete_repo(SG_context * pCtx, const char * szRepoName)
{
    SG_vhash * pDescriptor = NULL;
    SG_repo * pRepo = NULL;
    
	SG_ASSERT(pCtx!=NULL);
	SG_NULLARGCHECK_RETURN(szRepoName);
	
	SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDOUT, "Deleting %s...", szRepoName)  );
	
    SG_ERR_CHECK(  SG_closet__descriptors__get(pCtx, szRepoName, &pDescriptor)  );
    SG_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, &pRepo, pDescriptor)  );
    SG_VHASH_NULLFREE(pCtx, pDescriptor);
    SG_ERR_CHECK(  SG_repo__delete_repo_instance(pCtx, &pRepo)  );
    
    SG_ERR_CHECK(  SG_closet__descriptors__remove(pCtx, szRepoName)  );
    
	SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDOUT, " Done.\n", szRepoName)  );
    
	return;
fail:
    SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDOUT, " An error occurred deleting this repo.\n")  );

    SG_VHASH_NULLFREE(pCtx, pDescriptor);
	SG_REPO_NULLFREE(pCtx, pRepo);
}

void do_cmd_deleterepo(SG_context * pCtx, SG_uint32 count_args, const char ** paszArgs, SG_bool allBut, SG_bool force)
{
    SG_uint32 i;
	SG_stringarray * pHitList = NULL;
	SG_vhash * pDescriptors = NULL;
    SG_string * pStdin = NULL;
    SG_bool proceed = SG_TRUE;
	
	SG_ASSERT(pCtx!=NULL);
	if(count_args<1)
		SG_ERR_THROW_RETURN(SG_ERR_USAGE);
	SG_NULLARGCHECK_RETURN(paszArgs);
	
	SG_ERR_CHECK(  SG_stringarray__alloc(pCtx, &pHitList, 1)  );
	
    // Validate all repo names
    for(i=0; i<count_args; ++i)
    {
        SG_vhash * pTemp = NULL;
        SG_closet__descriptors__get(pCtx, paszArgs[i], &pTemp);
        if(SG_context__err_equals(pCtx, SG_ERR_NOT_FOUND))
            SG_ERR_RESET_THROW2(SG_ERR_NOTAREPOSITORY, (pCtx, "%s", paszArgs[i]));
        else
            SG_ERR_CHECK_CURRENT;

        SG_VHASH_NULLFREE(pCtx, pTemp);
    }
    
	if(!allBut)
    {
		for(i=0; i<count_args; ++i)
			SG_ERR_CHECK(  SG_stringarray__add(pCtx, pHitList, paszArgs[i])  );
	}
	else
    {
		SG_uint32 repo_count;
        SG_uint32 delete_count = 0;
        
        if(!force)
            SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDOUT, "Repos:\n")  );

		SG_ERR_CHECK(  SG_closet__descriptors__list(pCtx, &pDescriptors)  );
		SG_ERR_CHECK(  SG_vhash__count(pCtx, pDescriptors, &repo_count)  );
		for (i=0; i<repo_count; ++i) {
			SG_uint32 j;
			const char * szRepoName = NULL;
			SG_bool found_in_exception_list = SG_FALSE;
			SG_ERR_CHECK(  SG_vhash__get_nth_pair(pCtx, pDescriptors, i, &szRepoName, NULL)  );
			for (j=0; (j<count_args)&&(!found_in_exception_list); ++j)
				found_in_exception_list = (strcmp(szRepoName, paszArgs[j])==0);
			if(!found_in_exception_list)
            {
				SG_ERR_CHECK(  SG_stringarray__add(pCtx, pHitList, szRepoName)  );
                ++delete_count;
                if(!force)
                    SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDOUT, "     %s\n", szRepoName)  );
            }
		}
		SG_VHASH_NULLFREE(pCtx, pDescriptors);
        
        if(!force && delete_count>0)
        {
            if(delete_count==1)
                SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDOUT, "Delete this repo? (y/n) ")  );
            else
                SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDOUT, "Delete these %d repos? (y/n) ", delete_count)  );
            
            SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pStdin)  );
            SG_ERR_CHECK(  SG_console__readline_stdin(pCtx, &pStdin)  );
            proceed = (SG_string__sz(pStdin)[0]=='y'||SG_string__sz(pStdin)[0]=='Y');
            SG_STRING_NULLFREE(pCtx, pStdin);
            
            SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDOUT, "\n")  );

        }
        else if(!force)
        {
            SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDOUT, "     (None)\n")  );

        }
    }
	
    if(proceed)
	{
		SG_uint32 count;
		const char * const * ppHitList = NULL;
		SG_ERR_CHECK(  SG_stringarray__sz_array_and_count(pCtx, pHitList, &ppHitList, &count)  );
        for (i=0; i<count; ++i)
        {
			_vv__delete_repo(pCtx, ppHitList[i]);
            if(SG_context__has_err(pCtx))
            {
                if(count>1)
                {
                    SG_log_current_error_urgent(pCtx);
                    SG_context__err_reset(pCtx);
                }
                else
                    SG_ERR_RETHROW;
            }
        }
	}
	
	SG_STRINGARRAY_NULLFREE(pCtx, pHitList);
	
	return;
fail:
	SG_STRINGARRAY_NULLFREE(pCtx, pHitList);
	SG_VHASH_NULLFREE(pCtx, pDescriptors);
    SG_STRING_NULLFREE(pCtx, pStdin);
}

void do_cmd_listrepos(SG_context * pCtx)
{
    SG_vhash* pvhDescriptor = NULL;
	SG_varray* pva = NULL;
	SG_uint32 count = 0;
	SG_uint32 i;
	const char* ppRepoName = NULL;

	SG_ERR_CHECK(  SG_closet__descriptors__list(pCtx, &pvhDescriptor)  );
	SG_ERR_CHECK(  SG_vhash__get_keys(pCtx, pvhDescriptor, &pva)  );
	SG_ERR_CHECK(  SG_varray__count(pCtx, pva, &count)  );

	for (i=0; i<count; i++)
	{
		SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pva, i, &ppRepoName)  );
		SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDOUT, "%s\n", ppRepoName)  );
	}
	
	SG_VARRAY_NULLFREE(pCtx, pva);
    SG_VHASH_NULLFREE(pCtx, pvhDescriptor);
    return;

fail:	
	SG_VARRAY_NULLFREE(pCtx, pva);
	SG_VHASH_NULLFREE(pCtx, pvhDescriptor);
}

static void cmd_serve_mg_uri_callback(struct mg_connection *conn, const struct mg_request_info *request_info, SG_UNUSED_PARAM(void *user_data))
{
	void * pRequestHandle = NULL;
	void * pResponseHandle = NULL;

	SG_UNUSED(user_data);

	if ((request_info->post_data_len>0) && (request_info->post_data_len != SG_UINT64_MAX))
	{
		SG_uridispatch__begin_request(
			request_info->uri,
			request_info->query_string,
			request_info->request_method,
			request_info->is_ssl ? SG_TRUE : SG_FALSE,
			mg_get_header(conn, "Host"),
            mg_get_header(conn,"Accept"),
			mg_get_header(conn,"User-Agent"),
			request_info->post_data_len,
            mg_get_header(conn,"From"),
			NULL,
			&pRequestHandle,
			&pResponseHandle);

		while(pRequestHandle!=NULL)
		{
			SG_byte buffer[1024];
			int chunk_len = 0;
			chunk_len = mg_get_chunk(conn, (char*)buffer, sizeof(buffer));
			if(chunk_len>0)
				SG_uridispatch__chunk_request_body(buffer, (SG_uint32)chunk_len, &pRequestHandle, &pResponseHandle);
			else
				SG_uridispatch__abort_request(&pRequestHandle); // See comments on mg_get_chunk in sg_mongoose.h.
		}
	}
	else
	{
		SG_uridispatch__request(
			request_info->uri,
			request_info->query_string,
			request_info->request_method,
			request_info->is_ssl ? SG_TRUE : SG_FALSE,
			mg_get_header(conn, "Host"),
            mg_get_header(conn,"Accept"),
			mg_get_header(conn, "User-Agent"),
            mg_get_header(conn,"From"),
			mg_get_header(conn,"If-Modified-Since"),
			&pResponseHandle);
	}

	if(pResponseHandle!=NULL)
	{
		const char * pHttpStatusCode = "500 Internal Server Error";
		SG_uint32 nHeaders = 0;
		SG_uint32 i;
		char **headers = NULL;

		SG_uridispatch__get_response_headers(
			&pResponseHandle,
			&pHttpStatusCode,
			&headers, &nHeaders);

		SG_ASSERT((nHeaders == 0) || (headers != NULL));

		mg_printf(conn, "HTTP/1.1 %s\r\n", pHttpStatusCode);

		for ( i = 0; i < nHeaders; ++i )
		{
			SG_ASSERT(headers[i] != NULL);
			mg_printf(conn, "%s\r\n", headers[i]);
		}

		mg_printf(conn, "\r\n");

		for ( i = 0; i < nHeaders; ++i )
			SG_free__no_ctx(headers[i]);
		SG_free__no_ctx(headers);

		while(pResponseHandle!=NULL)
		{
			SG_byte buffer[1024];
			SG_uint32 length_got = 0;
			SG_uridispatch__chunk_response_body(
				&pResponseHandle,
				buffer,
				sizeof(buffer),
				&length_got);
			if(length_got>0)
				mg_write(conn, buffer, length_got);
		}
	}

	SG_ASSERT(pRequestHandle==NULL);
	SG_ASSERT(pResponseHandle==NULL);
}


static SG_bool shut_down_mg_flag = SG_FALSE;

void shut_down_mg(int SG_UNUSED_PARAM(sig))
{
	SG_UNUSED(sig);
	shut_down_mg_flag = SG_TRUE;
}

void do_cmd_serve(SG_context * pCtx, SG_option_state* pOptSt)
{
	struct mg_context * ctx;
    char* psz_collateral_root = NULL;
	SG_pathname *collateralRoot = NULL;
	SG_pathname * staticPath = NULL;
	SG_string * aliases = NULL;
	SG_string * ports = NULL;
    SG_repo* pRepo = NULL;

    // if we're in a working copy, then we want to grab
    // options from this repo scope

    _get_repo_from_cwd(pCtx, &pRepo, NULL);
    if (SG_context__err_equals(pCtx, SG_ERR_NOT_A_WORKING_COPY))
    {
        SG_context__err_reset(pCtx);
        pRepo = NULL;
    }

    SG_ERR_CHECK(  SG_localsettings__get__sz(pCtx, SG_LOCALSETTING__SSI_DIR, pRepo, &psz_collateral_root, NULL)  );
	SG_REPO_NULLFREE(pCtx, pRepo);
    SG_ERR_CHECK(  SG_pathname__alloc__sz(pCtx, &collateralRoot, psz_collateral_root)  );
    SG_NULLFREE(pCtx, psz_collateral_root);

	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &staticPath, collateralRoot, "static")  );

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &aliases)  );
	SG_ERR_CHECK(  SG_string__sprintf(pCtx, aliases, "/static=%s", SG_pathname__sz(staticPath))  );

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &ports)  );

	if (pOptSt->bNoPlain)
	{
		SG_ERR_CHECK(  SG_string__sprintf(pCtx, ports, "%lis", (long)pOptSt->iSPort)  );
	}
	else
	{
		SG_ERR_CHECK(  SG_string__sprintf(pCtx, ports, "%li,%lis", (long)pOptSt->iPort, (long)pOptSt->iSPort)  );
	}

	ctx = mg_start();
	mg_set_option(ctx, "ssl_cert", "ssl_cert.pem");
	mg_set_option(ctx, "aliases", SG_string__sz(aliases));
	mg_set_option(ctx, "ports", SG_string__sz(ports));

	SG_STRING_NULLFREE(pCtx, aliases);
	SG_STRING_NULLFREE(pCtx, ports);

	mg_set_uri_callback(ctx, "*", &cmd_serve_mg_uri_callback, NULL);

	(void)signal(SIGINT,shut_down_mg);
	while(!shut_down_mg_flag)
		SG_sleep_ms(10);
	(void)signal(SIGINT,SIG_DFL);
	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDOUT, "Shutting down server...\n")  );
	mg_stop(ctx);

fail:
    SG_NULLFREE(pCtx, psz_collateral_root);
	SG_STRING_NULLFREE(pCtx, aliases);
	SG_STRING_NULLFREE(pCtx, ports);
	SG_PATHNAME_NULLFREE(pCtx, collateralRoot);
	SG_PATHNAME_NULLFREE(pCtx, staticPath);
	SG_REPO_NULLFREE(pCtx, pRepo);
}

void do_cmd_localsettings(SG_context * pCtx, SG_bool verbose)
{
    SG_vhash* pvh = NULL;
    SG_string* pstr = NULL;

    SG_UNUSED(verbose);

    // TODO ok, this isn't the way to do this

    SG_ERR_CHECK(  SG_localsettings__list__vhash(pCtx,  &pvh )  );
    SG_ERR_CHECK(  SG_string__alloc(pCtx, &pstr)  );
    SG_ERR_CHECK(  SG_vhash__to_json(pCtx, pvh, pstr)  );
    SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDOUT, "%s\n", SG_string__sz(pstr))  );

fail:
    SG_STRING_NULLFREE(pCtx, pstr );
    SG_VHASH_NULLFREE(pCtx, pvh );
}

static void my_localsettings__import(
    SG_context* pCtx,
    const char* psz_path
    )
{
    SG_uint32 len32 = 0;
    SG_uint64 len64 = 0;
    SG_vhash* pvh = NULL;
    SG_pathname* pPath = NULL;
    SG_file* pFile = NULL;
	SG_byte* p = NULL;

    SG_ERR_CHECK(  SG_pathname__alloc__sz(pCtx, &pPath, psz_path)  );

    SG_ERR_CHECK(  SG_fsobj__length__pathname(pCtx, pPath, &len64, NULL)  );
    
    if (!SG_uint64__fits_in_uint32(len64))
    {
        SG_ERR_THROW(  SG_ERR_UNSPECIFIED  ); // TODO
    }

    len32 = (SG_uint32)len64;

	SG_ERR_CHECK(  SG_file__open__pathname(pCtx, pPath, SG_FILE_RDONLY|SG_FILE_OPEN_EXISTING, SG_FSOBJ_PERMS__UNUSED, &pFile)  );

    SG_ERR_CHECK(  SG_alloc(pCtx, 1,len32+1,&p)  );
    SG_ERR_CHECK(  SG_file__read(pCtx, pFile, len32, p, NULL)  );
    
    p[len32] = 0;
    
    SG_ERR_CHECK(  SG_VHASH__ALLOC__FROM_JSON(pCtx, &pvh, (const char*) p)  );
    
    SG_NULLFREE(pCtx, p);
    p = NULL;

    SG_ERR_CHECK(  SG_localsettings__import(pCtx, pvh)  );

fail:
    SG_VHASH_NULLFREE(pCtx, pvh);
    SG_PATHNAME_NULLFREE(pCtx, pPath);
	SG_FILE_NULLCLOSE(pCtx, pFile);
	SG_NULLFREE(pCtx, p);
}

static void my_localsettings__export(
    SG_context* pCtx,
    const char* psz_path
    )
{
    SG_vhash* pvh = NULL;
    SG_string* pstr = NULL;
    SG_pathname* pPath = NULL;
    SG_file* pFile = NULL;

    SG_ERR_CHECK(  SG_localsettings__list__vhash(pCtx,  &pvh )  );

    SG_ERR_CHECK(  SG_string__alloc(pCtx, &pstr)  );
    SG_ERR_CHECK(  SG_vhash__to_json(pCtx, pvh, pstr)  );
    SG_VHASH_NULLFREE(pCtx, pvh );

    SG_ERR_CHECK(  SG_pathname__alloc__sz(pCtx, &pPath, psz_path)  );
    SG_ERR_CHECK(  SG_file__open__pathname(pCtx, pPath, SG_FILE_WRONLY | SG_FILE_OPEN_OR_CREATE, 0777, &pFile)  );
    SG_ERR_CHECK(  SG_file__write__string(pCtx, pFile, pstr)  );
    SG_ERR_CHECK(  SG_file__close(pCtx, &pFile)  );

fail:
    SG_PATHNAME_NULLFREE(pCtx, pPath);
	SG_FILE_NULLCLOSE(pCtx, pFile);
    SG_STRING_NULLFREE(pCtx, pstr);
    SG_VHASH_NULLFREE(pCtx, pvh );
}

void do_cmd_localsettings_specific(SG_context * pCtx, SG_bool verbose, SG_uint32 count_args,const char** paszArgs)
{
    SG_string* pstr_old_value = NULL;
    SG_string* pstr_new_value = NULL;
    SG_string* pstr_path_found = NULL;
    SG_vhash* pvh = NULL;
    SG_string* pstr = NULL;

    SG_UNUSED(verbose);

    // paszArgs[0] is the subcommand.

    if (0 == strcmp(paszArgs[0], "export"))
    {
        SG_ERR_CHECK(  my_localsettings__export(pCtx, paszArgs[1] )  );
    }
    else if (0 == strcmp(paszArgs[0], "import"))
    {
        SG_ERR_CHECK(  my_localsettings__import(pCtx, paszArgs[1] )  );
    }
    else if (0 == strcmp(paszArgs[0], "defaults"))
    {
        SG_ERR_CHECK(  SG_localsettings__factory__list__vhash(pCtx,  &pvh )  );
        SG_ERR_CHECK(  SG_string__alloc(pCtx, &pstr)  );
        SG_ERR_CHECK(  SG_vhash__to_json(pCtx, pvh, pstr)  );
        SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDOUT, "%s\n", SG_string__sz(pstr))  );
        SG_STRING_NULLFREE(pCtx, pstr );
        SG_VHASH_NULLFREE(pCtx, pvh );
    }
    else if (0 == strcmp(paszArgs[0], "help"))
    {
        SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDOUT, "vv localsettings export (filename)\n")  );
        SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDOUT, "vv localsettings import (filename)\n")  );
        SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDOUT, "vv localsettings show (setting)\n")  );
        SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDOUT, "vv localsettings set (setting) (value)\n")  );
        SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDOUT, "vv localsettings reset (setting)\n")  );
        SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDOUT, "vv localsettings add-to (setting) (value) (value) ...\n")  );
        SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDOUT, "vv localsettings remove-from (setting) (value) (value) ...\n")  );
        SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDOUT, "vv localsettings defaults\n")  );
        SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDOUT, "vv localsettings help\n")  );
    }
    else
    {
        // for all of these subcommands,
        // paszArgs[1] is the setting path

        SG_ERR_CHECK(  SG_localsettings__get__convert_to_string(pCtx, paszArgs[1], NULL, &pstr_old_value, &pstr_path_found)  );

        if (0 == strcmp(paszArgs[0], "show"))
        {
            if (pstr_old_value)
            {
                SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDOUT, "%s: %s\n", paszArgs[1], SG_string__sz(pstr_old_value))  );
            }
        }
        else if (0 == strcmp(paszArgs[0], "set"))
        {
            SG_ERR_CHECK(  SG_localsettings__update__sz(pCtx, paszArgs[1], paszArgs[2])  );
            SG_ERR_CHECK(  SG_localsettings__get__convert_to_string(pCtx, paszArgs[1], NULL, &pstr_new_value, NULL)  );
            if (pstr_old_value)
            {
                SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDOUT, "%s: %s -> %s\n", paszArgs[1], SG_string__sz(pstr_old_value), SG_string__sz(pstr_new_value))  );
            }
            else
            {
                SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDOUT, "%s: %s\n", paszArgs[1], SG_string__sz(pstr_new_value))  );
            }
        }
        else if (0 == strcmp(paszArgs[0], "reset"))
        {
            SG_ERR_CHECK(  SG_localsettings__reset(pCtx, paszArgs[1])  );
        }
        else if (0 == strcmp(paszArgs[0], "add-to"))
        {
            SG_uint32 i;
            for(i=2;i<count_args;++i)
            {
                SG_ERR_CHECK(  SG_localsettings__varray__append(pCtx, paszArgs[1], paszArgs[i])  );
            }
            SG_ERR_CHECK(  SG_localsettings__get__convert_to_string(pCtx, paszArgs[1], NULL, &pstr_new_value, NULL)  );
            SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDOUT, "%s: %s\n", paszArgs[1], SG_string__sz(pstr_new_value))  );
        }
        else if (0 == strcmp(paszArgs[0], "remove-from"))
        {
            SG_uint32 i;
            for(i=2;i<count_args;++i)
            {
                SG_ERR_CHECK(  SG_localsettings__varray__remove_first_match(pCtx, paszArgs[1], paszArgs[i])  );
            }
            SG_ERR_CHECK(  SG_localsettings__get__convert_to_string(pCtx, paszArgs[1], NULL, &pstr_new_value, NULL)  );
            SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDOUT, "%s: %s\n", paszArgs[1], SG_string__sz(pstr_new_value))  );
        }
        else
        {
            SG_ERR_THROW(  SG_ERR_NOTIMPLEMENTED  );
        }
    }

fail:
    SG_STRING_NULLFREE(pCtx, pstr );
    SG_VHASH_NULLFREE(pCtx, pvh );
    SG_STRING_NULLFREE(pCtx, pstr_old_value);
    SG_STRING_NULLFREE(pCtx, pstr_new_value);
    SG_STRING_NULLFREE(pCtx, pstr_path_found);
}

void do_cmd_stamp(SG_context * pCtx, SG_bool bRemove, SG_stringarray * pasz_stamps,  SG_vector* pvec_changesets)
{
    SG_repo* pRepo = NULL;
    char* psz_hid_cs = NULL;
    SG_audit q;
    SG_uint32 nStampsIndex = 0;
    SG_uint32 nStampsCount = 0;
    SG_uint32 nChangesetsIndex = 0;
	SG_uint32 nChangesetsCount = 0;
	SG_rev_tag_obj * pCurrentRevOrTag = NULL;
	char* psz_rev = NULL;
	const char* pszCurrentStamp = NULL;
	
	if (pasz_stamps != NULL)
		SG_ERR_CHECK(  SG_stringarray__count(pCtx, pasz_stamps, &nStampsCount)  );
	if (nStampsCount <= 0 && !bRemove)
			SG_ERR_THROW2(  SG_ERR_INVALIDARG,
			                           (pCtx, "Must include at least one stamp.")  );

	if (pvec_changesets != NULL)
		SG_ERR_CHECK(  SG_vector__length(pCtx, pvec_changesets, &nChangesetsCount)  );
	if (nChangesetsCount <= 0)
		SG_ERR_THROW2(  SG_ERR_INVALIDARG,
		                           (pCtx, "Must include at least one changeset using either --rev or --tag")  );

    // TODO why isn't this line wrapped in SG_ERR_CHECK?
	_get_repo_from_cwd(pCtx, &pRepo, NULL);

    SG_ERR_CHECK(  SG_audit__init(pCtx,&q,pRepo,SG_AUDIT__WHEN__NOW,SG_AUDIT__WHO__FROM_SETTINGS)  );

	for (nChangesetsIndex = 0; nChangesetsIndex < nChangesetsCount; nChangesetsIndex++)
	{
			SG_ERR_CHECK(  SG_vector__get(pCtx, pvec_changesets, nChangesetsIndex, (void**)&pCurrentRevOrTag)  );
			if (pCurrentRevOrTag->bRev)
			{
				SG_ERR_CHECK(  SG_repo__hidlookup__dagnode(pCtx, pRepo, SG_DAGNUM__VERSION_CONTROL, pCurrentRevOrTag->pszRevTag, &psz_rev)  );
				if (!psz_rev)
				{
					SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "Could not locate changeset:  %s\n", pCurrentRevOrTag->pszRevTag)  );
				}
			}
			else
			{
				SG_ERR_CHECK(  SG_vc_tags__lookup__tag(pCtx, pRepo, pCurrentRevOrTag->pszRevTag, &psz_rev)  );
				if (psz_rev == NULL)
				{
					SG_console(pCtx, SG_CS_STDERR, "tag not found: %s\n", pCurrentRevOrTag->pszRevTag);
				}
			}
			if (psz_rev != NULL && !bRemove)
			{
				for (nStampsIndex = 0; nStampsIndex < nStampsCount; nStampsIndex++)
				{
					SG_ERR_CHECK(  SG_stringarray__get_nth(pCtx, pasz_stamps, nStampsIndex, &pszCurrentStamp)  );
					SG_ERR_CHECK(  SG_vc_stamps__add(pCtx, pRepo, psz_rev, pszCurrentStamp, &q)  );
				}
			}
			else if (psz_rev != NULL && bRemove)
			{
				const char * const * pasz_char_stamps = NULL;
				if (nStampsCount > 0)
					SG_ERR_CHECK(  SG_stringarray__sz_array(pCtx, pasz_stamps, &pasz_char_stamps)  );
				//Remove all stamps from this revision.
				SG_ERR_CHECK(  SG_vc_stamps__remove(pCtx, pRepo, &q, (const char *)psz_rev, nStampsCount, pasz_char_stamps)  );
			}
			SG_NULLFREE(pCtx, psz_rev);

	}

	/* fall through */
fail:
	SG_NULLFREE(pCtx, psz_rev);
	SG_NULLFREE(pCtx, psz_hid_cs);
	SG_REPO_NULLFREE(pCtx, pRepo);
}


void do_cmd_stamps(SG_context * pCtx)
{
    SG_repo* pRepo = NULL;
    SG_varray * pva_results = NULL;
    SG_uint32 count_results = 0;
    SG_uint32 index_results = 0;
    const char * psz_thisStamp = NULL;
    SG_int64 thisCount = 0;

    // TODO SG_ERR_CHECK?
	_get_repo_from_cwd(pCtx, &pRepo, NULL);

	SG_ERR_CHECK( SG_vc_stamps__list_all_stamps(pCtx, pRepo, &pva_results)  );

	SG_ERR_CHECK(  SG_varray__count(pCtx, pva_results, &count_results)  );
	for (index_results = 0; index_results < count_results; index_results++)
	{
		SG_vhash * pvhThisResult = NULL;
		SG_ERR_CHECK( SG_varray__get__vhash(pCtx, pva_results, index_results, &pvhThisResult)  );
		SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvhThisResult, "stamp", &psz_thisStamp)  );
		SG_ERR_CHECK(  SG_vhash__get__int64(pCtx, pvhThisResult, "count", &thisCount)  );
		SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDOUT, "%s:\t%d\n", psz_thisStamp, thisCount)  );
	}


	/* fall through */
fail:
	SG_VARRAY_NULLFREE(pCtx, pva_results);
	SG_REPO_NULLFREE(pCtx, pRepo);
}


/**
 * The DAGFRAG code now takes a generic FETCH-DAGNODE callback function
 * rather than assuming that it should call SG_repo__fetch_dagnode().
 * This allows the DAGFRAG code to not care whether it is above or below
 * the REPO API layer.
 *
 * my_fetch_dagnodes_from_repo() is a generic wrapper for
 * SG_repo__fetch_dagnode() and is used by the code in
 * sg_dagfrag to request dagnodes from disk.  we don't really
 * need this function since FN__sg_fetch_dagnode and
 * SG_repo__fetch_dagnode() have an equivalent prototype,
 * but with callbacks in loops calling callbacks calling callbacks,
 * it's easy to get lost....
 * 
 */
static FN__sg_fetch_dagnode my_fetch_dagnodes_from_repo;

void my_fetch_dagnodes_from_repo(SG_context * pCtx, void * pCtxFetchDagnode, const char * szHidDagnode, SG_dagnode ** ppDagnode)
{
	SG_ERR_CHECK_RETURN(  SG_repo__fetch_dagnode(pCtx, (SG_repo *)pCtxFetchDagnode,szHidDagnode,ppDagnode)  );
}

#if 0
/* TODO the "UI" (cmdline params) for this needs work.  What we have right now
 * is insufficient, but serves as a proof of concept.  We need a way to
 * specify how much history should be included.  Perhaps by number of revisions
 * back, or by date.  And we need a way to pick up the
 * tags/stamps/comments/etc. */
void do_cmd_fragball(SG_context * pCtx, const char* psz_filename, const char* psz_spec_cs)
{
	SG_pathname* pPathCwd = NULL;
    SG_pathname* pPath_fragball = NULL;
    SG_string* pstrRepoDescriptorName = NULL;
    SG_vhash* pvhDescriptor = NULL;
    SG_repo* pRepo = NULL;
    char* psz_hid_cs = NULL;
    SG_dagfrag* pFrag = NULL;
    SG_rbtree* prb_frags = NULL;
    char buf_dagnum[32];

	/* find the repo  - TODO: use _get_repo_from_cwd */
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC(pCtx, &pPathCwd)  );
	SG_ERR_CHECK(  SG_pathname__set__from_cwd(pCtx, pPathCwd)  );
	SG_ERR_CHECK(  SG_workingdir__find_mapping(pCtx, pPathCwd, NULL, &pstrRepoDescriptorName, NULL)  );

    SG_ERR_CHECK(  SG_closet__descriptors__get(pCtx, SG_string__sz(pstrRepoDescriptorName), &pvhDescriptor)  );
    SG_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, &pRepo, pvhDescriptor)  );

    /* lookup the changeset id */
    SG_ERR_CHECK(  SG_hidlookup__changeset(pCtx, pRepo, SG_DAGNUM__VERSION_CONTROL, psz_spec_cs, &psz_hid_cs)  );

    /* allocate a frag */
    SG_ERR_CHECK(  SG_dagfrag__alloc(pCtx, &pFrag, SG_DAGNUM__VERSION_CONTROL, my_fetch_dagnodes_from_repo, pRepo)  );
    SG_ERR_CHECK(  SG_dagfrag__add_leaf(pCtx, pFrag, psz_hid_cs, 2)  );

    /* write the fragball file */
    SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb_frags)  );
    SG_ERR_CHECK(  SG_dagnum__to_string(pCtx, SG_DAGNUM__VERSION_CONTROL, buf_dagnum, sizeof(buf_dagnum))  );
    SG_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb_frags, buf_dagnum, pFrag)  );
    SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_fragball, pPathCwd, psz_filename)  );
    SG_ERR_CHECK(  SG_fragball__write(pCtx, pRepo, pPath_fragball, prb_frags)  );

    /* fall through to common cleanup */

fail:
    SG_RBTREE_NULLFREE(pCtx, prb_frags);
    SG_DAGFRAG_NULLFREE(pCtx, pFrag);
    SG_NULLFREE(pCtx, psz_hid_cs);
    SG_VHASH_NULLFREE(pCtx, pvhDescriptor);
    SG_REPO_NULLFREE(pCtx, pRepo);
    SG_STRING_NULLFREE(pCtx, pstrRepoDescriptorName);
	SG_PATHNAME_NULLFREE(pCtx, pPath_fragball);
	SG_PATHNAME_NULLFREE(pCtx, pPathCwd);
}
#endif

void do_cmd_creategroup(SG_context * pCtx, const char* psz_name, const char* psz_descriptor_name)
{
    SG_vhash* pvh_descriptor = NULL;
    SG_repo* pRepo = NULL;
	SG_pathname* pPathCwd = NULL;

    if (psz_descriptor_name)
    {
        SG_ERR_CHECK(  SG_closet__descriptors__get(pCtx, psz_descriptor_name, &pvh_descriptor)  );
        SG_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, &pRepo, pvh_descriptor)  );
    }
    else
    {
        SG_ERR_CHECK(  _get_repo_from_cwd(pCtx, &pRepo, &pPathCwd)  );
    }

    SG_ERR_CHECK(  SG_group__create(pCtx, pRepo, psz_name)  );

    /* fall through */

fail:
    SG_PATHNAME_NULLFREE(pCtx, pPathCwd);
    SG_VHASH_NULLFREE(pCtx, pvh_descriptor);
    SG_REPO_NULLFREE(pCtx, pRepo);
}

#define MY_CMD_GROUP_OP__LIST              (1)
#define MY_CMD_GROUP_OP__ADD_USERS         (2)
#define MY_CMD_GROUP_OP__REMOVE_USERS      (3)
#define MY_CMD_GROUP_OP__ADD_SUBGROUPS     (4)
#define MY_CMD_GROUP_OP__REMOVE_SUBGROUPS  (5)

void do_cmd_group(SG_context * pCtx, const char* psz_group_name, const char* psz_descriptor_name, SG_uint32 op, const char** paszNames, SG_uint32 count_names)
{
    SG_vhash* pvh_descriptor = NULL;
    SG_repo* pRepo = NULL;
	SG_pathname* pPathCwd = NULL;
    SG_varray* pva = NULL;

    SG_ASSERT(paszNames);

    if (psz_descriptor_name)
    {
        SG_ERR_CHECK(  SG_closet__descriptors__get(pCtx, psz_descriptor_name, &pvh_descriptor)  );
        SG_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, &pRepo, pvh_descriptor)  );
    }
    else
    {
        SG_ERR_CHECK(  _get_repo_from_cwd(pCtx, &pRepo, &pPathCwd)  );
    }

    switch (op)
    {
        case MY_CMD_GROUP_OP__LIST:
            {
                SG_uint32 count_members = 0;
                SG_uint32 i = 0;

                if (count_names)
                {
                    // TODO error

                }

                SG_ERR_CHECK(  SG_group__list_members(pCtx, pRepo, psz_group_name, &pva)  );
                SG_ERR_CHECK(  SG_varray__count(pCtx, pva, &count_members)  );
                for (i=0; i<count_members; i++)
                {
                    SG_vhash* pvh = NULL;
                    const char* psz_member_type = NULL;
                    const char* psz_member_name = NULL;

                    SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pva, i, &pvh)  );

                    SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh, "type", &psz_member_type)  );
                    SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh, "name", &psz_member_name)  );
                    SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDOUT, "%s: %s\n", psz_member_type, psz_member_name)  );
                }
            }
            break;

        case MY_CMD_GROUP_OP__ADD_USERS:
            {
                SG_ERR_CHECK(  SG_group__add_users(pCtx, pRepo, psz_group_name, paszNames, count_names)  );
            }
            break;

        case MY_CMD_GROUP_OP__REMOVE_USERS:
            {
                SG_ERR_CHECK(  SG_group__remove_users(pCtx, pRepo, psz_group_name, paszNames, count_names)  );
            }
            break;

        case MY_CMD_GROUP_OP__ADD_SUBGROUPS:
            {
                SG_ERR_CHECK(  SG_group__add_subgroups(pCtx, pRepo, psz_group_name, paszNames, count_names)  );
            }
            break;

        case MY_CMD_GROUP_OP__REMOVE_SUBGROUPS:
            {
                SG_ERR_CHECK(  SG_group__remove_subgroups(pCtx, pRepo, psz_group_name, paszNames, count_names)  );
            }
            break;

        default:
            {
                SG_ERR_THROW(  SG_ERR_NOTIMPLEMENTED  );
            }
            break;
    }

    /* fall through */

fail:
    SG_PATHNAME_NULLFREE(pCtx, pPathCwd);
    SG_VHASH_NULLFREE(pCtx, pvh_descriptor);
    SG_REPO_NULLFREE(pCtx, pRepo);
}

void do_cmd_set_whoami(SG_context * pCtx, const char* psz_email, const char* psz_descriptor_name)
{
    SG_vhash* pvh_descriptor = NULL;
    SG_repo* pRepo = NULL;
	SG_pathname* pPathCwd = NULL;
    char* psz_admin_id = NULL;

    if (psz_descriptor_name)
    {
        SG_ERR_CHECK(  SG_closet__descriptors__get(pCtx, psz_descriptor_name, &pvh_descriptor)  );
        SG_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, &pRepo, pvh_descriptor)  );
    }
    else
    {
		_get_repo_from_cwd(pCtx, &pRepo, NULL);
		if (SG_context__err_equals(pCtx, SG_ERR_NOT_A_WORKING_COPY))
		{
			SG_context__err_reset(pCtx);
            pRepo = NULL;
		}

		SG_ERR_CHECK_CURRENT;
    }

    SG_ERR_CHECK(  SG_user__set_user__repo(pCtx, pRepo, psz_email)  );

    /* fall through */

fail:
    SG_NULLFREE(pCtx, psz_admin_id);
    SG_PATHNAME_NULLFREE(pCtx, pPathCwd);
    SG_VHASH_NULLFREE(pCtx, pvh_descriptor);
    SG_REPO_NULLFREE(pCtx, pRepo);
}

void do_cmd_createuser(SG_context * pCtx, const char* psz_email, const char* psz_descriptor_name)
{
    SG_vhash* pvh_descriptor = NULL;
    SG_repo* pRepo = NULL;
	SG_pathname* pPathCwd = NULL;

    if (psz_descriptor_name)
    {
        SG_ERR_CHECK(  SG_closet__descriptors__get(pCtx, psz_descriptor_name, &pvh_descriptor)  );
        SG_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, &pRepo, pvh_descriptor)  );
    }
    else
    {
        SG_ERR_CHECK(  _get_repo_from_cwd(pCtx, &pRepo, &pPathCwd)  );
    }

    SG_ERR_CHECK(  SG_user__create(pCtx, pRepo, psz_email)  );

    /* fall through */

fail:
    SG_PATHNAME_NULLFREE(pCtx, pPathCwd);
    SG_VHASH_NULLFREE(pCtx, pvh_descriptor);
    SG_REPO_NULLFREE(pCtx, pRepo);
}

void do_cmd_groups(SG_context * pCtx, const char* psz_descriptor_name)
{
    SG_vhash* pvh_descriptor = NULL;
    SG_repo* pRepo = NULL;
	SG_pathname* pPathCwd = NULL;
    SG_varray* pva_groups = NULL;
    SG_uint32 i = 0;
    SG_uint32 count = 0;

    if (psz_descriptor_name)
    {
        SG_ERR_CHECK(  SG_closet__descriptors__get(pCtx, psz_descriptor_name, &pvh_descriptor)  );
        SG_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, &pRepo, pvh_descriptor)  );
    }
    else
    {
        SG_ERR_CHECK(  _get_repo_from_cwd(pCtx, &pRepo, &pPathCwd)  );
    }

    SG_ERR_CHECK(  SG_group__list(pCtx, pRepo, &pva_groups)  );
    SG_ERR_CHECK(  SG_varray__count(pCtx, pva_groups, &count)  );
    for (i=0; i<count; i++)
    {
        SG_vhash* pvh = NULL;
        const char* psz_name = NULL;

        SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pva_groups, i, &pvh)  );

        SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh, "name", &psz_name)  );
        SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDOUT, "%s\n", psz_name)  );
    }

    /* fall through */

fail:
    SG_VARRAY_NULLFREE(pCtx, pva_groups);
    SG_PATHNAME_NULLFREE(pCtx, pPathCwd);
    SG_VHASH_NULLFREE(pCtx, pvh_descriptor);
    SG_REPO_NULLFREE(pCtx, pRepo);
}

void do_cmd_users(SG_context * pCtx, const char* psz_descriptor_name)
{
    SG_vhash* pvh_descriptor = NULL;
    SG_repo* pRepo = NULL;
	SG_pathname* pPathCwd = NULL;
    SG_varray* pva_users = NULL;
    SG_uint32 i = 0;
    SG_uint32 count = 0;

    if (psz_descriptor_name)
    {
        SG_ERR_CHECK(  SG_closet__descriptors__get(pCtx, psz_descriptor_name, &pvh_descriptor)  );
        SG_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, &pRepo, pvh_descriptor)  );
    }
    else
    {
        SG_ERR_CHECK(  _get_repo_from_cwd(pCtx, &pRepo, &pPathCwd)  );
    }

    SG_ERR_CHECK(  SG_user__list(pCtx, pRepo, &pva_users)  );
    SG_ERR_CHECK(  SG_varray__count(pCtx, pva_users, &count)  );
    for (i=0; i<count; i++)
    {
        SG_vhash* pvh = NULL;
        const char* psz_email = NULL;

        SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pva_users, i, &pvh)  );

        SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh, "email", &psz_email)  );
        SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDOUT, "%s\n", psz_email)  );
    }

    /* fall through */

fail:
    SG_VARRAY_NULLFREE(pCtx, pva_users);
    SG_PATHNAME_NULLFREE(pCtx, pPathCwd);
    SG_VHASH_NULLFREE(pCtx, pvh_descriptor);
    SG_REPO_NULLFREE(pCtx, pRepo);
}

void do_cmd_vacuum(SG_context * pCtx, const char* psz_descriptor_name)
{
    SG_vhash* pvh_descriptor = NULL;
    SG_repo* pRepo = NULL;

    SG_ERR_CHECK(  SG_closet__descriptors__get(pCtx, psz_descriptor_name, &pvh_descriptor)  );
    SG_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, &pRepo, pvh_descriptor)  );

    SG_ERR_CHECK(  SG_repo__vacuum(pCtx, pRepo)  );

    /* fall through */

fail:
    SG_VHASH_NULLFREE(pCtx, pvh_descriptor);
    SG_REPO_NULLFREE(pCtx, pRepo);
}

void do_cmd_vpack(SG_context * pCtx, const char* psz_descriptor_name)
{
    SG_vhash* pvh_descriptor = NULL;
    SG_repo* pRepo = NULL;

    SG_ERR_CHECK(  SG_closet__descriptors__get(pCtx, psz_descriptor_name, &pvh_descriptor)  );
    SG_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, &pRepo, pvh_descriptor)  );

    SG_ERR_CHECK(  SG_repo__pack__vcdiff(pCtx, pRepo)  );

    /* fall through */

fail:
    SG_VHASH_NULLFREE(pCtx, pvh_descriptor);
    SG_REPO_NULLFREE(pCtx, pRepo);
}

void do_cmd_zpack(SG_context * pCtx, const char* psz_descriptor_name)
{
    SG_vhash* pvh_descriptor = NULL;
    SG_repo* pRepo = NULL;

    SG_ERR_CHECK(  SG_closet__descriptors__get(pCtx, psz_descriptor_name, &pvh_descriptor)  );
    SG_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, &pRepo, pvh_descriptor)  );

    SG_ERR_CHECK(  SG_repo__pack__zlib(pCtx, pRepo)  );

    /* fall through */

fail:
    SG_VHASH_NULLFREE(pCtx, pvh_descriptor);
    SG_REPO_NULLFREE(pCtx, pRepo);
}


void do_cmd_unpack(SG_context * pCtx, const char* psz_descriptor_name)
{
    SG_vhash* pvh_descriptor = NULL;
    SG_repo* pRepo = NULL;

    SG_ERR_CHECK(  SG_closet__descriptors__get(pCtx, psz_descriptor_name, &pvh_descriptor)  );
    SG_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, &pRepo, pvh_descriptor)  );

    SG_ERR_CHECK(  SG_repo__unpack(pCtx, pRepo, SG_BLOBENCODING__VCDIFF)  );
    SG_ERR_CHECK(  SG_repo__unpack(pCtx, pRepo, SG_BLOBENCODING__ZLIB)  );

    /* fall through */

fail:
    SG_VHASH_NULLFREE(pCtx, pvh_descriptor);
    SG_REPO_NULLFREE(pCtx, pRepo);
}


void do_cmd_blobcount(SG_context * pCtx, const char* psz_descriptor_name)
{
    SG_vhash* pvh_descriptor = NULL;
    SG_repo* pRepo = NULL;
    SG_uint32 count_blobs_all = 0;
    SG_uint32 count_blobs_full = 0;
    SG_uint32 count_blobs_alwaysfull = 0;
    SG_uint32 count_blobs_zlib = 0;
    SG_uint32 count_blobs_vcdiff = 0;
    SG_uint64 total_blob_size_full = 0;
    SG_uint64 total_blob_size_alwaysfull = 0;
    SG_uint64 total_blob_size_zlib_full = 0;
    SG_uint64 total_blob_size_zlib_encoded = 0;
    SG_uint64 total_blob_size_vcdiff_full = 0;
    SG_uint64 total_blob_size_vcdiff_encoded = 0;
    SG_uint64 total_blob_size_all_full = 0;
    SG_uint64 total_blob_size_all_encoded = 0;
    SG_int_to_string_buffer buf;

    SG_ERR_CHECK(  SG_closet__descriptors__get(pCtx, psz_descriptor_name, &pvh_descriptor)  );
    SG_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, &pRepo, pvh_descriptor)  );

    SG_ERR_CHECK(  SG_repo__get_blob_stats(pCtx, pRepo,
                &count_blobs_full,
                &count_blobs_alwaysfull,
                &count_blobs_zlib,
                &count_blobs_vcdiff,
                &total_blob_size_full,
                &total_blob_size_alwaysfull,
                &total_blob_size_zlib_full,
                &total_blob_size_zlib_encoded,
                &total_blob_size_vcdiff_full,
                &total_blob_size_vcdiff_encoded)  );

    printf("full\n");
    printf("%12s  %d\n", "count", count_blobs_full);
    printf("%12s  %12s\n", "total", SG_int64_to_sz(total_blob_size_full, buf));

    printf("alwaysfull\n");
    printf("%12s  %d\n", "count", count_blobs_alwaysfull);
    printf("%12s  %12s\n", "total", SG_int64_to_sz(total_blob_size_alwaysfull, buf));

    printf("zlib\n");
    printf("%12s  %d\n", "count", count_blobs_zlib);
    printf("%12s  %12s\n", "full", SG_int64_to_sz(total_blob_size_zlib_full,buf));
    printf("%12s  %12s\n", "encoded", SG_int64_to_sz(total_blob_size_zlib_encoded,buf));
    if (total_blob_size_zlib_full)
    {
        printf("%12s  %d%%\n", "saved", (int) ((total_blob_size_zlib_full - total_blob_size_zlib_encoded) / (double) total_blob_size_zlib_full * 100.0));
    }

    printf("vcdiff\n");
    printf("%12s  %d\n", "count", count_blobs_vcdiff);
    printf("%12s  %12s\n", "full", SG_int64_to_sz(total_blob_size_vcdiff_full,buf));
    printf("%12s  %12s\n", "encoded", SG_int64_to_sz(total_blob_size_vcdiff_encoded,buf));
    if (total_blob_size_vcdiff_full)
    {
        printf("%12s  %d%%\n", "saved", (int) ((total_blob_size_vcdiff_full - total_blob_size_vcdiff_encoded) / (double) total_blob_size_vcdiff_full * 100.0));
    }

    count_blobs_all = count_blobs_full + count_blobs_zlib + count_blobs_vcdiff;
    total_blob_size_all_full = total_blob_size_full + total_blob_size_zlib_full + total_blob_size_vcdiff_full;
    total_blob_size_all_encoded = total_blob_size_full + total_blob_size_zlib_encoded + total_blob_size_vcdiff_encoded;

    printf("all\n");
    printf("%12s  %d\n", "count", count_blobs_all);
    printf("%12s  %12s\n", "full", SG_int64_to_sz(total_blob_size_all_full,buf));
    printf("%12s  %12s\n", "encoded", SG_int64_to_sz(total_blob_size_all_encoded,buf));
    printf("%12s  %d%%\n", "saved", (int) ((total_blob_size_all_full - total_blob_size_all_encoded) / (double) total_blob_size_all_full * 100.0));

    /* fall through */

fail:
    SG_VHASH_NULLFREE(pCtx, pvh_descriptor);
    SG_REPO_NULLFREE(pCtx, pRepo);
}

#if 0
void do_cmd_slurp(SG_context * pCtx, const char* psz_descriptor_name, const char* psz_fragball_name)
{
    SG_pathname* pPath_fragball = NULL;
    SG_vhash* pvh_descriptor = NULL;
    SG_repo* pRepo = NULL;

	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx, &pPath_fragball, psz_fragball_name)  );

    SG_ERR_CHECK(  SG_closet__descriptors__get(pCtx, psz_descriptor_name, &pvh_descriptor)  );
    SG_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, &pRepo, pvh_descriptor)  );

    SG_ERR_CHECK(  SG_fragball__slurp(pCtx, pRepo, pPath_fragball)  );

    /* fall through */

fail:
    SG_VHASH_NULLFREE(pCtx, pvh_descriptor);
    SG_REPO_NULLFREE(pCtx, pRepo);
    SG_PATHNAME_NULLFREE(pCtx, pPath_fragball);
}
#endif

void do_cmd_clone(SG_context * pCtx, const char* psz_existing, const char* psz_new)
{
	SG_client* pClient = NULL;

	SG_ERR_CHECK(  SG_client__open(pCtx, psz_existing, NULL_CREDENTIAL, &pClient)  );

	SG_ERR_CHECK(  SG_repo__create_empty_clone_from_remote(pCtx, pClient, psz_new)  );

	SG_ERR_CHECK(  SG_pull__clone(pCtx, psz_new, pClient)  );

	SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDOUT, "Use 'vv checkout %s <path>' to get a working copy.\n", psz_new)  );

	/* fall through*/
fail:
	SG_CLIENT_NULLFREE(pCtx, pClient);
}

void do_cmd_comment(SG_context * pCtx, const char* psz_spec_cs, SG_bool bRev, const char* psz_repo, const char* psz_comment)
{
    SG_repo* pRepo = NULL;
    char* psz_hid_cs = NULL;
    SG_audit q;
	
	// no repo given, try to assoc one with the current wd
	if (!psz_repo)
	{
		_get_repo_from_cwd(pCtx, &pRepo, NULL);
		if (SG_context__err_equals(pCtx, SG_ERR_NOT_A_WORKING_COPY))
		{
			SG_context__err_reset(pCtx);
			SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "There is no repository associated with the current directory.  Please provide a repository.\n")  );
			return;
		}

		SG_ERR_CHECK_CURRENT;
	}

    SG_ERR_CHECK(  SG_audit__init(pCtx,&q,pRepo,SG_AUDIT__WHEN__NOW,SG_AUDIT__WHO__FROM_SETTINGS)  );

	if (bRev)
		SG_ERR_CHECK(  SG_repo__hidlookup__dagnode(pCtx, pRepo, SG_DAGNUM__VERSION_CONTROL, psz_spec_cs, &psz_hid_cs)  );
	else
		SG_ERR_CHECK(  SG_vc_tags__lookup__tag(pCtx, pRepo, psz_spec_cs, &psz_hid_cs)  );
    SG_ERR_CHECK(  SG_vc_comments__add(pCtx, pRepo, psz_hid_cs, psz_comment, &q)  );
    SG_NULLFREE(pCtx, psz_hid_cs);

    SG_REPO_NULLFREE(pCtx, pRepo);

	return;

fail:
    SG_NULLFREE(pCtx, psz_hid_cs);
    SG_REPO_NULLFREE(pCtx, pRepo);
}

void do_cmd_tag(SG_context * pCtx, const char* psz_spec_cs, SG_bool bRev, SG_bool bForce, const char** ppszTags, SG_uint32 count_args)
{
    SG_repo* pRepo = NULL;
    SG_pathname* pPathCwd = NULL;
    SG_ERR_CHECK(  _get_repo_from_cwd(pCtx, &pRepo, &pPathCwd)  );
    SG_ERR_CHECK(  SG_tag__add_tags(pCtx, pRepo, NULL, psz_spec_cs, bRev, bForce, ppszTags, count_args)  );
fail:
    SG_PATHNAME_NULLFREE(pCtx, pPathCwd);
    SG_REPO_NULLFREE(pCtx, pRepo);
}

void do_cmd_dump_tags(SG_context * pCtx)
{
    SG_rbtree* prbTags = NULL;
    SG_rbtree_iterator* pIterator = NULL;
    const char* psz_tag = NULL;
    SG_bool b;
    SG_repo* pRepo = NULL;

	SG_ERR_CHECK(  _get_repo_from_cwd(pCtx, &pRepo, NULL)  );
    SG_ERR_CHECK(  SG_vc_tags__list(pCtx, pRepo, &prbTags)  );
	SG_REPO_NULLFREE(pCtx, pRepo);

    if (prbTags)
    {
        SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pIterator, prbTags, &b, &psz_tag, NULL)  );
        while (b)
        {
            SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDOUT, "%s\n", psz_tag)  );

            SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pIterator, &b, &psz_tag, NULL)  );
        }
        SG_rbtree__iterator__free(pCtx, pIterator);
        pIterator = NULL;
        SG_RBTREE_NULLFREE(pCtx, prbTags);
    }

	return;

fail:
    SG_REPO_NULLFREE(pCtx, pRepo);
    SG_RBTREE_NULLFREE(pCtx, prbTags);
}

void do_cmd_add(SG_context * pCtx, SG_uint32 count_args, const char** paszArgs, SG_bool bIgnoreWarnings, SG_bool bRecursive, const char* const* ppszIncludes, SG_uint32 nCountIncludes, const char* const* ppszExcludes, SG_uint32 nCountExcludes)
{
	SG_pathname* pPathCwd = NULL;
	SG_pendingtree* pPendingTree = NULL;

	SG_ERR_CHECK(  SG_PATHNAME__ALLOC(pCtx, &pPathCwd)  );
	SG_ERR_CHECK(  SG_pathname__set__from_cwd(pCtx, pPathCwd)  );

	SG_ERR_CHECK(  SG_pendingtree__alloc(pCtx, pPathCwd, bIgnoreWarnings, &pPendingTree)  );
	
	SG_ERR_CHECK(  SG_pendingtree__add(pCtx, pPendingTree, pPathCwd, count_args, paszArgs, bRecursive, ppszIncludes, nCountIncludes, ppszExcludes, nCountExcludes)  );
	
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

	SG_PATHNAME_NULLFREE(pCtx, pPathCwd);

	return;

fail:
	if (pPendingTree && SG_context__err_equals(pCtx,SG_ERR_PORTABILITY_WARNINGS))
		SG_ERR_IGNORE(  sg_report_portability_warnings(pCtx,pPendingTree)  );
	SG_PATHNAME_NULLFREE(pCtx, pPathCwd);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

	//FREE the includes and excludes in the calling function
}

#define sg_NUM_CHARS_FOR_DOT 7

static SG_dagfrag__foreach_member_callback my_frag_cb;

static void my_frag_cb(SG_context * pCtx, const SG_dagnode * pDagnode, SG_UNUSED_PARAM(SG_dagfrag_state qs), SG_UNUSED_PARAM(void * pVoidMyDotData))
{
    const char* pszHid = NULL;
    char bufHid_child[1 + sg_NUM_CHARS_FOR_DOT];
    SG_uint32 count;
    SG_uint32 i;
    const char** ppszParents = NULL;
    const char* pszLabel = NULL;

    SG_UNUSED(qs);
    SG_UNUSED(pVoidMyDotData);

    SG_ERR_CHECK(  SG_dagnode__get_id_ref(pCtx, pDagnode, &pszHid)  );
    
    /* first write out the label for this node.  If the node has a tag, we
     * display that.  Otherwise, we display the HID, truncated for brevity. */

    if (!pszLabel)
    {
        /* for display purposes, we truncate each ID to fewer characters */
        memcpy(bufHid_child, pszHid, sg_NUM_CHARS_FOR_DOT);
        bufHid_child[sg_NUM_CHARS_FOR_DOT] = 0;
        pszLabel = bufHid_child;
    }

    SG_console(pCtx, SG_CS_STDOUT, "_%s [label=\"%s\"];\n", pszHid, pszLabel);

    /* dot can't handle ids which start with a digit, so we prefix everything
     * with an underscore */

    /* now write out all the edges from this node to its parents */
    SG_ERR_CHECK(  SG_dagnode__get_parents(pCtx, pDagnode, &count, &ppszParents)  );
    for (i=0; i<count; i++)
    {
        SG_console(pCtx, SG_CS_STDOUT, "_%s -> _%s;\n", pszHid, ppszParents[i]);
    }

    SG_NULLFREE(pCtx, ppszParents);

    return;
    
fail:
    return;
}

static void _auto_merge_one_zing_dag(
	SG_context* pCtx,
	SG_repo* pRepo,
	SG_uint32 dagnum,
	const SG_audit* pAudit,
	SG_varray** ppvaErrors,
	SG_varray** ppvaLog)
{
	SG_varray* pvaErrors = NULL;
	SG_varray* pvaLog = NULL;
	char* pszLeaf = NULL;
	SG_varray* pvaReturnErrors = NULL;
	SG_varray* pvaReturnLog = NULL;

	if (SG_DAGNUM__IS_DB(dagnum))
	{
#if TRACE_PULL
		SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDOUT, "Auto-merging zing dag %d\n", dagnum)  );
#endif
		SG_ERR_CHECK(  SG_zing__get_leaf__merge_if_necessary(pCtx, pRepo, pAudit, dagnum, &pszLeaf, &pvaErrors, &pvaLog)  );
		SG_NULLFREE(pCtx, pszLeaf); // TODO: make zing handle null leaf arg
	}

	if (pvaErrors)
	{
		SG_uint32 errorIdx, errorCount;

		SG_ERR_CHECK(  SG_varray__count(pCtx, pvaErrors, &errorCount)  );
		if (errorCount > 0)
		{
			if (!pvaReturnErrors)
				SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pvaReturnErrors)  );
			for (errorIdx = 0; errorIdx < errorCount; errorIdx++)
			{
				const char* psz;
				SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pvaErrors, errorIdx, &psz)  );
				SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pvaReturnErrors, psz)  );
			}
		}
		SG_VARRAY_NULLFREE(pCtx, pvaErrors);
	}
	if (pvaLog)
	{
		SG_uint32 logIdx, logCount;

		SG_ERR_CHECK(  SG_varray__count(pCtx, pvaLog, &logCount)  );
		if (logCount > 0)
		{
			if (!pvaReturnLog)
				SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pvaReturnLog)  );
			for (logIdx = 0; logIdx < logCount; logIdx++)
			{
				const char* psz;
				SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pvaLog, logIdx, &psz)  );
				SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pvaReturnLog, psz)  );
			}

		}
		SG_VARRAY_NULLFREE(pCtx, pvaLog);
	}

	SG_RETURN_AND_NULL(pvaReturnErrors, ppvaErrors);
	SG_RETURN_AND_NULL(pvaReturnLog, ppvaLog);

	/* fall through */
fail:
	SG_VARRAY_NULLFREE(pCtx, pvaErrors);
	SG_VARRAY_NULLFREE(pCtx, pvaLog);
	SG_VARRAY_NULLFREE(pCtx, pvaReturnErrors);
	SG_VARRAY_NULLFREE(pCtx, pvaReturnLog);
	SG_NULLFREE(pCtx, pszLeaf);
}

// TODO this was swiped from the pull code
static void _auto_merge_all_zing_dags(
	SG_context* pCtx,
	SG_repo* pRepo,
	SG_varray** ppvaErrors,
	SG_varray** ppvaLog)
{
	SG_uint32 i, count;
	SG_uint32* dagnumArray = NULL;
	SG_audit audit;

	SG_ERR_CHECK(  SG_audit__init(pCtx, &audit, pRepo, SG_AUDIT__WHEN__NOW, SG_AUDIT__WHO__FROM_SETTINGS)  );


	SG_ERR_CHECK(  SG_repo__list_dags(pCtx, pRepo, &count, &dagnumArray)  );
    // Do all the audit dags first, because other dags rely on finding a single
    // leaf in the audit dag.
	for (i = 0; i < count; i++)
	{
		SG_uint32 dagnum = dagnumArray[i];
        if (SG_DAGNUM__IS_AUDIT(dagnum))
        {
            SG_ERR_CHECK(  _auto_merge_one_zing_dag(pCtx, pRepo, dagnum, &audit, ppvaErrors, ppvaLog)  );
        }

	}

	for (i = 0; i < count; i++)
	{
		SG_uint32 dagnum = dagnumArray[i];
		if ( 
                !SG_DAGNUM__IS_AUDIT(dagnum) 
                && SG_DAGNUM__IS_DB(dagnum)
           )
        {
			SG_ERR_CHECK(  _auto_merge_one_zing_dag(pCtx, pRepo, dagnum, &audit, ppvaErrors, ppvaLog)  );
        }
	}

	/* fall through */
fail:
	SG_NULLFREE(pCtx, dagnumArray);
}


void do_zingmerge(SG_context * pCtx, const char* psz_descriptor_name)
{
    SG_vhash* pvh_descriptor = NULL;
    SG_repo* pRepo = NULL;
	SG_pathname* pPathCwd = NULL;
    char* psz_admin_id = NULL;
    SG_varray* pva_errors = NULL;
    SG_varray* pva_log = NULL;

    if (psz_descriptor_name)
    {
        SG_ERR_CHECK(  SG_closet__descriptors__get(pCtx, psz_descriptor_name, &pvh_descriptor)  );
        SG_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, &pRepo, pvh_descriptor)  );
    }
    else
    {
        SG_ERR_CHECK(  _get_repo_from_cwd(pCtx, &pRepo, &pPathCwd)  );
    }

    SG_ERR_CHECK(  _auto_merge_all_zing_dags(pCtx, pRepo, &pva_errors, &pva_log)  );
    // TODO dump errors and log

    /* fall through */

fail:
    SG_NULLFREE(pCtx, psz_admin_id);
    SG_PATHNAME_NULLFREE(pCtx, pPathCwd);
    SG_VHASH_NULLFREE(pCtx, pvh_descriptor);
    SG_REPO_NULLFREE(pCtx, pRepo);
}

void do_dump_json(SG_context * pCtx, const char* psz_spec_hid)
{
    SG_repo* pRepo = NULL;
    SG_vhash* pvh = NULL;
    SG_string* pstr = NULL;
    char* psz_hid = NULL;

    /* open the repo */
	SG_ERR_CHECK(  _get_repo_from_cwd(pCtx, &pRepo, NULL)  );

    /* lookup the blob id */
    SG_ERR_CHECK(  SG_repo__hidlookup__blob(pCtx, pRepo, psz_spec_hid, &psz_hid)  );

    /* fetch the blob */
    SG_ERR_CHECK(  SG_repo__fetch_vhash(pCtx, pRepo, psz_hid, &pvh)  );
    SG_REPO_NULLFREE(pCtx, pRepo);

    /* print it out.  fix the CRLF */
    SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstr)  );
    SG_ERR_CHECK(  SG_vhash__to_json(pCtx, pvh, pstr)  );
    SG_VHASH_NULLFREE(pCtx, pvh);

    SG_ERR_CHECK(  SG_string__replace_bytes(pCtx, pstr,
                                          (SG_byte *)"\r\n",2,
                                          (SG_byte *)"\n",1,
                                          SG_UINT32_MAX,SG_TRUE,NULL)  );
    
    SG_console(pCtx, SG_CS_STDOUT, "%s", SG_string__sz(pstr));
    
    SG_NULLFREE(pCtx, psz_hid);
    SG_STRING_NULLFREE(pCtx, pstr);

	return;

fail:
    SG_NULLFREE(pCtx, psz_hid);
    SG_REPO_NULLFREE(pCtx, pRepo);
    SG_STRING_NULLFREE(pCtx, pstr);
    SG_VHASH_NULLFREE(pCtx, pvh);
}

void do_cmd_dump_json(SG_context * pCtx, SG_uint32 count_args, const char** paszArgs)
{
    SG_uint32 i;

    for (i=0; i<count_args; i++)
    {
        SG_ERR_CHECK_RETURN(  do_dump_json(pCtx, paszArgs[i])  );
    }
}

void do_dump_dag(SG_context * pCtx, const char* pszDescriptorName, SG_uint32 iDagNum)
{
    SG_repo* pRepo = NULL;
    SG_rbtree* prbLeaves = NULL;
    SG_vhash* pvhRepoDescriptor = NULL;
    SG_dagfrag* pFrag = NULL;
    char* psz_repo_id = NULL;
    char* psz_admin_id = NULL;

    /* open the repo */
    SG_ERR_CHECK(  SG_closet__descriptors__get(pCtx, pszDescriptorName, &pvhRepoDescriptor)  );
    SG_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, &pRepo, pvhRepoDescriptor)  );

    /* TODO don't hard-code 100 here.  But what should we do instead? */

    /* fetch the DAG, starting with the leaves, to 100 generations */
    if (0 == iDagNum)
    {
        iDagNum = SG_DAGNUM__VERSION_CONTROL;
    }

    SG_ERR_CHECK(  SG_repo__fetch_dag_leaves(pCtx, pRepo, iDagNum, &prbLeaves)  );
    SG_ERR_CHECK(  SG_repo__get_repo_id(pCtx, pRepo, &psz_repo_id)  );
    SG_ERR_CHECK(  SG_repo__get_admin_id(pCtx, pRepo, &psz_admin_id)  );
    SG_ERR_CHECK(  SG_dagfrag__alloc(pCtx, &pFrag, psz_repo_id, psz_admin_id, iDagNum)  );
    SG_ERR_CHECK(  SG_dagfrag__load_from_repo__multi(pCtx, pFrag, pRepo, prbLeaves, 100)  );

    /* now write the DAG in graphiz dot format */
    SG_console(pCtx, SG_CS_STDOUT, "digraph %s\n", pszDescriptorName);
    SG_console(pCtx, SG_CS_STDOUT, "{\n");
    SG_ERR_CHECK(  SG_dagfrag__foreach_member(pCtx, pFrag, my_frag_cb, NULL)  );
    SG_console(pCtx, SG_CS_STDOUT, "}\n");
    
    SG_REPO_NULLFREE(pCtx, pRepo);
    SG_DAGFRAG_NULLFREE(pCtx, pFrag);
    SG_RBTREE_NULLFREE(pCtx, prbLeaves);
    SG_VHASH_NULLFREE(pCtx, pvhRepoDescriptor);

	return;

fail:
    SG_REPO_NULLFREE(pCtx, pRepo);
    SG_DAGFRAG_NULLFREE(pCtx, pFrag);
    SG_RBTREE_NULLFREE(pCtx, prbLeaves);
}

void do_cmd_dump_dag(SG_context * pCtx, SG_uint32 count_args, const char** paszArgs)
{
	SG_pathname* pPathCwd = NULL;
    SG_string* pstrRepoDescriptorName = NULL;

    if (count_args == 0)
    {
        SG_ERR_CHECK(  SG_PATHNAME__ALLOC(pCtx, &pPathCwd)  );
        SG_ERR_CHECK(  SG_pathname__set__from_cwd(pCtx, pPathCwd)  );
        SG_ERR_CHECK(  SG_workingdir__find_mapping(pCtx, pPathCwd, NULL, &pstrRepoDescriptorName, NULL)  );
        SG_PATHNAME_NULLFREE(pCtx, pPathCwd);
        SG_ERR_CHECK(  do_dump_dag(pCtx, SG_string__sz(pstrRepoDescriptorName),SG_DAGNUM__VERSION_CONTROL)  );
        SG_STRING_NULLFREE(pCtx, pstrRepoDescriptorName);
    }
    else if (count_args == 2)
    {
        SG_ERR_CHECK(  do_dump_dag(pCtx, paszArgs[0],(SG_uint16) atoi(paszArgs[1]))  );
    }

    return;

fail:
    SG_PATHNAME_NULLFREE(pCtx, pPathCwd);
    SG_STRING_NULLFREE(pCtx, pstrRepoDescriptorName);
}

struct _add_significant_lca_edge_data
{
	const char * pszHid_ancestor;
};

SG_rbtree_foreach_callback _add_significant_lca_edge_cb;

void _add_significant_lca_edge_cb(SG_context * pCtx,
								  const char * pszHidDescendant,
								  SG_UNUSED_PARAM(void * assocData),
								  void * pVoidEdgeData)
{
	struct _add_significant_lca_edge_data * pEdgeData = (struct _add_significant_lca_edge_data *)pVoidEdgeData;

	SG_UNUSED(assocData);
	
	SG_ERR_CHECK_RETURN(  SG_console(pCtx, SG_CS_STDOUT, "_%s -> _%s;\n", pszHidDescendant, pEdgeData->pszHid_ancestor)  );
}

struct _add_significant_lca_node_data
{
	SG_daglca * pDagLca;
};

SG_daglca__foreach_callback _add_significant_lca_node_cb;

void _add_significant_lca_node_cb(SG_context * pCtx,
								  const char * pszHid,
								  SG_UNUSED_PARAM(SG_daglca_node_type nodeType),
								  SG_UNUSED_PARAM(SG_int32 generation),
								  SG_rbtree * prbImmediateDescendants,	// we must free this
								  SG_UNUSED_PARAM(void * pVoidNodeData))
{
	char bufHid_ancestor[1 + sg_NUM_CHARS_FOR_DOT];
    const char* pszLabel = NULL;

	SG_UNUSED(nodeType);		// TODO consider using this to change the shape of the node on the graph.
	SG_UNUSED(generation);
	SG_UNUSED(pVoidNodeData);

    if (!pszLabel)
    {
        /* for display purposes, we truncate each ID to fewer characters */
        memcpy(bufHid_ancestor, pszHid, sg_NUM_CHARS_FOR_DOT);
        bufHid_ancestor[sg_NUM_CHARS_FOR_DOT] = 0;
        pszLabel = bufHid_ancestor;
    }

    /* dot can't handle ids which start with a digit, so we prefix everything
     * with an underscore */

    SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDOUT, "_%s [label=\"%s\"];\n", pszHid, pszLabel)  );

    /* now write out all the edges from this node to its immediate descendants */

	if (prbImmediateDescendants)
	{
		struct _add_significant_lca_edge_data EdgeData;

		EdgeData.pszHid_ancestor = pszHid;

		SG_ERR_CHECK(  SG_rbtree__foreach(pCtx,
										  prbImmediateDescendants,
										  _add_significant_lca_edge_cb,
										  &EdgeData)  );

		SG_RBTREE_NULLFREE(pCtx, prbImmediateDescendants);
	}

	return;

fail:
	SG_RBTREE_NULLFREE(pCtx, prbImmediateDescendants);
}


void do_dump_lca(SG_context * pCtx, const char* pszDescriptorName, SG_uint32 iDagNum)
{
    SG_repo* pRepo = NULL;
    SG_rbtree* prbLeaves = NULL;
    SG_vhash* pvhRepoDescriptor = NULL;
	SG_daglca * pDagLca = NULL;

    /* open the repo */
    SG_ERR_CHECK(  SG_closet__descriptors__get(pCtx, pszDescriptorName, &pvhRepoDescriptor)  );
    SG_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, &pRepo, pvhRepoDescriptor)  );

	/* use the current leaves in the dag.
	 *
	 * TODO later let the user specify a set of leaves -- like they would be
	 * TODO allowed to do on a merge.
	 */

    if (0 == iDagNum)
    {
        iDagNum = SG_DAGNUM__VERSION_CONTROL;
    }

    SG_ERR_CHECK(  SG_repo__fetch_dag_leaves(pCtx, pRepo, iDagNum, &prbLeaves)  );
    SG_ERR_CHECK(  SG_daglca__alloc(pCtx, &pDagLca, iDagNum, my_fetch_dagnodes_from_repo, pRepo)  );
    SG_ERR_CHECK(  SG_daglca__add_leaves(pCtx, pDagLca, prbLeaves)  );
	SG_ERR_CHECK(  SG_daglca__compute_lca(pCtx, pDagLca)  );

    /* now write the "distilled essense" of the DAG in graphiz dot format */

    SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDOUT, "digraph %s\n", pszDescriptorName)  );
    SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDOUT, "{\n")  );
    SG_ERR_CHECK(  SG_daglca__foreach(pCtx, pDagLca, SG_TRUE, _add_significant_lca_node_cb, NULL)  );
    SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDOUT, "}\n")  );
    
    SG_REPO_NULLFREE(pCtx, pRepo);
    SG_DAGLCA_NULLFREE(pCtx, pDagLca);
    SG_RBTREE_NULLFREE(pCtx, prbLeaves);
    SG_VHASH_NULLFREE(pCtx, pvhRepoDescriptor);

	return;

fail:
    SG_REPO_NULLFREE(pCtx, pRepo);
    SG_DAGLCA_NULLFREE(pCtx, pDagLca);
    SG_RBTREE_NULLFREE(pCtx, prbLeaves);
    SG_VHASH_NULLFREE(pCtx, pvhRepoDescriptor);
}

void do_cmd_dump_lca(SG_context * pCtx, SG_uint32 count_args, const char** paszArgs)
{
	SG_pathname* pPathCwd = NULL;
    SG_string* pstrRepoDescriptorName = NULL;

    if (count_args == 0)
    {
        SG_ERR_CHECK(  SG_PATHNAME__ALLOC(pCtx, &pPathCwd)  );
        SG_ERR_CHECK(  SG_pathname__set__from_cwd(pCtx, pPathCwd)  );
        SG_ERR_CHECK(  SG_workingdir__find_mapping(pCtx, pPathCwd, NULL, &pstrRepoDescriptorName, NULL)  );
        SG_PATHNAME_NULLFREE(pCtx, pPathCwd);
        SG_ERR_CHECK(  do_dump_lca(pCtx, SG_string__sz(pstrRepoDescriptorName),SG_DAGNUM__VERSION_CONTROL)  );
        SG_STRING_NULLFREE(pCtx, pstrRepoDescriptorName);
    }
    else if (count_args == 2)
    {
        SG_ERR_CHECK(  do_dump_lca(pCtx, paszArgs[0],(SG_uint16) atoi(paszArgs[1]))  );
    }
	else
	{
		// TODO should we throw usage...
	}
	
    return;

fail:
    SG_PATHNAME_NULLFREE(pCtx, pPathCwd);
    SG_STRING_NULLFREE(pCtx, pstrRepoDescriptorName);
}


#ifdef DEBUG
void do_cmd_dump_treenode(SG_context * pCtx, const char * psz_spec_cs, SG_bool bRev)
{
	SG_pathname * pPathCwd = NULL;
    SG_repo* pRepo = NULL;
	SG_pendingtree* pPendingTree = NULL;
	char * psz_hid_cs = NULL;
    SG_changeset* pcs = NULL;
	const char * szHidSuperRoot = NULL;
	SG_string * pStrDump = NULL;

	SG_ERR_CHECK(  _get_repo_from_cwd(pCtx, &pRepo, &pPathCwd)  );

	if (psz_spec_cs && *psz_spec_cs)
	{
		// Map the given TAG or abbreviated CSET HID into a full HID for a CSET.

		if (bRev)
			SG_ERR_CHECK(  SG_repo__hidlookup__dagnode(pCtx, pRepo, SG_DAGNUM__VERSION_CONTROL, psz_spec_cs, &psz_hid_cs)  );
		else
			SG_ERR_CHECK(  SG_vc_tags__lookup__tag(pCtx, pRepo, psz_spec_cs, &psz_hid_cs)  );
	}
	else
	{
		// assume the current baseline.  NOTE that our dump will not show the dirt in the WD.
		// 
		// when we have an uncomitted merge, we will have more than one parent.
		// what does this command mean then?  It feels like we we should throw
		// an error and say that you have to commit first.

		const SG_varray * pva_wd_parents;		// we do not own this
		const char * psz_hid_parent_0;			// we do not own this
		SG_uint32 nrParents;

		SG_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathCwd, SG_TRUE, &pPendingTree)  );
		SG_ERR_CHECK(  SG_pendingtree__get_wd_parents__ref(pCtx, pPendingTree, &pva_wd_parents)  );
		SG_ERR_CHECK(  SG_varray__count(pCtx, pva_wd_parents, &nrParents)  );
		if (nrParents > 1)
			SG_ERR_THROW(  SG_ERR_CANNOT_DO_WHILE_UNCOMMITTED_MERGE  );

		SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pva_wd_parents, 0, &psz_hid_parent_0)  );
		SG_ERR_CHECK(  SG_strdup(pCtx, psz_hid_parent_0, &psz_hid_cs)  );
	}

	// load the contents of the desired CSET and get the super-root treenode.

    SG_ERR_CHECK(  SG_changeset__load_from_repo(pCtx, pRepo, psz_hid_cs, &pcs)  );
	SG_ERR_CHECK(  SG_changeset__get_root(pCtx, pcs,&szHidSuperRoot)  );

	// let the treenode dump routine populate our string.
	// TODO we may want to crack this open and dump the user's acutal root.

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pStrDump)  );
	SG_ERR_CHECK(  SG_treenode_debug__dump__by_hid(pCtx,szHidSuperRoot,pRepo,4,SG_TRUE,pStrDump)  );

	SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDOUT, "CSet[%s]:\n%s\n", psz_hid_cs, SG_string__sz(pStrDump))  );
	
    /* fall through to common cleanup */

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathCwd);
    SG_REPO_NULLFREE(pCtx, pRepo);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
    SG_NULLFREE(pCtx, psz_hid_cs);
	SG_CHANGESET_NULLFREE(pCtx, pcs);
	SG_STRING_NULLFREE(pCtx, pStrDump);
}
#endif


void do_cmd_hid(SG_context * pCtx, const char* pszPath)
{
	SG_pathname* pPathCwd = NULL;
	SG_pendingtree* pPendingTree = NULL;
    SG_bool bIgnoreWarnings = SG_FALSE;
    SG_repo* pRepo = NULL;
    SG_pathname* pPath = NULL;
    SG_file* pFile = NULL;
    char* pszHid = NULL;

	// we need a pRepo because HIDs are now repo-specific.

	SG_ERR_CHECK(  SG_PATHNAME__ALLOC(pCtx, &pPathCwd)  );
	SG_ERR_CHECK(  SG_pathname__set__from_cwd(pCtx, pPathCwd)  );

	SG_ERR_CHECK(  SG_pendingtree__alloc(pCtx, pPathCwd, bIgnoreWarnings, &pPendingTree)  );
    SG_ERR_CHECK(  SG_pendingtree__get_repo(pCtx, pPendingTree, &pRepo)  );

    SG_PATHNAME__ALLOC__SZ(pCtx, &pPath, pszPath);
    SG_ERR_CHECK(  SG_file__open__pathname(pCtx, pPath, SG_FILE_RDONLY | SG_FILE_OPEN_EXISTING, SG_FSOBJ_PERMS__UNUSED, &pFile)  );
    SG_ERR_CHECK(  SG_repo__alloc_compute_hash__from_file(pCtx, pRepo, pFile, &pszHid)  );

    SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDOUT, "%s\n", pszHid)  );

fail:
	SG_FILE_NULLCLOSE(pCtx, pFile);
    SG_NULLFREE(pCtx, pszHid);
    SG_PATHNAME_NULLFREE(pCtx, pPath);
	SG_PATHNAME_NULLFREE(pCtx, pPathCwd);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
}


void do_cmd_remove(SG_context * pCtx, SG_uint32 count_args, const char** paszArgs, const char* const* ppszIncludes, SG_uint32 nCountIncludes, const char* const* ppszExcludes, SG_uint32 nCountExcludes)
{
	SG_pathname* pPathCwd = NULL;
	SG_pendingtree* pPendingTree = NULL;
    SG_bool bIgnoreWarnings = SG_FALSE;

	SG_ERR_CHECK(  SG_PATHNAME__ALLOC(pCtx, &pPathCwd)  );
	SG_ERR_CHECK(  SG_pathname__set__from_cwd(pCtx, pPathCwd)  );

	SG_ERR_CHECK(  SG_pendingtree__alloc(pCtx, pPathCwd, bIgnoreWarnings, &pPendingTree)  );
	
	SG_ERR_CHECK(  SG_pendingtree__remove(pCtx, pPendingTree, pPathCwd, count_args, paszArgs, ppszIncludes, nCountIncludes, ppszExcludes, nCountExcludes)  );
	
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

	SG_PATHNAME_NULLFREE(pCtx, pPathCwd);

	return;

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathCwd);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	if (ppszIncludes)
		SG_NULLFREE(pCtx, ppszIncludes);

	if (ppszExcludes)
		SG_NULLFREE(pCtx, ppszExcludes);

}

void do_cmd_rename(SG_context * pCtx, const char* pszPath, const char* pszNewName, SG_bool bIgnoreWarnings, SG_bool bForce)
{
	SG_pathname* pPath = NULL;
	SG_pathname* pPathCwd = NULL;
	SG_pathname* pPathDir = NULL;
	SG_pathname* pPathRename = NULL;
	SG_pendingtree* pPendingTree = NULL;

	SG_ERR_CHECK(  SG_PATHNAME__ALLOC(pCtx, &pPathCwd)  );
	SG_ERR_CHECK(  SG_pathname__set__from_cwd(pCtx, pPathCwd)  );

	SG_ERR_CHECK(  SG_pendingtree__alloc(pCtx, pPathCwd, bIgnoreWarnings, &pPendingTree)  );

	if (pszPath[0] == '@')
	{
		SG_ERR_CHECK(  SG_workingdir__construct_absolute_path_from_repo_path(pCtx, pPathCwd, pszPath, &pPath)  );
	}
	else
		SG_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx, &pPath, pszPath)  );

	if (bForce)
	{
		SG_bool bExists = SG_FALSE;
		SG_fsobj_type fsType;
		SG_ERR_CHECK(  SG_PATHNAME__ALLOC__COPY(pCtx, &pPathDir, pPath)  );
		SG_ERR_CHECK(  SG_pathname__remove_last(pCtx, pPathDir)  );

		SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathRename, pPathDir, pszNewName)  );

		SG_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPathRename, &bExists, &fsType, NULL)  );

		if (bExists)
		{
			const char** args;
			SG_ERR_CHECK(  SG_alloc(pCtx, 1, sizeof(const char *), &args)  );
			args[0] = pszNewName;
			SG_pendingtree__remove(pCtx, pPendingTree, pPathDir, 1, args, NULL, 0, NULL, 0);
			if (SG_context__has_err(pCtx) && SG_context__err_equals(pCtx, SG_ERR_NOT_FOUND))
			{
				SG_context__err_reset(pCtx);
				// something exists at the new location, but the pending tree doesn't know about it
				if (fsType == SG_FSOBJ_TYPE__DIRECTORY)
					SG_ERR_CHECK(  SG_fsobj__rmdir_recursive__pathname(pCtx, pPathRename)  );
				else
					SG_ERR_CHECK(  SG_fsobj__remove__pathname(pCtx, pPathRename)  );
			}
			else
			{
				SG_NULLFREE(pCtx, args);
				SG_ERR_CHECK_CURRENT;
			}
			SG_NULLFREE(pCtx, args);
		}
		 
	}
	
	SG_ERR_CHECK(  SG_pendingtree__rename(pCtx, pPendingTree, pPath, pszNewName, SG_TRUE)  );
	
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

	SG_PATHNAME_NULLFREE(pCtx, pPath);
	SG_PATHNAME_NULLFREE(pCtx, pPathCwd);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathRename);

	return;

fail:
	if (pPendingTree && SG_context__err_equals(pCtx,SG_ERR_PORTABILITY_WARNINGS))
		SG_ERR_IGNORE(  sg_report_portability_warnings(pCtx,pPendingTree)  );
	SG_PATHNAME_NULLFREE(pCtx, pPath);
	SG_PATHNAME_NULLFREE(pCtx, pPathCwd);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathRename);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
}

void do_cmd_move(SG_context * pCtx, const char* pszPathMoveMe, const char* pszPathMoveTo, SG_bool bIgnoreWarnings, SG_bool bForce)
{
	SG_pathname* pPathMoveMe = NULL;
	SG_pathname* pPathMoveTo = NULL;
	SG_pathname* pPathCwd = NULL;
	SG_pendingtree* pPendingTree = NULL;

	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx, &pPathMoveMe, pszPathMoveMe)  );
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx, &pPathMoveTo, pszPathMoveTo)  );

	/* TODO check to see if this exists before going down into the pending tree code? */
	
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC(pCtx, &pPathCwd)  );
	SG_ERR_CHECK(  SG_pathname__set__from_cwd(pCtx, pPathCwd)  );

	SG_ERR_CHECK(  SG_pendingtree__alloc(pCtx, pPathCwd, bIgnoreWarnings, &pPendingTree)  );
	
	SG_ERR_CHECK(  SG_pendingtree__move(pCtx, pPendingTree, pPathMoveMe, pPathMoveTo, bForce)  );
	
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

	SG_PATHNAME_NULLFREE(pCtx, pPathMoveMe);
	SG_PATHNAME_NULLFREE(pCtx, pPathMoveTo);
	SG_PATHNAME_NULLFREE(pCtx, pPathCwd);

	return;

fail:
	if (pPendingTree && SG_context__err_equals(pCtx,SG_ERR_PORTABILITY_WARNINGS))
		SG_ERR_IGNORE(  sg_report_portability_warnings(pCtx,pPendingTree)  );
	SG_PATHNAME_NULLFREE(pCtx, pPathMoveMe);
	SG_PATHNAME_NULLFREE(pCtx, pPathMoveTo);
	SG_PATHNAME_NULLFREE(pCtx, pPathCwd);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
}

static void do_cmd_checkout(SG_context * pCtx, const char* pszRidescName, const char* psz_spec_hid_cs, SG_bool bRev, const char* pszRelPath)
{
	SG_pathname* pPath = NULL;
	SG_bool bExists;
	SG_fsobj_stat stat;
	SG_repo* pRepo = NULL;
	SG_vhash* pvhDescriptor = NULL;
	char * psz_hid_cs = NULL;
	SG_bool bWeCreatedFolder = SG_FALSE;
	SG_string *pStrRepoDescriptorName = NULL;

	/*
	 * Retrieve everything.  A new directory will be created at pszRelPath.
	 * Its name will be the name of the descriptor.
	 */
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx, &pPath, pszRelPath)  );

	SG_workingdir__find_mapping(pCtx, pPath, NULL, &pStrRepoDescriptorName, NULL);
	SG_ERR_REPLACE(SG_ERR_NOT_FOUND, SG_ERR_NOT_A_WORKING_COPY);

	if ((! SG_context__has_err(pCtx)) || (! SG_context__err_equals(pCtx, SG_ERR_NOT_A_WORKING_COPY)))
	{
		SG_console(pCtx, SG_CS_STDERR, "%s is already part of a working directory for repository %s\n", 
			   SG_pathname__sz(pPath),
			   SG_string__sz(pStrRepoDescriptorName)); 
		SG_ERR_RESET_THROW(SG_ERR_WORKING_DIRECTORY_IN_USE);
	}

	SG_context__err_reset(pCtx);

	SG_ERR_CHECK(  SG_fsobj__exists_stat__pathname(pCtx, pPath, &bExists, &stat)  );

	if (!bExists)
	{
		SG_ERR_CHECK(  SG_fsobj__mkdir_recursive__pathname(pCtx, pPath)  );
		bWeCreatedFolder = SG_TRUE;
	}
	else
	{
		if (stat.type == SG_FSOBJ_TYPE__DIRECTORY)
		{
			SG_dir* pDir = NULL;
			SG_error err;
			SG_string* pStr = NULL;
			SG_fsobj_stat stat;

			SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pStr)  );
			SG_ERR_CHECK(  SG_dir__open(pCtx, pPath, &pDir, &err, pStr, &stat)  );

			while (SG_IS_OK(err))
			{
				const char * sz = SG_string__sz(pStr);

				SG_ASSERT( sz && sz[0] );
				if (   (sz[0] == '.')
						&& (   (sz[1] == 0)
						|| (   (sz[1] == '.')
						&& (sz[2] == 0))))
				{
					// we ignore "." and ".." on all platforms.
				}
				else
				{
					SG_console(pCtx, SG_CS_STDERR, "The given directory, %s, is not empty.\n", pszRelPath);
					SG_STRING_NULLFREE(pCtx, pStr);
					SG_DIR_NULLCLOSE(pCtx, pDir);
					goto fail;
				}
		
				SG_dir__read(pCtx,pDir,pStr,&stat);
				(void)SG_context__get_err(pCtx,&err);
			}
			if (err == SG_ERR_NOMOREFILES)
				SG_context__err_reset(pCtx);

			SG_STRING_NULLFREE(pCtx, pStr);
			SG_DIR_NULLCLOSE(pCtx, pDir);
		}
		else
		{
			SG_console(pCtx, SG_CS_STDERR, "The given path, %s, is not a directory.\n", pszRelPath);
			goto fail;
		}
	}

	SG_ERR_CHECK(  SG_closet__descriptors__get(pCtx, pszRidescName, &pvhDescriptor)  );
    SG_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, &pRepo, pvhDescriptor)  );

	if (bRev)
		SG_ERR_CHECK(  SG_repo__hidlookup__dagnode(pCtx, pRepo, SG_DAGNUM__VERSION_CONTROL, psz_spec_hid_cs, &psz_hid_cs)  );
	else
		SG_ERR_CHECK(  SG_vc_tags__lookup__tag(pCtx, pRepo, psz_spec_hid_cs, &psz_hid_cs)  );

	SG_workingdir__create_and_get(pCtx, pszRidescName, pPath, SG_TRUE, psz_hid_cs);

	// output some helpful error messages here if we can
	if (SG_context__has_err(pCtx))
	{
		if (SG_context__err_equals(pCtx, SG_ERR_AMBIGUOUS_ID_PREFIX))
		{
			SG_context__push_level(pCtx);
			SG_console(pCtx, SG_CS_STDERR, "The revision or tag could not be found:  %s\n", psz_spec_hid_cs); 
			SG_context__pop_level(pCtx);
			SG_ERR_RETHROW;
		}
		else if (SG_context__err_equals(pCtx, SG_ERR_VHASH_KEYNOTFOUND))
		{
			SG_context__push_level(pCtx);
			SG_console(pCtx, SG_CS_STDERR, "The repository could not be found:  %s\n", pszRidescName); 
			SG_context__pop_level(pCtx);
			SG_ERR_RETHROW;
		}
		else if (SG_context__err_equals(pCtx, SG_ERR_MULTIPLE_HEADS_FROM_DAGNODE))
		{
			SG_context__push_level(pCtx);
			SG_console(pCtx, SG_CS_STDERR, "More than one head found, cannot complete command as specified.\n"); 
			SG_context__pop_level(pCtx);
			SG_ERR_RETHROW;
		}
		else
			SG_ERR_RETHROW;
			
	}

fail:
	if (SG_context__has_err(pCtx)  && bWeCreatedFolder == SG_TRUE)
	{
		SG_ERR_IGNORE(  SG_fsobj__rmdir_recursive__pathname(pCtx, pPath)  );
	}
	SG_NULLFREE(pCtx, psz_hid_cs);
	SG_REPO_NULLFREE(pCtx, pRepo);
	SG_VHASH_NULLFREE(pCtx, pvhDescriptor);
	SG_PATHNAME_NULLFREE(pCtx, pPath);
	SG_STRING_NULLFREE(pCtx, pStrRepoDescriptorName);

	return;
}

void do_cmd_new(SG_context * pCtx, const char* pszRidescName)
{
	SG_pathname* pPathCwd = NULL;
	SG_changeset* pcsFirst = NULL;
	char* pszidGidActualRoot = NULL;
	const char* pszidFirstChangeset;
    char buf_admin_id[SG_GID_BUFFER_LENGTH];
	SG_string *pStrRepoDescriptorName = NULL;

	SG_ERR_CHECK(  SG_PATHNAME__ALLOC(pCtx, &pPathCwd)  );
	SG_ERR_CHECK(  SG_pathname__set__from_cwd(pCtx, pPathCwd)  );
	
	SG_workingdir__find_mapping(pCtx, pPathCwd, NULL, &pStrRepoDescriptorName, NULL);
	SG_ERR_REPLACE(SG_ERR_NOT_FOUND, SG_ERR_NOT_A_WORKING_COPY);

	if ((! SG_context__has_err(pCtx)) || (! SG_context__err_equals(pCtx, SG_ERR_NOT_A_WORKING_COPY)))
	{
		SG_console(pCtx, SG_CS_STDERR, "The current directory is already part of a working directory for repository %s\n", SG_string__sz(pStrRepoDescriptorName)); 
		SG_ERR_RESET_THROW(SG_ERR_WORKING_DIRECTORY_IN_USE);
	}

	SG_context__err_reset(pCtx);

	SG_STRING_NULLFREE(pCtx, pStrRepoDescriptorName);

    // TODO we don't know how to deal with admin ids yet.  but we need
    // one to create a repo.  so for now we just create a gid here.
    SG_ERR_CHECK(  SG_gid__generate(pCtx, buf_admin_id, sizeof(buf_admin_id))  );
	
    SG_ERR_CHECK(  SG_repo__create__completely_new__closet(pCtx, buf_admin_id, NULL, pszRidescName, &pcsFirst, &pszidGidActualRoot)  );

	SG_ERR_CHECK(  SG_changeset__get_id_ref(pCtx, pcsFirst, &pszidFirstChangeset)  );
	
	SG_ERR_CHECK(  SG_workingdir__create_and_get(pCtx, pszRidescName, pPathCwd, SG_TRUE, pszidFirstChangeset)  );
	
	SG_NULLFREE(pCtx, pszidGidActualRoot);
	SG_CHANGESET_NULLFREE(pCtx, pcsFirst);
	SG_PATHNAME_NULLFREE(pCtx, pPathCwd);

	return;

fail:
	SG_STRING_NULLFREE(pCtx, pStrRepoDescriptorName);
	SG_PATHNAME_NULLFREE(pCtx, pPathCwd);
}


void do_cmd_vcdiff(SG_context * pCtx, SG_uint32 count_args, const char** paszArgs)
{
	SG_pathname* pPath1 = NULL;
	SG_pathname* pPath2 = NULL;
	SG_pathname* pPath3 = NULL;

	if (
		(count_args == 6)
		&& (0 == strcmp(paszArgs[2], "deltify"))
		)
	{
		SG_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx, &pPath1, paszArgs[3])  );
		SG_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx, &pPath2, paszArgs[4])  );
		SG_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx, &pPath3, paszArgs[5])  );
		SG_ERR_CHECK(  SG_vcdiff__deltify__files(pCtx, pPath1, pPath2, pPath3)  );
	}
	else if (
		(count_args == 6)
		&& (0 == strcmp(paszArgs[2], "undeltify"))
		)
	{
		SG_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx, &pPath1, paszArgs[3])  );
		SG_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx, &pPath2, paszArgs[4])  );
		SG_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx, &pPath3, paszArgs[5])  );
		SG_ERR_CHECK(  SG_vcdiff__undeltify__files(pCtx, pPath1, pPath2, pPath3)  );
	}
	else
	{
		SG_ERR_THROW_RETURN(  SG_ERR_USAGE  );
	}
	
	SG_PATHNAME_NULLFREE(pCtx, pPath1);
	SG_PATHNAME_NULLFREE(pCtx, pPath2);
	SG_PATHNAME_NULLFREE(pCtx, pPath3);

	return;

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPath1);
	SG_PATHNAME_NULLFREE(pCtx, pPath2);
	SG_PATHNAME_NULLFREE(pCtx, pPath3);
}


void do_cmd_commit(SG_context * pCtx,
                   SG_uint32 count_args, const char** paszArgs,
                   const char* psz_comment,
                   const char* psz_user,
                   SG_stringarray* psa_stamps,
                   SG_bool recursive,
                   const char* const* ppszIncludes, SG_uint32 nCountIncludes,
                   const char* const* ppszExcludes, SG_uint32 nCountExcludes,
                   const char* const* ppszAssocs, SG_uint32 nCountAssocs)
{
	SG_pathname* pPathCwd = NULL;
	SG_pendingtree* pPendingTree = NULL;
    SG_bool bIgnoreWarnings = SG_FALSE;
    SG_dagnode* pdn = NULL;
    SG_repo* pRepo = NULL;
    const char* psz_hid_cs = NULL;
    SG_audit q;
	SG_uint32 iCountStamps = 0;

	if(!recursive)
        SG_ERR_THROW2(SG_ERR_NOTIMPLEMENTED, (pCtx, "nonrecursive commit"));

	SG_ERR_CHECK(  SG_PATHNAME__ALLOC(pCtx, &pPathCwd)  );
	SG_ERR_CHECK(  SG_pathname__set__from_cwd(pCtx, pPathCwd)  );

    /* Let's do a basic scan first */
	SG_ERR_CHECK(  SG_pendingtree__alloc(pCtx, pPathCwd, bIgnoreWarnings, &pPendingTree)  );
	SG_ERR_CHECK(  SG_pendingtree__scan(pCtx, pPendingTree, SG_TRUE, ppszIncludes, nCountIncludes, ppszExcludes, nCountExcludes)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

    /* Now the commit */
	SG_ERR_CHECK(  SG_pendingtree__alloc(pCtx, pPathCwd, bIgnoreWarnings, &pPendingTree)  );
    SG_ERR_CHECK(  SG_pendingtree__get_repo(pCtx, pPendingTree, &pRepo)  );
	if (psz_user)
    {
        SG_ERR_CHECK(  SG_audit__init(pCtx,&q,pRepo,SG_AUDIT__WHEN__NOW,psz_user)  );
    }
	else
    {
        SG_ERR_CHECK(  SG_audit__init(pCtx,&q,pRepo,SG_AUDIT__WHEN__NOW,SG_AUDIT__WHO__FROM_SETTINGS)  );
    }

	SG_ERR_CHECK(  SG_pendingtree__commit(pCtx, pPendingTree, &q, pPathCwd, count_args, paszArgs, ppszIncludes, nCountIncludes, ppszExcludes, nCountExcludes, ppszAssocs, nCountAssocs, &pdn)  );

    SG_ERR_CHECK(  SG_dagnode__get_id_ref(pCtx, pdn, &psz_hid_cs)  );

    if (psz_comment)
    {
        SG_ERR_CHECK(  SG_vc_comments__add(pCtx, pRepo, psz_hid_cs, psz_comment, &q)  );
    }

	if (psa_stamps)
	{
		SG_uint32 iCount = 0;
		const char* pszstamp;
		SG_ERR_CHECK(  SG_stringarray__count(pCtx, psa_stamps, &iCountStamps )  );
		while (iCount < iCountStamps)
		{
			SG_ERR_CHECK(  SG_stringarray__get_nth(pCtx, psa_stamps, iCount, &pszstamp)  );
			SG_ERR_CHECK(  SG_vc_stamps__add(pCtx, pRepo, psz_hid_cs, pszstamp, &q)  );
			iCount++;
		}

	}

    SG_ERR_CHECK(  my_dump_log(pCtx, pRepo, psz_hid_cs, SG_FALSE)  );

	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
    pRepo = NULL;
    SG_DAGNODE_NULLFREE(pCtx, pdn);
	SG_PATHNAME_NULLFREE(pCtx, pPathCwd);

	return;

fail:
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
    pRepo = NULL;
    SG_DAGNODE_NULLFREE(pCtx, pdn);
	SG_PATHNAME_NULLFREE(pCtx, pPathCwd);
}

void do_cmd_revert(SG_context * pCtx,
				   SG_option_state * pOptSt,
				   SG_uint32 count_args, const char** paszArgs)
{
	SG_pathname* pPathCwd = NULL;
	SG_pendingtree* pPendingTree = NULL;
	SG_pendingtree_action_log_enum eActionLog;

	if (pOptSt->bTest)
	{
		eActionLog = SG_PT_ACTION__LOG_IT;
	}
	else
	{
		eActionLog = SG_PT_ACTION__DO_IT;
		if (pOptSt->bVerbose)
			eActionLog |= SG_PT_ACTION__LOG_IT;
	}
	
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC(pCtx, &pPathCwd)  );
	SG_ERR_CHECK(  SG_pathname__set__from_cwd(pCtx, pPathCwd)  );

	SG_ERR_CHECK(  SG_pendingtree__alloc(pCtx, pPathCwd, pOptSt->bIgnoreWarnings, &pPendingTree)  );
	SG_ERR_CHECK(  SG_pendingtree__revert(pCtx,
										  pPendingTree,
										  eActionLog,
										  pPathCwd,
										  count_args, paszArgs,
										  pOptSt->bRecursive,
										  pOptSt->ppIncludes,
										  pOptSt->iCountIncludes,
										  pOptSt->ppExcludes,
										  pOptSt->iCountExcludes)  );

	if (eActionLog & SG_PT_ACTION__LOG_IT)
		SG_ERR_IGNORE(  sg_report_action_log(pCtx,pPendingTree)  );

fail:
	if (pPendingTree && SG_context__err_equals(pCtx,SG_ERR_PORTABILITY_WARNINGS))
		SG_ERR_IGNORE(  sg_report_portability_warnings(pCtx,pPendingTree)  );

	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_PATHNAME_NULLFREE(pCtx, pPathCwd);
}

void do_cmd_scan(SG_context * pCtx, SG_bool bForce)
{
	SG_pathname* pPathCwd = NULL;
	SG_pendingtree* pPendingTree = NULL;
    SG_bool bIgnoreWarnings = SG_FALSE;
	
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC(pCtx, &pPathCwd)  );
	SG_ERR_CHECK(  SG_pathname__set__from_cwd(pCtx, pPathCwd)  );

	SG_ERR_CHECK(  SG_pendingtree__alloc(pCtx, pPathCwd, bIgnoreWarnings, &pPendingTree)  );
	
	if (bForce)
		SG_ERR_CHECK(  SG_pendingtree__clear_wd_timestamp_cache(pCtx, pPendingTree)  );		// flush the timestamp cache we loaded with the rest of wd.json

	SG_ERR_CHECK(  SG_pendingtree__scan(pCtx, pPendingTree, SG_TRUE, NULL, 0, NULL, 0)  );			// This does scan and implicit save and close of vfile.
	
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

	SG_PATHNAME_NULLFREE(pCtx, pPathCwd);

	return;

fail:
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_PATHNAME_NULLFREE(pCtx, pPathCwd);
}

void do_cmd_addremove(SG_context * pCtx, SG_uint32 count_args, const char** paszArgs, SG_bool bIgnoreWarnings, SG_bool bRecursive, const char* const* ppszIncludes, SG_uint32 nCountIncludes, const char* const* ppszExcludes, SG_uint32 nCountExcludes)
{
	SG_pathname* pPathCwd = NULL;
	SG_pendingtree* pPendingTree = NULL;	
	
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC(pCtx, &pPathCwd)  );
	SG_ERR_CHECK(  SG_pathname__set__from_cwd(pCtx, pPathCwd)  );

	SG_ERR_CHECK(  SG_pendingtree__alloc(pCtx, pPathCwd, bIgnoreWarnings, &pPendingTree)  );
	
	SG_ERR_CHECK(  SG_pendingtree__addremove(pCtx, pPendingTree, count_args, paszArgs, bRecursive, ppszIncludes, nCountIncludes, ppszExcludes, nCountExcludes)  );			// This does scan and implicit save and close of vfile.
	
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

	SG_PATHNAME_NULLFREE(pCtx, pPathCwd);

	return;

fail:
	if (pPendingTree && SG_context__err_equals(pCtx,SG_ERR_PORTABILITY_WARNINGS))
		SG_ERR_IGNORE(  sg_report_portability_warnings(pCtx,pPendingTree)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_PATHNAME_NULLFREE(pCtx, pPathCwd);
	if (ppszIncludes)
		SG_NULLFREE(pCtx, ppszIncludes);

	if (ppszExcludes)
		SG_NULLFREE(pCtx, ppszExcludes);

}


void do_cmd_status(SG_context * pCtx,
				   SG_uint32 count_args, const char ** paszArgs,
				   SG_bool bRecursive,
				   SG_bool bVerbose,
				   const char* const* ppszIncludes, SG_uint32 nCountIncludes,
				   const char* const* ppszExcludes, SG_uint32 nCountExcludes)
{
	SG_treediff2 * pTreeDiff = NULL;
	SG_string * pStrStatusReport = NULL;
	SG_pathname* pPathCwd = NULL;
	SG_pendingtree* pPendingTree = NULL;
	SG_bool bIgnoreWarnings = SG_FALSE;
	char * psz_rev_1 = NULL;
	char * psz_rev_2 = NULL;

	SG_ERR_CHECK(  SG_PATHNAME__ALLOC(pCtx, &pPathCwd)  );
	SG_ERR_CHECK(  SG_pathname__set__from_cwd(pCtx, pPathCwd)  );

	if (count_args > 0)
	{
		SG_ERR_CHECK(  SG_fsobj__verify_that_all_exist(pCtx, count_args, paszArgs)  );
	}

	SG_ERR_CHECK(  SG_pendingtree__alloc(pCtx, pPathCwd, bIgnoreWarnings, &pPendingTree)  );

	// TODO put the optional HID of the first --rev or --tag in psz_rev_1
	// TODO put the optional HID of the second --rev or --tag in psz_rev_2
	// TODO i'm assuming that the files_or_folders are in (count_args,paszArgs)
	SG_ERR_CHECK(   SG_pendingtree__diff_or_status(pCtx, pPendingTree,
												   psz_rev_1, psz_rev_2,
												   &pTreeDiff)   );

	if ((count_args > 0) || (nCountIncludes > 0) || (nCountExcludes > 0) || !bRecursive)
		SG_ERR_CHECK(  SG_treediff2__file_spec_filter(pCtx, pTreeDiff,
													  count_args, paszArgs, bRecursive,
													  ppszIncludes, nCountIncludes,
													  ppszExcludes, nCountExcludes)  );

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pStrStatusReport)  );
	SG_ERR_CHECK(  SG_treediff2__report_status_to_string(pCtx, pTreeDiff,pStrStatusReport,bVerbose)  );
	SG_ERR_CHECK(  SG_console(pCtx,SG_CS_STDOUT,SG_string__sz(pStrStatusReport))  );

	// fall thru to common cleanup.

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathCwd);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_STRING_NULLFREE(pCtx, pStrStatusReport);
	SG_TREEDIFF2_NULLFREE(pCtx, pTreeDiff);
}

void do_cmd_cat(SG_context * pCtx, SG_uint32 count_args, const char ** paszArgs, const char * psz_repo, const char * psz_changeset, const char * psz_tag)
{
	SG_pathname* pPathCwd = NULL;
    SG_string* pstrRepoDescriptorName = NULL;
    SG_vhash* pvhDescriptor = NULL;
    SG_repo* pRepo = NULL;
	SG_changeset* pChangeset = NULL;
	const char* pszidBaseline = NULL;
	SG_rbtree * prbLeaves = NULL;
	SG_rbtree_iterator * pIter = NULL;
	const char * pszHid_k;
	SG_bool bFound;
	SG_uint32 i = 0;
	SG_pathname* pPathTop = NULL;
	SG_pendingtree* pPendingTree = NULL;
	SG_bool bBaselineOrHead = SG_FALSE;
	SG_bool bHavePendingTree = SG_TRUE;
	char* pszChangeset = NULL;

	// TODO 4/21/10 BUGBUG When this routine has an error, it resets it, prints a message, and returns
	// TODO 4/21/10        without propagating the info to the caller.  I think it would be better if
	// TODO 4/21/10        these were SG_ERR_THROW2() or _RETHROW2() with the detailed error passed in
	// TODO 4/21/10        throw and either the original error code or a normalized one thrown.

	SG_ERR_CHECK(  SG_PATHNAME__ALLOC(pCtx, &pPathCwd)  );
	SG_ERR_CHECK(  SG_pathname__set__from_cwd(pCtx, pPathCwd)  );

	if (psz_repo)
	{
		SG_closet__descriptors__get(pCtx, psz_repo, &pvhDescriptor);

		if (SG_context__has_err(pCtx))
		{
			// psz_repo not found
			if (SG_context__err_equals(pCtx, SG_ERR_VHASH_KEYNOTFOUND))
			{
				SG_context__err_reset(pCtx);
				SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "Could not find repo:  %s\n", psz_repo)  );
				goto fail;
			}
			else // some other error, propagate
				SG_ERR_RETHROW;
		}
	}
	else // if no repo, use repo associated with current working directory
	{
		SG_workingdir__find_mapping(pCtx, pPathCwd, &pPathTop, &pstrRepoDescriptorName, NULL);
		if (SG_context__has_err(pCtx))
		{
			// if no repo assoc with this cwd
			if (SG_context__err_equals(pCtx, SG_ERR_VHASH_KEYNOTFOUND))
			{
				SG_context__err_reset(pCtx);
				SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "There is no repository associated with this directory.  Please specify a repository using --repo.\n")  );
				goto fail;
			}
			else // some other error, propagate
				SG_ERR_RETHROW;
		}

		SG_ERR_CHECK(  SG_closet__descriptors__get(pCtx, SG_string__sz(pstrRepoDescriptorName), &pvhDescriptor)  );
		SG_STRING_NULLFREE(pCtx, pstrRepoDescriptorName);
		SG_PATHNAME_NULLFREE(pCtx, pPathTop);
	}

	if (pvhDescriptor)
		SG_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, &pRepo, pvhDescriptor)  );
	else // no repo given and no repo associated with working dir?
	{
		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "Cannot find repository.\n")  );
		goto fail;
	}


	SG_pendingtree__alloc(pCtx, pPathCwd, SG_TRUE, &pPendingTree);
	if (SG_context__has_err(pCtx))
	{
		// not sure the best way to figure out if we have a pending tree or not.
		// for now, assume if the alloc fails, we have no pending tree, and no checkout
		SG_context__err_reset(pCtx);
		bHavePendingTree = SG_FALSE;
	}

	if (psz_changeset)
	{
		char * psz_hid_cs = NULL;
		SG_ERR_CHECK(  SG_repo__hidlookup__dagnode(pCtx, pRepo, SG_DAGNUM__VERSION_CONTROL, psz_changeset, &psz_hid_cs)  );
		SG_ERR_CHECK(  SG_changeset__load_from_repo(pCtx, pRepo, psz_hid_cs, &pChangeset)  );
		SG_ERR_CHECK(  SG_strdup(pCtx, psz_hid_cs, &pszChangeset)  );
		SG_NULLFREE(pCtx, psz_hid_cs);
	}
	else if (psz_tag)
	{
		char* pszTempCsID = NULL;
		SG_ERR_CHECK(  SG_vc_tags__lookup__tag(pCtx, pRepo, psz_tag, &pszTempCsID)  );
		if (pszTempCsID)
		{
			// TODO Review: revisit this once we figure out tags and make sure that we
			// TODO         found exactly one match and that it belongs to SG_DAGNUM__VERSION_CONTROL.
			SG_ERR_CHECK(  SG_changeset__load_from_repo(pCtx, pRepo, pszTempCsID, &pChangeset)  );
			SG_ERR_CHECK(  SG_strdup(pCtx, pszTempCsID, &pszChangeset)  );
			SG_NULLFREE(pCtx, pszTempCsID);
		}
		else  // tag doesn't match anything
		{	
			SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "Could not locate tag:  %s\n", psz_tag)  );
			goto fail;
		}

	}
	else
	{
		// no changeset id or tag specified
		// use baseline of current working dir
		// if nothing is checked out to the current dir
		// use the head of the repo
		bBaselineOrHead = SG_TRUE;

		if (bHavePendingTree)
		{
			const SG_varray * pva_wd_parents;		// we do not own this
			SG_uint32 nrParents;

			SG_ERR_CHECK(  SG_pendingtree__get_wd_parents__ref(pCtx, pPendingTree, &pva_wd_parents)  );
			SG_ERR_CHECK(  SG_varray__count(pCtx, pva_wd_parents, &nrParents)  );
			if (nrParents > 1)
				SG_ERR_THROW(  SG_ERR_CANNOT_DO_WHILE_UNCOMMITTED_MERGE  );

			SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pva_wd_parents, 0, &pszidBaseline)  );
		}

		if (pszidBaseline)
		{
			SG_ERR_CHECK(  SG_changeset__load_from_repo(pCtx, pRepo, pszidBaseline, &pChangeset)  );
			SG_ERR_CHECK(  SG_strdup(pCtx, pszidBaseline, &pszChangeset)  );
		}
		else
		{
			// nothing checked out to current dir, use head
			// if there is more than one head, abort
			SG_uint32 iCount = 0;

			SG_ERR_CHECK(  SG_repo__fetch_dag_leaves(pCtx,pRepo,SG_DAGNUM__VERSION_CONTROL,&prbLeaves)  );
			SG_ERR_CHECK(  SG_rbtree__count(pCtx, prbLeaves, &iCount)  );

			if (iCount == 1)
			{
				SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx,&pIter,prbLeaves,&bFound,&pszHid_k,NULL)  );
				SG_ERR_CHECK(  SG_changeset__load_from_repo(pCtx, pRepo, pszHid_k, &pChangeset)  );
			}
			else
			{
				SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "More than one head found, cannot complete command as specified.\n")  );
				goto fail;
			}
		}

	}

	if (pChangeset)
	{
		SG_bool bFoundAtLeastOne = SG_FALSE;
		
		for (i = 0; i < count_args; i++)
		{
			SG_treenode_entry* ptne = NULL;
			const char* pszhidblob = NULL;
			SG_byte * pBuf = NULL;
			SG_uint64 lenBuf;
			const char* pszAbsRepoPath = NULL;
			SG_bool bGivenAbsRepoPath = SG_FALSE;

			if (paszArgs[i][0] == '@')
			{
				pszAbsRepoPath = paszArgs[i];
				bGivenAbsRepoPath = SG_TRUE;
			}
			//else don't translate the path, give it to pendingtree and let it translate
			
			// if we were given an absolute repo path, do a straight tree-walk
			// we can also do a straight tree-walk if no changeset or tag
			if (bGivenAbsRepoPath)
			{
				const char * pszHidTreeNode = NULL;
				char * pszTreeNodeEntryGID = NULL;
				SG_treenode * pTreenodeRoot = NULL;

				//Load the root treenode, then recursively get for the entry
				SG_ERR_CHECK(  SG_changeset__get_root(pCtx, pChangeset, &pszHidTreeNode) );
				SG_ERR_CHECK(  SG_treenode__load_from_repo(pCtx, pRepo, pszHidTreeNode, &pTreenodeRoot)  );
				SG_ERR_CHECK(  SG_treenode__find_treenodeentry_by_path(pCtx, pRepo, pTreenodeRoot, pszAbsRepoPath, &pszTreeNodeEntryGID, &ptne)  );
				SG_NULLFREE(pCtx, pszTreeNodeEntryGID);
				SG_TREENODE_NULLFREE(pCtx, pTreenodeRoot);
			}
			else
			{	
				// we can't do anything with a disk path or relative path, without a pending tree
				if (!bHavePendingTree)
				{
					SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "Cannot process disk path or relative path without a mapped working directory.\n")  );
				}
				else
				{
					char* pszgid = NULL;
					SG_pathname* pTempPathname = NULL;
					
					// find the gid via the pendingtree
					SG_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx, &pTempPathname, paszArgs[i])  );
					SG_pendingtree__get_gid_from_local_path(pCtx, pPendingTree, pTempPathname, &pszgid);

					if (SG_context__has_err(pCtx))
					{
						SG_PATHNAME_NULLFREE(pCtx, pTempPathname);
						SG_NULLFREE(pCtx, pszgid);
						SG_ERR_CHECK_CURRENT;
					}

					// get the treendx and find the treenode_entry using the gid
					SG_ERR_CHECK(  SG_repo__treendx__get_path_in_dagnode(pCtx, pRepo, SG_DAGNUM__VERSION_CONTROL, pszgid, pszChangeset, &ptne)  );

					SG_PATHNAME_NULLFREE(pCtx, pTempPathname);
					SG_NULLFREE(pCtx, pszgid);
				}
			}
			if (ptne)
			{
				SG_ERR_CHECK(  SG_treenode_entry__get_hid_blob(pCtx, ptne, &pszhidblob)  );
				bFoundAtLeastOne = SG_TRUE;
				SG_ERR_CHECK(  SG_repo__fetch_blob_into_memory(pCtx, pRepo, pszhidblob, &pBuf, &lenBuf)  );
				SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDOUT, "%s", pBuf)  );
				SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDOUT, "\n")  );
			
				SG_TREENODE_ENTRY_NULLFREE(pCtx, ptne);
				SG_NULLFREE(pCtx, pBuf);
			}
		}

		if (bFoundAtLeastOne == SG_FALSE)
		{
			SG_console(pCtx, SG_CS_STDERR, "No blob hids found for given files\n");
		}


	}
	// else fall thru to fail

fail:
    SG_VHASH_NULLFREE(pCtx, pvhDescriptor);
    SG_REPO_NULLFREE(pCtx, pRepo);
    SG_STRING_NULLFREE(pCtx, pstrRepoDescriptorName);
	SG_PATHNAME_NULLFREE(pCtx, pPathCwd);
	SG_PATHNAME_NULLFREE(pCtx, pPathTop);
	SG_RBTREE_NULLFREE(pCtx, prbLeaves);
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIter);
	SG_CHANGESET_NULLFREE(pCtx, pChangeset);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_NULLFREE(pCtx, pszChangeset);
}

void do_cmd_tags(SG_context* pCtx, const char* psz_repo)
{
	SG_pathname* pPathCwd = NULL;
	SG_pathname* pPathTop = NULL;
    SG_string* pstrRepoDescriptorName = NULL;
    SG_vhash* pvhDescriptor = NULL;
    SG_repo* pRepo = NULL;
	SG_rbtree * prbLeaves = NULL;
	SG_rbtree_iterator * pIter = NULL;
	const char* psz_tag = NULL;
	const char* psz_csid = NULL;
    SG_bool b;

	if (psz_repo)
	{
		SG_closet__descriptors__get(pCtx, psz_repo, &pvhDescriptor);

		if (SG_context__has_err(pCtx))
		{
			// psz_repo not found
			if (SG_context__err_equals(pCtx, SG_ERR_VHASH_KEYNOTFOUND))
			{
				SG_context__err_reset(pCtx);
				SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "Could not find repo:  %s\n", psz_repo)  );
				goto fail;
			}
			else // some other error, propagate
				SG_ERR_RETHROW;
		}
	}
	else // if no repo, use repo associated with current working directory
	{
		SG_ERR_CHECK(  SG_PATHNAME__ALLOC(pCtx, &pPathCwd)  );
		SG_ERR_CHECK(  SG_pathname__set__from_cwd(pCtx, pPathCwd)  );
		SG_workingdir__find_mapping(pCtx, pPathCwd, &pPathTop, &pstrRepoDescriptorName, NULL);
		if (SG_context__has_err(pCtx))
		{
			// if no repo assoc with this cwd
			if (SG_context__err_equals(pCtx, SG_ERR_VHASH_KEYNOTFOUND))
			{
				SG_context__err_reset(pCtx);
				SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "There is no repository associated with this directory.  Please specify a repository.\n")  );
				SG_STRING_NULLFREE(pCtx, pstrRepoDescriptorName);
				SG_PATHNAME_NULLFREE(pCtx, pPathTop);
				SG_PATHNAME_NULLFREE(pCtx, pPathCwd);
				goto fail;
			}
			else // some other error, propagate
			{
				SG_STRING_NULLFREE(pCtx, pstrRepoDescriptorName);
				SG_PATHNAME_NULLFREE(pCtx, pPathTop);
				SG_PATHNAME_NULLFREE(pCtx, pPathCwd);
				SG_ERR_RETHROW;
			}
		}

		SG_ERR_CHECK(  SG_closet__descriptors__get(pCtx, SG_string__sz(pstrRepoDescriptorName), &pvhDescriptor)  );
		SG_STRING_NULLFREE(pCtx, pstrRepoDescriptorName);
		SG_PATHNAME_NULLFREE(pCtx, pPathTop);
		SG_PATHNAME_NULLFREE(pCtx, pPathCwd);
	}

	if (pvhDescriptor)
		SG_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, &pRepo, pvhDescriptor)  );
	else // no repo given and no repo associated with working dir?
	{
		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "Cannot find repository.")  );
		goto fail;
	}

	SG_ERR_CHECK(  SG_vc_tags__list(pCtx, pRepo, &prbLeaves)  );

    if (prbLeaves)
    {
        SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pIter, prbLeaves, &b, &psz_tag, (void**)&psz_csid)  );
        while (b)
        {
            SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDOUT, "%14s: %s\n", psz_tag, psz_csid)  );

            SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pIter, &b, &psz_tag, (void**)&psz_csid)  );
        }
        SG_rbtree__iterator__free(pCtx, pIter);
        pIter = NULL;
        SG_RBTREE_NULLFREE(pCtx, prbLeaves);
    }


fail:
	SG_VHASH_NULLFREE(pCtx, pvhDescriptor);
	SG_REPO_NULLFREE(pCtx, pRepo);
	SG_STRING_NULLFREE(pCtx, pstrRepoDescriptorName);
	SG_RBTREE_NULLFREE(pCtx, prbLeaves);
}

//////////////////////////////////////////////////////////////////

/**
 * Perform a version control tree-diff using 2 changesets.
 */
void do_cmd_diff(SG_context * pCtx,
				 SG_uint32 count_args, const char ** paszArgs,
				 SG_bool bRecursive,
				 const char* const* ppszIncludes, SG_uint32 nCountIncludes,
				 const char* const* ppszExcludes, SG_uint32 nCountExcludes,
				 const char* pszRev1, SG_bool b1Rev,
				 const char* pszRev2, SG_bool b2Rev)
{
	SG_treediff2 * pTreeDiff = NULL;
	SG_pathname* pPathCwd = NULL;
	SG_pendingtree* pPendingTree = NULL;
	SG_bool bIgnoreWarnings = SG_FALSE;
	char * psz_rev_1 = NULL;
	char * psz_rev_2 = NULL;
	SG_repo* pRepo = NULL;

	SG_ERR_CHECK(  SG_PATHNAME__ALLOC(pCtx, &pPathCwd)  );
	SG_ERR_CHECK(  SG_pathname__set__from_cwd(pCtx, pPathCwd)  );

	SG_ERR_CHECK(  SG_pendingtree__alloc(pCtx, pPathCwd, bIgnoreWarnings, &pPendingTree)  );

	SG_ERR_CHECK(  SG_pendingtree__get_repo(pCtx, pPendingTree, &pRepo)  );
	if (pszRev1)
	{
		if (b1Rev)
			SG_ERR_CHECK(  SG_repo__hidlookup__dagnode(pCtx, pRepo,SG_DAGNUM__VERSION_CONTROL, pszRev1, &psz_rev_1)  );
		else
			SG_ERR_CHECK(  SG_vc_tags__lookup__tag(pCtx, pRepo, pszRev1, &psz_rev_1)  );
	}

	if (pszRev2)
	{
		if (b2Rev)
			SG_ERR_CHECK(  SG_repo__hidlookup__dagnode(pCtx, pRepo, SG_DAGNUM__VERSION_CONTROL, pszRev2, &psz_rev_2)  );
		else
			SG_ERR_CHECK(  SG_vc_tags__lookup__tag(pCtx, pRepo, pszRev2, &psz_rev_2)  );
	}

	SG_ERR_CHECK(   SG_pendingtree__diff_or_status(pCtx, pPendingTree,
												   psz_rev_1, psz_rev_2,
												   &pTreeDiff)   );

	if ((count_args > 0) || (nCountIncludes > 0) || (nCountExcludes > 0))
		SG_ERR_CHECK(  SG_treediff2__file_spec_filter(pCtx, pTreeDiff,
													  count_args, paszArgs, bRecursive,
													  ppszIncludes, nCountIncludes,
													  ppszExcludes, nCountExcludes)  );

	// TODO for now we hard-code the internal diff tool.
	// TODO later, look this up from some type of config.

	SG_ERR_CHECK(  SG_diff_ext__diff(pCtx, pTreeDiff,SG_internalfilediff__unified,NULL,NULL)  );

	// fall thru to common cleanup.

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathCwd);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_TREEDIFF2_NULLFREE(pCtx, pTreeDiff);
	SG_NULLFREE(pCtx, psz_rev_1);
	SG_NULLFREE(pCtx, psz_rev_2);
}

/**
 * List the heads/tips/leaves.
 */
void do_cmd_heads(SG_context * pCtx)
{
	SG_rbtree * prbLeaves = NULL;
	SG_rbtree_iterator * pIter = NULL;
	SG_repo * pRepo = NULL;
	const char * pszHid_k;
	SG_bool bFound;

	SG_ERR_CHECK(  _get_repo_from_cwd(pCtx, &pRepo, NULL)  );

	SG_ERR_CHECK(  SG_repo__fetch_dag_leaves(pCtx,pRepo,SG_DAGNUM__VERSION_CONTROL,&prbLeaves)  );

	SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx,&pIter,prbLeaves,&bFound,&pszHid_k,NULL)  );
	while (bFound)
	{
		SG_ERR_CHECK(  my_dump_log(pCtx,pRepo,pszHid_k,SG_FALSE)  );

		SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx,pIter,&bFound,&pszHid_k,NULL)  );
	}

	// fall thru to common cleanup.

fail:
	SG_REPO_NULLFREE(pCtx, pRepo);
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIter);
	SG_RBTREE_NULLFREE(pCtx, prbLeaves);
}

/**
 * List the working folder's parent changesets
 */
void do_cmd_parents(SG_context * pCtx, SG_uint32 count_args, const char ** paszArgs, SG_vector* pvec_changesets)
{
	SG_pathname* pPathCwd = NULL;
	SG_vhash* pvhOutput = NULL;		
	SG_repo * pRepo = NULL;	
	char * pszHid = NULL;
	const char* pszArgumentPath = NULL;
	SG_bool bNoArgs = SG_TRUE;
	SG_stringarray * pStringArrayParents = NULL;

	SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, & pvhOutput)  );
	
	// use the CWD to get a descriptor to the REPO and the top of the Working Directory.
	SG_ERR_CHECK(  _get_repo_from_cwd(pCtx, &pRepo, &pPathCwd)  );

	//If a file argument is given, the revision in which the file was last changed (before the working 
	//directory revision or the argument to --rev if given) is printed
	if (count_args == 1)
	{			
		bNoArgs = SG_FALSE;
		pszArgumentPath = paszArgs[0];
	}
	if (pvec_changesets)
	{
		SG_uint32 nResultCount = 0;
		SG_uint32 nParentIndex = 0;
		SG_uint32 revsCount = 0;
		SG_uint32 k = 0;
		bNoArgs = SG_FALSE;
		SG_ERR_CHECK(  SG_vector__length(pCtx, pvec_changesets, &revsCount)  );

		for (k=0; k<revsCount; k++)
		{
			SG_rev_tag_obj* pRTobj = NULL;
			const char * pszCurrentParentHid = NULL;

			// TODO 4/20/10 is there a reason we are only handling --rev and not --tag ?

			SG_ERR_CHECK(  SG_vector__get(pCtx, pvec_changesets, k, (void**)&pRTobj)  );
			SG_ERR_CHECK(  SG_parents__list(pCtx, pRepo, NULL, pRTobj->pszRevTag, pRTobj->bRev, pszArgumentPath, &pStringArrayParents)  );
			if (pStringArrayParents != NULL)
			{
				SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDOUT, "Parents of %s:\n", pRTobj->pszRevTag)  );
				SG_ERR_CHECK(  SG_stringarray__count(pCtx, pStringArrayParents, &nResultCount)  );
				for (nParentIndex = 0; nParentIndex < nResultCount;  nParentIndex++)
				{
					SG_ERR_CHECK(  SG_stringarray__get_nth(pCtx, pStringArrayParents, nParentIndex, &pszCurrentParentHid)  );
					SG_ERR_CHECK(  my_dump_log(pCtx, pRepo, pszCurrentParentHid, SG_FALSE)  );
				}
				SG_STRINGARRAY_NULLFREE(pCtx, pStringArrayParents);
			}
		}
	}
	else
	{
		SG_uint32 nResultCount = 0;
		SG_uint32 nParentIndex = 0;
		const char * pszCurrentParentHid = NULL;
		SG_ERR_CHECK(  SG_parents__list(pCtx, pRepo, NULL, NULL, SG_TRUE, pszArgumentPath, &pStringArrayParents)  );
		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDOUT, "Parents of pending changes in working folder:\n" )  );
		SG_ERR_CHECK(  SG_stringarray__count(pCtx, pStringArrayParents, &nResultCount)  );
		for (nParentIndex = 0; nParentIndex < nResultCount;  nParentIndex++)
		{
			SG_ERR_CHECK(  SG_stringarray__get_nth(pCtx, pStringArrayParents, nParentIndex, &pszCurrentParentHid)  );
			SG_ERR_CHECK(  my_dump_log(pCtx, pRepo, pszCurrentParentHid, SG_FALSE)  );
		}
	}

fail:
	SG_REPO_NULLFREE(pCtx, pRepo);
	SG_PATHNAME_NULLFREE(pCtx, pPathCwd);
	SG_VHASH_NULLFREE(pCtx, pvhOutput);	
	SG_STRINGARRAY_NULLFREE(pCtx, pStringArrayParents);
	if (pszHid)
		SG_NULLFREE(pCtx, pszHid);
}

void do_cmd_pull(SG_context* pCtx, const char** paszArgs)
{
	SG_string* pstrDescriptorName = NULL;
	SG_client* pClient = NULL;
	SG_varray* pvaErr = NULL;
	SG_varray* pvaLog = NULL;

	SG_ERR_CHECK(  _get_descriptor_name_from_cwd(pCtx, &pstrDescriptorName, NULL)  );

	SG_ERR_CHECK(  SG_client__open(pCtx, paszArgs[0], NULL_CREDENTIAL, &pClient)  );

	SG_ERR_CHECK(  SG_pull__all(pCtx, SG_string__sz(pstrDescriptorName), pClient, &pvaErr, &pvaLog)  );

	// TODO: display err, log arrays

	/* fall through */
fail:
	SG_STRING_NULLFREE(pCtx, pstrDescriptorName);
	SG_CLIENT_NULLFREE(pCtx, pClient);
	SG_VARRAY_NULLFREE(pCtx, pvaErr);
	SG_VARRAY_NULLFREE(pCtx, pvaLog);
}

void do_cmd_push(
	SG_context * pCtx,
	SG_option_state * pOptSt,
	SG_uint32 count_args, 
	const char** paszArgs)
{
	SG_repo* pThisRepo = NULL;
	SG_client* pClient = NULL;
	char* pszRevFullHid = NULL;
	SG_rbtree* prbDagnodes = NULL;

	SG_ASSERT(count_args == 1);

	SG_ERR_CHECK(  _get_repo_from_cwd(pCtx, &pThisRepo, NULL)  );

	SG_ERR_CHECK(  SG_client__open(pCtx, paszArgs[0], NULL_CREDENTIAL, &pClient)  );

	if (pOptSt->pvec_rev_tags)
	{
		SG_uint32 i;
		SG_uint32 count = pOptSt->iCountRevs + pOptSt->iCountTags;
		SG_ERR_CHECK(  SG_RBTREE__ALLOC__PARAMS(pCtx, &prbDagnodes, count, NULL)  );
		for (i = 0; i < count; i++)
		{
			SG_rev_tag_obj* pRevTag = NULL;
			SG_ERR_CHECK(  SG_vector__get(pCtx, pOptSt->pvec_rev_tags, i, (void**) &pRevTag)  );
			if (pRevTag->bRev)
			{
				SG_ERR_CHECK(  SG_repo__hidlookup__dagnode(pCtx, pThisRepo, SG_DAGNUM__VERSION_CONTROL, pRevTag->pszRevTag, &pszRevFullHid)  );
			}
			else
			{
				SG_ERR_CHECK(  SG_vc_tags__lookup__tag(pCtx, pThisRepo, pRevTag->pszRevTag, &pszRevFullHid)  );
			}
			SG_ERR_CHECK(  SG_rbtree__add(pCtx, prbDagnodes, pszRevFullHid)  );
			SG_NULLFREE(pCtx, pszRevFullHid);
		}
	}

	if (prbDagnodes)
		SG_ERR_CHECK(  SG_push__dagnodes(pCtx, pThisRepo, pClient, SG_DAGNUM__VERSION_CONTROL, prbDagnodes, pOptSt->bForce)  );
	else
		SG_ERR_CHECK(  SG_push__all(pCtx, pThisRepo, pClient, pOptSt->bForce)  );

	/* fall through */
fail:
	SG_REPO_NULLFREE(pCtx, pThisRepo);
	SG_CLIENT_NULLFREE(pCtx, pClient);
	SG_RBTREE_NULLFREE(pCtx, prbDagnodes);
}

/* ---------------------------------------------------------------- */
/* All the cmd funcs are below */

/*
 * If you are implementing a new cmd func, pick one of the existing
 * ones and copy it as a starting point.  All cmd funcs should follow
 * a similar structure:
 *
 * 1.  Check the flags.  Set bUsageError if anything is wrong.
 *
 * 2.  Check the number of args, set bUsageError if necessary.
 *
 * 3.  If no problems with flags/args, try to do the command.
 *     This will either succeed, throw a hard error, or throw a
 *     usage-error (when it is too complex to fully check here).
 *
 * 4.  if we have a usage-error, print some help and throw.
 */

/**
 * A private macro to run the command and handle hard- and usage-errors.
 * Invoke the command handler using pCtx as an argument.
 *
 * We Assume arg/flag checking code do a THROW for hard errors
 * and *only* set bUsageError when a bad arg/flag was seen.
 *
 * We do the THROW if bUsageError set.  (This allows *ALL*
 * flags/args to reported by the flag/arg checker, rather
 * that just the first.)
 *
 * Try to run the actual command.
 *
 * If it threw an error, we rethrow it.  If something within
 * the command threw Usage, we set our flag to be consistent.
 *
 * If something within the command-block set our flag (but
 * didn't throw), then we take care of it.
 */
#define INVOKE(_c_)	SG_STATEMENT(						\
	SG_ASSERT( (SG_context__has_err(pCtx)==SG_FALSE) );	\
	if (*pbUsageError)									\
		SG_ERR_THROW(  SG_ERR_USAGE  );					\
	_c_;												\
	if (SG_context__has_err(pCtx))						\
	{													\
		if (SG_context__err_equals(pCtx,SG_ERR_USAGE))	\
			*pbUsageError = SG_TRUE;						\
		SG_ERR_RETHROW;									\
	}													\
	else if (*pbUsageError)								\
	{													\
		SG_ERR_THROW(  SG_ERR_USAGE  );					\
	}													)

	
/* ---------------------------------------------------------------- */

DECLARE_CMD_FUNC(hid)
{
	const char** paszArgs = NULL;

	SG_UNUSED(pOptSt);
	SG_UNUSED(pszAppName);
	SG_UNUSED(pszCommandName);

	SG_ERR_CHECK(  _parse_and_count_args(pCtx, pGetopt, &paszArgs, 1, pbUsageError)  );

	INVOKE(  do_cmd_hid(pCtx,paszArgs[0])  );

fail:
	SG_NULLFREE(pCtx, paszArgs);
}

DECLARE_CMD_FUNC(dump_json)
{
	const char** paszArgs = NULL;
	SG_uint32 count_args = 0;

	SG_UNUSED(pOptSt);
	SG_UNUSED(pszAppName);
	SG_UNUSED(pszCommandName);

	SG_ERR_CHECK(  SG_getopt__parse_all_args(pCtx, pGetopt, &paszArgs, &count_args)  );

	INVOKE(  do_cmd_dump_json(pCtx, count_args, paszArgs)  );

fail:
	SG_NULLFREE(pCtx, paszArgs);
}

DECLARE_CMD_FUNC(deleterepo)
{
	const char** paszArgs= NULL;
	SG_uint32 count_args = 0;
	
	SG_UNUSED(pszAppName);
	SG_UNUSED(pszCommandName);

	SG_ERR_CHECK(  SG_getopt__parse_all_args(pCtx, pGetopt, &paszArgs, &count_args)  );
	
	INVOKE(  do_cmd_deleterepo(pCtx, count_args, paszArgs, pOptSt->bAllBut, pOptSt->bForce)  );

fail:
	SG_NULLFREE(pCtx, paszArgs);
}

DECLARE_CMD_FUNC(listrepos)
{
	SG_UNUSED(pGetopt);
	SG_UNUSED(pOptSt);
	SG_UNUSED(pszAppName);
	SG_UNUSED(pszCommandName);

	INVOKE(  do_cmd_listrepos(pCtx)  );

fail:
	;
}

DECLARE_CMD_FUNC(serve)
{
	SG_UNUSED(pGetopt);
	SG_UNUSED(pszAppName);
	SG_UNUSED(pszCommandName);

	INVOKE(  do_cmd_serve(pCtx, pOptSt)  );

fail:
	;
}

DECLARE_CMD_FUNC(history)
{
	const char** paszArgs= NULL;
	const char * pszStamp=NULL;
	SG_uint32 count_args = 0;
	SG_uint32 count_stamps = 0;
	SG_UNUSED(pszCommandName);
	SG_UNUSED(pszAppName);

	SG_ERR_CHECK(  SG_getopt__parse_all_args(pCtx, pGetopt, &paszArgs, &count_args)  );

	/* TODO Put check for valid args here */
	//History only supports one stamp argument at the moment.
	if (pOptSt->psa_stamps != NULL)
	{
		SG_ERR_CHECK(  SG_stringarray__count(pCtx, pOptSt->psa_stamps, &count_stamps)  );
		if (count_stamps > 1)
		{
			SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "History queries only support one stamp at a time.\n")  );
			*pbUsageError = SG_TRUE;
			goto fail;
		}
		else if (count_stamps == 1)
		{
			SG_ERR_CHECK(  SG_stringarray__get_nth(pCtx, pOptSt->psa_stamps, 0, &pszStamp)  );
		}
	}

	INVOKE(
		do_cmd_history(pCtx, count_args, paszArgs, pOptSt->iCountTags + pOptSt->iCountRevs, pOptSt->pvec_rev_tags, pOptSt->psz_username, pszStamp, pOptSt->psz_from_date, pOptSt->psz_to_date, pOptSt->bLeaves, pOptSt->iMaxHistoryResults);
		);

fail:
	SG_NULLFREE(pCtx, paszArgs);
}

DECLARE_CMD_FUNC(localsettings)
{
	const char** paszArgs= NULL;
	SG_uint32 count_args = 0;

	SG_UNUSED(pszAppName);
	SG_UNUSED(pOptSt);
	SG_UNUSED(pszCommandName);


	SG_ERR_CHECK(  SG_getopt__parse_all_args(pCtx, pGetopt, &paszArgs, &count_args)  );

	INVOKE(
		if (0 == count_args)
            do_cmd_localsettings(pCtx, pOptSt->bVerbose);
		else
			do_cmd_localsettings_specific(pCtx, pOptSt->bVerbose,count_args , paszArgs);
		);

fail:
	SG_NULLFREE(pCtx, paszArgs);
}


DECLARE_CMD_FUNC(dump_dag)
{
	const char** paszArgs = NULL;
	SG_uint32 count_args = 0;

	SG_UNUSED(pOptSt);
	SG_UNUSED(pszAppName);
	SG_UNUSED(pszCommandName);

	SG_ERR_CHECK(  SG_getopt__parse_all_args(pCtx, pGetopt, &paszArgs, &count_args)  );

	/* Put check for valid args here */

	INVOKE(  do_cmd_dump_dag(pCtx, count_args, paszArgs)  );

fail:
	SG_NULLFREE(pCtx, paszArgs);
}

DECLARE_CMD_FUNC(dump_lca)
{
	const char** paszArgs = NULL;
	SG_uint32 count_args = 0;

	SG_UNUSED(pOptSt);
	SG_UNUSED(pszAppName);
	SG_UNUSED(pszCommandName);

	SG_ERR_CHECK(  SG_getopt__parse_all_args(pCtx, pGetopt, &paszArgs, &count_args)  );

	/* Put check for valid args here */

	INVOKE(  do_cmd_dump_lca(pCtx, count_args, paszArgs)  );

fail:
	SG_NULLFREE(pCtx, paszArgs);
}

#ifdef DEBUG
DECLARE_CMD_FUNC(dump_treenode)
{
	const char** paszArgs = NULL;
	char* pszChangesetId = NULL;
	SG_uint32 count_args = 0;
	SG_bool bRev = SG_TRUE;

	SG_UNUSED(pOptSt);
	SG_UNUSED(pszAppName);
	SG_UNUSED(pszCommandName);

	SG_ERR_CHECK(  SG_getopt__parse_all_args(pCtx, pGetopt, &paszArgs, &count_args)  );

	if (1 == pOptSt->iCountRevs || 1 == pOptSt->iCountTags)
	{
		SG_rev_tag_obj* pRTobj = NULL;
		SG_ERR_CHECK(  SG_vector__get(pCtx, pOptSt->pvec_rev_tags, 0, (void**) &pRTobj)  );
		pszChangesetId = pRTobj->pszRevTag;
		bRev = pRTobj->bRev;
	}

	INVOKE(  do_cmd_dump_treenode(pCtx, pszChangesetId, bRev)  );

fail:
	SG_NULLFREE(pCtx, paszArgs);
}
#endif


DECLARE_CMD_FUNC(commit)
{
	const char** paszArgs = NULL;
	SG_uint32 count_args = 0;

	SG_UNUSED(pszCommandName);
	SG_UNUSED(pszAppName);

    /* TODO we need to put in "the usual way" of prompting the user for a
     * comment:  Launch vi or notepad and capture the result.  For now, this
     * --comment=="whatever" will have to suffice. */

	SG_ERR_CHECK(  SG_getopt__parse_all_args(pCtx, pGetopt, &paszArgs, &count_args)  );

	/* Put check for valid args here */
	if (pOptSt->bPromptForDescription)
	{
		SG_string* pStrInput;
		SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pStrInput)  );
		SG_console(pCtx, SG_CS_STDOUT, "Please provide a message:\n");
		SG_ERR_CHECK(  SG_console__read_stdin(pCtx, &pStrInput)  );
		SG_ERR_CHECK(  SG_strdup(pCtx, SG_string__sz(pStrInput), &pOptSt->psz_message)  );
		SG_STRING_NULLFREE(pCtx, pStrInput);
	}

	INVOKE(  do_cmd_commit(pCtx,
        count_args, paszArgs,
        pOptSt->psz_message,
        pOptSt->psz_username,
        pOptSt->psa_stamps,
        pOptSt->bRecursive,
        pOptSt->ppIncludes, pOptSt->iCountIncludes,
        pOptSt->ppExcludes, pOptSt->iCountExcludes,
        pOptSt->ppAssocs, pOptSt->iCountAssocs)  );

fail:
	SG_NULLFREE(pCtx, paszArgs);
}

DECLARE_CMD_FUNC(revert)
{
	const char** paszArgs = NULL;
	SG_uint32 count_args = 0;

	if (pOptSt->bTest && pOptSt->bVerbose)		// don't allow both --test and --verbose
	{
		_print_invalid_option_combination(pCtx, pszAppName, pszCommandName, (SG_int32)'T', (SG_int32)opt_verbose);
		*pbUsageError = SG_TRUE;
		goto fail;
	}
	else if (pOptSt->bAll && !pOptSt->bRecursive)	// Don't allow revert if --all is specified with --nonrecursive
	{
		_print_invalid_option(pCtx, pszAppName, pszCommandName, (SG_int32)'N');
        *pbUsageError = SG_TRUE;		
		goto fail;
	}
	
	SG_ERR_CHECK(  SG_getopt__parse_all_args(pCtx, pGetopt, &paszArgs, &count_args)  );

	if ((!paszArgs && !pOptSt->bAll))				// Don't allow revert if no paszArgs are specified (unless --all)
	{
		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "Missing pathnames or --all\n")  );
		*pbUsageError = SG_TRUE;
		goto fail;
	}

	// TODO 4/21/10 If I say "revert --all" should any of the other --include/--exclude/--recursive
	// TODO 4/21/10 and/or files_or_folders stuff be allowed?

	INVOKE(  do_cmd_revert(pCtx,
						   pOptSt,
						   count_args, paszArgs)  );

fail:
	SG_NULLFREE(pCtx, paszArgs);
}

DECLARE_CMD_FUNC(remove)
{
	const char** paszArgs = NULL;
	SG_uint32 count_args = 0;
	
	SG_UNUSED(pszCommandName);
	SG_UNUSED(pszAppName);

	SG_ERR_CHECK(  SG_getopt__parse_all_args(pCtx, pGetopt, &paszArgs, &count_args)  );

	/* Put check for valid args here */

	INVOKE(  do_cmd_remove(pCtx, count_args, paszArgs, pOptSt->ppIncludes, pOptSt->iCountIncludes, pOptSt->ppExcludes, pOptSt->iCountExcludes)  );

fail:
	SG_NULLFREE(pCtx, paszArgs);
}

DECLARE_CMD_FUNC(add)
{
	const char** paszArgs = NULL;
	SG_uint32 count_args = 0;

	SG_UNUSED(pszCommandName);
	SG_UNUSED(pszAppName);

	SG_ERR_CHECK(  SG_getopt__parse_all_args(pCtx, pGetopt, &paszArgs, &count_args)  );

	/* Put check for valid args here */

	INVOKE(  do_cmd_add(pCtx, count_args, paszArgs, pOptSt->bIgnoreWarnings, pOptSt->bRecursive, pOptSt->ppIncludes, pOptSt->iCountIncludes, pOptSt->ppExcludes, pOptSt->iCountExcludes)  );

fail:
	SG_NULLFREE(pCtx, paszArgs);
}

DECLARE_CMD_FUNC(help)
{
	const char** paszArgs = NULL;
	SG_uint32 count_args = 0;
	
	SG_UNUSED(pszCommandName);

	SG_ERR_CHECK(  SG_getopt__parse_all_args(pCtx, pGetopt, &paszArgs, &count_args)  );

	if (
		(0 != count_args)
		&& (1 != count_args)
		)
	{
		*pbUsageError = SG_TRUE;
	}
		
	INVOKE(
		if (0 == count_args)
			do_cmd_help__list_all_commands(pCtx, pszAppName, pOptSt->bShowAll);
		else
		{
			do_cmd_help__list_command_help(pCtx, paszArgs[0], SG_TRUE);

			if (SG_context__err_equals(pCtx, SG_ERR_USAGE))
			{
				do_cmd_help__list_all_commands(pCtx, pszAppName, pOptSt->bShowAll);
				SG_context__err_reset(pCtx);
			}
		}
		);

fail:
	SG_NULLFREE(pCtx, paszArgs);
}

DECLARE_CMD_FUNC(vacuum)
{
	const char** paszArgs = NULL;
	
	SG_UNUSED(pOptSt);
	SG_UNUSED(pszCommandName);
	SG_UNUSED(pszAppName);

	SG_ERR_CHECK(  _parse_and_count_args(pCtx, pGetopt, &paszArgs, 1, pbUsageError)  );

    INVOKE(  do_cmd_vacuum(pCtx, paszArgs[0])  );

fail:
	SG_NULLFREE(pCtx, paszArgs);
}

DECLARE_CMD_FUNC(version)
{
	SG_UNUSED(pOptSt);
	SG_UNUSED(pGetopt);
	SG_UNUSED(pszCommandName);
	SG_UNUSED(pszAppName);
	SG_UNUSED(pbUsageError);

	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDOUT, "%s.%s.%s.%s%s\n", MAJORVERSION, MINORVERSION, REVVERSION, BUILDNUMBER, DEBUGSTRING)  );
}

DECLARE_CMD_FUNC(vpack)
{
	const char** paszArgs = NULL;
	
	SG_UNUSED(pOptSt);
	SG_UNUSED(pszAppName);
	SG_UNUSED(pszCommandName);

	SG_ERR_CHECK(  _parse_and_count_args(pCtx, pGetopt, &paszArgs, 1, pbUsageError)  );

    INVOKE(  do_cmd_vpack(pCtx, paszArgs[0])  );

fail:
	SG_NULLFREE(pCtx, paszArgs);
}

DECLARE_CMD_FUNC(zpack)
{
	const char** paszArgs = NULL;
	
	SG_UNUSED(pOptSt);
	SG_UNUSED(pszAppName);
	SG_UNUSED(pszCommandName);

	SG_ERR_CHECK(  _parse_and_count_args(pCtx, pGetopt, &paszArgs, 1, pbUsageError)  );

    INVOKE(  do_cmd_zpack(pCtx, paszArgs[0])  );

fail:
	SG_NULLFREE(pCtx, paszArgs);
}


DECLARE_CMD_FUNC(unpack)
{
	const char** paszArgs = NULL;
	
	SG_UNUSED(pOptSt);
	SG_UNUSED(pszCommandName);
	SG_UNUSED(pszAppName);

	SG_ERR_CHECK(  _parse_and_count_args(pCtx, pGetopt, &paszArgs, 1, pbUsageError)  );

    INVOKE(  do_cmd_unpack(pCtx, paszArgs[0])  );

fail:
	SG_NULLFREE(pCtx, paszArgs);
}


DECLARE_CMD_FUNC(blobcount)
{
	const char** paszArgs = NULL;
	
	SG_UNUSED(pOptSt);
	SG_UNUSED(pszCommandName);
	SG_UNUSED(pszAppName);

	SG_ERR_CHECK(  _parse_and_count_args(pCtx, pGetopt, &paszArgs, 1, pbUsageError)  );

    INVOKE(  do_cmd_blobcount(pCtx, paszArgs[0])  );

fail:
	SG_NULLFREE(pCtx, paszArgs);
}


DECLARE_CMD_FUNC(group)
{
    SG_uint32 op = 0;
	const char** paszArgs = NULL;
    SG_uint32 count_args = 0;
	
	SG_UNUSED(pszCommandName);
	SG_UNUSED(pszAppName);

	SG_ERR_CHECK(  SG_getopt__parse_all_args(pCtx, pGetopt, &paszArgs, &count_args)  );

    if (count_args < 1)
    {
            SG_ERR_THROW2(  SG_ERR_INVALIDARG,
                           (pCtx, "Must include group name")  );
    }

    if (pOptSt->bAdd)
    {
        if (pOptSt->bRemove)
        {
            SG_ERR_THROW2(  SG_ERR_INVALIDARG,
                           (pCtx, "Cannot --add and --remove")  );
        }
        else
        {
            if (pOptSt->bSubgroups)
            {
                op = MY_CMD_GROUP_OP__ADD_SUBGROUPS;
            }
            else
            {
                op = MY_CMD_GROUP_OP__ADD_USERS;
            }
        }
    }
    else if (pOptSt->bRemove)
    {
        if (pOptSt->bSubgroups)
        {
            op = MY_CMD_GROUP_OP__REMOVE_SUBGROUPS;
        }
        else
        {
            op = MY_CMD_GROUP_OP__REMOVE_USERS;
        }
    }
    else
    {
        if (pOptSt->bSubgroups)
        {
            SG_ERR_THROW2(  SG_ERR_INVALIDARG,
                           (pCtx, "--subgroups is invalid with --add or --remove")  );
        }

        op = MY_CMD_GROUP_OP__LIST;
    }

    if (op != MY_CMD_GROUP_OP__LIST)
    {
        if (count_args < 2)
        {
            SG_ERR_THROW2(  SG_ERR_INVALIDARG,
                           (pCtx, "Must include at least one name to add or remove")  );
        }
    }

    INVOKE(  do_cmd_group(pCtx, paszArgs[0], pOptSt->psz_repo, op, paszArgs+1, count_args-1)  );

fail:
	SG_NULLFREE(pCtx, paszArgs);
}
    
DECLARE_CMD_FUNC(zingmerge)
{
	const char** paszArgs = NULL;
	SG_uint32 count_args = 0;
	
	SG_UNUSED(pszCommandName);
	SG_UNUSED(pszAppName);

	SG_ERR_CHECK(  SG_getopt__parse_all_args(pCtx, pGetopt, &paszArgs, &count_args)  );

    INVOKE(  do_zingmerge(pCtx, pOptSt->psz_repo)  );

fail:
	SG_NULLFREE(pCtx, paszArgs);
}
    
DECLARE_CMD_FUNC(whoami)
{
	const char** paszArgs = NULL;
	SG_uint32 count_args = 0;
	
	SG_UNUSED(pszCommandName);
	SG_UNUSED(pszAppName);

    // TODO what about scope of whoami information?

	SG_ERR_CHECK(  SG_getopt__parse_all_args(pCtx, pGetopt, &paszArgs, &count_args)  );
    if (1 == count_args)
    {
        INVOKE(  do_cmd_set_whoami(pCtx, paszArgs[0], pOptSt->psz_repo)  );
    }
    else
    {
        // TODO usage error
    }

fail:
	SG_NULLFREE(pCtx, paszArgs);
}
    
DECLARE_CMD_FUNC(createuser)
{
	const char** paszArgs = NULL;
	
	SG_UNUSED(pszCommandName);
	SG_UNUSED(pszAppName);

	SG_ERR_CHECK(  _parse_and_count_args(pCtx, pGetopt, &paszArgs, 1, pbUsageError)  );

    INVOKE(  do_cmd_createuser(pCtx, paszArgs[0], pOptSt->psz_repo)  );

fail:
	SG_NULLFREE(pCtx, paszArgs);
}
    
DECLARE_CMD_FUNC(creategroup)
{
	const char** paszArgs = NULL;
	
	SG_UNUSED(pszCommandName);
	SG_UNUSED(pszAppName);

	SG_ERR_CHECK(  _parse_and_count_args(pCtx, pGetopt, &paszArgs, 1, pbUsageError)  );

    INVOKE(  do_cmd_creategroup(pCtx, paszArgs[0], pOptSt->psz_repo)  );

fail:
	SG_NULLFREE(pCtx, paszArgs);
}
    
DECLARE_CMD_FUNC(users)
{
	SG_UNUSED(pszCommandName);
	SG_UNUSED(pszAppName);
	SG_UNUSED(pGetopt);

    INVOKE(  do_cmd_users(pCtx, pOptSt->psz_repo)  );

fail:
    ;
}
    
DECLARE_CMD_FUNC(groups)
{
	SG_UNUSED(pszCommandName);
	SG_UNUSED(pszAppName);
	SG_UNUSED(pGetopt);

    INVOKE(  do_cmd_groups(pCtx, pOptSt->psz_repo)  );

fail:
    ;
}
    
DECLARE_CMD_FUNC(clone)
{
	const char** paszArgs = NULL;
	
	SG_UNUSED(pOptSt);
	SG_UNUSED(pszCommandName);
	SG_UNUSED(pszAppName);

	SG_ERR_CHECK(  _parse_and_count_args(pCtx, pGetopt, &paszArgs, 2, pbUsageError)  );

    INVOKE(  do_cmd_clone(pCtx, paszArgs[0], paszArgs[1])  );

fail:
	SG_NULLFREE(pCtx, paszArgs);
}
    
DECLARE_CMD_FUNC(checkout)
{
	const char** paszArgs = NULL;
	const char* psz_hid = NULL;
	const char* pszTag = NULL;
	const char* pszRev = NULL;
	SG_bool bRev = SG_FALSE;

	SG_UNUSED(pszAppName);

	// the options parser will accept multiple --rev/--tag values
	SG_ERR_CHECK(  _validate_rev_tag_options(pCtx, pszCommandName, pOptSt, pbUsageError)  );
	if (*pbUsageError)
	{
		goto fail;
	}
	else
	{
		SG_rev_tag_obj* pRTobj = NULL;
		// rev/tag options passed validation, we either have none
		// or exactly one rev or tag
		if (pOptSt->iCountRevs == 1 || pOptSt->iCountTags == 1)
		{
			SG_ERR_CHECK(  SG_vector__get(pCtx, pOptSt->pvec_rev_tags, 0, (void**) &pRTobj)  );
			if (pRTobj->bRev)
				pszRev = pRTobj->pszRevTag;
			else
				pszTag = pRTobj->pszRevTag;

			bRev = pRTobj->bRev;
		}
	}

	if (! *pbUsageError)
	{
		// lookup is done in do_cmd_checkout	
		if (pOptSt->iCountRevs == 1)
			psz_hid = pszRev;
		else if (pOptSt->iCountTags == 1)
			psz_hid = pszTag;
	}

	SG_ERR_CHECK(  _parse_and_count_args(pCtx, pGetopt, &paszArgs, 2, pbUsageError)  );

	INVOKE(  do_cmd_checkout(pCtx, paszArgs[0], psz_hid, bRev, paszArgs[1])  );

fail:
	SG_NULLFREE(pCtx, paszArgs);
}

DECLARE_CMD_FUNC(rename)
{
	const char** paszArgs = NULL;
	
	SG_UNUSED(pszCommandName);
	SG_UNUSED(pszAppName);

	SG_ERR_CHECK(  _parse_and_count_args(pCtx, pGetopt, &paszArgs, 2, pbUsageError)  );
		
	INVOKE(  do_cmd_rename(pCtx, paszArgs[0], paszArgs[1], pOptSt->bIgnoreWarnings, pOptSt->bForce)  );

fail:
	SG_NULLFREE(pCtx, paszArgs);
}

DECLARE_CMD_FUNC(resolve)
{
	const char** paszArgs = NULL;
	SG_uint32 count_args = 0;
	SG_uint32 sum_simple;
	SG_uint32 sum_fix;
	SG_uint32 sum_verb;

	SG_UNUSED(pszCommandName);
	SG_UNUSED(pszAppName);

	SG_ERR_CHECK(  SG_getopt__parse_all_args(pCtx, pGetopt, &paszArgs, &count_args)  );

	// <mark>   :== --mark <x>
	// <unmark> :== --unmark <x>
	// <simple> :== --list | --listall | <mark> | --markall | <unmark> | --unmarkall
	// <fix>    :== TODO
	// <verb>   :== <simple> | <fix>
	//
	// vv resolve <verb>

	sum_simple = (pOptSt->bList + pOptSt->bListAll
				  + pOptSt->bMark + pOptSt->bMarkAll
				  + pOptSt->bUnmark + pOptSt->bUnmarkAll);
	sum_fix = 0;	// TODO
	sum_verb = sum_simple + sum_fix;

	if (sum_verb != 1)
	{
		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "TODO require exactly one option message...\n")  );
		*pbUsageError = SG_TRUE;
		goto fail;
	}

	if (sum_simple && paszArgs)
	{
		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "TODO no args allowed with simple option message...\n")  );
		*pbUsageError = SG_TRUE;
		goto fail;
	}

	// TODO check constraints on <fix>

	INVOKE(  do_cmd_resolve(pCtx, pOptSt, count_args, paszArgs)  );

fail:
	SG_NULLFREE(pCtx, paszArgs);
}

DECLARE_CMD_FUNC(move)
{
	const char** paszArgs = NULL;
	
	SG_UNUSED(pszCommandName);
	SG_UNUSED(pszAppName);

	SG_ERR_CHECK(  _parse_and_count_args(pCtx, pGetopt, &paszArgs, 2, pbUsageError)  );

	INVOKE(  do_cmd_move(pCtx, paszArgs[0], paszArgs[1], pOptSt->bIgnoreWarnings, pOptSt->bForce)  );

fail:
	SG_NULLFREE(pCtx, paszArgs);
}

DECLARE_CMD_FUNC(new)
{
	const char** paszArgs = NULL;
	SG_uint32 count_args = 0;
	
	SG_UNUSED(pOptSt);
	SG_UNUSED(pszCommandName);
	SG_UNUSED(pszAppName);

	SG_ERR_CHECK(  SG_getopt__parse_all_args(pCtx, pGetopt, &paszArgs, &count_args)  );

	if (1 != count_args && 2 != count_args)
	{
		*pbUsageError = SG_TRUE;
	}

	INVOKE(  do_cmd_new(pCtx, paszArgs[0])  );

fail:
	SG_NULLFREE(pCtx, paszArgs);
}

DECLARE_CMD_FUNC(parents)
{
	const char** paszArgs = NULL;
	SG_uint32 count_args = 0;

	SG_UNUSED(pszCommandName);
	SG_UNUSED(pszAppName);

	SG_ERR_CHECK(  SG_getopt__parse_all_args(pCtx, pGetopt, &paszArgs, &count_args)  );

	if (count_args > 1)  //We support only one argument
		*pbUsageError = SG_TRUE;
	else
		INVOKE(  do_cmd_parents(pCtx, count_args, paszArgs, pOptSt->pvec_rev_tags)  );

fail:
	SG_NULLFREE(pCtx, paszArgs);
}


DECLARE_CMD_FUNC(scan)
{
	const char** paszArgs = NULL;

	SG_UNUSED(pszCommandName);
	SG_UNUSED(pszAppName);

	SG_ERR_CHECK(  _parse_and_count_args(pCtx, pGetopt, &paszArgs, 0, pbUsageError)  );
	
	INVOKE(  do_cmd_scan(pCtx, pOptSt->bForce)  );

fail:
	SG_NULLFREE(pCtx, paszArgs);
}

DECLARE_CMD_FUNC(addremove)
{
	SG_uint32 i;
	const char** paszArgs = NULL;
	SG_uint32 count_args = 0;
	
	SG_ERR_CHECK(  SG_getopt__parse_all_args(pCtx, pGetopt, &paszArgs, &count_args)  );

	if (0 != count_args)
	{
		for (i=0; i<count_args; i++)
		{
			SG_console(pCtx, SG_CS_STDERR, "Invalid argument for '%s %s': %s\n", pszAppName, pszCommandName, paszArgs[i]);
		}
		*pbUsageError = SG_TRUE;
	}
		
	INVOKE(  do_cmd_addremove(pCtx, count_args, paszArgs, pOptSt->bIgnoreWarnings, pOptSt->bRecursive, pOptSt->ppIncludes, pOptSt->iCountIncludes, pOptSt->ppExcludes, pOptSt->iCountExcludes)  );

fail:
	SG_NULLFREE(pCtx, paszArgs);
}

DECLARE_CMD_FUNC(status)
{
	const char** paszArgs = NULL;
	SG_uint32 count_args = 0;
	
	SG_UNUSED(pszCommandName);
	SG_UNUSED(pszAppName);

	SG_ERR_CHECK(  SG_getopt__parse_all_args(pCtx, pGetopt, &paszArgs, &count_args)  );

	// TODO get --rev or --tag 1
	// TODO get --rev or --tag 2

	INVOKE(  do_cmd_status(pCtx,
						   count_args,paszArgs,
						   pOptSt->bRecursive,
						   pOptSt->bVerbose,
						   pOptSt->ppIncludes, pOptSt->iCountIncludes,
						   pOptSt->ppExcludes, pOptSt->iCountExcludes)  );

fail:
	SG_NULLFREE(pCtx, paszArgs);
	if (pOptSt->psa_excludes)
		SG_STRINGARRAY_NULLFREE(pCtx, pOptSt->psa_excludes);

	if (pOptSt->psa_includes)
		SG_STRINGARRAY_NULLFREE(pCtx, pOptSt->psa_includes);
}

DECLARE_CMD_FUNC(stamp)
{
	const char** paszArgs = NULL;
	SG_uint32 count_args = 0;

	SG_UNUSED(pszCommandName);
	SG_UNUSED(pszAppName);

	SG_ERR_CHECK(  SG_getopt__parse_all_args(pCtx, pGetopt, &paszArgs, &count_args)  );

    INVOKE(  do_cmd_stamp(pCtx, pOptSt->bRemove, pOptSt->psa_stamps, pOptSt->pvec_rev_tags)  );

fail:
	SG_NULLFREE(pCtx, paszArgs);
}

DECLARE_CMD_FUNC(stamps)
{
	SG_UNUSED(pszCommandName);
	SG_UNUSED(pszAppName);
	SG_UNUSED(pOptSt);
	SG_UNUSED(pGetopt);

    INVOKE(  do_cmd_stamps(pCtx)  );

fail:
    return;
}

#if 0
DECLARE_CMD_FUNC(slurp)
{
	SG_bool bUsageError = SG_FALSE;
	SG_uint32 i;
	cmdinfo* thisCmd;
	const char** paszArgs;
	SG_uint32 count_args = 0;
	
	_get_canonical_command(pszCommandName, &thisCmd);


	SG_ERR_CHECK(  SG_getopt__parse_all_args(pCtx, pGetopt, &paszArgs, &count_args)  );

    if (2 != count_args)
    {
		bUsageError = SG_TRUE;
    }

    INVOKE(  do_cmd_slurp(pCtx, paszArgs[0], paszArgs[1])  );

fail:
	if (bUsageError)
	{
        SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "Usage: %s %s", pszAppName, pszCommandName)  );
        SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, " descriptor_name filename.fragball\n")  );
	}
	
	SG_NULLFREE(pCtx, paszArgs);
}

DECLARE_CMD_FUNC(fragball)
{
	SG_bool bUsageError = SG_FALSE;
	SG_uint32 i;
	cmdinfo* thisCmd;
	const char** paszArgs;
	SG_uint32 count_args = 0;
	
	_get_canonical_command(pszCommandName, &thisCmd);

	SG_ERR_CHECK(  SG_getopt__parse_all_args(pCtx, pGetopt, &paszArgs, &count_args)  );

    if (2 != count_args)
    {
		bUsageError = SG_TRUE;
    }

    INVOKE(  do_cmd_fragball(pCtx, paszArgs[0], paszArgs[1])  );

fail:
	if (bUsageError)
	{
        SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "Usage: %s %s", pszAppName, pszCommandName)  );
        SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, " filename.fragball changesetid\n")  );
	}
	
	SG_NULLFREE(pCtx, paszArgs);
}
#endif

DECLARE_CMD_FUNC(comment)
{
	const char** paszArgs = NULL;
	SG_uint32 count_args = 0;
	char* pszChangesetId = NULL;
	const char* pszRepoDesc = NULL;
	SG_bool bRev;
	
	SG_UNUSED(pszAppName);

	SG_ERR_CHECK(  _validate_rev_tag_options(pCtx, pszCommandName, pOptSt, pbUsageError)  );
	if (*pbUsageError)
	{
		goto fail;
	}

	SG_ERR_CHECK(  SG_getopt__parse_all_args(pCtx, pGetopt, &paszArgs, &count_args)  );

	// REPOSITORY is the only valid arg, but not mandatory
	if (1 < count_args)
    {
		*pbUsageError = SG_TRUE;
    }
	else if (1 == count_args)
	{
		pszRepoDesc = paszArgs[0];
	}
		
	if (1 == pOptSt->iCountRevs || 1 == pOptSt->iCountTags)
	{
		SG_rev_tag_obj* pRTobj = NULL;
		SG_ERR_CHECK(  SG_vector__get(pCtx, pOptSt->pvec_rev_tags, 0, (void**) &pRTobj)  );
		pszChangesetId = pRTobj->pszRevTag;
		bRev = pRTobj->bRev;
	}
	else
	{
		// do we want to default to tip if we have one?
		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "Please provide a revision using --rev or --tag\n")  );
		*pbUsageError = SG_TRUE;
		goto fail;
	}

	if (pOptSt->bPromptForDescription) //&& _isatty(stdin))
	{
		SG_string* pStrInput;
		SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pStrInput)  );
		SG_console(pCtx, SG_CS_STDOUT, "Please provide a message:\n");
		SG_ERR_CHECK(  SG_console__read_stdin(pCtx, &pStrInput)  );
		SG_ERR_CHECK(  SG_strdup(pCtx, SG_string__sz(pStrInput), &pOptSt->psz_message)  );
		SG_STRING_NULLFREE(pCtx, pStrInput);
	}

	INVOKE(  do_cmd_comment(pCtx, pszChangesetId, bRev, pszRepoDesc, pOptSt->psz_message)  );

fail:
	SG_NULLFREE(pCtx, paszArgs);
}

DECLARE_CMD_FUNC(tag)
{
	const char** paszArgs = NULL;
	SG_uint32 count_args = 0;
	SG_bool bRev = SG_FALSE;
	const char * pszRevTag = NULL;
	
	SG_UNUSED(pszAppName);

	SG_ERR_CHECK(  _validate_rev_tag_options(pCtx, pszCommandName, pOptSt, pbUsageError)  );
	if (*pbUsageError)
	{
		goto fail;
	}

	if (pOptSt->iCountRevs == 1)
	{
		SG_rev_tag_obj* pRTobj = NULL;
		bRev = SG_TRUE;
		SG_ERR_CHECK(  SG_vector__get(pCtx, pOptSt->pvec_rev_tags, 0, (void**) &pRTobj)  );
		pszRevTag = pRTobj->pszRevTag;

	}
	else if (pOptSt->iCountTags == 1)
	{
		SG_rev_tag_obj* pRTobj = NULL;
		bRev = SG_FALSE;
		SG_ERR_CHECK(  SG_vector__get(pCtx, pOptSt->pvec_rev_tags, 0, (void**) &pRTobj)  );
		pszRevTag = pRTobj->pszRevTag;
	}

	SG_ERR_CHECK(  SG_getopt__parse_all_args(pCtx, pGetopt, &paszArgs, &count_args)  );

	INVOKE(
		if (pOptSt->bRemove)
			do_cmd_remove_tags(pCtx, pszRevTag, bRev, count_args, paszArgs);
		else 
			do_cmd_tag(pCtx, pszRevTag, bRev, pOptSt->bForce, paszArgs, count_args);
		);

fail:
	SG_NULLFREE(pCtx, paszArgs);
}


DECLARE_CMD_FUNC(cat)
{
	SG_uint32 count_args = 0;
	const char** paszArgs = NULL;
	const char* pszTag = NULL;
	const char* pszRev = NULL;
	SG_rev_tag_obj* pRTobj = NULL;

	SG_UNUSED(pszAppName);

	// the options parser will accept multiple --rev/--tag values
	SG_ERR_CHECK(  _validate_rev_tag_options(pCtx, pszCommandName, pOptSt, pbUsageError)  );
	if (*pbUsageError)
	{
		goto fail;
	}


	// rev/tag options passed validation, we either have none
	// or exactly one rev or tag
	if (pOptSt->iCountRevs == 1 || pOptSt->iCountTags == 1)
	{
		SG_ERR_CHECK(  SG_vector__get(pCtx, pOptSt->pvec_rev_tags, 0, (void**)&pRTobj)  );
		if (pRTobj->bRev)
			pszRev = pRTobj->pszRevTag;
		else
			pszTag = pRTobj->pszRevTag;
	}

	SG_ERR_CHECK(  SG_getopt__parse_all_args(pCtx, pGetopt, &paszArgs, &count_args)  );

	INVOKE(  do_cmd_cat(pCtx, count_args, paszArgs, pOptSt->psz_repo, pszRev, pszTag)  );

fail:
	SG_NULLFREE(pCtx, paszArgs);
}

DECLARE_CMD_FUNC(vcdiff)
{
	SG_uint32 count_args = 0;
	const char** paszArgs = NULL;

	SG_UNUSED(pOptSt);
	SG_UNUSED(pszCommandName);
	SG_UNUSED(pszAppName);

	SG_ERR_CHECK(  SG_getopt__parse_all_args(pCtx, pGetopt, &paszArgs, &count_args)  );

	INVOKE(  do_cmd_vcdiff(pCtx, count_args, paszArgs)  );

fail:
	SG_NULLFREE(pCtx, paszArgs);
}


DECLARE_CMD_FUNC(diff)
{
	SG_uint32 count_args = 0;
	const char** paszArgs = NULL;
	const char* pszRev1 = NULL;
	SG_bool b1Rev = SG_FALSE;
	const char* pszRev2 = NULL;
	SG_bool b2Rev = SG_FALSE;

	SG_UNUSED(pszCommandName);
	SG_UNUSED(pszAppName);

	SG_ERR_CHECK(  SG_getopt__parse_all_args(pCtx, pGetopt, &paszArgs, &count_args)  );

	// get any --rev or --tag options
	if (pOptSt->pvec_rev_tags)
	{
		if (pOptSt->iCountRevs + pOptSt->iCountTags > 2)
		{
			SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "diff accepts a maximum of two --rev/--tag options\n")  );
			*pbUsageError = SG_TRUE;
		}
		else
		{
			SG_rev_tag_obj* pRTobj = NULL;
			SG_uint32 iLength = 0;
			SG_ERR_CHECK(  SG_vector__length(pCtx, pOptSt->pvec_rev_tags, &iLength)  );
			SG_ERR_CHECK(  SG_vector__get(pCtx, pOptSt->pvec_rev_tags, 0, (void**)&pRTobj)  );
			pszRev1 = pRTobj->pszRevTag;
			b1Rev = pRTobj->bRev;
			if (iLength == 2)
			{
				SG_ERR_CHECK(  SG_vector__get(pCtx, pOptSt->pvec_rev_tags, 1, (void**)&pRTobj)  );
				pszRev2 = pRTobj->pszRevTag;
				b2Rev = pRTobj->bRev;
			}
		}
	}

	INVOKE(  do_cmd_diff(pCtx,
						 count_args, paszArgs, pOptSt->bRecursive,
						 pOptSt->ppIncludes, pOptSt->iCountIncludes,
						 pOptSt->ppExcludes, pOptSt->iCountExcludes,
						 pszRev1, b1Rev, pszRev2, b2Rev)  );

fail:
	SG_NULLFREE(pCtx, paszArgs);
}

DECLARE_CMD_FUNC(merge)
{
	const char** paszArgs = NULL;

	SG_UNUSED(pszCommandName);
	SG_UNUSED(pszAppName);

	/**
	 * Merge all of the named CSETs with the current BASELINE and update the WD
	 * to reflect the merge.  WE DO NOT AUTO-COMMIT THIS.
	 * If no CSETs are given, we lookup all of the heads/tips and use them.
	 *
	 * We DO NOT allow any files/folders args (nor any of the --include/--exclude stuff)
	 * because MERGE is a CSET-level operation.
	 */

	SG_ERR_CHECK(  _parse_and_count_args(pCtx, pGetopt, &paszArgs, 0, pbUsageError)  );

	INVOKE(  do_cmd_merge(pCtx, pOptSt)  );

fail:
	SG_NULLFREE(pCtx, paszArgs);
}

DECLARE_CMD_FUNC(tags)
{
	SG_uint32 count_args = 0;
	const char** paszArgs = NULL;

	SG_UNUSED(pOptSt);
	SG_UNUSED(pszCommandName);
	SG_UNUSED(pszAppName);

	SG_ERR_CHECK(  SG_getopt__parse_all_args(pCtx, pGetopt, &paszArgs, &count_args)  );

	/* Put check for valid args here */

	INVOKE(  
		if (1 == count_args)
			do_cmd_tags(pCtx, paszArgs[0]);
		else if (0 == count_args)
			do_cmd_tags(pCtx, NULL);
		else
			*pbUsageError = SG_TRUE;
	);

fail:
	SG_NULLFREE(pCtx, paszArgs);
}

DECLARE_CMD_FUNC(heads)
{
	SG_UNUSED(pGetopt);
	SG_UNUSED(pOptSt);
	SG_UNUSED(pszCommandName);
	SG_UNUSED(pszAppName);

	INVOKE(  do_cmd_heads(pCtx)  );

fail:
	;
}

DECLARE_CMD_FUNC(update)
{
	const char ** paszArgs = NULL;

	if (pOptSt->bTest && pOptSt->bVerbose)		// don't allow both --test and --verbose
	{
		_print_invalid_option_combination(pCtx, pszAppName, pszCommandName, (SG_int32)'T', (SG_int32)opt_verbose);
		*pbUsageError = SG_TRUE;
		goto fail;
	}

	SG_ERR_CHECK(  _validate_rev_tag_options(pCtx, pszCommandName, pOptSt, pbUsageError)  );
	if (*pbUsageError)
	{
		goto fail;
	}

	// TODO see if there is a function in SG_getopt to return the number of non-option arguments
	// TODO rather than extracting an array of everything left over.  (Or hack __parse_all_args
	// TODO to take a null for the array pointer and have it just return the count.)
	SG_ERR_CHECK(  _parse_and_count_args(pCtx, pGetopt, &paszArgs, 0, pbUsageError)  );

	INVOKE(  do_cmd_update(pCtx, pOptSt)  );

fail:
	SG_NULLFREE(pCtx, paszArgs);
}

DECLARE_CMD_FUNC(pull)
{
	const char ** paszArgs = NULL;

	SG_UNUSED(pOptSt);
	SG_UNUSED(pszCommandName);
	SG_UNUSED(pszAppName);

	SG_ERR_CHECK(  _parse_and_count_args(pCtx, pGetopt, &paszArgs, 1, pbUsageError)  );

	INVOKE(  do_cmd_pull(pCtx, paszArgs)  );

fail:
	SG_NULLFREE(pCtx, paszArgs);
}

DECLARE_CMD_FUNC(push)
{
	const char ** paszArgs = NULL;
	SG_uint32 count_args;

	SG_UNUSED(pszCommandName);
	SG_UNUSED(pszAppName);

	SG_ERR_CHECK(  SG_getopt__parse_all_args(pCtx, pGetopt, &paszArgs, &count_args)  );
	if (count_args != 1)
		*pbUsageError = SG_TRUE;


	INVOKE(  do_cmd_push(pCtx, pOptSt, count_args, paszArgs)  );

fail:
	SG_NULLFREE(pCtx, paszArgs);
}

//////////////////////////////////////////////////////////////////

/**
 * convert (argc,argv) into a SG_getopt.  This takes care of converting
 * platform strings into UTF-8.
 *
 */

static void _intern_argv(SG_context * pCtx, int argc, ARGV_CHAR_T ** argv, SG_getopt ** ppGetopt)
{
	if (argc < 1)				// this should never happen
	{
		SG_console(pCtx, SG_CS_STDERR, "Usage sg command [args]\n");
		SG_ERR_THROW_RETURN(  SG_ERR_USAGE  );
	}

	SG_ERR_CHECK_RETURN(  SG_getopt__alloc(pCtx, argc, argv, ppGetopt)  );

	(*ppGetopt)->interleave = 1;

}

static void _parse_option_port(SG_context *pCtx, const char *portname, const char *optstr, SG_int32 *pval)
{
	SG_int64 val;

	SG_ASSERT(SG_INT32_MAX >= 65535);
	SG_NULLARGCHECK_RETURN(portname);
	SG_NULLARGCHECK_RETURN(optstr);
	SG_NULLARGCHECK_RETURN(pval);

	SG_ERR_CHECK(  SG_int64__parse(pCtx, &val, optstr)  );

	if ((val < 1) || (val > 65535))
	{
		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "invalid value for --%s: %li -- must be an integer from 1 to 65535", portname, (long)val)  );
		SG_context__err(pCtx, SG_ERR_USAGE, __FILE__, __LINE__, "invalid port value");
	}
	else
	{
		*pval = (SG_int32)val;
	}

fail:
	;
}

static void _parse_options(SG_context * pCtx, SG_getopt ** ppGetopt, SG_option_state ** ppOptSt)
{
	const char* pszOptArg = NULL;
	SG_uint32 iOptId;
	SG_bool bNoMoreOptions = SG_FALSE;

	SG_ERR_CHECK(  SG_alloc1(pCtx, *ppOptSt)  );
	// SG_zero(**ppOptSt); -- NOTE unnecessary since SG_alloc1 calls calloc().

	SG_ERR_CHECK(  SG_alloc(pCtx, (*ppGetopt)->count_args, sizeof(SG_int32), &(*ppOptSt)->paRecvdOpts)  );

	// init default values -- NOTE only need to set things that are non-zero (because of above calloc())
	(*ppOptSt)->bRecursive = SG_TRUE;
	(*ppOptSt)->bPromptForDescription = SG_TRUE;
	(*ppOptSt)->iPort = 8080;
	(*ppOptSt)->iSPort = 8081;
	(*ppOptSt)->iMaxHistoryResults = SG_UINT32_MAX;

	while (1)
	{
		SG_getopt__long(pCtx, *ppGetopt, sg_cl_options, &iOptId, &pszOptArg, &bNoMoreOptions);

		if (bNoMoreOptions)
		{
			SG_context__err_reset(pCtx);
			break;
		}
		else if (SG_context__has_err(pCtx))
		{
			const char* pszErr;
			SG_context__err_get_description(pCtx, &pszErr);
			SG_context__push_level(pCtx);
			SG_console(pCtx, SG_CS_STDERR, pszErr);
			SG_console(pCtx, SG_CS_STDERR, "\n\n");
			SG_context__pop_level(pCtx);
			SG_ERR_RETHROW;
		}

		(*ppOptSt)->paRecvdOpts[(*ppOptSt)->count_options] = iOptId;
		(*ppOptSt)->count_options++;

		// TODO:  we should probably do some checking per option here
		// i.e. do the strings being passed in look like they should
		// for things like opt_changeset

		switch(iOptId)
		{
		case opt_all:
			{
				(*ppOptSt)->bAll = SG_TRUE;
			}
			break;
		case opt_all_but:
			{
				(*ppOptSt)->bAllBut = SG_TRUE;
			}
			break;
		case opt_show_all:
			{
				(*ppOptSt)->bShowAll = SG_TRUE;
			}
			break;
		case 'a':
			{
				// if not alloc'ed, alloc
				// add assoc to psa_assocs/increment counter at the end
				if (!(*ppOptSt)->psa_assocs)
				{
					SG_ERR_CHECK( SG_stringarray__alloc(pCtx, &(*ppOptSt)->psa_assocs, (*ppOptSt)->count_options) );
				}
				SG_ERR_CHECK(  SG_stringarray__add(pCtx, (*ppOptSt)->psa_assocs, pszOptArg)  );
			}
			break;
		case opt_cert:
			{
				SG_strdup(pCtx, pszOptArg, &(*ppOptSt)->psz_cert);
			}
			break;
		case opt_changeset:
			{
				SG_strdup(pCtx, pszOptArg, &(*ppOptSt)->psz_changeset);
			}
			break;
		case 'C':
			{
				(*ppOptSt)->bClean = SG_TRUE;
			}
			break;
		case opt_comment:
			{
				SG_strdup(pCtx, pszOptArg, &(*ppOptSt)->psz_comment);
			}
			break;
		case opt_verbose:
			{
				(*ppOptSt)->bVerbose = SG_TRUE;
			}
			break;
		case 'F':
			{
				(*ppOptSt)->bForce = SG_TRUE;
			}
			break;
		case opt_global:
			{
				(*ppOptSt)->bGlobal = SG_TRUE;
			}
			break;
		case 'X':
			{
				// if not alloc'ed, alloc
				// add exclude to psa_excludes/increment counter at the end
				if (!(*ppOptSt)->psa_excludes)
				{
					SG_ERR_CHECK( SG_stringarray__alloc(pCtx, &(*ppOptSt)->psa_excludes, (*ppOptSt)->count_options) );
				}
				SG_ERR_CHECK(  SG_stringarray__add(pCtx, (*ppOptSt)->psa_excludes, pszOptArg)  );
			}
			break;
		case opt_exclude_from:
			{
				// read excludes from file to SG_stringarray
				// increment counter at the end
				// from file will alloc if needed
				SG_ERR_CHECK( SG_file_spec__read_patterns_from_file(pCtx, pszOptArg, &(*ppOptSt)->psa_excludes) );
			}
			break;
		case opt_ignore_portability_warnings:
			{
				(*ppOptSt)->bIgnoreWarnings = SG_TRUE;
			}
			break;
		case 'I':
			{
				// if not alloc'ed, alloc
				// add include to psa_includes, increment counter at the end
				if (!(*ppOptSt)->psa_includes)
				{
					SG_ERR_CHECK( SG_stringarray__alloc(pCtx, &(*ppOptSt)->psa_includes, (*ppOptSt)->count_options) );
				}
				SG_ERR_CHECK(  SG_stringarray__add(pCtx, (*ppOptSt)->psa_includes, pszOptArg)  );
			}
			break;
		case opt_include_from:
			{
				// read includes from file to SG_stringarray
				// increment counter at the end
				// from file will alloc if needed
				SG_ERR_CHECK( SG_file_spec__read_patterns_from_file(pCtx, pszOptArg, &(*ppOptSt)->psa_includes) );
			}
			break;
		case 'm':
			{
				SG_strdup(pCtx, pszOptArg, &(*ppOptSt)->psz_message);
				(*ppOptSt)->bPromptForDescription = SG_FALSE;
			}
			break;
		case 'N':
			{
				(*ppOptSt)->bRecursive = SG_FALSE;
			}
			break;
		case opt_no_plain:
			{
				(*ppOptSt)->bNoPlain = SG_TRUE;
			}
			break;
		case 'P':
			{
				SG_strdup(pCtx, pszOptArg, &(*ppOptSt)->psz_password);
			}
			break;
		case opt_port:
			{
				SG_ERR_CHECK(  _parse_option_port(pCtx, "port", pszOptArg, &(*ppOptSt)->iPort)  );
			}
			break;
		case opt_add:
			{
				(*ppOptSt)->bAdd = SG_TRUE;
			}
			break;
		case opt_subgroups:
			{
				(*ppOptSt)->bSubgroups = SG_TRUE;
			}
			break;
		case opt_remove:
			{
				(*ppOptSt)->bRemove = SG_TRUE;
			}
			break;
		case opt_repo:
			{
				SG_strdup(pCtx, pszOptArg, &(*ppOptSt)->psz_repo);
			}
			break;
		case 'r':
			{
				SG_rev_tag_obj* pRT = NULL;

				if (!(*ppOptSt)->pvec_rev_tags)
				{
					SG_ERR_CHECK(  SG_VECTOR__ALLOC(pCtx, &(*ppOptSt)->pvec_rev_tags, (*ppOptSt)->count_options)  );
				}
				SG_ERR_CHECK(  SG_alloc1(pCtx, pRT)  );
				pRT->bRev = SG_TRUE;
				SG_ERR_CHECK(  SG_strdup(pCtx, pszOptArg, &(pRT->pszRevTag))  );
				SG_ERR_CHECK(  SG_vector__append(pCtx, (*ppOptSt)->pvec_rev_tags, (void*)pRT, NULL)  );
				(*ppOptSt)->iCountRevs++;
			}
			break;
		case opt_sport:
			{
				SG_ERR_CHECK(  _parse_option_port(pCtx, "sport", pszOptArg, &(*ppOptSt)->iSPort)  );
			}
			break;
		case opt_stamp:
			{
				// if not alloc'ed, alloc
				// add stamp to psa_stamps
				if (!(*ppOptSt)->psa_stamps)
				{
					SG_ERR_CHECK( SG_stringarray__alloc(pCtx, &(*ppOptSt)->psa_stamps, (*ppOptSt)->count_options) );
				}
				SG_ERR_CHECK(  SG_stringarray__add(pCtx, (*ppOptSt)->psa_stamps, pszOptArg)  );
			}
			break;
		case opt_tag:
			{
				SG_rev_tag_obj* pRT = NULL;

				if (!(*ppOptSt)->pvec_rev_tags)
				{
					SG_ERR_CHECK(  SG_VECTOR__ALLOC(pCtx, &(*ppOptSt)->pvec_rev_tags, (*ppOptSt)->count_options)  );
				}
				SG_ERR_CHECK(  SG_alloc1(pCtx, pRT)  );
				pRT->bRev = SG_FALSE;
				SG_ERR_CHECK(  SG_strdup(pCtx, pszOptArg, &(pRT->pszRevTag))  );
				SG_ERR_CHECK(  SG_vector__append(pCtx, (*ppOptSt)->pvec_rev_tags, (void*)pRT, NULL)  );
				(*ppOptSt)->iCountTags++;
			}
			break;
		case 'T':
			{
				(*ppOptSt)->bTest = SG_TRUE;
			}
			break;
		case 'U':
			{
				SG_strdup(pCtx, pszOptArg, &(*ppOptSt)->psz_username);
			}
			break;
		case 'u':
			{
				// not sure the best way to do this
				(*ppOptSt)->iUnified = (SG_int32) pszOptArg;
			}
		case opt_to:
			{
				SG_strdup(pCtx, pszOptArg, &(*ppOptSt)->psz_to_date);
			}
			break;
		case opt_from:
			{
				SG_strdup(pCtx, pszOptArg, &(*ppOptSt)->psz_from_date);
			}
			break;
		case opt_leaves:
			{
				(*ppOptSt)->bLeaves = SG_TRUE;
			}
			break;
		case opt_max:
			{
				SG_uint32 parsed = SG_UINT32_MAX;
				// not sure the best way to do this
				SG_ERR_CHECK(SG_uint32__parse(pCtx, &parsed, pszOptArg));
				(*ppOptSt)->iMaxHistoryResults = parsed;
			}
			break;

		case opt_list:
			{
				(*ppOptSt)->bList = SG_TRUE;
			}
			break;

		case opt_list_all:
			{
				(*ppOptSt)->bListAll = SG_TRUE;
			}
			break;

		case opt_mark:
			{
				(*ppOptSt)->bMark = SG_TRUE;
				SG_strdup(pCtx, pszOptArg, &(*ppOptSt)->psz_mark);
			}
			break;

		case opt_mark_all:
			{
				(*ppOptSt)->bMarkAll = SG_TRUE;
			}
			break;

		case opt_unmark:
			{
				(*ppOptSt)->bUnmark = SG_TRUE;
				SG_strdup(pCtx, pszOptArg, &(*ppOptSt)->psz_unmark);
			}
			break;

		case opt_unmark_all:
			{
				(*ppOptSt)->bUnmarkAll = SG_TRUE;
			}
			break;

		default:
			break;
		}

	}

    if((*ppOptSt)->psa_assocs!=NULL)
        SG_ERR_CHECK(  SG_stringarray__sz_array_and_count(pCtx, (*ppOptSt)->psa_assocs, &(*ppOptSt)->ppAssocs, &(*ppOptSt)->iCountAssocs)  );
    if((*ppOptSt)->psa_includes!=NULL)
        SG_ERR_CHECK(  SG_stringarray__sz_array_and_count(pCtx, (*ppOptSt)->psa_includes, &(*ppOptSt)->ppIncludes, &(*ppOptSt)->iCountIncludes)  );
    if((*ppOptSt)->psa_excludes!=NULL)
        SG_ERR_CHECK(  SG_stringarray__sz_array_and_count(pCtx, (*ppOptSt)->psa_excludes, &(*ppOptSt)->ppExcludes, &(*ppOptSt)->iCountExcludes)  );

	return;
fail:
	;
}

//////////////////////////////////////////////////////////////////

/**
 * Dispatch to appropriate command using the given SG_getopt.
 *
 */
static void _dispatch(SG_context * pCtx, SG_getopt * pGetopt, SG_option_state* pOptSt, SG_bool initialUsageError)
{
	SG_uint32 i;
	const char *szCmd = SG_getopt__get_command_name(pGetopt);
	const char *szAppName = SG_getopt__get_app_name(pGetopt);
	const char *szExtraHelp = "";

	for (i=0; i<VERBCOUNT; i++)
	{
		if (
			(0 == strcmp(szCmd, commands[i].name))
			|| (commands[i].alias1 && (0 == strcmp(szCmd, commands[i].alias1)))
			|| (commands[i].alias2 && (0 == strcmp(szCmd, commands[i].alias2)))
			|| (commands[i].alias3 && (0 == strcmp(szCmd, commands[i].alias3)))
			)
		{
			SG_bool	bUsageError = initialUsageError;
			szExtraHelp = commands[i].extraHelp;

			SG_ERR_IGNORE(  SG_log_debug(pCtx,
										 "Performing '%s' command.",
										 szCmd)  );

			// TODO consider just passing pArgv to the command functions.

			if (! bUsageError)
			{
				SG_ERR_CHECK(  _inspect_all_options(pCtx, szAppName, szCmd, pOptSt, &bUsageError)  );

				SG_ERR_CHECK(  commands[i].fn(	
									pCtx,
									szAppName,
									szCmd,
									pGetopt,
									pOptSt,
									&bUsageError)  
							);
			}

			if (bUsageError)
			{
				SG_ERR_THROW(SG_ERR_USAGE);
			}

			return;
		}
	}

	do_cmd_help__list_all_commands(pCtx, szAppName, SG_FALSE);

	SG_ERR_THROW_RETURN(  SG_ERR_USAGE  );

fail:
	if (SG_context__err_equals(pCtx, SG_ERR_USAGE))
	{
		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "Usage: %s %s %s\n\n", szAppName, szCmd, szExtraHelp)  );
		SG_ERR_IGNORE(  do_cmd_help__list_command_help(pCtx, szCmd, SG_FALSE)  );
//		SG_ERR_IGNORE(  _do_cmd_
	}
}

//////////////////////////////////////////////////////////////////

/**
 * Print error message on STDERR for the given error and convert the
 * SG_ERR_ value into a proper ExitStatus for the platform.
 *
 * TODO investigate if (-1,errno) is appropriate for all 3 platforms.
 *
 * DO NOT WRAP THE CALL TO THIS FUNCTION WITH SG_ERR_IGNORE() (because we
 * need to be able to access the original error/status).
 */
static int _compute_exit_status_and_print_error_message(SG_context * pCtx, SG_getopt * pGetopt)
{
	SG_error err;
	char bufErrorMessage[SG_ERROR_BUFFER_SIZE+1];
	int exitStatus;

	SG_context__get_err(pCtx,&err);

	if (err == SG_ERR_OK)
	{
		SG_ERR_IGNORE(  SG_log_debug(pCtx,
									 "Command '%s' completed normally.\n\n",
									 SG_getopt__get_command_name(pGetopt))  );

		exitStatus = 0;
	}
	else if (err == SG_ERR_USAGE)
	{
#if 1
		// TODO 2010/05/13 We need to define a set of exit codes
		// TODO            for all of the commands in vv.
		// TODO            Something like:
		// TODO            0 : ok
		// TODO            1 : fail
		// TODO            2 : usage or other error
		// TODO
		// TODO            Like how /bin/diff returns 0,1,2.
#endif
		// We special case SG_ERR_USAGE because the code that signalled that
		// error has already printed whatever context-specific usage info.

		exitStatus = -2;  //Don't return -1, as exec() assumes that -1 means the exe could not be found.
	}
	else
	{
		SG_string * pStrContextDump = NULL;

		SG_error__get_message(err, bufErrorMessage, SG_ERROR_BUFFER_SIZE);

		// try to write full stacktrace and everything from the context to the log.
		// if we can't get that, just write the short form.
		//
		// TODO consider having the log_debug level do the full dump and the
		// TODO terser levels just do the summary.

		SG_context__err_to_string(pCtx,&pStrContextDump);

		SG_ERR_IGNORE(  SG_log_debug(pCtx,
									 "Command '%s' completed with error:\n%s\n\n",
									 SG_getopt__get_command_name(pGetopt),
									 (  (pStrContextDump)
										? SG_string__sz(pStrContextDump)
										: bufErrorMessage))  );

		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "%s: %s\n",
								   SG_getopt__get_app_name(pGetopt),
								   (  (pStrContextDump)
										? SG_string__sz(pStrContextDump)
										: bufErrorMessage))  );

		SG_STRING_NULLFREE(pCtx, pStrContextDump);

		// the shell and command tools in general only expect ERRNO-type errors.

		if (SG_ERR_TYPE(err) == __SG_ERR__ERRNO__)
			exitStatus = SG_ERR_ERRNO_VALUE(err);
		else
			exitStatus = 1;  // this means we threw an error, there's no unix error code that will
							 // cover all the possible errors that could come out of vv, so this 
							 // one will have to do (EPERM)
	}

	return exitStatus;
}

//////////////////////////////////////////////////////////////////

#include "sg_logfile_typedefs.h"
#include "sg_logfile_prototypes.h"

SG_logger_callback LogMessage_cb;

void LogMessage_cb(SG_context * pCtx, const char * message, SG_log_message_level message_level, SG_int64 timestamp, void * pContext)
{
	SG_time time = SG_TIME_ZERO_INITIALIZER;

	const char * szPrefix = NULL;
	SG_string * pDate = NULL;
	SG_string * pTime = NULL;
	SG_string * pLine = NULL;

	SG_logfile ** ppLogFile = (SG_logfile**)pContext;
	SG_logfile * pLogFile = NULL;
	if( ppLogFile!=NULL )
		pLogFile = *ppLogFile;

	if(message_level==SG_LOG_MESSAGE_LEVEL_URGENT)
		szPrefix = "***";
	else if(message_level==SG_LOG_MESSAGE_LEVEL_NORMAL)
		szPrefix = " **";
	else
		szPrefix = "  *";

	SG_ERR_CHECK(  SG_time__decode__local(pCtx, timestamp, &time)  );

	SG_ERR_CHECK(  SG_string__alloc__format(pCtx, &pDate, "%d/%d", time.month , time.mday )  );
	SG_ERR_CHECK(  SG_string__alloc__format(pCtx, &pTime, "%d:%02d:%02d.%03d", time.hour, time.min, time.sec, time.msec )  );

	SG_ERR_CHECK(  SG_string__alloc__format(pCtx, &pLine, "%s %s %s %s\n", szPrefix, SG_string__sz(pDate), SG_string__sz(pTime), message)  );

	if (pLogFile != NULL)
		SG_ERR_CHECK(  SG_logfile__write__time(pCtx, pLogFile, SG_string__sz(pLine), time)  );
	else
		SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDERR,SG_string__sz(pLine))  );

	SG_STRING_NULLFREE(pCtx, pDate);
	SG_STRING_NULLFREE(pCtx, pTime);
	SG_STRING_NULLFREE(pCtx, pLine);
	return;

fail:
	if( pLogFile != NULL )
	{
		SG_string * pErrorString = NULL;
		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,"* * * An error occurred writing to the log file. Writing to STDERR instead. * * *\n")  );
		if( SG_IS_OK(SG_context__err_to_string(pCtx,&pErrorString)) && pErrorString!=NULL )
		{
			SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, SG_string__sz(pErrorString))  );
			SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,"\n* * * * *\n\n")  );
		}
		SG_ERR_IGNORE(  SG_logfile__nullfree(pCtx, ppLogFile)  );
		SG_ERR_IGNORE(  LogMessage_cb(pCtx, message, message_level, timestamp, pContext)  );
	}
	SG_STRING_NULLFREE(pCtx, pDate);
	SG_STRING_NULLFREE(pCtx, pTime);
	SG_STRING_NULLFREE(pCtx, pLine);
	return;
}

static void _emit_msg_to_stdout(SG_context* pCtx, const char* pszMsg)
{
	SG_ERR_CHECK_RETURN(  SG_console(pCtx, SG_CS_STDOUT, pszMsg)  );
	SG_ERR_CHECK_RETURN(  SG_console__flush(pCtx, SG_CS_STDOUT)  );
}

//////////////////////////////////////////////////////////////////

static int _my_main(SG_context * pCtx, SG_getopt * pGetopt, SG_option_state* pOptSt, SG_bool bUsageError)
{
	int exitStatus;

	SG_logfile * pLogFile = NULL;
    char* psz_log_level = NULL;
    char* psz_log_path = NULL;
    SG_pathname* pPath = NULL;
	SG_uint32 logLevel = SG_LOGGER_OUTPUT_LEVEL_NORMAL;

    SG_ERR_CHECK(  SG_localsettings__get__sz(pCtx, SG_LOCALSETTING__LOG_PATH, NULL, &psz_log_path, NULL)  );
    if (psz_log_path)
    {
        SG_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx, &pPath, psz_log_path)  );
    }
    else
    {
        SG_ERR_CHECK(  SG_PATHNAME__ALLOC__USER_TEMP_DIRECTORY(pCtx, &pPath)  );
    }
	SG_ERR_CHECK(  SG_logfile__alloc(pCtx, &pPath, &pLogFile)  );
    SG_ERR_CHECK(  SG_localsettings__get__sz(pCtx, SG_LOCALSETTING__LOG_LEVEL, NULL, &psz_log_level, NULL)  );
    if (psz_log_level)
    {
        if( SG_stricmp(psz_log_level, "quiet")==0 )
        {
            logLevel = SG_LOGGER_OUTPUT_LEVEL_QUIET;
        }
        if( SG_stricmp(psz_log_level, "normal")==0 )
        {
            logLevel = SG_LOGGER_OUTPUT_LEVEL_NORMAL;
        }
        if( SG_stricmp(psz_log_level, "verbose")==0 )
        {
            logLevel = SG_LOGGER_OUTPUT_LEVEL_VERBOSE;
        }
    }

	SG_ERR_CHECK(  SG_context__msg__add_listener(pCtx, _emit_msg_to_stdout)  );

	SG_ERR_CHECK(  SG_logging__register_logger(pCtx, LogMessage_cb, &pLogFile, logLevel)  );

	SG_ERR_IGNORE(  SG_log_debug(pCtx,
								 "\\\n---- " WHATSMYNAME " logging at output level verbose. ----")  );

	SG_ERR_CHECK(  _dispatch(pCtx,pGetopt,pOptSt, bUsageError)  );

fail:
    SG_NULLFREE(pCtx, psz_log_level);
    SG_NULLFREE(pCtx, psz_log_path);
	exitStatus = _compute_exit_status_and_print_error_message(pCtx, pGetopt);		// DO NOT WRAP THIS WITH SG_ERR_IGNORE

	SG_ERR_IGNORE(  SG_context__msg__rm_listener(pCtx, _emit_msg_to_stdout)  );

	SG_ERR_IGNORE(  SG_logging__unregister_logger(pCtx, LogMessage_cb, &pLogFile)  );
	SG_ERR_IGNORE(  SG_logfile__nullfree(pCtx, &pLogFile)  );

	return exitStatus;
}

static void _sg_rev_tag_obj_nullfree(SG_context* pCtx, SG_rev_tag_obj* pThis)
{
	if (!pThis)
		return;

	SG_NULLFREE(pCtx, pThis->pszRevTag);
	SG_NULLFREE(pCtx, pThis);
}

static void _sg_option_state__free(SG_context* pCtx, SG_option_state* pThis)
{
	if (!pThis)
		return;

	SG_NULLFREE(pCtx, pThis->psz_repo);
	SG_NULLFREE(pCtx, pThis->psz_cert);
	SG_NULLFREE(pCtx, pThis->psz_changeset);
	SG_NULLFREE(pCtx, pThis->psz_comment);
	SG_NULLFREE(pCtx, pThis->psz_message);
	SG_NULLFREE(pCtx, pThis->psz_password);
	SG_NULLFREE(pCtx, pThis->psz_username);
	SG_NULLFREE(pCtx, pThis->psz_from_date);
	SG_NULLFREE(pCtx, pThis->psz_to_date);
	SG_NULLFREE(pCtx, pThis->psz_mark);
	SG_NULLFREE(pCtx, pThis->psz_unmark);

	SG_STRINGARRAY_NULLFREE(pCtx, pThis->psa_includes);
	SG_STRINGARRAY_NULLFREE(pCtx, pThis->psa_excludes);
	SG_STRINGARRAY_NULLFREE(pCtx, pThis->psa_stamps);

	SG_NULLFREE(pCtx, pThis->paRecvdOpts);
	SG_vector__free__with_assoc(pCtx, pThis->pvec_rev_tags, (SG_free_callback*) _sg_rev_tag_obj_nullfree);

    SG_STRINGARRAY_NULLFREE(pCtx, pThis->psa_assocs);

	SG_NULLFREE(pCtx, pThis);
}

//////////////////////////////////////////////////////////////////

#if defined(WINDOWS)
int wmain(int argc, wchar_t ** argv)
#else
int main(int argc, char ** argv)
#endif
{
	SG_context * pCtx = NULL;
	SG_getopt * pGetopt = NULL;
	SG_option_state * pOptSt = NULL;
	int exitStatus = 0;
	SG_bool bUsageError = SG_FALSE;

	SG_context__alloc(&pCtx);
	SG_ERR_CHECK(  SG_lib__global_initialize(pCtx)  );

	SG_ERR_CHECK(  _intern_argv(pCtx, argc, argv, &pGetopt)  );

	_parse_options(pCtx, &pGetopt, &pOptSt);

	if (SG_context__has_err(pCtx))
	{
		if (SG_context__err_equals(pCtx, SG_ERR_GETOPT_BAD_ARG))
		{
			bUsageError = SG_TRUE;
			SG_context__err_reset(pCtx);
		}
		else
			SG_ERR_RETHROW;
	}

	SG_ERR_CHECK(  exitStatus = _my_main(pCtx, pGetopt, pOptSt, bUsageError)  );

fail:
	SG_GETOPT_NULLFREE(pCtx, pGetopt);
	SG_ERR_IGNORE(  _sg_option_state__free(pCtx, pOptSt)  );

	SG_ERR_IGNORE(  SG_lib__global_cleanup(pCtx)  );
	SG_CONTEXT_NULLFREE(pCtx);

	return exitStatus;
}

