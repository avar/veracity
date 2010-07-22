/**
 * @copyright
 * 
 * Copyright (C) 2003, 2010 SourceGear LLC. All rights reserved.
 * This file contains file-diffing-related source from subversion's
 * libsvn_delta, forked at version 0.20.0, which contained the
 * following copyright notice:
 *
 *      Copyright (c) 2000-2003 CollabNet.  All rights reserved.
 *      
 *      This software is licensed as described in the file COPYING, which
 *      you should have received as part of this distribution.  The terms
 *      are also available at http://subversion.tigris.org/license-1.html.
 *      If newer versions of this license are posted there, you may use a
 *      newer version instead, at your option.
 *      
 *      This software consists of voluntary contributions made by many
 *      individuals.  For exact contribution history, see the revision
 *      history and logs, available at http://subversion.tigris.org/.
 * 
 * @endcopyright
 * 
 * @file sg_diffcore_typedefs.h
 * 
 * @details 
 * 
 */

#ifndef H_SG_DIFFCORE_TYPEDEFS_H
#define H_SG_DIFFCORE_TYPEDEFS_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////


/// Opaque type diffs between datasources.
typedef struct _sg_diff_t SG_diff_t; // Formerly "svn_diff_t"


enum _SG_diff_datasource
{
    SG_DIFF_DATASOURCE__ORIGINAL = 0, // The oldest form of the data.
    SG_DIFF_DATASOURCE__MODIFIED = 1, // The same data, but potentially changed by the user.
    SG_DIFF_DATASOURCE__LATEST   = 2, // The latest version of the data, possibly different than the user's modified version.
    SG_DIFF_DATASOURCE__ANCESTOR = 3  // The common ancestor of original and modified.
};
typedef enum _SG_diff_datasource SG_diff_datasource; // Formerly "svn_diff_datasource_e"


enum _SG_diff_options
{
    SG_DIFF_OPTION__REGULAR                      = 0x00000000,
    SG_DIFF_OPTION__IGNORE_WHITESPACE            = 0x00000001,
    SG_DIFF_OPTION__IGNORE_CASE                  = 0x00000002,
//    SG_DIFF_OPTION__IGNORE_BLANK_LINES           = 0x00000004,

    SG_DIFF_OPTION__NATIVE_EOL                   = 0x00000000,
    SG_DIFF_OPTION__ANY_EOL                      = 0x00000010,

    SG_DIFF_OPTION__NO_INTRALINE_HIGHLIGHT       = 0x00000000,
    SG_DIFF_OPTION__INTRALINE_HIGHLIGHT          = 0x00001000,
};
typedef enum _SG_diff_options SG_diff_options; // Formerly "svn_diff_options_e" (but was SourceGear original code)


enum _SG_diff_stat_type
{
    SG_DIFF_STAT_TYPE__ADDITION = 0,
    SG_DIFF_STAT_TYPE__DELETION = 1,
    SG_DIFF_STAT_TYPE__CHANGE   = 2,
    SG_DIFF_STAT_TYPE__CONFLICT = 3,
    SG_DIFF_STAT_TYPES_COUNT
};
typedef enum _SG_diff_stat_type SG_diff_stat_type; // Formerly "svn_diff_stat_type"


/// A vtable for reading data from the three datasources.
struct _SG_diff_functions
{
    void (*datasource_open)(SG_context * pCtx, void * pDiffBaton, SG_diff_datasource datasource);

    void (*datasource_close)(SG_context * pCtx, void * pDiffBaton, SG_diff_datasource datasource);

    void (*datasource_get_next_token)(SG_context * pCtx, void * pDiffBaton, SG_diff_datasource datasource, void **ppToken);

    SG_int32 (*token_compare)(void * pDiffBaton, void * pToken1, void * pToken2);

    // This will only ever get call on the most recent token fetched by datasource_get_next_token (if the tokens were equal according to token_compare).
    void (*token_discard)(void * pDiffBaton, void * pToken);

    void (*token_discard_all)(void * pDiffBaton);
};
typedef struct _SG_diff_functions SG_diff_functions; // Formerly "svn_diff_fns_t"


typedef void (*SG_diff_output_function)(
    SG_context * pCtx,
    void * pOutputBaton,
    SG_int32 originalStart, SG_int32 originalLength,
    SG_int32 modifiedStart, SG_int32 modifiedLength,
    SG_int32 latestStart, SG_int32 latestLength);

typedef void (*SG_diff_output_function__conflict)(
    SG_context * pCtx,
    void * pOutputBaton,
    SG_int32 originalStart, SG_int32 originalLength,
    SG_int32 modifiedStart, SG_int32 modifiedLength,
    SG_int32 latestStart, SG_int32 latestLength,
    SG_diff_t * pResolvedDiff);

/// A vtable for displaying (or consuming) differences between datasources.
struct _SG_diff_output_functions
{
    SG_diff_output_function output_common;
    SG_diff_output_function output_diff_modified;
    SG_diff_output_function output_diff_latest;
    SG_diff_output_function output_diff_common;
    SG_diff_output_function__conflict output_conflict;
};
typedef struct _SG_diff_output_functions SG_diff_output_functions; // Formerly "svn_diff_output_fns_t"


/// Opaque doc handle for _sg_diffcore routines.
typedef struct _sg_diffcore_context SG_diffcore_context; // Formerly "svn_sgd_t" (but was SourceGear original code)


enum _SG_diffcore__doc_ndx
{
    SG_DIFFCORE__DOC_NDX_UNSPECIFIED = -1,
    SG_DIFFCORE__DOC_NDX_ORIGINAL    =  0,
    SG_DIFFCORE__DOC_NDX_MODIFIED    =  1,
    SG_DIFFCORE__DOC_NDX_LATEST      =  2
};
typedef enum _SG_diffcore__doc_ndx SG_diffcore__doc_ndx;


//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_DIFF_EXT_TYPEDEFS_H
