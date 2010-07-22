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
 * @file sg_diffcore_prototypes.h
 * 
 * @details 
 * 
 */

#ifndef H_SG_DIFFCORE_PROTOTYPES_H
#define H_SG_DIFFCORE_PROTOTYPES_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////
// The Main Events
//////////////////////////////////////////////////////////////////

void SG_diff(SG_context * pCtx, const SG_diff_functions * pVtable, void * pDiffBaton, SG_diff_t ** ppDiff);
void SG_diff3(SG_context * pCtx, const SG_diff_functions * pVtable, void * pDiffBaton, SG_diff_t ** ppDiff);

void SG_diff__free(SG_context * pCtx, SG_diff_t * pDiff);


//////////////////////////////////////////////////////////////////
// Utility Functions
//////////////////////////////////////////////////////////////////

SG_bool SG_diff_contains_conflicts(SG_diff_t * pDiff);
SG_bool SG_diff_contains_diffs(SG_diff_t * pDiff);


//////////////////////////////////////////////////////////////////
// Displaying Diffs
//////////////////////////////////////////////////////////////////

void SG_diff_output(SG_context * pCtx, const SG_diff_output_functions * pVtable, void * pOutputBaton, SG_diff_t * pDiff);


//////////////////////////////////////////////////////////////////
// Diffs on files
//////////////////////////////////////////////////////////////////

void SG_diff_file(
    SG_context * pCtx,
    const SG_pathname * pPathnameOriginal, const SG_pathname * pPathnameModified,
    SG_diff_options options,
    SG_diff_t ** ppDiff);

void SG_diff3_file(
    SG_context * pCtx,
    const SG_pathname * pPathnameOriginal, const SG_pathname * pPathnameModified, const SG_pathname * pPathnameLatest,
    SG_diff_options options,
    SG_diff_t ** ppDiff);

void SG_diff_file_output_unified(
    SG_context * pCtx,
    SG_diff_t * pDiff,
    const SG_pathname * pPathnameOriginal, const SG_pathname * pPathnameModified,
    const char * szHeaderOriginal, const char * szHeaderModified,
    SG_diff_options options,
    int context,
    SG_file * pOutputFile);

void SG_diff_file_output_normal(
    SG_context * pCtx,
    SG_diff_t * pDiff,
    const SG_pathname * pPathnameOriginal, const SG_pathname * pPathnameModified,
    SG_diff_options options,
    SG_file * pOutputFile);

void SG_diff3_file_output(
    SG_context * pCtx,
    SG_diff_t * pDiff,
    const SG_pathname * pPathnameOriginal, const SG_pathname * pPathnameModified, const SG_pathname * pPathnameLatest,
    const char * szConflictOriginal, const char * szConflictModified, const char * szConflictLatest,
    const char * szConflictSeparator,
    SG_bool displayOriginalInConflict, SG_bool displayResolvedConflicts,
    SG_file *pOutputFile);

END_EXTERN_C;

#endif//H_SG_DIFFCORE_PROTOTYPES_H
