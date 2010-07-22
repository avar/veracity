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

/* TODO zip64 extensions */

#ifndef Z_BUFSIZE
#define Z_BUFSIZE (16384)
#endif

#ifndef DEF_MEM_LEVEL
#if MAX_MEM_LEVEL >= 8
#  define DEF_MEM_LEVEL 8
#else
#  define DEF_MEM_LEVEL  MAX_MEM_LEVEL
#endif
#endif

#define SIZEDATA_INDATABLOCK (4096-(4*4))

#define LOCALHEADERMAGIC    (0x04034b50)
#define CENTRALHEADERMAGIC  (0x02014b50)
#define ENDHEADERMAGIC      (0x06054b50)

#define FLAG_LOCALHEADER_OFFSET (0x06)
#define CRC_LOCALHEADER_OFFSET  (0x0e)

#define SIZECENTRALHEADER (0x2e) /* 46 */

static void sg_zip__end_file_raw(SG_context* pCtx, SG_zip* zi, SG_uint32 uncompressed_size, SG_uint32 crc32);
static void sg_zip__flush(SG_context* pCtx, SG_zip* zi);

typedef struct linkedlist_datablock_internal_s
{
  struct linkedlist_datablock_internal_s* next_datablock;
  SG_uint32  avail_in_this_block;
  SG_uint32  filled_in_this_block;
  SG_uint32  unused; /* for future use and alignement */
  unsigned char data[SIZEDATA_INDATABLOCK];
} linkedlist_datablock_internal;

typedef struct linkedlist_data_s
{
    linkedlist_datablock_internal* first_block;
    linkedlist_datablock_internal* last_block;
} linkedlist_data;

typedef struct
{
    z_stream stream;            /* zLib stream structure for inflate */
    int  stream_initialised;    /* 1 is stream is initialised */
    uInt pos_in_buffered_data;  /* last written byte in buffered_data */

    SG_uint64 pos_local_header;     /* offset of the local header of the file
                                     currenty writing */
    char* central_header;       /* central header data for the current file */
    SG_uint32 size_centralheader;   /* size of the central header for cur file */
    SG_uint32 flag;                 /* flag of the file currently writing */

    int  method;                /* compression method of file currenty wr.*/
    Byte buffered_data[Z_BUFSIZE];/* buffer contain compressed data to be writ*/
    SG_uint32 dosDate;
    SG_uint32 crc32;
} curfile_info;

struct _SG_zip
{
    SG_file* pFile;
    linkedlist_data central_dir;/* datablock with central dir in construction*/
    int  in_opened_file_inzip;  /* 1 if a file in the zip is currently writ.*/
    curfile_info ci;            /* info on the file curretly writing */

    SG_uint32 begin_pos;            /* position of the beginning of the zipfile */
    SG_uint32 add_position_when_writing_offset;
    SG_uint32 number_entry;
} _SG_zip;

static void allocate_new_datablock(SG_context * pCtx,
								   linkedlist_datablock_internal ** ppNew)
{
    linkedlist_datablock_internal* ldi = NULL;

	SG_ERR_CHECK_RETURN(  SG_alloc1(pCtx, ldi)  );

    ldi->next_datablock = NULL ;
    ldi->filled_in_this_block = 0 ;
    ldi->avail_in_this_block = SIZEDATA_INDATABLOCK ;

	*ppNew = ldi;
}

static void free_datablock(SG_context * pCtx, linkedlist_datablock_internal* ldi)
{
    while (ldi)
    {
        linkedlist_datablock_internal* ldinext = ldi->next_datablock;
        SG_NULLFREE(pCtx, ldi);
        ldi = ldinext;
    }
}

static void add_data_in_datablock(SG_context* pCtx, linkedlist_data* ll, const SG_byte* buf, SG_uint32 len)
{
    linkedlist_datablock_internal* ldi;
    const unsigned char* from_copy;

	SG_NULLARGCHECK_RETURN( ll );

    if (! ll->last_block)
    {
		linkedlist_datablock_internal * pNew = NULL;

		SG_ERR_CHECK_RETURN(  allocate_new_datablock(pCtx, &pNew)  );
		ll->first_block = ll->last_block = pNew;
    }

    ldi = ll->last_block;
    from_copy = (unsigned char*)buf;

    while (len>0)
    {
        uInt copy_this;
        uInt i;
        unsigned char* to_copy;

        if (ldi->avail_in_this_block==0)
        {
			linkedlist_datablock_internal * pNew2 = NULL;

			SG_ERR_CHECK_RETURN(  allocate_new_datablock(pCtx, &pNew2)  );
			ldi->next_datablock = pNew2;
            ldi = ldi->next_datablock ;
            ll->last_block = ldi;
        }

        if (ldi->avail_in_this_block < len)
        {
            copy_this = (SG_uint32)ldi->avail_in_this_block;
        }
        else
        {
            copy_this = (SG_uint32)len;
        }

        to_copy = &(ldi->data[ldi->filled_in_this_block]);

        for (i=0;i<copy_this;i++)
        {
            *(to_copy+i)=*(from_copy+i);
        }

        ldi->filled_in_this_block += copy_this;
        ldi->avail_in_this_block -= copy_this;
        from_copy += copy_this ;
        len -= copy_this;
    }
}

void SG_zip__open(SG_context* pCtx, const SG_pathname* pathname, SG_zip** pp)
{
    SG_zip* pz = NULL;

	SG_ERR_CHECK_RETURN(  SG_alloc1(pCtx, pz)  );

    SG_ERR_CHECK(  SG_file__open__pathname(pCtx, pathname, SG_FILE_WRONLY | SG_FILE_OPEN_OR_CREATE, 0777, &pz->pFile)  );

    pz->begin_pos = 0;
    pz->in_opened_file_inzip = 0;
    pz->ci.stream_initialised = 0;
    pz->number_entry = 0;
    pz->add_position_when_writing_offset = 0;

    pz->central_dir.first_block = NULL;
    pz->central_dir.last_block = NULL;

    *pp = pz;

    return;

fail:
    SG_FILE_NULLCLOSE(pCtx, pz->pFile);
    SG_NULLFREE(pCtx, pz);

}

static void ziplocal_putValue (SG_context* pCtx, SG_file* pFile, SG_uint32 x, SG_uint32 nbByte)
{
    unsigned char buf[4];
    SG_uint32 n;

    for (n = 0; n < nbByte; n++)
    {
        buf[n] = (unsigned char)(x & 0xff);
        x >>= 8;
    }

    if (x != 0)
    {
        /* data overflow - hack for ZIP64 (X Roche) */
        for (n = 0; n < nbByte; n++)
        {
            buf[n] = 0xff;
        }
    }

    SG_ERR_CHECK_RETURN(  SG_file__write(pCtx, pFile, nbByte, buf, NULL)  );
}

static void ziplocal_putValue_inmemory (void* dest, SG_uint32 x, SG_uint32 nbByte)
{
    unsigned char* buf=(unsigned char*)dest;
    SG_uint32 n;

    for (n = 0; n < nbByte; n++)
    {
        buf[n] = (unsigned char)(x & 0xff);
        x >>= 8;
    }

    if (x != 0)
    {
       /* data overflow - hack for ZIP64 */
       for (n = 0; n < nbByte; n++)
       {
          buf[n] = 0xff;
       }
    }
}

static void my_dosdate_now(SG_context* pCtx, SG_uint32* pi)
{
    SG_uint32 year;
    SG_time tim;
    SG_uint32 t;
    SG_uint32 d_date;
    SG_uint32 d_time;

    SG_ERR_CHECK_RETURN(  SG_time__local_time(pCtx, &tim)  );

    year = tim.year;

    if (year>1980)
    {
        year-=1980;
    }
    else if (year>80)
    {
        year-=80;
    }

    d_date = (year << 9) | (tim.month << 5) | tim.mday;
    d_time = (tim.hour << 11) | (tim.min << 5) | (tim.sec >> 1);
    t = (d_date << 16) | d_time;

    *pi = t;
}

void sg_zip__begin_file(
        SG_context* pCtx,
        SG_zip* zi,
        const char* filename,
		SG_bool bIsFolderOnly
        )
{
    SG_uint32 size_filename;
    SG_uint32 size_comment;
    SG_uint32 i;
    SG_uint32 level = 8; /* TODO is this what we want? */
	SG_uint32 efa_flag = 0;

	SG_NULLARGCHECK_RETURN( zi );
	SG_NULLARGCHECK_RETURN( filename );

	if (bIsFolderOnly)
	{
		efa_flag |= 16;
	}

    if (zi->in_opened_file_inzip == 1)
    {
        SG_ERR_CHECK(  SG_zip__end_file(pCtx, zi)  );
    }

    size_comment = 0;

    size_filename = (SG_uint32)strlen(filename);

    SG_ERR_CHECK(  my_dosdate_now(pCtx, &(zi->ci.dosDate))  );

    zi->ci.flag = 0;
    if ((level==8) || (level==9))
    {
        zi->ci.flag |= 2;
    }
    if ((level==2))
    {
        zi->ci.flag |= 4;
    }
    if ((level==1))
    {
        zi->ci.flag |= 6;
    }

    zi->ci.crc32 = 0;
    zi->ci.method = Z_DEFLATED;
    zi->ci.stream_initialised = 0;
    zi->ci.pos_in_buffered_data = 0;
    SG_ERR_CHECK(  SG_file__tell(pCtx, zi->pFile, &zi->ci.pos_local_header)  );
    zi->ci.size_centralheader = SIZECENTRALHEADER + size_filename + 0 + 0;

	SG_ERR_CHECK(  SG_alloc(pCtx, (SG_uint32)zi->ci.size_centralheader,sizeof(char),&(zi->ci.central_header))  );

    ziplocal_putValue_inmemory(zi->ci.central_header,(SG_uint32)CENTRALHEADERMAGIC,4);
    /* version info */
    ziplocal_putValue_inmemory(zi->ci.central_header+4,(SG_uint32)0,2); /* versionmadeby, 0, FAT */
    ziplocal_putValue_inmemory(zi->ci.central_header+6,(SG_uint32)20,2); /* version needed.  20 is default. TODO zip64 */
    ziplocal_putValue_inmemory(zi->ci.central_header+8,(SG_uint32)zi->ci.flag,2);
    ziplocal_putValue_inmemory(zi->ci.central_header+10,(SG_uint32)zi->ci.method,2);
    ziplocal_putValue_inmemory(zi->ci.central_header+12,(SG_uint32)zi->ci.dosDate,4);
    ziplocal_putValue_inmemory(zi->ci.central_header+16,(SG_uint32)0,4); /*crc*/
    ziplocal_putValue_inmemory(zi->ci.central_header+20,(SG_uint32)0,4); /*compr size*/
    ziplocal_putValue_inmemory(zi->ci.central_header+24,(SG_uint32)0,4); /*uncompr size*/
    ziplocal_putValue_inmemory(zi->ci.central_header+28,(SG_uint32)size_filename,2);
    ziplocal_putValue_inmemory(zi->ci.central_header+30,(SG_uint32)0,2); /* extra field length */
    ziplocal_putValue_inmemory(zi->ci.central_header+32,(SG_uint32)0,2); /* comment length */
    ziplocal_putValue_inmemory(zi->ci.central_header+34,(SG_uint32)0,2); /*disk nm start*/

    ziplocal_putValue_inmemory(zi->ci.central_header+36,(SG_uint32)0,2); /* internal fa */
    ziplocal_putValue_inmemory(zi->ci.central_header+38,(SG_uint32)efa_flag,4); /* external fa */

    ziplocal_putValue_inmemory(zi->ci.central_header+42,(SG_uint32)zi->ci.pos_local_header- zi->add_position_when_writing_offset,4);

    for (i=0;i<size_filename;i++)
    {
        *(zi->ci.central_header+SIZECENTRALHEADER+i) = *(filename+i);
    }

    /* write the local header */
    SG_ERR_CHECK(  ziplocal_putValue(pCtx, zi->pFile,(SG_uint32)LOCALHEADERMAGIC,4)  );
    SG_ERR_CHECK(  ziplocal_putValue(pCtx, zi->pFile,(SG_uint32)20,2)  );/* version needed to extract */
    SG_ERR_CHECK(  ziplocal_putValue(pCtx, zi->pFile,(SG_uint32)zi->ci.flag,2)  );
    SG_ERR_CHECK(  ziplocal_putValue(pCtx, zi->pFile,(SG_uint32)zi->ci.method,2)  );
    SG_ERR_CHECK(  ziplocal_putValue(pCtx, zi->pFile,(SG_uint32)zi->ci.dosDate,4)  );
    SG_ERR_CHECK(  ziplocal_putValue(pCtx, zi->pFile,(SG_uint32)0,4)  ); /* crc 32, unknown */
    SG_ERR_CHECK(  ziplocal_putValue(pCtx, zi->pFile,(SG_uint32)0,4)  ); /* compressed size, unknown */
    SG_ERR_CHECK(  ziplocal_putValue(pCtx, zi->pFile,(SG_uint32)0,4)  ); /* uncompressed size, unknown */
    SG_ERR_CHECK(  ziplocal_putValue(pCtx, zi->pFile,(SG_uint32)size_filename,2)  );
    SG_ERR_CHECK(  ziplocal_putValue(pCtx, zi->pFile,(SG_uint32)0,2)  );

    if (size_filename>0)
    {
        SG_ERR_CHECK(  SG_file__write(pCtx, zi->pFile,size_filename,(SG_byte*)filename,NULL)  );
    }

    zi->ci.stream.avail_in = (SG_uint32)0;
    zi->ci.stream.avail_out = (SG_uint32)Z_BUFSIZE;
    zi->ci.stream.next_out = zi->ci.buffered_data;
    zi->ci.stream.total_in = 0;
    zi->ci.stream.total_out = 0;

    if (zi->ci.method == Z_DEFLATED)
    {
        int zerr = Z_OK;

        zi->ci.stream.zalloc = (alloc_func)0;
        zi->ci.stream.zfree = (free_func)0;
        zi->ci.stream.opaque = (voidpf)0;

        zerr = deflateInit2(&zi->ci.stream, level, Z_DEFLATED, -MAX_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY);
        if (zerr==Z_OK)
        {
            zi->ci.stream_initialised = 1;
            zi->in_opened_file_inzip = 1;
        }
        else
        {
            SG_ERR_THROW(  SG_ERR_ZLIB(zerr)  );
        }
    }

fail:
    return;
}


void SG_zip__begin_file(SG_context* pCtx,
        SG_zip* zi,
        const char* filename)
{
	sg_zip__begin_file(pCtx, zi, filename, SG_FALSE);
}

static void sg_zip__flush(SG_context* pCtx, SG_zip* zi)
{
    SG_ERR_CHECK(  SG_file__write(pCtx, zi->pFile, zi->ci.pos_in_buffered_data, zi->ci.buffered_data, NULL)  );

    zi->ci.pos_in_buffered_data = 0;

fail:
    return;
}

void SG_zip__write(SG_context* pCtx, SG_zip* zi, const SG_byte* buf, SG_uint32 len)
{
    int zerr = Z_OK;

    SG_NULLARGCHECK_RETURN( zi );
	SG_ARGCHECK_RETURN((zi->in_opened_file_inzip != 0), zi->in_opened_file_inzip);

    zi->ci.stream.next_in = (void*)buf;
    zi->ci.stream.avail_in = len;
    zi->ci.crc32 = crc32(zi->ci.crc32,buf,len);

    while ((zerr==Z_OK) && (zi->ci.stream.avail_in>0))
    {
        if (zi->ci.stream.avail_out == 0)
        {
            SG_ERR_CHECK(  sg_zip__flush(pCtx, zi)  );
            zi->ci.stream.avail_out = (SG_uint32)Z_BUFSIZE;
            zi->ci.stream.next_out = zi->ci.buffered_data;
        }

        if (zi->ci.method == Z_DEFLATED)
        {
            SG_uint32 uTotalOutBefore = zi->ci.stream.total_out;
            zerr=deflate(&zi->ci.stream,  Z_NO_FLUSH);
            zi->ci.pos_in_buffered_data += (SG_uint32)(zi->ci.stream.total_out - uTotalOutBefore) ;

        }
        else
        {
            uInt copy_this,i;
            if (zi->ci.stream.avail_in < zi->ci.stream.avail_out)
            {
                copy_this = zi->ci.stream.avail_in;
            }
            else
            {
                copy_this = zi->ci.stream.avail_out;
            }

            for (i=0;i<copy_this;i++)
            {
                *(((char*)zi->ci.stream.next_out)+i) =
                    *(((const char*)zi->ci.stream.next_in)+i);
            }

            zi->ci.stream.avail_in -= copy_this;
            zi->ci.stream.avail_out-= copy_this;
            zi->ci.stream.next_in+= copy_this;
            zi->ci.stream.next_out+= copy_this;
            zi->ci.stream.total_in+= copy_this;
            zi->ci.stream.total_out+= copy_this;
            zi->ci.pos_in_buffered_data += copy_this;
        }
    }

    if (zerr != Z_OK)
    {
		// Review: Jeff says: shouldn't we just wrap the z-error?
        SG_ERR_THROW(  SG_ERR_UNSPECIFIED  );
    }

fail:
    return;
}

static void sg_zip__end_file_raw(SG_context* pCtx, SG_zip* zi, SG_uint32 uncompressed_size, SG_uint32 crc32)
{
    SG_uint32 compressed_size;
    int zerr = Z_OK;

	SG_NULLARGCHECK_RETURN( zi );
	SG_ARGCHECK_RETURN((zi->in_opened_file_inzip != 0), zi->in_opened_file_inzip);

    zi->ci.stream.avail_in = 0;

    if (zi->ci.method == Z_DEFLATED)
    {
        while (Z_OK == zerr)
        {
            SG_uint32 uTotalOutBefore;

            if (zi->ci.stream.avail_out == 0)
            {
                SG_ERR_CHECK(  sg_zip__flush(pCtx, zi)  );
                zi->ci.stream.avail_out = (SG_uint32)Z_BUFSIZE;
                zi->ci.stream.next_out = zi->ci.buffered_data;
            }
            uTotalOutBefore = zi->ci.stream.total_out;
            zerr = deflate(&zi->ci.stream,  Z_FINISH);
            zi->ci.pos_in_buffered_data += (SG_uint32)(zi->ci.stream.total_out - uTotalOutBefore) ;
        }
    }

    if ((zi->ci.pos_in_buffered_data>0))
    {
        SG_ERR_CHECK(  sg_zip__flush(pCtx, zi)  );
    }

    if (zi->ci.method == Z_DEFLATED)
    {
        zerr = deflateEnd(&zi->ci.stream);
        zi->ci.stream_initialised = 0;
    }

    crc32 = (SG_uint32)zi->ci.crc32;
    uncompressed_size = (SG_uint32)zi->ci.stream.total_in;
    compressed_size = (SG_uint32)zi->ci.stream.total_out;

    ziplocal_putValue_inmemory(zi->ci.central_header+16,crc32,4); /*crc*/
    ziplocal_putValue_inmemory(zi->ci.central_header+20, compressed_size,4); /*compr size*/
    if (zi->ci.stream.data_type == Z_ASCII)
    {
        ziplocal_putValue_inmemory(zi->ci.central_header+36,(SG_uint32)Z_ASCII,2);
    }
    ziplocal_putValue_inmemory(zi->ci.central_header+24, uncompressed_size,4); /*uncompr size*/

    SG_ERR_CHECK(  add_data_in_datablock(pCtx, &zi->central_dir,(SG_byte*)zi->ci.central_header, (SG_uint32)zi->ci.size_centralheader)  );
    SG_NULLFREE(pCtx, zi->ci.central_header);

    {
        SG_uint64 cur_pos_inzip;

        SG_ERR_CHECK(  SG_file__tell(pCtx, zi->pFile, &cur_pos_inzip)  );
        SG_ERR_CHECK(  SG_file__seek(pCtx, zi->pFile, zi->ci.pos_local_header + 14)  );

        SG_ERR_CHECK(  ziplocal_putValue(pCtx, zi->pFile,crc32,4)  ); /* crc 32, unknown */

        /* compressed size, unknown */
        SG_ERR_CHECK(  ziplocal_putValue(pCtx, zi->pFile,compressed_size,4)  );

        /* uncompressed size, unknown */
        SG_ERR_CHECK(  ziplocal_putValue(pCtx, zi->pFile,uncompressed_size,4)  );

        SG_ERR_CHECK(  SG_file__seek(pCtx, zi->pFile, cur_pos_inzip)  );
    }

    zi->number_entry ++;
    zi->in_opened_file_inzip = 0;

fail:
    return;
}

void SG_zip__end_file(SG_context* pCtx, SG_zip* file)
{
    SG_ERR_CHECK_RETURN(  sg_zip__end_file_raw (pCtx, file,0,0)  );
}

void SG_zip__add_folder(SG_context* pCtx,
        SG_zip* zi,
        const char* foldername)
{
	sg_zip__begin_file(pCtx, zi, foldername, SG_TRUE);
	SG_zip__end_file(pCtx, zi);
}

void SG_zip__nullclose (SG_context* pCtx, SG_zip** pzi)
{
    SG_uint32 size_centraldir = 0;
    SG_uint64 centraldir_pos_inzip;

    SG_zip* zi = *pzi;

    SG_NULLARGCHECK_RETURN( zi );

    if (zi->in_opened_file_inzip == 1)
    {
        SG_ERR_CHECK(  SG_zip__end_file (pCtx, zi)  );
    }

    SG_ERR_CHECK(  SG_file__tell(pCtx, zi->pFile, &centraldir_pos_inzip)  );

    {
        linkedlist_datablock_internal* ldi = zi->central_dir.first_block ;
        while (ldi)
        {
            if ((ldi->filled_in_this_block>0))
            {
                SG_ERR_CHECK(  SG_file__write(pCtx, zi->pFile, ldi->filled_in_this_block, ldi->data, NULL)  );
            }

            size_centraldir += ldi->filled_in_this_block;
            ldi = ldi->next_datablock;
        }
    }
    free_datablock(pCtx, zi->central_dir.first_block);
    zi->central_dir.first_block = NULL;
    zi->central_dir.last_block = NULL;

    /* Magic End */
    SG_ERR_CHECK(  ziplocal_putValue(pCtx, zi->pFile,(SG_uint32)ENDHEADERMAGIC,4)  );

    /* number of this disk */
    SG_ERR_CHECK(  ziplocal_putValue(pCtx, zi->pFile,(SG_uint32)0,2)  );

    /* number of the disk with the start of the central directory */
    SG_ERR_CHECK(  ziplocal_putValue(pCtx, zi->pFile,(SG_uint32)0,2)  );

    /* total number of entries in the central dir on this disk */
    SG_ERR_CHECK(  ziplocal_putValue(pCtx, zi->pFile,(SG_uint32)zi->number_entry,2)  );

    /* total number of entries in the central dir */
    SG_ERR_CHECK(  ziplocal_putValue(pCtx, zi->pFile,(SG_uint32)zi->number_entry,2)  );

    /* size of the central directory */
    SG_ERR_CHECK(  ziplocal_putValue(pCtx, zi->pFile,(SG_uint32)size_centraldir,4)  );

    /* offset of start of central directory with respect to the starting disk number */
    SG_ERR_CHECK(  ziplocal_putValue(pCtx, zi->pFile, (SG_uint32)(centraldir_pos_inzip - zi->add_position_when_writing_offset),4)  );

    /* zipfile comment length */
    SG_ERR_CHECK(  ziplocal_putValue(pCtx, zi->pFile,(SG_uint32)0,2)  );

    SG_ERR_CHECK(  SG_file__close(pCtx, &zi->pFile)  );

    SG_NULLFREE(pCtx, zi);
    *pzi = NULL;

fail:
    return;
}

void SG_zip__store__bytes(SG_context* pCtx, SG_zip* pz, const char* filename, const SG_byte* buf, SG_uint32 len)
{
    SG_ERR_CHECK(  SG_zip__begin_file(pCtx, pz, filename)  );
    SG_ERR_CHECK(  SG_zip__write(pCtx, pz, buf, len)  );
    SG_ERR_CHECK(  SG_zip__end_file(pCtx, pz)  );

fail:
    return;
}

void SG_zip__store__file(SG_context* pCtx, SG_zip* pz, const char* filename, const SG_pathname* pPath)
{
    SG_uint64 len = 0;
    SG_uint64 sofar = 0;
    SG_file* pFile = NULL;
    SG_byte* pBytes = NULL;

    SG_ERR_CHECK(  SG_alloc(pCtx, SG_STREAMING_BUFFER_SIZE, 1, &pBytes)  );
    SG_ERR_CHECK(  SG_fsobj__length__pathname(pCtx, pPath, &len, NULL)  );
    SG_ERR_CHECK(  SG_file__open__pathname(pCtx, pPath, SG_FILE_RDONLY | SG_FILE_OPEN_EXISTING, SG_FSOBJ_PERMS__UNUSED, &pFile)  );
    SG_ERR_CHECK(  SG_zip__begin_file(pCtx, pz, filename)  );
    while (sofar < len)
    {
        SG_uint32 got = 0;

		// Review: Jeff says: BUGBUG SG_file__read() is going to throw an EOF error at the
		//                    end of the file rather than returning zero bytes read.
		//                    So this loop should always fail....

        SG_ERR_CHECK(  SG_file__read(pCtx, pFile, SG_STREAMING_BUFFER_SIZE, pBytes, &got)  );
        if (!got)
        {
            SG_ERR_THROW(  SG_ERR_UNSPECIFIED  );  /* TODO better err */
        }
        SG_ERR_CHECK(  SG_zip__write(pCtx, pz, pBytes, got)  );
        sofar += got;
    }
    SG_ERR_CHECK(  SG_file__close(pCtx, &pFile)  );
    SG_ERR_CHECK(  SG_zip__end_file(pCtx, pz)  );

    SG_NULLFREE(pCtx, pBytes);

    return;

fail:
    SG_FILE_NULLCLOSE(pCtx, pFile);
    SG_NULLFREE(pCtx, pBytes);
}
