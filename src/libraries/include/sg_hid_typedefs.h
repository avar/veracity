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
 * @file sg_hid_typedefs.h
 *
 * @details
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_HID_TYPEDEFS_H
#define H_SG_HID_TYPEDEFS_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

/**
 * A HID is a Hash ID.  This value is the result of a hash computation
 * using an algorithm like SHA-256 or MD5.
 *
 * When a REPO is created we allow a Hash Algorithm to be chosen for that REPO.
 * THAT ALGORITHM AND ONLY THAT ALGORITHM WILL BE USED; that value cannot be
 * changed and it is inherited by all REPO instances.  The REPO internally
 * stores the chosen algorithm.
 *
 * THEREFORE, a HID DOES NOT have a COMPILE-TIME FIXED-LENGTH; the length
 * depends upon the algorithm.  We define a MAXIMUM HID length as a guideline
 * so that code can use stack buffers when convenient; this is an upperbound
 * and will waste some space.
 *
 * WE DO NOT include an algorithm id in the HID because ALL HIDs within a
 * REPO will have been computed using the same algorithm.  And since HIDs
 * are frequently seen by the user (a changeset is named by its HID), we
 * don't want to force them to type a 'h' prefix or have to worry about
 * stripping out such prefixes when we display a history or have to worry
 * about whether they should or should not appear in any JSON that we
 * hand up.
 *
 * WE ALWAYS store/fetch/parse/print/use a HID as a lowercase hex character
 * string of n digits; that is, we NEVER try to pass this around as binary
 * value.
 *
 * So, in memory a HID is a simple "const char *" consisting of the
 * lowercase hex digits.  We DO NOT wrap this in a compilcated typedef
 * because we use HIDs as keys in RBTREEs and VHASHes and in callbacks
 * and etc.  Tis better to just be honest here.
 *
 * If we assume at some point in the near future that we'll have 1024 bit
 * hashes, that gives us 256 hex digits (bytes).
 */
#define SG_HID_MAX_BUFFER_LENGTH		257

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_HID_TYPEDEFS_H
