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

#include <sg.h>
#include <zlib.h>

/* unz_file_info contain information about a file in the zipfile */
typedef struct unz_file_info_s
{
    SG_uint16 version;              /* version made by                 2 bytes */
    SG_uint16 version_needed;       /* version needed to extract       2 bytes */
    SG_uint16 flag;                 /* general purpose bit flag        2 bytes */
    SG_uint16 compression_method;   /* compression method              2 bytes */
    SG_uint32 dosDate;              /* last mod file date in Dos fmt   4 bytes */
    SG_uint32 crc;                  /* crc-32                          4 bytes */
    SG_uint32 compressed_size;      /* compressed size                 4 bytes */
    SG_uint32 uncompressed_size;    /* uncompressed size               4 bytes */
    SG_uint16 size_filename;        /* filename length                 2 bytes */
    SG_uint16 size_file_extra;      /* extra field length              2 bytes */
    SG_uint16 size_file_comment;    /* file comment length             2 bytes */
    SG_uint16 disk_num_start;       /* disk number start               2 bytes */
    SG_uint16 internal_fa;          /* internal file attributes        2 bytes */
    SG_uint32 external_fa;          /* external file attributes        4 bytes */
} unz_file_info;

typedef struct unz_global_info_s
{
    SG_uint16 number_entry;         /* total number of entries in the central dir on this disk */
    SG_uint16 size_comment;         /* size of the global comment of the zipfile */
} unz_global_info;

#ifndef UNZ_BUFSIZE
#define UNZ_BUFSIZE (16384)
#endif

#ifndef UNZ_MAXFILENAMEINZIP
#define UNZ_MAXFILENAMEINZIP (4096)  /* TODO this is probably WAY more than we need */
#endif

#define SIZECENTRALDIRITEM (0x2e)
#define SIZEZIPLOCALHEADER (0x1e)

/* unz_file_info_interntal contain internal info about a file in zipfile*/
typedef struct unz_file_info_internal_s
{
    SG_uint32 offset_curfile;/* relative offset of local header 4 bytes */
} unz_file_info_internal;


/* file_in_zip_read_info_s contain internal information about a file in zipfile,
    when reading and decompress it */
typedef struct
{
    char  *read_buffer;         /* internal buffer for compressed data */
    z_stream stream;            /* zLib stream structure for inflate */

    SG_uint32 pos_in_zipfile;       /* position in byte on the zipfile, for fseek*/
    SG_uint32 stream_initialised;   /* flag set if stream structure is initialised*/

    SG_uint32 offset_local_extrafield;/* offset of the local extra field */
    SG_uint32 size_local_extrafield;/* size of the local extra field */
    SG_uint32 pos_local_extrafield;   /* position in the local extra field in read*/

    SG_uint32 crc32;                /* crc32 of all data uncompressed */
    SG_uint32 crc32_wait;           /* crc32 we must obtain after decompress all */
    SG_uint32 rest_read_compressed; /* number of byte to be decompressed */
    SG_uint32 rest_read_uncompressed;/*number of byte to be obtained after decomp*/
    SG_file* pFile;        /* io structore of the zipfile */
    SG_uint32 compression_method;   /* compression method (0==store) */
    SG_uint32 byte_before_the_zipfile;/* byte before the zipfile, (>0 for sfx)*/
} file_in_zip_read_info_s;

struct _SG_unzip
{
    SG_file* pFile;        /* io structore of the zipfile */
    unz_global_info gi;       /* public global information */
    SG_uint32 byte_before_the_zipfile;/* byte before the zipfile, (>0 for sfx)*/
    SG_uint32 num_file;             /* number of the current file in the zipfile*/
    SG_uint32 pos_in_central_dir;   /* pos of the current file in the central dir*/
    SG_bool current_file_ok;      /* flag about the usability of the current file*/
    SG_uint64 central_pos;          /* position of the beginning of the central dir*/

    SG_uint32 size_central_dir;     /* size of the central directory  */
    SG_uint32 offset_central_dir;   /* offset of start of central directory with
                                   respect to the starting disk number */

    unz_file_info cur_file_info; /* public info about the current file in zip*/
    unz_file_info_internal cur_file_info_internal; /* private info about it*/
    char cur_file_name[UNZ_MAXFILENAMEINZIP];

    file_in_zip_read_info_s* pfile_in_zip_read; /* structure about the current
                                        file if we are decompressing it */
};

void sg_unzip__get_uint16(SG_context* pCtx, SG_file* pFile, SG_uint16* pX)
{
    SG_uint16 x = 0;
    SG_uint8 b = 0;

    SG_ERR_CHECK(  SG_file__read(pCtx, pFile, 1, &b, NULL)  );
    x = (SG_uint16) b;

    SG_ERR_CHECK(  SG_file__read(pCtx, pFile, 1, &b, NULL)  );
    x |= (((SG_uint16)b)<<8);

    *pX = x;

fail:
    return;
}

void sg_unzip__get_uint32(SG_context* pCtx, SG_file* pFile, SG_uint32* pX)
{
    SG_uint32 x = 0;
    SG_uint8 b = 0;

    SG_ERR_CHECK(  SG_file__read(pCtx, pFile, 1, &b, NULL)  );
    x = (SG_uint32) b;

    SG_ERR_CHECK(  SG_file__read(pCtx, pFile, 1, &b, NULL)  );
    x |= (((SG_uint32)b)<<8);

    SG_ERR_CHECK(  SG_file__read(pCtx, pFile, 1, &b, NULL)  );
    x |= (((SG_uint32)b)<<16);

    SG_ERR_CHECK(  SG_file__read(pCtx, pFile, 1, &b, NULL)  );
    x |= (((SG_uint32)b)<<24);

    *pX = x;

fail:
    return;
}

#ifndef BUFREADCOMMENT
#define BUFREADCOMMENT (0x400)
#endif

static void sg_unzip__locate_central_dir(SG_context* pCtx, SG_file* pFile, SG_uint64* piPosition)
{
    unsigned char* buf;
    SG_uint64 uSizeFile;
    SG_uint32 uBackRead;
    SG_uint32 uMaxBack=0xffff; /* maximum size of global comment */
    SG_uint64 uPosFound=0;

    SG_ERR_CHECK(  SG_file__seek_end(pCtx, pFile, &uSizeFile)  );

    if (uMaxBack > uSizeFile)
    {
        uMaxBack = (SG_uint32) uSizeFile;
    }

    buf = (unsigned char*)SG_malloc(BUFREADCOMMENT+4);
    if (!buf)
    {
        SG_ERR_THROW(  SG_ERR_MALLOCFAILED  );
    }

    uBackRead = 4;
    while (uBackRead<uMaxBack)
    {
        SG_uint32 uReadSize;
        SG_uint64 uReadPos;
        int i;

        if (uBackRead+BUFREADCOMMENT>uMaxBack)
        {
            uBackRead = uMaxBack;
        }
        else
        {
            uBackRead += BUFREADCOMMENT;
        }
        uReadPos = uSizeFile-uBackRead ;

        uReadSize = (SG_uint32) (((BUFREADCOMMENT+4) < (uSizeFile-uReadPos)) ?
                     (BUFREADCOMMENT+4) : (uSizeFile-uReadPos));
        SG_ERR_CHECK(  SG_file__seek(pCtx, pFile, uReadPos)  );

        SG_ERR_CHECK(  SG_file__read(pCtx, pFile, uReadSize, buf, NULL)  );

        for (i=(int)uReadSize-3; (i--)>0;)
        {
            if (((*(buf+i))==0x50) && ((*(buf+i+1))==0x4b) &&
                ((*(buf+i+2))==0x05) && ((*(buf+i+3))==0x06))
            {
                uPosFound = uReadPos+i;
                break;
            }
        }

        if (uPosFound!=0)
        {
            break;
        }
    }
    SG_NULLFREE(pCtx, buf);

    *piPosition = uPosFound;

fail:
    return;
}

void SG_unzip__open(SG_context* pCtx, const SG_pathname* pPath, SG_unzip** ppResult)
{
    SG_unzip us;
    SG_unzip *s;
    SG_uint64 central_pos = 0;
    SG_uint32 uL;

    SG_uint16 number_disk = 0;          /* number of the current dist, used for
                                   spaning ZIP, unsupported, always 0*/
    SG_uint16 number_disk_with_CD = 0;  /* number the the disk with central dir, used
                                   for spaning ZIP, unsupported, always 0*/
    SG_uint16 number_entry_CD = 0;      /* total number of entries in
                                   the central dir
                                   (same than number_entry on nospan) */


    SG_ERR_CHECK(  SG_file__open__pathname(pCtx, pPath, SG_FILE_RDONLY | SG_FILE_OPEN_EXISTING, SG_FSOBJ_PERMS__UNUSED, &us.pFile)  );

    SG_ERR_CHECK(  sg_unzip__locate_central_dir(pCtx, us.pFile, &central_pos)  );

    SG_ERR_CHECK(  SG_file__seek(pCtx, us.pFile, central_pos)  );

    /* the signature, already checked */
    SG_ERR_CHECK(  sg_unzip__get_uint32(pCtx, us.pFile,&uL)  );

    /* number of this disk */
    SG_ERR_CHECK(  sg_unzip__get_uint16(pCtx, us.pFile,&number_disk)  );

    /* number of the disk with the start of the central directory */
    SG_ERR_CHECK(  sg_unzip__get_uint16(pCtx, us.pFile,&number_disk_with_CD)  );

    /* total number of entries in the central dir on this disk */
    SG_ERR_CHECK(  sg_unzip__get_uint16(pCtx, us.pFile,&us.gi.number_entry)  );

    /* total number of entries in the central dir */
    SG_ERR_CHECK(  sg_unzip__get_uint16(pCtx, us.pFile,&number_entry_CD)  );

    if ((number_entry_CD!=us.gi.number_entry) ||
        (number_disk_with_CD!=0) ||
        (number_disk!=0))
    {
        SG_ERR_THROW(  SG_ERR_ZIP_BAD_FILE  );
    }

    /* size of the central directory */
    SG_ERR_CHECK(  sg_unzip__get_uint32(pCtx, us.pFile,&us.size_central_dir)  );

    /* offset of start of central directory with respect to the
          starting disk number */
    SG_ERR_CHECK(  sg_unzip__get_uint32(pCtx, us.pFile,&us.offset_central_dir)  );

    /* zipfile comment length */
    SG_ERR_CHECK(  sg_unzip__get_uint16(pCtx, us.pFile,&us.gi.size_comment)  );

    if (central_pos<us.offset_central_dir+us.size_central_dir)
    {
        SG_ERR_THROW(  SG_ERR_ZIP_BAD_FILE  );
    }

    us.byte_before_the_zipfile = (SG_uint32) (central_pos - (us.offset_central_dir+us.size_central_dir));
    us.central_pos = central_pos;
    us.pfile_in_zip_read = NULL;

    us.current_file_ok = SG_FALSE;

    s = (SG_unzip*) SG_malloc(sizeof(SG_unzip));
    if (!s)
    {
        SG_ERR_THROW(  SG_ERR_MALLOCFAILED  );
    }
    *s=us;

    *ppResult = s;

    return;

fail:
    /* TODO free stuff */
    SG_FILE_NULLCLOSE(pCtx, us.pFile);
}

void SG_unzip__nullclose(SG_context* pCtx, SG_unzip** pp)
{
    SG_unzip* s = *pp;

    if (!s)
    {
        return;
    }

    if (s->pfile_in_zip_read)
    {
        SG_ERR_CHECK(  SG_unzip__currentfile__close(pCtx, s)  );
    }

    SG_ERR_CHECK(  SG_file__close(pCtx, &s->pFile)  );
    SG_NULLFREE(pCtx, s);


fail:
    return;
}

static void sg_unzip__currentfile__get_info(SG_context* pCtx, SG_unzip* s)
{
    SG_uint32 uMagic;

    SG_NULLARGCHECK_RETURN( s );

    SG_ERR_CHECK(  SG_file__seek(pCtx, s->pFile, s->pos_in_central_dir + s->byte_before_the_zipfile)  );

    /* we check the magic */
    SG_ERR_CHECK(  sg_unzip__get_uint32(pCtx, s->pFile,&uMagic)  );
    if (uMagic != 0x02014b50)
    {
        SG_ERR_THROW(  SG_ERR_ZIP_BAD_FILE  );
    }

    SG_ERR_CHECK(  sg_unzip__get_uint16(pCtx, s->pFile,&s->cur_file_info.version)  );

    SG_ERR_CHECK(  sg_unzip__get_uint16(pCtx, s->pFile,&s->cur_file_info.version_needed)  );

    SG_ERR_CHECK(  sg_unzip__get_uint16(pCtx, s->pFile,&s->cur_file_info.flag)  );

    SG_ERR_CHECK(  sg_unzip__get_uint16(pCtx, s->pFile,&s->cur_file_info.compression_method)  );

    SG_ERR_CHECK(  sg_unzip__get_uint32(pCtx, s->pFile,&s->cur_file_info.dosDate)  );

    SG_ERR_CHECK(  sg_unzip__get_uint32(pCtx, s->pFile,&s->cur_file_info.crc)  );

    SG_ERR_CHECK(  sg_unzip__get_uint32(pCtx, s->pFile,&s->cur_file_info.compressed_size)  );

    SG_ERR_CHECK(  sg_unzip__get_uint32(pCtx, s->pFile,&s->cur_file_info.uncompressed_size)  );

    SG_ERR_CHECK(  sg_unzip__get_uint16(pCtx, s->pFile,&s->cur_file_info.size_filename)  );

    SG_ERR_CHECK(  sg_unzip__get_uint16(pCtx, s->pFile,&s->cur_file_info.size_file_extra)  );

    SG_ERR_CHECK(  sg_unzip__get_uint16(pCtx, s->pFile,&s->cur_file_info.size_file_comment)  );

    SG_ERR_CHECK(  sg_unzip__get_uint16(pCtx, s->pFile,&s->cur_file_info.disk_num_start)  );

    SG_ERR_CHECK(  sg_unzip__get_uint16(pCtx, s->pFile,&s->cur_file_info.internal_fa)  );

    SG_ERR_CHECK(  sg_unzip__get_uint32(pCtx, s->pFile,&s->cur_file_info.external_fa)  );

    SG_ERR_CHECK(  sg_unzip__get_uint32(pCtx, s->pFile,&s->cur_file_info_internal.offset_curfile)  );

    SG_ASSERT (s->cur_file_info.size_filename < UNZ_MAXFILENAMEINZIP);

    SG_ERR_CHECK(  SG_file__read(pCtx, s->pFile, s->cur_file_info.size_filename, (SG_byte*) s->cur_file_name, NULL)  );
    s->cur_file_name[s->cur_file_info.size_filename] = 0;

fail:
    return;
}

void SG_unzip__goto_first_file(SG_context* pCtx, SG_unzip* s, SG_bool* pb, const char** ppsz_name, SG_uint64* piLength)
{
    SG_NULLARGCHECK_RETURN( s );

    s->pos_in_central_dir = s->offset_central_dir;
    s->num_file = 0;
    SG_ERR_CHECK(  sg_unzip__currentfile__get_info(pCtx, s)  );
    s->current_file_ok = SG_TRUE;
    if (pb)
    {
        *pb = SG_TRUE;
    }
    if (ppsz_name)
    {
        *ppsz_name = s->cur_file_name;
    }
    if (piLength)
    {
        *piLength = s->cur_file_info.uncompressed_size;
    }

fail:
    return;
}

void SG_unzip__goto_next_file(SG_context* pCtx, SG_unzip* s, SG_bool *pb, const char** ppsz_name, SG_uint64* piLength)
{
    SG_NULLARGCHECK_RETURN( s );

    if (!s->current_file_ok)
    {
        *pb = SG_FALSE;
        return;
    }

    /* TODO so what does the following mean? */
    if (s->gi.number_entry != 0xffff)    /* 2^16 files overflow hack */
    {
        if ((s->num_file + 1) == s->gi.number_entry)
        {
            *pb = SG_FALSE;
            return;
        }
    }

    s->pos_in_central_dir += (SIZECENTRALDIRITEM + s->cur_file_info.size_filename +
            s->cur_file_info.size_file_extra + s->cur_file_info.size_file_comment);
    s->num_file++;
    SG_ERR_CHECK(  sg_unzip__currentfile__get_info(pCtx, s)  );
    s->current_file_ok = SG_TRUE;
    if (pb)
    {
        *pb = SG_TRUE;
    }
    if (ppsz_name)
    {
        *ppsz_name = s->cur_file_name;
    }
    if (piLength)
    {
        *piLength = s->cur_file_info.uncompressed_size;
    }

fail:
    return;
}


void SG_unzip__locate_file(SG_context* pCtx, SG_unzip* s, const char* psz_filename, SG_bool* pb, SG_uint64* piLength)
{
    SG_bool b = SG_FALSE;

    /* We remember the 'current' position in the file so that we can jump
     * back there if we fail.
     */
    unz_file_info cur_file_infoSaved;
    unz_file_info_internal cur_file_info_internalSaved;
    SG_uint32 num_fileSaved;
    SG_uint32 pos_in_central_dirSaved;

    SG_NULLARGCHECK_RETURN( s );

	SG_ARGCHECK_RETURN((strlen(psz_filename) < UNZ_MAXFILENAMEINZIP), psz_filename);

    /* TODO hmmm.  why do we require the current file state to be "ok" here ? */
    if (!s->current_file_ok)
    {
        *pb = SG_FALSE;
        return;
    }

    /* Save the current state */
    num_fileSaved = s->num_file;
    pos_in_central_dirSaved = s->pos_in_central_dir;
    cur_file_infoSaved = s->cur_file_info;
    cur_file_info_internalSaved = s->cur_file_info_internal;

    SG_ERR_CHECK(  SG_unzip__goto_first_file(pCtx, s, &b, NULL, NULL)  );

    while (b)
    {
        if (strcmp(s->cur_file_name, psz_filename) == 0)
        {
            break;
        }

        SG_ERR_CHECK(  SG_unzip__goto_next_file(pCtx, s, &b, NULL, NULL)  );
    }

    if (b)
    {
        if (pb)
        {
            *pb = SG_TRUE;
        }
        if (piLength)
        {
            *piLength = s->cur_file_info.uncompressed_size;
        }
    }
    else
    {
        if (pb)
        {
            *pb = SG_FALSE;
            goto fail;
        }
        else
        {
            SG_ERR_THROW(  SG_ERR_NOT_FOUND  );
        }
    }

    return;

fail:
    /* We failed, so restore the state of the 'current file' to where we
     * were.
     */
    s->num_file = num_fileSaved ;
    s->pos_in_central_dir = pos_in_central_dirSaved ;
    s->cur_file_info = cur_file_infoSaved;
    s->cur_file_info_internal = cur_file_info_internalSaved;

}

/*
  Read the local header of the current zipfile
  Check the coherency of the local header and info in the end of central
        directory about this file
  store in *piSizeVar the size of extra info in local header
        (filename and size of extra field data)
*/
static void sg_unzip__check_coherency(
        SG_context* pCtx,
        SG_unzip* s,
        SG_uint32* piSizeVar,
        SG_uint32* poffset_local_extrafield,
        SG_uint32* psize_local_extrafield
        )
{
    /* TODO what is uFlags?  It seems to be unused here. */
    SG_uint32 uMagic,uData,uFlags = 0;
    SG_uint16 u16 = 0;
    SG_uint16 size_filename = 0;
    SG_uint16 size_extra_field = 0;

    *piSizeVar = 0;
    *poffset_local_extrafield = 0;
    *psize_local_extrafield = 0;

    SG_ERR_CHECK(  SG_file__seek(pCtx, s->pFile, s->cur_file_info_internal.offset_curfile + s->byte_before_the_zipfile)  );

    SG_ERR_CHECK(  sg_unzip__get_uint32(pCtx, s->pFile,&uMagic)  );

    if (uMagic!=0x04034b50)
    {
        SG_ERR_THROW(  SG_ERR_ZIP_BAD_FILE  );
    }

    SG_ERR_CHECK(  sg_unzip__get_uint16(pCtx, s->pFile,&u16)  );

    SG_ERR_CHECK(  sg_unzip__get_uint16(pCtx, s->pFile,&u16)  );

    SG_ERR_CHECK(  sg_unzip__get_uint16(pCtx, s->pFile,&u16)  );
    if (u16 != s->cur_file_info.compression_method)
    {
        SG_ERR_THROW(  SG_ERR_ZIP_BAD_FILE  );
    }
    if ((s->cur_file_info.compression_method!=0) && (s->cur_file_info.compression_method != Z_DEFLATED))
    {
        SG_ERR_THROW(  SG_ERR_ZIP_BAD_FILE  );
    }

    SG_ERR_CHECK(  sg_unzip__get_uint32(pCtx, s->pFile,&uData)  ); /* date/time */

    SG_ERR_CHECK(  sg_unzip__get_uint32(pCtx, s->pFile,&uData)  ); /* crc */
    if ((uData!=s->cur_file_info.crc) && ((uFlags & 8)==0))
    {
        SG_ERR_THROW(  SG_ERR_ZIP_BAD_FILE  );
    }

    SG_ERR_CHECK(  sg_unzip__get_uint32(pCtx, s->pFile,&uData)  ); /* size compr */
    if ((uData!=s->cur_file_info.compressed_size) && ((uFlags & 8)==0))
    {
        SG_ERR_THROW(  SG_ERR_ZIP_BAD_FILE  );
    }

    SG_ERR_CHECK(  sg_unzip__get_uint32(pCtx, s->pFile,&uData)  ); /* size uncompr */
    if ((uData!=s->cur_file_info.uncompressed_size) && ((uFlags & 8)==0))
    {
        SG_ERR_THROW(  SG_ERR_ZIP_BAD_FILE  );
    }


    SG_ERR_CHECK(  sg_unzip__get_uint16(pCtx, s->pFile,&size_filename)  );
    if (size_filename!=s->cur_file_info.size_filename)
    {
        SG_ERR_THROW(  SG_ERR_ZIP_BAD_FILE  );
    }

    *piSizeVar += (SG_uint32)size_filename;

    SG_ERR_CHECK(  sg_unzip__get_uint16(pCtx, s->pFile,&size_extra_field)  );

    *poffset_local_extrafield= s->cur_file_info_internal.offset_curfile +
                                    SIZEZIPLOCALHEADER + size_filename;
    *psize_local_extrafield = (SG_uint32)size_extra_field;

    *piSizeVar += (SG_uint32)size_extra_field;

fail:
    return;
}

void SG_unzip__currentfile__open(SG_context* pCtx, SG_unzip* s)
{
    int zerr=Z_OK;
    SG_uint32 iSizeVar;
    file_in_zip_read_info_s* pfile_in_zip_read_info = NULL;
    SG_uint32 offset_local_extrafield;  /* offset of the local extra field */
    SG_uint32  size_local_extrafield;    /* size of the local extra field */

    SG_NULLARGCHECK_RETURN( s );
	SG_NULLARGCHECK_RETURN(s->current_file_ok);

    if (s->pfile_in_zip_read)
    {
        SG_unzip__currentfile__close(pCtx, s);
    }

    SG_ERR_CHECK(  sg_unzip__check_coherency(pCtx, s,&iSizeVar, &offset_local_extrafield,&size_local_extrafield)  );

    pfile_in_zip_read_info = (file_in_zip_read_info_s*) SG_malloc(sizeof(file_in_zip_read_info_s));
    if (!pfile_in_zip_read_info)
    {
        SG_ERR_THROW(  SG_ERR_MALLOCFAILED  );
    }

    pfile_in_zip_read_info->read_buffer = (char*)SG_malloc(UNZ_BUFSIZE);
    if (!pfile_in_zip_read_info->read_buffer)
    {
        SG_ERR_THROW(  SG_ERR_MALLOCFAILED  );
    }
    pfile_in_zip_read_info->offset_local_extrafield = offset_local_extrafield;
    pfile_in_zip_read_info->size_local_extrafield = size_local_extrafield;
    pfile_in_zip_read_info->pos_local_extrafield=0;

    pfile_in_zip_read_info->stream_initialised=0;

    if ((s->cur_file_info.compression_method!=0) &&
        (s->cur_file_info.compression_method!=Z_DEFLATED))
    {
        SG_ERR_THROW(  SG_ERR_ZIP_BAD_FILE  );
    }

    pfile_in_zip_read_info->crc32_wait = s->cur_file_info.crc;
    pfile_in_zip_read_info->crc32=0;
    pfile_in_zip_read_info->compression_method = s->cur_file_info.compression_method;
    pfile_in_zip_read_info->pFile = s->pFile;
    pfile_in_zip_read_info->byte_before_the_zipfile=s->byte_before_the_zipfile;

    pfile_in_zip_read_info->stream.total_out = 0;

    if (s->cur_file_info.compression_method==Z_DEFLATED)
    {
      pfile_in_zip_read_info->stream.zalloc = (alloc_func)0;
      pfile_in_zip_read_info->stream.zfree = (free_func)0;
      pfile_in_zip_read_info->stream.opaque = (voidpf)0;
      pfile_in_zip_read_info->stream.next_in = (voidpf)0;
      pfile_in_zip_read_info->stream.avail_in = 0;

      zerr=inflateInit2(&pfile_in_zip_read_info->stream, -MAX_WBITS);
      if (zerr == Z_OK)
      {
          pfile_in_zip_read_info->stream_initialised=1;
      }
      else
      {
          SG_ERR_THROW(  SG_ERR_ZLIB(zerr)  );
      }

        /* windowBits is passed < 0 to tell that there is no zlib header.
         * Note that in this case inflate *requires* an extra "dummy" byte
         * after the compressed stream in order to complete decompression and
         * return Z_STREAM_END.
         * In unzip, i don't wait absolutely Z_STREAM_END because I known the
         * size of both compressed and uncompressed data
         */
    }
    pfile_in_zip_read_info->rest_read_compressed =
            s->cur_file_info.compressed_size ;
    pfile_in_zip_read_info->rest_read_uncompressed =
            s->cur_file_info.uncompressed_size ;


    pfile_in_zip_read_info->pos_in_zipfile = s->cur_file_info_internal.offset_curfile + SIZEZIPLOCALHEADER + iSizeVar;

    pfile_in_zip_read_info->stream.avail_in = (SG_uint32)0;

    s->pfile_in_zip_read = pfile_in_zip_read_info;

    return;

fail:
    SG_NULLFREE(pCtx, pfile_in_zip_read_info);
}

void SG_unzip__currentfile__read(SG_context* pCtx, SG_unzip* s, SG_byte* pBuf, SG_uint32 iLenBuf, SG_uint32* piBytesRead)
{
    int zerr=Z_OK;
    SG_uint32 iRead = 0;
    file_in_zip_read_info_s* pfile_in_zip_read_info;

    SG_NULLARGCHECK_RETURN( s );

    pfile_in_zip_read_info = s->pfile_in_zip_read;

	SG_NULLARGCHECK_RETURN( pfile_in_zip_read_info );

    if (!pfile_in_zip_read_info->read_buffer)
    {
        SG_ERR_THROW_RETURN( SG_ERR_UNSPECIFIED );
    }

    if (!iLenBuf)
    {
        return;
    }

    pfile_in_zip_read_info->stream.next_out = pBuf;

    pfile_in_zip_read_info->stream.avail_out = iLenBuf;

    if (iLenBuf > pfile_in_zip_read_info->rest_read_uncompressed)
    {
        pfile_in_zip_read_info->stream.avail_out = (SG_uint32)pfile_in_zip_read_info->rest_read_uncompressed;
    }

    while (pfile_in_zip_read_info->stream.avail_out>0)
    {
        if ((pfile_in_zip_read_info->stream.avail_in==0) &&
            (pfile_in_zip_read_info->rest_read_compressed>0))
        {
            SG_uint32 uReadThis = UNZ_BUFSIZE;

            if (pfile_in_zip_read_info->rest_read_compressed<uReadThis)
            {
                uReadThis = (SG_uint32)pfile_in_zip_read_info->rest_read_compressed;
            }
            if (uReadThis == 0)
            {
				//TODO - maybe we should change this
                SG_ERR_THROW(  SG_ERR_EOF  );
            }

            SG_ERR_CHECK(  SG_file__seek(pCtx, s->pFile, pfile_in_zip_read_info->pos_in_zipfile + pfile_in_zip_read_info->byte_before_the_zipfile)  );
            SG_ERR_CHECK(  SG_file__read(pCtx, s->pFile, uReadThis, (SG_byte*) pfile_in_zip_read_info->read_buffer, NULL)  );

            pfile_in_zip_read_info->pos_in_zipfile += uReadThis;

            pfile_in_zip_read_info->rest_read_compressed-=uReadThis;

            pfile_in_zip_read_info->stream.next_in = (Bytef*)pfile_in_zip_read_info->read_buffer;
            pfile_in_zip_read_info->stream.avail_in = (SG_uint32)uReadThis;
        }

        if (pfile_in_zip_read_info->compression_method==0)
        {
            SG_uint32 uDoCopy,i ;

            if ((pfile_in_zip_read_info->stream.avail_in == 0) &&
                (pfile_in_zip_read_info->rest_read_compressed == 0))
            {
                if (iRead == 0)
                {
                    SG_ERR_THROW(  SG_ERR_EOF  );
                }
                goto done;
            }

            if (pfile_in_zip_read_info->stream.avail_out <
                            pfile_in_zip_read_info->stream.avail_in)
            {
                uDoCopy = pfile_in_zip_read_info->stream.avail_out ;
            }
            else
            {
                uDoCopy = pfile_in_zip_read_info->stream.avail_in ;
            }

            for (i=0;i<uDoCopy;i++)
            {
                *(pfile_in_zip_read_info->stream.next_out+i) =
                        *(pfile_in_zip_read_info->stream.next_in+i);
            }

            pfile_in_zip_read_info->crc32 = crc32(pfile_in_zip_read_info->crc32,
                                pfile_in_zip_read_info->stream.next_out,
                                uDoCopy);

            pfile_in_zip_read_info->rest_read_uncompressed-=uDoCopy;
            pfile_in_zip_read_info->stream.avail_in -= uDoCopy;
            pfile_in_zip_read_info->stream.avail_out -= uDoCopy;
            pfile_in_zip_read_info->stream.next_out += uDoCopy;
            pfile_in_zip_read_info->stream.next_in += uDoCopy;
            pfile_in_zip_read_info->stream.total_out += uDoCopy;
            iRead += uDoCopy;
        }
        else
        {
            SG_uint32 uTotalOutBefore,uTotalOutAfter;
            const Bytef *bufBefore;
            SG_uint32 uOutThis;
            int flush=Z_SYNC_FLUSH;

            uTotalOutBefore = pfile_in_zip_read_info->stream.total_out;
            bufBefore = pfile_in_zip_read_info->stream.next_out;

            /*
            if ((pfile_in_zip_read_info->rest_read_uncompressed ==
                     pfile_in_zip_read_info->stream.avail_out) &&
                (pfile_in_zip_read_info->rest_read_compressed == 0))
                flush = Z_FINISH;
            */
            zerr = inflate(&pfile_in_zip_read_info->stream,flush);

            if ((zerr>=0) && (pfile_in_zip_read_info->stream.msg))
            {
                SG_ERR_THROW(  SG_ERR_ZLIB(zerr)  );
            }

            uTotalOutAfter = pfile_in_zip_read_info->stream.total_out;
            uOutThis = uTotalOutAfter-uTotalOutBefore;

            pfile_in_zip_read_info->crc32 = crc32(pfile_in_zip_read_info->crc32,bufBefore, (SG_uint32)(uOutThis));

            pfile_in_zip_read_info->rest_read_uncompressed -= uOutThis;

            iRead += (SG_uint32)(uTotalOutAfter - uTotalOutBefore);

            if (zerr == Z_STREAM_END)
            {
            //    return (iRead==0) ? UNZ_EOF : iRead;
            //    break;
            }
        }
    }

done:
    *piBytesRead = iRead;

fail:
    return;
}

void SG_unzip__currentfile__close(SG_context* pCtx, SG_unzip* s)
{

    file_in_zip_read_info_s* pfile_in_zip_read_info;

    SG_NULLARGCHECK_RETURN( s );

    pfile_in_zip_read_info = s->pfile_in_zip_read;

	SG_ARGCHECK_RETURN((pfile_in_zip_read_info!=NULL), s);

    if (pfile_in_zip_read_info->rest_read_uncompressed == 0)
    {
        if (pfile_in_zip_read_info->crc32 != pfile_in_zip_read_info->crc32_wait)
        {
            SG_ERR_THROW(  SG_ERR_ZIP_CRC  );
        }
    }

    SG_NULLFREE(pCtx, pfile_in_zip_read_info->read_buffer);
    pfile_in_zip_read_info->read_buffer = NULL;
    if (pfile_in_zip_read_info->stream_initialised)
    {
        inflateEnd(&pfile_in_zip_read_info->stream);
    }

    pfile_in_zip_read_info->stream_initialised = 0;
    SG_NULLFREE(pCtx, pfile_in_zip_read_info);

    s->pfile_in_zip_read=NULL;

fail:
    return;
}

void SG_unzip__fetch_vhash(SG_context* pCtx, SG_unzip* punzip, const char* psz_filename, SG_vhash** ppvh)
{
    SG_bool b = SG_FALSE;
    SG_uint64 len = 0;
    SG_vhash* pvh = NULL;
    SG_byte* pBytes = NULL;
    SG_uint32 sofar = 0;
    SG_uint32 got = 0;

    SG_ERR_CHECK(  SG_unzip__goto_first_file(pCtx, punzip, NULL, NULL, NULL)  );
    SG_ERR_CHECK(  SG_unzip__locate_file(pCtx, punzip, psz_filename, &b, &len)  );
    if (!b)
    {
        SG_ERR_THROW(  SG_ERR_NOT_FOUND  );
    }

    /* TODO verify that len is reasonable for a malloc */
    pBytes = SG_malloc(1 + (SG_uint32)len);
    if (!pBytes)
    {
        SG_ERR_THROW(  SG_ERR_MALLOCFAILED  );
    }

    SG_ERR_CHECK(  SG_unzip__currentfile__open(pCtx, punzip)  );
    while (sofar < (SG_uint32) len)
    {
        SG_ERR_CHECK(  SG_unzip__currentfile__read(pCtx, punzip, pBytes + sofar, (SG_uint32) len - sofar, &got)  );
        if (!got)
        {
            SG_ERR_THROW(  SG_ERR_UNSPECIFIED  ); /* TODO better error */
        }
        sofar += got;
    }
    SG_ERR_CHECK(  SG_unzip__currentfile__close(pCtx, punzip)  );

    pBytes[len] = 0;

    SG_ERR_CHECK(  SG_VHASH__ALLOC__FROM_JSON(pCtx, &pvh, (char*) pBytes)  );

    *ppvh = pvh;
    pvh = NULL;

    /* fall through */

fail:
    SG_NULLFREE(pCtx, pBytes);
    SG_VHASH_NULLFREE(pCtx, pvh);
}

void SG_unzip__fetch_next_file_into_file(SG_context* pCtx, SG_unzip* punzip, SG_pathname* pPath, SG_bool* pb, const char** ppsz_name, SG_uint64* piLength)
{
   SG_byte* pBytes = NULL;
    SG_uint32 sofar = 0;
    SG_uint32 got = 0;
    SG_file* pFile = NULL;
    SG_bool b = SG_FALSE;
    SG_uint64 len = 0;
    const char* psz_name = NULL;

    SG_ERR_CHECK(  SG_unzip__goto_next_file(pCtx, punzip, &b, &psz_name, &len)  );
    if (!b)
    {
        *pb = SG_FALSE;
        goto done;
    }

    pBytes = SG_malloc((SG_uint32)SG_STREAMING_BUFFER_SIZE);
    if (!pBytes)
    {
        SG_ERR_THROW(  SG_ERR_MALLOCFAILED  );
    }

    SG_ERR_CHECK(  SG_file__open__pathname(pCtx, pPath, SG_FILE_WRONLY | SG_FILE_CREATE_NEW, SG_FSOBJ_PERMS__UNUSED, &pFile)  );

    SG_ERR_CHECK(  SG_unzip__currentfile__open(pCtx, punzip)  );
    while (sofar < (SG_uint32) len)
    {
        SG_uint32 want = 0;
        if ((len - sofar) > SG_STREAMING_BUFFER_SIZE)
        {
            want = SG_STREAMING_BUFFER_SIZE;
        }
        else
        {
            want = (SG_uint32) (len - sofar);
        }
        SG_ERR_CHECK(  SG_unzip__currentfile__read(pCtx, punzip, pBytes, want, &got)  );
        if (!got)
        {
            SG_ERR_THROW(  SG_ERR_UNSPECIFIED  ); /* TODO better error */
        }
        SG_ERR_CHECK(  SG_file__write(pCtx, pFile, got, pBytes, NULL)  );

        sofar += got;
    }
    SG_NULLFREE(pCtx, pBytes);
    SG_ERR_CHECK(  SG_file__close(pCtx, &pFile)  );
    SG_ERR_CHECK(  SG_unzip__currentfile__close(pCtx, punzip)  );

    *pb = SG_TRUE;
    *ppsz_name = psz_name;
    *piLength = len;

done:
    /* fall through */

fail:
    SG_FILE_NULLCLOSE(pCtx, pFile);
    SG_NULLFREE(pCtx, pBytes);

}
