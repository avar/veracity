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
 * @file sg_blob_typedefs.h
 *
 * @details A BLOB is Binary Large OBject (BLOB).
 *
 * We use Blobs to store arbitrary content in the Repository.  This
 * includes files under source control and our control files.
 *
 * Blobs are identified by a Blob Hash Identifier (HID).
 * The HID uniquely identifies the Blob and is the only handle to
 * loookup/access the Blob Content.  Think of the HID as a pointer
 * to an on-disk data structure.
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_BLOB_TYPEDEFS_H
#define H_SG_BLOB_TYPEDEFS_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

/**
 * The Blob Encoding identifies how the original binary data is encoded.
 */
typedef SG_byte SG_blob_encoding;

#define SG_BLOBENCODING__FULL               ((SG_blob_encoding)'f')
#define SG_BLOBENCODING__ALWAYSFULL         ((SG_blob_encoding)'a')
#define SG_BLOBENCODING__ZLIB		        ((SG_blob_encoding)'z')
#define SG_BLOBENCODING__VCDIFF			    ((SG_blob_encoding)'v')

#define SG_IS_BLOBENCODING_FULL(e) ((SG_BLOBENCODING__FULL == (e)) || (SG_BLOBENCODING__ALWAYSFULL == (e)))

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_BLOB_TYPEDEFS_H

