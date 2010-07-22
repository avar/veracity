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
 * @file sg_portability_typedefs.h
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_PORTABILITY_TYPEDEFS_H
#define H_SG_PORTABILITY_TYPEDEFS_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

/**
 * Working space and individual results for evaluating the portability
 * of an individual file/directory.  This can be used as assoc-data during
 * the filename/directory collision tests.
 */
typedef struct _sg_portability_item SG_portability_item;

/**
 * A container for SG_portability_items to allow us to detect
 * potential filename collisions due to the different ways that
 * different platforms mangle filenames, such as case folding.
 */
typedef struct _sg_portability_dir SG_portability_dir;

//////////////////////////////////////////////////////////////////

/**
 * A bitmask of file/directory name portability issues (aka quirks).
 *
 * Each bit represents a particular quirk/issue that we currently
 * know about.  We allow the user to configure which issues they
 * care about and which ones they don't care about.
 *
 * When a file or directory is added to version control (and when
 * files/directories added in different repo instances are merged),
 * we optionally scan the pathname/entryname and can warn them if
 * there are any portability problems.
 *
 * There are several classes of problems:
 * [1] Problems within an individual name.  Bogus characters or length.
 *     For example, the Win32-layer on Windows doesn't allow control
 *     characters in filenames.  (The NTFS-layer might, but the
 *     Win32-layer doesn't.)
 * [2] Name collisions.
 *     2 or more distinct entrynames that will be effectively mapped
 *     to the same name by at least one OS or filesystem.
 *     For example, "foo" and "FOO" or the various NFC/NFD names.
 * [3] OS- and filesystem-specific features.
 *     For example, a symlink.
 */
typedef SG_uint64 SG_portability_flags;		// a bitmask

/**
 * No portability testing.
 */
#define SG_PORT_FLAGS__NONE				((SG_portability_flags)0x0000000000000000ULL)
#define SG_PORT_FLAGS__ALL				((SG_portability_flags)0xffffffffffffffffULL)

#define SG_PORT_FLAGS__INDIV__FIRST		((SG_portability_flags)0x0000000000000001ULL)
#define SG_PORT_FLAGS__MASK_INDIV		((SG_portability_flags)0x000000000000ffffULL)

#define SG_PORT_FLAGS__COLLISION__FIRST	((SG_portability_flags)0x0000000000010000ULL)
#define SG_PORT_FLAGS__MASK_COLLISION	((SG_portability_flags)0x00000000ffff0000ULL)

#define SG_PORT_FLAGS__FSOBJ__FIRST		((SG_portability_flags)0x0000000100000000ULL)
#define SG_PORT_FLAGS__MASK_FSOBJ		((SG_portability_flags)0x0000ffff00000000ULL)

//////////////////////////////////////////////////////////////////
// Bits for INDIVIDUAL ENTRYNAMES in isolation.  These identify portability problems
// within the entryname itself, such as potentially bogus characters.
//////////////////////////////////////////////////////////////////

/**
 * Invalid characters in [0x00 .. 0x7f] in entrynames.
 */
#define SG_PORT_FLAGS__INDIV__INVALID_00_7F			(SG_PORT_FLAGS__INDIV__FIRST << 0)

/**
 * Restricted (but not invalid) characters in [0x00 .. 0x7f] in entrynames.
 */
#define SG_PORT_FLAGS__INDIV__RESTRICTED_00_7F		(SG_PORT_FLAGS__INDIV__FIRST << 1)

/**
 * Entryname length limit ( <= 255 bytes ).
 */
#define SG_PORT_FLAGS__INDIV__ENTRYNAME_LENGTH		(SG_PORT_FLAGS__INDIV__FIRST << 2)

/**
 * Relative pathname length limit ( <= 256 code points/characters ).
 */
#define SG_PORT_FLAGS__INDIV__PATHNAME_LENGTH		(SG_PORT_FLAGS__INDIV__FIRST << 3)

/**
 * Final dot/space in entryname.
 */
#define SG_PORT_FLAGS__INDIV__FINAL_DOT_SPACE		(SG_PORT_FLAGS__INDIV__FIRST << 4)

/**
 * Reserved MSDOS device names.
 */
#define SG_PORT_FLAGS__INDIV__MSDOS_DEVICE			(SG_PORT_FLAGS__INDIV__FIRST << 5)

/**
 * Non-Basic Multilingual Plane ( > U+FFFF ) characters in entrynames.
 */
#define SG_PORT_FLAGS__INDIV__NONBMP				(SG_PORT_FLAGS__INDIV__FIRST << 6)

/**
 * Unicode Ignorable characters in entryname.
 */
#define SG_PORT_FLAGS__INDIV__IGNORABLE				(SG_PORT_FLAGS__INDIV__FIRST << 7)

/**
 * Entrynames might have problems on CHARSET-based (rather than UTF-8-based) systems.
 */
#define SG_PORT_FLAGS__INDIV__CHARSET				(SG_PORT_FLAGS__INDIV__FIRST << 8)

/**
 * Entrynames that appear to be a DOS shortname.
 */
#define SG_PORT_FLAGS__INDIV__SHORTNAMES			(SG_PORT_FLAGS__INDIV__FIRST << 9)

/**
 * "Services for Macintosh" rename that subsitutes u+f001..u+f029
 * for the set of NTFS-invalid chars [0x00..0x1f, "<", ">", ...] and for
 * final_{space,dot} characters.
 *
 * This flag is somewhat related to SG_PORT_FLAGS__INDIV__INVALID_00_7F and to
 * SG_PORT_FLAGS__INDIV__FINAL_DOT_SPACE.
 */
#define SG_PORT_FLAGS__INDIV__SFM_NTFS				(SG_PORT_FLAGS__INDIV__FIRST << 10)

/**
 * Non-canonical decompositions.
 */
#define SG_PORT_FLAGS__INDIV__NON_CANONICAL_NFD		(SG_PORT_FLAGS__INDIV__FIRST << 11)

/**
 * One special filename that is considered wrong, for debugging purposes
 */
#define SG_PORT_FLAGS__INDIV__DEBUG_BAD_NAME		(SG_PORT_FLAGS__INDIV__FIRST << 12)
#define SG_PORT_DEBUG_BAD_NAME "__a849ac99ab884f598cce2b5f6c0c90078c76bc14e71511dd9504001ec206ff65__bad__"

/**
 * Leading '-' in a filename can cause problems for users.  That is, if I create the file "-rf" on
 * a Windows machine and then a user on Linux does an "rm *" in their working directory, the "*"
 * globbing will grab the "-rf" filename and the rm command will see it as an argument rather than
 * a filename....
 */
#define SG_PORT_FLAGS__INDIV__LEADING_DASH			(SG_PORT_FLAGS__INDIV__FIRST << 13)

// TODO anything else?

//////////////////////////////////////////////////////////////////
// Bits for ENTRYNAME COLLISIONS.  These identify portability problems
// by pair-wise comparing items within a directory and looking for items
// that may collide on various filesystems.  For example, trying to have
// "foo" and "FOO" in the same directory works on some platforms/filesystems
// but does not on others.
//
// NOTE: An exact-match collision of 2 entrynames gets reported immediately
// NOTE: via the bIsDuplicate argument and the second instance does not
// NOTE: even get added to the collider.  We DO NOT update the first instance.
// NOTE: That is, there is NO __EXACT_MATCH bit in the flags (because they
// NOTE: are (probably) hard errors and (probably) not an ignorable warning).
//////////////////////////////////////////////////////////////////

/**
 * Original file/directory as read from disk.  (When set it means that the original
 * unmangled name collided with one of the mangled names of another entry.)
 */
#define SG_PORT_FLAGS__COLLISION__ORIGINAL					(SG_PORT_FLAGS__COLLISION__FIRST << 0)

/**
 * Case folded the way Windows/NTFS/MSDOS and/or Apple/HFS* does it.
 */
#define SG_PORT_FLAGS__COLLISION__CASE						(SG_PORT_FLAGS__COLLISION__FIRST << 1)

/**
 * Final spaces/dots stripped as on Windows.
 */
#define SG_PORT_FLAGS__COLLISION__FINAL_DOT_SPACE			(SG_PORT_FLAGS__COLLISION__FIRST << 2)

/**
 * VMHGFS generated "%25" collapsed to "%".
 */
#define SG_PORT_FLAGS__COLLISION__PERCENT25					(SG_PORT_FLAGS__COLLISION__FIRST << 3)

/**
 * Test for potential collisions between the CHARSET-based mapping of various UTF-8 names.
 * That is, if 2 different UTF-8 strings get mapped to the same CHARSET-based string.
 */
#define SG_PORT_FLAGS__COLLISION__CHARSET					(SG_PORT_FLAGS__COLLISION__FIRST << 4)

/**
 * Mac SFM characters.
 */
#define SG_PORT_FLAGS__COLLISION__SFM_NTFS					(SG_PORT_FLAGS__COLLISION__FIRST << 5)

/**
 * Test for potential collisions when non-bmp characters are replaced.
 */
#define SG_PORT_FLAGS__COLLISION__NONBMP					(SG_PORT_FLAGS__COLLISION__FIRST << 6)

/**
 * Test for multiple items in a directory that only vary by differences in
 * any Unicode Ignorable characters or that would collide with a file when
 * the Ignorables are stripped out.
 */
#define SG_PORT_FLAGS__COLLISION__IGNORABLE					(SG_PORT_FLAGS__COLLISION__FIRST << 7)

/**
 * Collapse all NFC/NFD/non-canonical-NFD to NFC.
 */
#define SG_PORT_FLAGS__COLLISION__NFC						(SG_PORT_FLAGS__COLLISION__FIRST << 8)

/**
 * We define that all entrynames that begin with DEBUG_BAD_NAME collide
 * on some other mythical platform.
 */
#define SG_PORT_FLAGS__COLLISION__DEBUG_BAD_NAME			(SG_PORT_FLAGS__COLLISION__FIRST << 9)

#if 0
/**
 * Do we want to have (if we can figure out how) to have a collider that
 * takes any explicit shortname-format entrynames that they created (on
 * Linux, for example) and predicts if they would collide with an automatically
 * generated shortname on another entryname in the directory?  That is, if the
 * Linux user explicitly created "progra~1" and "PROGRAM FILES" in the same
 * directory, and then created a working directory on Windows.  Since the shortname
 * that Windows will create will depend upon the order that we populate the working
 * directory, it may or may not collide.
 *
 * Currently, we only raise __INDIV__SHORTNAMES when we see one and don't try to
 * check for collisions.  I'm wondering if this is sufficient.
 *
 * TODO decide this.
 */
#define SG_PORT_FLAGS__COLLISION__SHORTNAMES				(SG_PORT_FLAGS__COLLISION__FIRST << x)
#endif



// TODO anything else?

//////////////////////////////////////////////////////////////////
// Bits for OS- and Filesystem-specific features.
//////////////////////////////////////////////////////////////////

#if 0
/**
 * TODO does any one use this yet?
 *
 * Test for symlinks.
 */
#define SG_PORT_FLAGS__FSOBJ__SYMLINKS				(SG_PORT_FLAGS__FSOBJ__FIRST << 0)

// TODO anything else?
#endif

//////////////////////////////////////////////////////////////////

/**
 * Callback for SG_portability_dir__foreach_with_issues.
 *
 * pszEntryName is set to a copy of the original string provided to __add_item().
 * You DO NOT own this.
 *
 * see __get_item_result_flags() for the meanings of flagsWarnings and flagsLogged.
 *
 * pVoidAssocData contains any associated data for this item that was given
 * when __add_item__with_assoc() was called.
 */
typedef void (SG_portability_dir_foreach_callback)(SG_context * pCtx,
												   const char * pszEntryName,
												   SG_portability_flags flagsWarnings,
												   SG_portability_flags flagsLogged,
												   void * pVoidAssocData,
												   void * pCtxVoid);

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_PORTABILITY_TYPEDEFS_H
