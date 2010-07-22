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
 * @file sg_tid_typedefs.h
 *
 * @details
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_TID_TYPEDEFS_H
#define H_SG_TID_TYPEDEFS_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

/**
 * A TID is a Temporary ID.  Similar to one of our GIDs and also
 * similar to a traditional UUID or GUID, it is a unique, generated,
 * random value (probably not a hash).
 *
 * TIDs are weaker than GIDs.  They are intended to be used when we
 * need a unique name/id for some short-term operation.  For example,
 * to name a temporary directory or file or a table column for the
 * duration of an operation.  We need them to be unique for a short
 * period of time on the current system -- rather than absolutely
 * unique for all time and all systems.
 *
 * We reserve the right to just create a GID and lop off some of the
 * trailing digits.
 *
 * There are times when our GIDs are just too long.  For example, if
 * we create a temp file nested inside a couple of temp directories,
 * we could easily have a 200 character pathname which can cause
 * problems if your $TEMP directory is deeply nested to begin with or
 * if you are building very long command lines and etc.
 * Or, for example, limits on the length of table and column names
 * in various SQL databases cause us problems if we try to use a GID
 * or HID as a table or column name.
 *
 * To distinguish a TID from a GID or HID, a TID starts with a 't'.
 *
 * We optionally allow the length of the random part to be specified
 * and/or for you to truncate a TID by stuffing a zero byte somewhere
 * inside the buffer; so we only define a MAXIMUM buffer size required
 * to hold a TID and we do not define an EXPECTED/ACTUAL length.
 * (And any verification routine can only check for the initial 't'
 * followed by some number of hex digits, but it cannot verify any
 * length constraint.)
 *
 * By default, a TID has the same length as a GID.
 */
#define SG_TID_LENGTH_PREFIX			(1)
#define SG_TID_LENGTH_RANDOM_MAX		(SG_GID_LENGTH_RANDOM)

#define SG_TID_MAX_BUFFER_LENGTH		(SG_TID_LENGTH_PREFIX + SG_TID_LENGTH_RANDOM_MAX + 1)

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_TID_TYPEDEFS_H
