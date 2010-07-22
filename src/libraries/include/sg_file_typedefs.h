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
 * @file sg_file_typedefs.h
 *
 */

#ifndef H_SG_FILE_TYPEDEFS_H
#define H_SG_FILE_TYPEDEFS_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////
// flags for SG_file_open():

typedef SG_uint32 SG_file_flags;

// we have 3 access modes:
// note: these bit values do not match <fcntl.h>
#define SG_FILE_RDONLY			((SG_file_flags)0x0001)
#define SG_FILE_WRONLY			((SG_file_flags)0x0002)
#define SG_FILE_RDWR			((SG_file_flags)0x0004)
#define SG_FILE__ACCESS_MASK	((SG_file_flags)0x0007)

// we have 3 open cases:
// [1] open existing file (fail it not present)
// [2] create a new file (fail if already present)
// [3] open existing or create new file
#define SG_FILE_OPEN_EXISTING	((SG_file_flags)0x0010)	// like open(
#define SG_FILE_CREATE_NEW		((SG_file_flags)0x0020)	// like open(O_CREAT|O_EXCL
#define SG_FILE_OPEN_OR_CREATE	((SG_file_flags)0x0040)	// like open(O_CREAT
#define SG_FILE__OPEN_MASK		((SG_file_flags)0x0070)

#define SG_FILE_TRUNC			((SG_file_flags)0x0100)	// truncate (existing) upon opening (requires write access)

#define SG_FILE_LOCK			((SG_file_flags)0x0200)	// request a lock on the file

END_EXTERN_C;

#endif//H_SG_FILE_TYPEDEFS_H

