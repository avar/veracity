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
 * @fsobj sg_fsobj_typedefs.h
 *
 */

#ifndef H_SG_FSOBJ_TYPEDEFS_H
#define H_SG_FSOBJ_TYPEDEFS_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////
// permission for creating new file (see mode_t in open(2)) and for chmod.
// when creating a file, these are subject to umask on Linux/Mac.
// Windows only recognizes the bits in 0600 - it'll ignore any other bits.
// (and you can't turn off read-access, so it's really just write-access
// that you can turn off.)  and windows lies about the execute bit.

typedef SG_uint32 SG_fsobj_perms;

// just use the [0000..0777] octal constants we've all come to know and love.

#define SG_FSOBJ_PERMS__MASK				((SG_fsobj_perms)0777)
#define SG_FSOBJ_PERMS__UNUSED				((SG_fsobj_perms)0777)	// placeholder when we're not creating files.

#if defined(WINDOWS)
#define SG_FSOBJ_PERMS__WINDOWS_WRITABLE				((SG_fsobj_perms)0200)
#define SG_FSOBJ_PERMS__WINDOWS_IS_WRITABLE(perms)		((perms & SG_FSOBJ_PERMS__WINDOWS_WRITABLE) == SG_FSOBJ_PERMS__WINDOWS_WRITABLE)
#define SG_FSOBJ_PERMS__WINDOWS_IS_READONLY(perms)		((perms & SG_FSOBJ_PERMS__WINDOWS_WRITABLE) != SG_FSOBJ_PERMS__WINDOWS_WRITABLE)
#endif

//////////////////////////////////////////////////////////////////

typedef SG_uint32 SG_fsobj_type;		// normalize S_IFMT part of st.st_mode

#define SG_FSOBJ_TYPE__UNSPECIFIED		((SG_fsobj_type)0x0000)
#define SG_FSOBJ_TYPE__REGULAR			((SG_fsobj_type)0x0001)
#define SG_FSOBJ_TYPE__SYMLINK			((SG_fsobj_type)0x0002)
#define SG_FSOBJ_TYPE__DIRECTORY		((SG_fsobj_type)0x0003)
#define SG_FSOBJ_TYPE__OTHER			((SG_fsobj_type)0x0004)

END_EXTERN_C;

#endif//H_SG_FSOBJ_TYPEDEFS_H

