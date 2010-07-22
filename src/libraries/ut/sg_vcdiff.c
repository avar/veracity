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

#include "sg.h"

typedef struct _sg_vcdiff_hash         sg_vcdiff_hash;
typedef struct _sg_vcdiff_instrcache   sg_vcdiff_instrcache;
typedef struct _sg_vcdiff_window       sg_vcdiff_window;
typedef struct _sg_vcdiff_encoder      sg_vcdiff_encoder;
typedef struct _sg_vcdiff_decoder      sg_vcdiff_decoder;
typedef struct _sg_vcdiff_hashconfig   sg_vcdiff_hashconfig;


#define _SG_VCDIFF_DEFAULT_WINDOW_SIZE (128*1024)

#define _SG_VCDIFF_OP_NOOP 0
#define _SG_VCDIFF_OP_ADD  1
#define _SG_VCDIFF_OP_RUN  2
#define _SG_VCDIFF_OP_COPY 3

// CODE TABLE:
// VCDiff has a default codetable containing RUN/ADD/COPY instructions with common
// sizes for efficiency.  It also allows sizes to be defined by the diff file.
// We implement this default code table below, and the corresponding cache to
// code table instructions.  (An application-specific code table can be defined inside
// the VCDiff file, however we do not currently have any support for that.)
//
// VCDiff uses a cache of matches nearby and a cache of same matches in-memory
// for efficiency.  This is described in draft-korn-vcdiff-06.txt and implemented
// below.  It contains a simple circular array of size 4 (by default) and an
// unchained hash of size 256*3 (by default, size must be a multiple of 256) where
// collisions are simply overwritten.
//
// The encoder and decoder both insert into the cache in-order in the same mannar,
// thus the cache between the encoder and decoder is identical for every step
// (which is why size *must* be the same between encoding and decoding step.)
// The VCDiff algorithm has steps which say "copy this address from this slot in
// the near cache" and "copy this address from this slot in the same cache."

#define CODE_TABLE_SIZE 256
#define NearSize 4
#define SameSize 3

// the maximum size that the instruction codetable encodes, anything bigger must be separately encoded
const SG_uint32 MaxSize[] = { 0, 17, 0, 18 };	// corresponds to Type enum above, NoOp, Add, Run, Copy

// the locations to start looking for instructions (unpaired)
const SG_uint32 StartSearch[] = { 0, 1, 0, 19, 35, 51, 67, 83, 99, 115, 131, 147 };
const SG_uint32 SearchLength[] = { 0, 18, 1, 16 };

// the maximum size for squashability -- SquashLastSize is the maximum size for the last argument, for each
// type, and SquashNextSize is the maximum size for the next argument (in instruction pairs)... if the
// sizes of both are in these ranges (inclusive) then it will try to squash two single instructions into a pair
const SG_uint32 SquashLastSize[] = { 0, 4, 0, 4 };
const SG_uint32 SquashNextSize[] = { 0, 1, 0, 6 };

// this is mapped by the *next* instruction, again taking advantage of the fact that only copy instrs have mode > 0
const SG_uint32 SquashStartSearch[] = { 0, 247, 0, 163, 175, 187, 199, 211, 223, 235, 239, 243 };
const SG_uint32 SquashSearchLength[] = { 0, 9, 0, 12 };

// if we need an unsized instruction, these are the addrs
// take advantage that copy is the only instr with mode > 0
const SG_byte UnsizedInstruction[] = { 0, 1, 0, 19, 35, 51, 67, 83, 99, 115, 131, 147 };

const SG_byte Type1[] = {
 2,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
 1,  1,  1,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
 3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
 3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
 3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
 3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
 3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
 3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
 3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
 3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
 3,  3,  3,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
 1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
 1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
 1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
 1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
 1,  1,  1,  1,  1,  1,  1,  3,  3,  3,  3,  3,  3,  3,  3,  3,
};
const SG_byte Size1[] = {
 0,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
15, 16, 17,  0,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
16, 17, 18,  0,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
16, 17, 18,  0,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
16, 17, 18,  0,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
16, 17, 18,  0,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
16, 17, 18,  0,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
16, 17, 18,  0,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
16, 17, 18,  0,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
16, 17, 18,  0,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
16, 17, 18,  1,  1,  1,  2,  2,  2,  3,  3,  3,  4,  4,  4,  1,
 1,  1,  2,  2,  2,  3,  3,  3,  4,  4,  4,  1,  1,  1,  2,  2,
 2,  3,  3,  3,  4,  4,  4,  1,  1,  1,  2,  2,  2,  3,  3,  3,
 4,  4,  4,  1,  1,  1,  2,  2,  2,  3,  3,  3,  4,  4,  4,  1,
 1,  1,  2,  2,  2,  3,  3,  3,  4,  4,  4,  1,  2,  3,  4,  1,
 2,  3,  4,  1,  2,  3,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
};
const SG_byte Mode1[] = {
 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
 0,  0,  0,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
 1,  1,  1,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
 2,  2,  2,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
 3,  3,  3,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
 4,  4,  4,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
 5,  5,  5,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
 6,  6,  6,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,
 7,  7,  7,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,
 8,  8,  8,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
 0,  0,  0,  0,  0,  0,  0,  0,  1,  2,  3,  4,  5,  6,  7,  8,
};
const SG_byte Type2[] = {
 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
 0,  0,  0,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
 3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
 3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
 3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
 3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
 3,  3,  3,  3,  3,  3,  3,  1,  1,  1,  1,  1,  1,  1,  1,  1,
};
const SG_byte Size2[] = {
 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
 0,  0,  0,  4,  5,  6,  4,  5,  6,  4,  5,  6,  4,  5,  6,  4,
 5,  6,  4,  5,  6,  4,  5,  6,  4,  5,  6,  4,  5,  6,  4,  5,
 6,  4,  5,  6,  4,  5,  6,  4,  5,  6,  4,  5,  6,  4,  5,  6,
 4,  5,  6,  4,  5,  6,  4,  5,  6,  4,  5,  6,  4,  5,  6,  4,
 5,  6,  4,  5,  6,  4,  5,  6,  4,  5,  6,  4,  4,  4,  4,  4,
 4,  4,  4,  4,  4,  4,  4,  1,  1,  1,  1,  1,  1,  1,  1,  1,
};
const SG_byte Mode2[] = {
 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,
 1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,
 2,  2,  2,  2,  2,  2,  2,  3,  3,  3,  3,  3,  3,  3,  3,  3,
 3,  3,  3,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  5,
 5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  6,  6,  6,  6,  7,
 7,  7,  7,  8,  8,  8,  8,  0,  0,  0,  0,  0,  0,  0,  0,  0,
};

struct _sg_vcdiff_instrcache
{
    // instruction cache
    SG_uint32 Near[NearSize];	    // array of size NearSize
    SG_uint32 Same[SameSize*256];	// same hash, array of size s_same * 256
    SG_uint32 NearPosition;	        // the circular index for Near
};

struct _sg_vcdiff_hashconfig
{
	SG_uint16 slots_per_bucket;
	SG_uint32 num_buckets;
	SG_uint16 key_size;
	SG_uint16 step_size;
	SG_uint16 enough;
};

struct _sg_vcdiff_hash
{
	const sg_vcdiff_hashconfig* pConfig;
    SG_byte* WindowBuffer;
	SG_byte* limit_front;
	SG_byte* limit_back;
    SG_uint32* Buckets;
	SG_uint16* counts;
};

struct _sg_vcdiff_window
{
    // WINDOW DATA
    SG_uint32 SourceSize/* = 0*/;		// size of the source window used to encode
    SG_uint64 SourcePosition/* = 0*/;	// offset into source of the window used to encode
    SG_uint32 DeltaLength/* = 0*/;		// the length of delta data (everything that follows)
    SG_uint32 TargetWinSize/* = 0*/;	// size of the target window to be reconstructed
    SG_uint32 WindowBufferLength;
    SG_bool HasAddRun/* = false*/;		// delta contains ADD/RUN data (sizes)
    SG_bool HasInstr/* = false*/;		// delta contains instructions (should always be true)
    SG_bool HasCopyAddr/* = false*/;	// delta contains COPY addresses
    SG_uint32 AddRunLength/* = 0*/;		// length of ADD/RUN data
    SG_uint32 InstrLength/* = 0*/;		// length of instruction data
    SG_uint32 CopyAddrLength/* = 0*/;	// length of COPY address data

	SG_byte* AddRunData;			// ADD/RUN data
    SG_byte* InstrData;			// instruction data
    SG_byte* CopyAddrData;			// COPY address data
	SG_byte* WindowBuffer;
};

struct _sg_vcdiff_decoder
{
    sg_vcdiff_window* Window;

    SG_readstream* DeltaAccessor;
    SG_seekreader* SourceAccessor;

    SG_uint32 InstrPointer/* = 0*/;
    SG_uint32 CopyAddrPointer/* = 0*/;
    SG_uint32 AddRunPointer/* = 0*/;

    SG_uint32 BufferPointer/* = 0*/;

    sg_vcdiff_instrcache CodeTable;
};

struct _sg_vcdiff_encoder
{
    sg_vcdiff_window* Window;

    SG_writestream* DeltaAccessor;
    SG_readstream* TargetAccessor;
    SG_seekreader* SourceAccessor;

    sg_vcdiff_hash* SourceHash;
    sg_vcdiff_hash* TargetHash;

    SG_uint32 LastInstructionPointer/* = 0*/;

    sg_vcdiff_instrcache CodeTable;
};


/* TODO this routine allocates a LOT of memory.  Any way to tighten things up? */
void sg_vcdiff_window__init(SG_context* pCtx, sg_vcdiff_window* pThis, SG_uint32 window_size)
{
	memset(pThis, 0, sizeof(sg_vcdiff_window));

    pThis->WindowBuffer = SG_malloc(window_size * 2);
	if (!pThis->WindowBuffer)
	{
		SG_ERR_THROW(  SG_ERR_MALLOCFAILED  );
	}

    // we have to instantiate these to the maximum that they could ever really get filled with
    // addrundata could only be as big as the actual target window (in the case of one really big ADD)
    // instrdata could only be as big as one instruction followed by a small address
    // copyaddrdata could only be 4x as big as the target window, if every address was encoded
	pThis->AddRunData = SG_malloc(window_size * 2);
	if (!pThis->AddRunData)
	{
		SG_ERR_THROW(  SG_ERR_MALLOCFAILED  );
	}

	pThis->InstrData = SG_malloc(window_size * 2);
	if (!pThis->InstrData)
	{
		SG_ERR_THROW(  SG_ERR_MALLOCFAILED  );
	}

	pThis->CopyAddrData = SG_malloc(window_size * 4);
	if (!pThis->CopyAddrData)
	{
		SG_ERR_THROW(  SG_ERR_MALLOCFAILED  );
	}

	return;

fail:
	if (pThis->WindowBuffer)
	{
		SG_NULLFREE(pCtx, pThis->WindowBuffer);
		pThis->WindowBuffer = NULL;
	}
	if (pThis->AddRunData)
	{
		SG_NULLFREE(pCtx, pThis->AddRunData);
		pThis->AddRunData = NULL;
	}
	if (pThis->InstrData)
	{
		SG_NULLFREE(pCtx, pThis->InstrData);
		pThis->InstrData = NULL;
	}
	if (pThis->CopyAddrData)
	{
		SG_NULLFREE(pCtx, pThis->CopyAddrData);
		pThis->CopyAddrData = NULL;
	}

}

void sg_vcdiff_window__reset(sg_vcdiff_window* pThis)
{
    pThis->SourceSize = 0;
    pThis->SourcePosition = 0;
    pThis->DeltaLength = 0;
    pThis->TargetWinSize = 0;
	pThis->WindowBufferLength = 0;
    pThis->HasAddRun = SG_FALSE;
    pThis->HasInstr = SG_FALSE;
    pThis->HasCopyAddr = SG_FALSE;
    pThis->AddRunLength = 0;
    pThis->InstrLength = 0;
    pThis->CopyAddrLength = 0;
}

void sg_vcdiff_decoder__init(sg_vcdiff_decoder* pThis, sg_vcdiff_window* window, SG_readstream* deltaAccessor, SG_seekreader* sourceAccessor)
{
    pThis->Window = window;

	pThis->DeltaAccessor = deltaAccessor;
    pThis->SourceAccessor = sourceAccessor;

    pThis->InstrPointer = 0;
    pThis->CopyAddrPointer = 0;
    pThis->AddRunPointer = 0;
    pThis->BufferPointer = 0;

	memset(&pThis->CodeTable, 0, sizeof(sg_vcdiff_instrcache));
}

void sg_vcdiff__hash__free(SG_context * pCtx, sg_vcdiff_hash* pThis)
{
	SG_NULLFREE(pCtx, pThis->Buckets);
	SG_NULLFREE(pCtx, pThis->counts);
	SG_NULLFREE(pCtx, pThis);
}

void sg_vcdiff_encoder__init(sg_vcdiff_encoder* pThis, sg_vcdiff_window* window, SG_writestream* deltaAccessor, SG_readstream* targetAccessor, SG_seekreader* sourceAccessor, sg_vcdiff_hash* SourceHash, sg_vcdiff_hash* TargetHash)
{
    pThis->Window = window;

	pThis->SourceHash = SourceHash;
    pThis->TargetHash = TargetHash;
    pThis->DeltaAccessor = deltaAccessor;
    pThis->TargetAccessor = targetAccessor;
    pThis->SourceAccessor = sourceAccessor;

    pThis->LastInstructionPointer = 0;

	memset(&pThis->CodeTable, 0, sizeof(sg_vcdiff_instrcache));
}

// NUMBER ENCODING FUNCTIONS -- TO COPE WITH VCDIFF-FORMAT INTEGERS
//  vcdiff defines integers as the combination of n 7-bit numbers, read
//  from n bytes, most significant first.  the number continues to the
//  next byte only if the highest bit of the current byte is set.
//
//  example: the number 123456789 is represented as:
//  +----------+----------+---------+----------+
//  | 10111010 | 11101111 | 1011010 | 00010101 |
//  +----------+----------+---------+----------+
//    MSB + 58   MSB+111    MSB+26    0+21
//
//  the number 2 is represented as:
//  +----------+
//  | 00000010 |
//  +----------+
//    0+2

#define _SG_VCDIFF_ENCODE_MAX_BYTES 10

// takes a normal system integer and creates a byte array representative
//    of that number in VCDiff-number format
void sg_vcdiff__encode_number(SG_uint64 number, SG_uint32* pDataLength, SG_byte* dataBytes)
{
	SG_uint32 i;

	*pDataLength = 0;

    for(i = 0; i < _SG_VCDIFF_ENCODE_MAX_BYTES; i++)
	{
		// take the current subsection of the number (ie, the first
		// 3 bytes or any preceding 7 byte chunk)
		SG_byte dataPiece = (SG_byte)((number >> (int)(7 * (_SG_VCDIFF_ENCODE_MAX_BYTES - (i + 1)))) & 0x7f);

		// we write this subsection of the number if it is nonzero OR if there was a subsection of the number written
		// preceding it
		if(dataPiece > 0 || *pDataLength > 0)
		{
			dataBytes[*pDataLength] = dataPiece;

			// turn on the continuance bit IF there's part of
			// the number before this
			if(*pDataLength > 0)
			{
				dataBytes[(*pDataLength - 1)] |= 0x80;
			}

			(*pDataLength)++;
		}
	}

    // if we haven't written anything so far, then dataInt = 0
    // and we need to explicitly write that
    if(*pDataLength == 0)
	{
		dataBytes[(*pDataLength)++] = 0;
	}
}

SG_byte sg_vcdiff__get_encoded_number_length(SG_uint64 number)
{
	SG_byte tempbuf[_SG_VCDIFF_ENCODE_MAX_BYTES];
	SG_uint32 len;
	sg_vcdiff__encode_number(number, &len, tempbuf);
	return (SG_byte)len;	// cast for VS2005
}

// takes a bytestream and position pointer and reads that byte array until
//    a number in VCDiff format is read.  the number in system format is created and is length is
//    computed.
void sg_vcdiff__decode_number(SG_context* pCtx, SG_byte* dataBytes, SG_uint32 dataBytesLength, SG_uint32 position, SG_uint32* pNumber, SG_uint32* pDataLength)
{
    *pNumber = 0;
    *pDataLength = 0;

    do
	{
		if(
			((position + *pDataLength) >= dataBytesLength)
			|| (*pDataLength >= _SG_VCDIFF_ENCODE_MAX_BYTES)
			)
		{
			SG_ERR_THROW_RETURN( SG_ERR_VCDIFF_NUMBER_ENCODING );
		}

		*pNumber = (*pNumber << 7) + (SG_uint32)(dataBytes[(position + *pDataLength)] & 0x7f);
	} while((dataBytes[(position + (*pDataLength)++)] & 0x80) == 0x80);

}

void sg_vcdiff__write_number(SG_context* pCtx, SG_writestream* pFile, SG_uint64 dataInt)
{
    SG_byte p[_SG_VCDIFF_ENCODE_MAX_BYTES];
    SG_uint32 len;
    sg_vcdiff__encode_number(dataInt, &len, p);
	SG_ERR_CHECK_RETURN(  SG_writestream__write(pCtx, pFile, len, p, NULL)  );
}

void sg_vcdiff__read_byte(SG_context* pCtx, SG_readstream* pFile, SG_byte* pDataByte)
{
    SG_byte b = 0;
	SG_uint32 iBytesRetrieved = 0;

	SG_ERR_CHECK_RETURN(  SG_readstream__read(pCtx, pFile, 1, &b, &iBytesRetrieved, NULL)  );

	if (iBytesRetrieved == 1)
	{
		*pDataByte = b;
		return;
	}

	SG_ERR_THROW_RETURN( SG_ERR_INCOMPLETEREAD );
}

void sg_vcdiff__read_uint64(SG_context* pCtx, SG_readstream* pFile, SG_uint64* pDataInt)
{
    SG_uint32 dataLength = 0;
    SG_byte b = 0;

    *pDataInt = 0;

    do
	{
		SG_ERR_CHECK_RETURN(  sg_vcdiff__read_byte(pCtx, pFile, &b)  );

		(*pDataInt) = ((*pDataInt) << 7) | (SG_uint32)(b & 0x7f);
		dataLength++;
	} while (b & 0x80);

#if 1
    if (dataLength > _SG_VCDIFF_ENCODE_MAX_BYTES)
	{
		SG_ERR_THROW_RETURN( SG_ERR_VCDIFF_NUMBER_ENCODING );
	}
#endif

}

void sg_vcdiff__read_uint32(SG_context* pCtx, SG_readstream* pFile, SG_uint32* pDataInt)
{
	SG_uint64 val;

    *pDataInt = 0;

	SG_ERR_CHECK_RETURN(  sg_vcdiff__read_uint64(pCtx, pFile, &val)  );

	if (val <= SG_UINT32_MAX)
	{
		*pDataInt = (SG_uint32) val;
		return;
	}
	else
	{
		SG_ERR_THROW_RETURN( SG_ERR_VCDIFF_NUMBER_ENCODING );
	}
}

void sg_vcdiff__read_window(SG_context* pCtx, sg_vcdiff_window* pw, SG_readstream* windowAccessor, SG_uint32 max_window_size)
{
    SG_error err;
	SG_byte windowIndicator = 0;
	SG_byte deltaIndicator = 0;

    // read window indicator: a bitfield indicating the type of window
    //  bit 0: the segment of data is encoded from the source file
    //  bit 1: the segment of data is encoded from the target file
    sg_vcdiff__read_byte(pCtx, windowAccessor, &windowIndicator);
	SG_context__get_err(pCtx, &err);
	if (err == SG_ERR_EOF)
	{
		SG_context__err_reset(pCtx);
		return;
	}
	SG_ERR_CHECK_CURRENT;


    // now that we no we have a window, initialize the class
    if((windowIndicator & 0x01) != 0x01)
	{
		/* we don't support windows which don't use source data */
		SG_ERR_THROW(  SG_ERR_VCDIFF_UNSUPPORTED  );
	}
    if((windowIndicator & 0x02) == 0x02)
	{
		/* we don't support windows which use target data */
		SG_ERR_THROW(  SG_ERR_VCDIFF_UNSUPPORTED  );
	}

    SG_ERR_CHECK(  sg_vcdiff__read_uint32(pCtx, windowAccessor, &(pw->SourceSize))  );

    SG_ERR_CHECK(  sg_vcdiff__read_uint64(pCtx, windowAccessor, &(pw->SourcePosition))  );

    // read delta length
	SG_ERR_CHECK(  sg_vcdiff__read_uint32(pCtx, windowAccessor, &(pw->DeltaLength))  );

    // read target window size
	SG_ERR_CHECK(  sg_vcdiff__read_uint32(pCtx, windowAccessor, &(pw->TargetWinSize))  );

	pw->WindowBufferLength = pw->SourceSize + pw->TargetWinSize;

	if (pw->WindowBufferLength > max_window_size)
	{
		// this delta uses windows that are too big
		SG_ERR_THROW(  SG_ERR_VCDIFF_UNSUPPORTED  );
	}

    // read delta indicator: a bitfield indicating what is in this window
    deltaIndicator = 0;

    SG_ERR_CHECK(  sg_vcdiff__read_byte(pCtx, windowAccessor, &deltaIndicator)  );

    if (deltaIndicator & 0x01)
	{
		pw->HasAddRun = SG_TRUE;
	}
    if (deltaIndicator & 0x02)
	{
		pw->HasInstr = SG_TRUE;
	}
    if (deltaIndicator & 0x04)
	{
		pw->HasCopyAddr = SG_TRUE;
	}

    // read lengths
    // TODO: shouldn't we only read this if HasAddRun/HasInstr/HasCopyAddr is true??
	SG_ERR_CHECK(  sg_vcdiff__read_uint32(pCtx, windowAccessor, &(pw->AddRunLength))  );

	SG_ERR_CHECK(  sg_vcdiff__read_uint32(pCtx, windowAccessor, &(pw->InstrLength))  );

	SG_ERR_CHECK(  sg_vcdiff__read_uint32(pCtx, windowAccessor, &(pw->CopyAddrLength))  );

    // read add/run data
    if(pw->AddRunLength > 0)
	{
		SG_ERR_CHECK(  SG_readstream__read(pCtx, windowAccessor, pw->AddRunLength, pw->AddRunData, NULL, NULL)  );
	}

    // read instruction section
    if(pw->InstrLength > 0)
	{
		SG_ERR_CHECK(  SG_readstream__read(pCtx, windowAccessor, pw->InstrLength, pw->InstrData, NULL, NULL)  );
	}

    // read copyaddr section
    if(pw->CopyAddrLength > 0)
	{
		SG_ERR_CHECK(  SG_readstream__read(pCtx, windowAccessor, pw->CopyAddrLength, pw->CopyAddrData, NULL, NULL)  );
	}

fail:
	return;
}

void sg_vcdiff__write_window(SG_context* pCtx, sg_vcdiff_window* pw, SG_writestream* windowAccessor)
{
	SG_byte windowIndicator = 0;
    SG_byte deltaIndicator = 0;
	SG_uint32 DeltaLength;

    // write window indicator: a bitAccessor indicating the type of window
    //  bit 0: the segment of data is encoded from the source file
    //  bit 1: the segment of data is encoded from the target file
    windowIndicator |= 0x01;

	SG_ERR_CHECK(  SG_writestream__write(pCtx, windowAccessor, 1, &windowIndicator, NULL)  );

    // write source size/position
    SG_ERR_CHECK(  sg_vcdiff__write_number(pCtx, windowAccessor, pw->SourceSize)  );
    SG_ERR_CHECK(  sg_vcdiff__write_number(pCtx, windowAccessor, pw->SourcePosition)  );

	DeltaLength = pw->DeltaLength;

	DeltaLength += sg_vcdiff__get_encoded_number_length(pw->TargetWinSize);
	DeltaLength += 1; // deltaIndicator
	DeltaLength += sg_vcdiff__get_encoded_number_length(pw->AddRunLength);
	DeltaLength += sg_vcdiff__get_encoded_number_length(pw->InstrLength);
	DeltaLength += sg_vcdiff__get_encoded_number_length(pw->CopyAddrLength);

    // write delta length
    SG_ERR_CHECK(  sg_vcdiff__write_number(pCtx, windowAccessor, DeltaLength)  );

    // write target window size
    SG_ERR_CHECK(  sg_vcdiff__write_number(pCtx, windowAccessor, pw->TargetWinSize)  );

    // write delta indicator bitfield
    if(pw->HasAddRun)
	{
		deltaIndicator |= 0x01;
	}
    if(pw->HasInstr)
	{
		deltaIndicator |= 0x02;
	}
    if(pw->HasCopyAddr)
	{
		deltaIndicator |= 0x04;
	}

    SG_ERR_CHECK(  SG_writestream__write(pCtx, windowAccessor, 1, &deltaIndicator, NULL)  );

    // write lengths
    SG_ERR_CHECK(  sg_vcdiff__write_number(pCtx, windowAccessor, pw->AddRunLength)  );
    SG_ERR_CHECK(  sg_vcdiff__write_number(pCtx, windowAccessor, pw->InstrLength)  );
    SG_ERR_CHECK(  sg_vcdiff__write_number(pCtx, windowAccessor, pw->CopyAddrLength)  );

    // write add/run data
    if(pw->AddRunLength > 0)
	{
		SG_ERR_CHECK(  SG_writestream__write(pCtx, windowAccessor, pw->AddRunLength, pw->AddRunData, NULL)  );
	}

    // write instruction data
    if(pw->InstrLength > 0)
	{
		SG_ERR_CHECK(  SG_writestream__write(pCtx, windowAccessor, pw->InstrLength, pw->InstrData, NULL)  );
	}

    // write copy address data
    if(pw->CopyAddrLength > 0)
	{
		SG_ERR_CHECK(  SG_writestream__write(pCtx, windowAccessor, pw->CopyAddrLength, pw->CopyAddrData, NULL)  );
	}

fail:
	return;
}

void sg_vcdiff__read_header(SG_context* pCtx, SG_readstream* headerAccessor)
{
    SG_byte fileHeader[4];
    SG_byte headerIndicator = 0;

	// read fileheader : should be 'V', 'C', 'D' (with the high bit set) and a version number
	// (currently zero)

	SG_ERR_CHECK(  SG_readstream__read(pCtx, headerAccessor, 4, fileHeader, NULL, NULL)  );

	if(
		fileHeader[0] != 0xD6 ||			// 'V' + (1 << 7)
		fileHeader[1] != 0xC3 ||			// 'C' + (1 << 7)
		fileHeader[2] != 0xC4			// 'D' + (1 << 7)
		)
	{
		SG_ERR_THROW(  SG_ERR_VCDIFF_INVALID_FORMAT  );
	}

	if(fileHeader[3] > 0)
	{
		SG_ERR_THROW(  SG_ERR_VCDIFF_UNSUPPORTED  );
	}

    // read header indicator: bytefield to specify what other header options follow
    //  bit 0:  if set, indicates that a secondary compressor is used
    //          and the compressor ID is specified in the next byte
    //  bit 1:  if set, indicates that an application-specific code
    //          table is used, and its length is specified in the next
    //          integer.  (after the compressor ID, if bit 0 is set.)

	SG_ERR_CHECK(  sg_vcdiff__read_byte(pCtx, headerAccessor, &headerIndicator)  );

    if((headerIndicator & 0x01) == 0x01)
	{
		//ErrorMessage = "secondary compressor is not supported";
		SG_ERR_THROW(  SG_ERR_VCDIFF_UNSUPPORTED  );
	}
    if((headerIndicator & 0x02) == 0x02)
	{
		//ErrorMessage = "specified code table is not supported";
		SG_ERR_THROW(  SG_ERR_VCDIFF_UNSUPPORTED  );
	}

fail:
	return;

}

void sg_vcdiff__write_header(SG_context* pCtx, SG_writestream* headerAccessor)
{
	SG_byte headerIndicator = 0;
    SG_byte fileHeader[4];

    // write fileheader : should be 'V', 'C', 'D' (with the high bit set)
    // and a version number (currently zero)
    fileHeader[0] = 0xD6;		// 'V' + (1 << 7)
    fileHeader[1] = 0xC3;		// 'C' + (1 << 7)
    fileHeader[2] = 0xC4;		// 'D' + (1 << 7)
    fileHeader[3] = 0;			// vcdiff version 0

	SG_ERR_CHECK(  SG_writestream__write(pCtx, headerAccessor, 4, fileHeader, NULL)  );

    // write header indicator, saying no secondary compressor and
    // no app-specific code table.
    SG_ERR_CHECK(  SG_writestream__write(pCtx, headerAccessor, 1, &headerIndicator, NULL)  );

fail:
	return;
}

void sg_vcdiff_decoder__init_window_buffer(SG_context* pCtx, sg_vcdiff_decoder* pThis)
{
	sg_vcdiff_window* pw = pThis->Window;

    if(!pThis->SourceAccessor)
	{
		SG_ERR_THROW(  SG_ERR_VCDIFF_INVALID_FORMAT  );
	}

    SG_ASSERT(pw->SourceSize);

    SG_ERR_CHECK(  SG_seekreader__read(pCtx, pThis->SourceAccessor, pw->SourcePosition, pw->SourceSize, pw->WindowBuffer, NULL)  );

    pThis->BufferPointer = pw->SourceSize;

fail:
	return;
}

void sg_vcdiff_decoder__apply_run_instruction(SG_context* pCtx, sg_vcdiff_decoder* pThis, SG_uint32 size)
{
	sg_vcdiff_window* pw = pThis->Window;
	SG_byte runData;
	SG_uint32 i;

	if ((pThis->AddRunPointer + 1) > pw->AddRunLength)
	{
		//ErrorMessage = "out of add/run data"; //string.Format("out of add/run data (ptr={0}, size={1}, len={2})", AddRunPointer, size, Window->AddRunLength);

		SG_ERR_THROW_RETURN( SG_ERR_VCDIFF_INVALID_FORMAT );
	}

    runData = pw->AddRunData[pThis->AddRunPointer++];

    for(i = 0; i < size; i++)
	{
		pw->WindowBuffer[pThis->BufferPointer++] = runData;
	}

}

void sg_vcdiff_decoder__apply_add_instruction(SG_context* pCtx, sg_vcdiff_decoder* pThis, SG_uint32 size)
{
	sg_vcdiff_window* pw = pThis->Window;
	SG_uint32 i;

	if ((pThis->AddRunPointer + size) > pw->AddRunLength)
	{

		//ErrorMessage = "out of add/run data"; //String.Format("out of add/run data (ptr={0}, size={1}, datalength={2})", AddRunPointer, size, Window->AddRunLength);
		SG_ERR_THROW_RETURN( SG_ERR_VCDIFF_INVALID_FORMAT );
	}

    for(i = 0; i < size; i++)
	{
		pw->WindowBuffer[pThis->BufferPointer++] = pw->AddRunData[pThis->AddRunPointer + i];
	}

    pThis->AddRunPointer += size;

}

void sg_vcdiff_instrcache__update_cache(sg_vcdiff_instrcache* pThis, SG_uint32 addr)
{
	// populate the circular Near array
	pThis->Near[pThis->NearPosition] = addr;
	pThis->NearPosition = (pThis->NearPosition + 1) % NearSize;

	// populate the simple Same hash
	pThis->Same[addr % (256 * SameSize)] = addr;
}

void sg_vcdiff_decoder__apply_copy_instruction(SG_context* pCtx, sg_vcdiff_decoder* pThis, SG_uint32 size, SG_byte mode)
{
	SG_uint32 i;
    SG_uint32 position = 0;
    SG_int32 nearPosition, samePosition;
	sg_vcdiff_window* pw = pThis->Window;

    // mode is an integer specifying where the copy position is relative to
    // mode=0 means from beginning of file, mode=1 means from current cursor position
    // or, higher modes indicate an address in the near address cache or the same address cache
    // but for modes that are 0, 1 or in the near cache, we need to read the address
    if(
		(mode == 0)
		|| (mode == 1)
		|| (mode < (NearSize + 2))
		)
	{
		SG_uint32 addrLength;

		SG_ERR_CHECK(  sg_vcdiff__decode_number(pCtx, pw->CopyAddrData, pw->CopyAddrLength, pThis->CopyAddrPointer,  &position,  &addrLength)  );

		pThis->CopyAddrPointer += addrLength;
	}
    // if it's in the same cache, the position is the next *byte* in the copy address block
    else if(((mode - NearSize) - 2) < SameSize)
	{
		position = pw->CopyAddrData[pThis->CopyAddrPointer++];
	}

    if(mode == 0)
	{
		// address is an offset from the start of window data
	}
    else if(mode == 1)
	{
		// address is based on offset from current position as an integer
		position = pThis->BufferPointer - position;
	}
    else if(
		(nearPosition = (mode - 2)) >= 0
		&& (nearPosition < NearSize)
		)
	{
		// copy address is offset from an address in the near cache
		position += pThis->CodeTable.Near[nearPosition];
	}
    else if(
		(samePosition = ((mode - NearSize) - 2)) >= 0
		&& (samePosition < SameSize)
		)
	{
		// copy address is an entry in the same cache
		position = (SG_uint32)pThis->CodeTable.Same[samePosition * 256 + position];
	}
    else
	{
		//ErrorMessage = "unknown copy mode number";

		SG_ERR_THROW(  SG_ERR_VCDIFF_INVALID_FORMAT  );
	}

    // update this address in the cache
    sg_vcdiff_instrcache__update_cache(&(pThis->CodeTable), position);

    for(i = 0; i < size; i++)
	{
		pw->WindowBuffer[pThis->BufferPointer++] = pw->WindowBuffer[position + i];
	}

fail:
	return;
}

void sg_vcdiff_decoder__apply_instruction(SG_context* pCtx, sg_vcdiff_decoder* pThis, SG_byte type, SG_uint32 size, SG_byte mode)
{
    switch(type)
	{
	case _SG_VCDIFF_OP_NOOP:
		return;
	case _SG_VCDIFF_OP_ADD:
		SG_ERR_CHECK_RETURN(  sg_vcdiff_decoder__apply_add_instruction(pCtx, pThis, size)  );
		return;
	case _SG_VCDIFF_OP_RUN:
		SG_ERR_CHECK_RETURN(  sg_vcdiff_decoder__apply_run_instruction(pCtx, pThis, size)  );
		return;
	case _SG_VCDIFF_OP_COPY:
		SG_ERR_CHECK_RETURN(  sg_vcdiff_decoder__apply_copy_instruction(pCtx, pThis, size, mode) );
		return;
	default:
		//ErrorMessage = "unknown instruction type";
		SG_ERR_THROW2_RETURN( SG_ERR_VCDIFF_INVALID_FORMAT, (pCtx, "unknown instruction type") );
	}
}

void sg_vcdiff_decoder__apply(SG_context* pCtx, sg_vcdiff_decoder* pThis)
{
    SG_uint32 count;		// number of instruction pairs
    SG_uint32 sizeLength;
	sg_vcdiff_window* pw = pThis->Window;

    // fill the window buffer with the source
    SG_ERR_CHECK(  sg_vcdiff_decoder__init_window_buffer(pCtx, pThis)  );

    // walk through the instruction vector
    for(pThis->InstrPointer = 0, count = 0; pThis->InstrPointer < pw->InstrLength; count++)
	{
		SG_byte instrNumber = pw->InstrData[pThis->InstrPointer++];

		// remember, instructions are always paired
		SG_byte type1 = Type1[instrNumber];
		SG_uint32 size1 = Size1[instrNumber];
		SG_byte mode1 = Mode1[instrNumber];
		SG_byte type2 = Type2[instrNumber];
		SG_uint32 size2 = Size2[instrNumber];
		SG_byte mode2 = Mode2[instrNumber];

		// if the size for the specified instruction is set to 0 in the code
		// table, that means that the size is specified as the next SG_byte in
		// the delta file (the InstrData vector) and is, in fact, the next byte
		// in the instruction array.  these to if()s test to see if they need to
		// read the next byte as the size, and if so, do so.
		if(type1 != _SG_VCDIFF_OP_NOOP && size1 == 0)
		{
			SG_ERR_CHECK(  sg_vcdiff__decode_number(pCtx, pw->InstrData, pw->InstrLength, pThis->InstrPointer,  &size1,  &sizeLength)  );

			pThis->InstrPointer += sizeLength;
		}

		if(type2 != _SG_VCDIFF_OP_NOOP && size2 == 0)
		{
			SG_ERR_CHECK(  sg_vcdiff__decode_number(pCtx, pw->InstrData, pw->InstrLength, pThis->InstrPointer,  &size2,  &sizeLength)  );

			pThis->InstrPointer += sizeLength;
		}

		// apply the instruction pair
		SG_ERR_CHECK(  sg_vcdiff_decoder__apply_instruction(pCtx, pThis, type1, size1, mode1)  );
		SG_ERR_CHECK(  sg_vcdiff_decoder__apply_instruction(pCtx, pThis, type2, size2, mode2)  );
	}

fail:
	return;
}

void sg_vcdiff_window__free_buffers(SG_context * pCtx, sg_vcdiff_window* pThis)
{
    if (pThis->WindowBuffer)
	{
		SG_NULLFREE(pCtx, pThis->WindowBuffer);
		pThis->WindowBuffer = NULL;
	}
    if (pThis->AddRunData)
	{
		SG_NULLFREE(pCtx, pThis->AddRunData);
		pThis->AddRunData = NULL;
	}
    if (pThis->InstrData)
	{
		SG_NULLFREE(pCtx, pThis->InstrData);
		pThis->InstrData = NULL;
	}
    if (pThis->CopyAddrData)
	{
		SG_NULLFREE(pCtx, pThis->CopyAddrData);
		pThis->CopyAddrData = NULL;
	}
}

// REQUIRED READING FOR VCDIFF:
//  * "Delta Algorithms: An Empirical Analysis"   James J Hunt, Kiem-Phong Vo, Walter F Tichy
//    http://citeseer.nj.nec.com/hunt98delta.html
//  * "Vdelta answers [Subversion dev mailing list]"  Greg Hudson
//    http://subversion.tigris.org/servlets/ReadMsg?msgId=22655&listName=dev
//  * "More vdelta information [Subversion dev mailing list]"  Greg Hudson
//    http://subversion.tigris.org/servlets/ReadMsg?msgId=22661&listName=dev
//  * "The VCDIFF Generic Differencing and Compression Data Format"  David G Korn, Joshua P MacDonald, Jeffrey C Mogul, Kiem-Phong Vo
//    http://www.ietf.org/internet-drafts/draft-korn-vcdiff-06.txt
//
// You should really probably read all of those a few times before trying to delve into the code.

struct _sg_vcdiff_undeltify_state
{
    SG_seekreader* psr_source;
    SG_readstream* pstrm_delta;
	SG_writestream* pstrm_output;
    sg_vcdiff_window window;

};

#define DECODE_MAX_WINDOW_SIZE (2 * _SG_VCDIFF_DEFAULT_WINDOW_SIZE)

void SG_vcdiff__undeltify__begin(SG_context* pCtx, SG_seekreader* psr_source, SG_readstream* pstrm_delta, SG_vcdiff_undeltify_state** ppst )
{
    SG_vcdiff_undeltify_state* pst = NULL;

    SG_ERR_CHECK(  SG_alloc(pCtx, 1, sizeof(SG_vcdiff_undeltify_state), &pst)  );

    pst->psr_source = psr_source;
    pst->pstrm_delta = pstrm_delta;

	SG_ERR_CHECK(  sg_vcdiff_window__init(pCtx, &pst->window, DECODE_MAX_WINDOW_SIZE)  );

	// read the delta header
	SG_ERR_CHECK(  sg_vcdiff__read_header(pCtx, pst->pstrm_delta)  );

    *ppst = pst;
     pst = NULL;

    return;

fail:
    SG_NULLFREE(pCtx, pst);

    return;
}


void SG_vcdiff__undeltify__chunk(SG_context* pCtx, SG_vcdiff_undeltify_state* pst, SG_byte** ppResult, SG_uint32* pi_got)
{
    sg_vcdiff_window__reset(&pst->window);
    SG_ERR_CHECK(  sg_vcdiff__read_window(pCtx, &pst->window, pst->pstrm_delta, DECODE_MAX_WINDOW_SIZE)  );

    if(pst->window.TargetWinSize == 0)
    {
        *ppResult = NULL;
        *pi_got = 0;

        return;
    }

    {
        sg_vcdiff_decoder windowDecoder;

        sg_vcdiff_decoder__init(&windowDecoder, &pst->window, pst->pstrm_delta, pst->psr_source);

        SG_ERR_CHECK(  sg_vcdiff_decoder__apply(pCtx, &windowDecoder)  );
    }

    *ppResult = pst->window.WindowBuffer + pst->window.SourceSize;
    *pi_got = pst->window.TargetWinSize;

fail:
    return;
}


void SG_vcdiff__undeltify__end(SG_context* pCtx, SG_vcdiff_undeltify_state* pst)
{
	sg_vcdiff_window__free_buffers(pCtx, &pst->window);
    SG_NULLFREE(pCtx, pst);
}


void SG_vcdiff__undeltify__streams(SG_context* pCtx, SG_seekreader* SourceAccessor, SG_writestream* TargetAccessor, SG_readstream* DeltaAccessor)
{
    SG_vcdiff_undeltify_state* pst = NULL;

    SG_ERR_CHECK(  SG_vcdiff__undeltify__begin(pCtx, SourceAccessor, DeltaAccessor, &pst)  );

    while (1)
    {
        SG_byte* p = NULL;
        SG_uint32 got = 0;

        SG_ERR_CHECK(  SG_vcdiff__undeltify__chunk(pCtx, pst, &p, &got)  );
        if (!p)
        {
            break;
        }
        SG_ERR_CHECK(  SG_writestream__write(pCtx, TargetAccessor, got, p, NULL)  );
    }

    SG_ERR_CHECK(  SG_vcdiff__undeltify__end(pCtx, pst)  );

    return;

fail:
    SG_NULLFREE(pCtx, pst);

}

void sg_vcdiff__hash__alloc(SG_context* pCtx, const sg_vcdiff_hashconfig* pConfig,
								sg_vcdiff_hash ** ppNew)
{
	sg_vcdiff_hash* pThis = NULL;

	SG_ERR_CHECK_RETURN(  SG_alloc1(pCtx, pThis)  );

	pThis->pConfig = pConfig;

	SG_ERR_CHECK(  SG_alloc(pCtx, pConfig->num_buckets * pConfig->slots_per_bucket, sizeof(SG_uint32), &pThis->Buckets)  );
	SG_ERR_CHECK(  SG_alloc(pCtx, pConfig->num_buckets, sizeof(SG_uint16), &pThis->counts)  );

	*ppNew = pThis;
	return;

fail:
	SG_ERR_IGNORE(  sg_vcdiff__hash__free(pCtx, pThis)  );
}

void sg_vcdiff__hash__init(sg_vcdiff_hash* pThis, SG_byte* windowBuffer, SG_uint32 windowBufferLength, SG_uint32 front_limit)
{
	pThis->WindowBuffer = windowBuffer;
	pThis->limit_front = windowBuffer + front_limit;
	pThis->limit_back = windowBuffer + windowBufferLength;

	memset(pThis->counts, 0, pThis->pConfig->num_buckets * sizeof(SG_uint16));
}

SG_uint32 sg_vcdiff__hash__hash(sg_vcdiff_hash* pThis, SG_byte* key)
{
	SG_uint32 keyLength = pThis->pConfig->key_size;

	if (keyLength == 4)
	{
		return ((key[0]*33*33*33 + key[1]*33*33 + key[2]*33 + key[3]) % (pThis->pConfig->num_buckets));
	}
	else
	{
		SG_uint32 hash = 0;
		SG_uint32 i;

		// this is bernstein's popular "times 33" hash algorithm as
		// implemented by engelschall (search for engelschall fast hash on google)
		for(i = 0; i < keyLength; i++)
		{
			hash *= 33;
			hash += *key++;
		}

		return (hash % (pThis->pConfig->num_buckets));
	}
}

void sg_vcdiff__hash__add(SG_UNUSED_PARAM(SG_context* pCtx), sg_vcdiff_hash* pThis, SG_uint32 iBucket, SG_uint32 position)
{
	SG_UNUSED( pCtx );
	if (pThis->counts[iBucket] >= pThis->pConfig->slots_per_bucket)
	{
		return;
	}

	pThis->Buckets[iBucket * pThis->pConfig->slots_per_bucket + pThis->counts[iBucket]] = position;
	(pThis->counts[iBucket])++;

}

SG_uint32 sg_vcdiff__hash__compare_forward(sg_vcdiff_hash* pThis, SG_uint32 pos_front, SG_uint32 pos_back)
{
	SG_byte* pFront = pThis->WindowBuffer + pos_front;
	SG_byte* pBack = pThis->WindowBuffer + pos_back;

	while(
		(pFront < pThis->limit_front)
		&& (pBack < pThis->limit_back)
		&& (*pFront == *pBack)
		)
	{
		pFront++;
		pBack++;
	}

	return (SG_uint32)(pBack - (pThis->WindowBuffer + pos_back));	// cast for VS2005
}

SG_bool sg_vcdiff__hash__find_match(sg_vcdiff_hash* pThis, SG_uint32 iBucket, SG_uint32 position, SG_uint32 minLength, SG_uint32* pBestPosition, SG_uint32* pBestSize)
{
	SG_uint32* pBucket = &(pThis->Buckets[iBucket * pThis->pConfig->slots_per_bucket]);
    SG_uint32 matchLength;
	SG_uint32 iSlot;
	SG_uint32 count = pThis->counts[iBucket];

    (*pBestPosition) = 0;
    (*pBestSize) = 0;

	for (iSlot=0; iSlot < count; iSlot++)
	{
		SG_uint32 ipos = pBucket[iSlot];

		matchLength = sg_vcdiff__hash__compare_forward(pThis, ipos, position);

		if(matchLength > (*pBestSize))
		{
			(*pBestPosition) = ipos;
			(*pBestSize) = matchLength;
		}

		if ((*pBestSize) >= pThis->pConfig->enough)
		{
			return SG_TRUE;
		}
	}

    return (*pBestSize) >= minLength;
}

void sg_vcdiff_encoder__init_window_buffer(SG_context* pCtx, sg_vcdiff_encoder* pThis)
{
	sg_vcdiff_window* pw = pThis->Window;
    SG_uint32 got = 0;

    // put the source in
    SG_ERR_CHECK(  SG_seekreader__read(pCtx, pThis->SourceAccessor, pw->SourcePosition, pw->SourceSize, pw->WindowBuffer, NULL)  );

    // put the target in
	SG_readstream__read(pCtx, pThis->TargetAccessor, pw->TargetWinSize, pw->WindowBuffer + pw->SourceSize, &got, NULL);
	SG_ERR_CHECK_CURRENT_DISREGARD(SG_ERR_EOF);
    pw->TargetWinSize = got;

	sg_vcdiff__hash__init(pThis->SourceHash, pw->WindowBuffer, pw->WindowBufferLength, pw->SourceSize);

	sg_vcdiff__hash__init(pThis->TargetHash, pw->WindowBuffer, pw->WindowBufferLength, pw->WindowBufferLength);

fail:
	return;
}

SG_bool sg_vcdiff_encoder__squash_instruction(sg_vcdiff_encoder* pThis, SG_byte type, SG_uint32 size, SG_byte mode)
{
	SG_uint32 i;
    SG_byte lastType;
    SG_byte lastSize;
    SG_byte lastMode;
	SG_byte lastInstruction;
	sg_vcdiff_window* pw = pThis->Window;

    // bail if there was no previous instruction to try to squash with
    if(pw->InstrLength == 0)
	{
		return SG_FALSE;
	}

    // get the last instruction in the instr list
    lastInstruction = (SG_byte)pw->InstrData[pThis->LastInstructionPointer];

    // make sure that this instruction isn't already paired
    if(Type2[lastInstruction] != _SG_VCDIFF_OP_NOOP)
	{
		return SG_FALSE;
	}

    lastType = Type1[lastInstruction];
    lastSize = Size1[lastInstruction];
    lastMode = Mode1[lastInstruction];

    // see if this is a candidate for squashing
    if(
		lastSize <= SquashLastSize[lastType] &&
		size <= SquashNextSize[(int)type]
		)
	{
		SG_byte instrNumber = 0;
		SG_uint32 instrStart = SquashStartSearch[(int)(type + mode)];
		SG_uint32 instrEnd = instrStart + SquashSearchLength[(int)type];
		SG_bool instrFound = SG_FALSE;

		for(i = instrStart; i < instrEnd; i++)
		{
			if(
				Type1[i] == (SG_byte)lastType && Size1[i] == lastSize && Mode1[i] == lastMode &&
				Type2[i] == (SG_byte)type && Size2[i] == size && Mode2[i] == mode
				)
			{
				instrNumber = (SG_byte)i;
				instrFound = SG_TRUE;
				break;
			}
		}

		if(instrFound)
		{
			pw->InstrData[(pw->InstrLength - 1)] = (SG_byte)instrNumber;
			return SG_TRUE;
		}
	}

    return SG_FALSE;
}

void sg_vcdiff_encoder__write_instruction(SG_UNUSED_PARAM(SG_context* pCtx), sg_vcdiff_encoder* pThis, SG_byte type, SG_uint32 size, SG_byte mode)
{
    SG_bool instrFound = SG_FALSE;
    SG_uint32 instrNumber = 0;
	SG_uint32 i;
	sg_vcdiff_window* pw = pThis->Window;

	SG_UNUSED( pCtx );
    if(sg_vcdiff_encoder__squash_instruction(pThis, type, size, mode))
	{
		return;
	}

    // if the size is not one of the pre-computed sizes in the codetable, then we need to
    // use the codetable instruction entry where size=0 and then encode the size separately
    // in the
    // here, the size may be in the code table, let's try to find it
    if(size <= MaxSize[(int)type])
	{
		SG_uint32 startSearch = StartSearch[(int)(type + mode)];
		SG_uint32 endSearch = startSearch + SearchLength[(int)type];

		for(i = startSearch; i < endSearch; i++)
		{
			if(
				Type1[i] == (SG_byte)type && Size1[i] == size && Mode1[i] == mode &&
				Type2[i] == (SG_byte)_SG_VCDIFF_OP_NOOP
				)
			{
				instrNumber = i;
				instrFound = SG_TRUE;
				break;
			}
		}
	}

    // okay, we didn't find an exact instruction, let's find the generic (size=0) instruction and then
    // stick the size in the
    if(! instrFound)
	{
		instrNumber = UnsizedInstruction[(int)(type + mode)];
		instrFound = SG_TRUE;
	}

    // we need to record the pointer to the last instruction so that we may be able to squash it later...
    pThis->LastInstructionPointer = pw->InstrLength;

    pw->InstrData[pw->InstrLength++] = (SG_byte)instrNumber;
    pw->DeltaLength++;

    // if the size needs to be encoded separately, do it
    if(Size1[instrNumber] == 0)
	{
		SG_uint32 dataLength;

		sg_vcdiff__encode_number(size, &dataLength, pw->InstrData + pw->InstrLength);

		pw->InstrLength += dataLength;
		pw->DeltaLength += dataLength;
	}
}

SG_bool sg_vcdiff_instrcache__check_cache(sg_vcdiff_instrcache* pThis, SG_uint32 addr, SG_byte* pMode, SG_uint32* pPosition)
{
	SG_uint32 samePosition = (SG_uint32)(addr % (256 * SameSize));
	SG_uint32 i;

	if(pThis->Same[samePosition] == addr)
	{
		*pMode = (SG_byte)((samePosition / 256) + (2 + NearSize));
		*pPosition = (samePosition % 256);
		return SG_TRUE;
	}

	for(i = 0; i < NearSize; i++)
	{
		if((addr - pThis->Near[i]) < 128)
		{
			*pMode = (SG_byte)(i + 2);
			*pPosition = (SG_uint32)(addr - pThis->Near[i]);
			return SG_TRUE;
		}
	}

	*pMode = 0;
	*pPosition = 0;
	return SG_FALSE;
}

SG_bool sg_all_bytes_the_same(SG_byte* p, SG_uint32 len)
{
	SG_uint32 i;
	for (i=1; i<len; i++)
	{
		if (p[i] != p[i-1]) return SG_FALSE;
	}
	return SG_TRUE;
}

void sg_vcdiff_encoder__process_window_buffer(SG_context* pCtx, sg_vcdiff_encoder* pThis)
{
    SG_bool addMode = SG_FALSE;
    SG_uint32 addPosition = 0;
    SG_uint32 bestPosition, bestSize;
	sg_vcdiff_window* pw = pThis->Window;
    const SG_uint32 bufferLength = pw->WindowBufferLength;
	SG_uint32 i;
	SG_uint32 bufferPosition;
	SG_uint32 addLength;
	const SG_uint32 SourceKeySize = pThis->SourceHash->pConfig->key_size;
	const SG_uint32 TargetKeySize = pThis->TargetHash->pConfig->key_size;

	if (
		(pw->SourceSize == pw->TargetWinSize)
		&& (0 == memcmp(pw->WindowBuffer, pw->WindowBuffer + pw->SourceSize, pw->SourceSize))
		)
	{
		SG_uint32 dataLength;

		// write a copy instruction for the entire window

		SG_ERR_CHECK(  sg_vcdiff_encoder__write_instruction(pCtx, pThis, _SG_VCDIFF_OP_COPY, pw->SourceSize, 0)  );

		sg_vcdiff__encode_number(0, &dataLength, pw->CopyAddrData + pw->CopyAddrLength);
		pw->CopyAddrLength += dataLength;
		pw->DeltaLength += dataLength;

		return;
	}

	if (pw->SourceSize > SourceKeySize)
	{
		SG_byte *p;
		SG_byte *plimit = (pw->WindowBuffer + pw->SourceSize - SourceKeySize);

		for(p = pw->WindowBuffer, bufferPosition = 0; p < plimit; p++, bufferPosition += pThis->SourceHash->pConfig->step_size)
		{
			if (sg_all_bytes_the_same(p, SourceKeySize))
			{
			}
			else
			{
				SG_uint32 iBucket = sg_vcdiff__hash__hash(pThis->SourceHash, p);
				sg_vcdiff__hash__add(pCtx, pThis->SourceHash, iBucket, bufferPosition);
			}
		}
	}

    for(bufferPosition = pw->SourceSize; bufferPosition < bufferLength; )
	{
		// if there aren't enough bytes left to check the source hash, then just
		// write an add instruction and be done
		if((bufferLength - bufferPosition) < SourceKeySize)
		{
			for(i = bufferPosition; i < bufferLength; i++)
			{
				pw->AddRunData[pw->AddRunLength++] = pw->WindowBuffer[i];
				pw->DeltaLength++;
			}

			addLength = (addMode) ? (bufferLength - addPosition) : (bufferLength - bufferPosition);

			SG_ERR_CHECK(  sg_vcdiff_encoder__write_instruction(pCtx, pThis, _SG_VCDIFF_OP_ADD, addLength, 0)  );

			break;
		}
		else
		{
			SG_byte* pCurrent = pw->WindowBuffer + bufferPosition;
			const SG_uint32 iBucket_Source = sg_vcdiff__hash__hash(pThis->SourceHash, pCurrent);
			const SG_uint32 iBucket_Target = sg_vcdiff__hash__hash(pThis->TargetHash, pCurrent);

			// a run instruction
			// deal with these, because we *don't* want to insert long repeating bytes into the hash, because if (say)
			// a file has a multiple long strings of nulls, that's a buttload of hash collisions, which slows down the
			// encoder *greatly*
			if (sg_all_bytes_the_same(pCurrent, TargetKeySize))
			{
				SG_byte b = pCurrent[0];
				SG_byte* p = pw->WindowBuffer + bufferPosition + TargetKeySize;
				SG_byte *plimit = pw->WindowBuffer + pw->WindowBufferLength;

				// if we were doing an ADD, we want to make sure to write that instruction out, to finish up
				if(addMode)
				{
					SG_uint32 addLength = (bufferPosition - addPosition);

#if 0
					if (addLength >= TargetKeySize)
					{
						const SG_uint32 iBucket_Add = sg_vcdiff__hash__hash(pThis->TargetHash, pw->WindowBuffer + addPosition);
						sg_vcdiff__hash__add(pCtx, pThis->TargetHash, iBucket_Add, addPosition);
					}
#endif
					addMode = SG_FALSE;

					SG_ERR_CHECK(  sg_vcdiff_encoder__write_instruction(pCtx, pThis, _SG_VCDIFF_OP_ADD, addLength, 0)  );
				}

				while(
					(p < plimit)
					&& (*p == b)
					)
				{
					p++;
				}

				{
					SG_uint32 runLength = (SG_uint32)(p - (pw->WindowBuffer + bufferPosition));	// cast for VS2005

					SG_ERR_CHECK(  sg_vcdiff_encoder__write_instruction(pCtx, pThis, _SG_VCDIFF_OP_RUN, runLength, 0)  );

					pw->AddRunData[pw->AddRunLength++] = b;
					pw->DeltaLength++;

					bufferPosition += runLength;
				}
			}
			// a copy instruction
			else if (
				sg_vcdiff__hash__find_match(pThis->SourceHash, iBucket_Source, bufferPosition, SourceKeySize, &bestPosition, &bestSize)
				|| sg_vcdiff__hash__find_match(pThis->TargetHash, iBucket_Target, bufferPosition, TargetKeySize, &bestPosition, &bestSize)
				)
			{
				SG_uint32 dataLength;
				SG_uint32 relativePosition = bestPosition;
				SG_byte mode = 0;

				// if we were doing an ADD, we want to make sure to write that instruction out, to finish up
				if(addMode)
				{
					SG_uint32 addLength = (bufferPosition - addPosition);

#if 0
					if (addLength >= TargetKeySize)
					{
						const SG_uint32 iBucket_Add = sg_vcdiff__hash__hash(pThis->TargetHash, pw->WindowBuffer + addPosition);
						sg_vcdiff__hash__add(pCtx, pThis->TargetHash, iBucket_Add, addPosition);
					}
#endif
					addMode = SG_FALSE;

					SG_ERR_CHECK(  sg_vcdiff_encoder__write_instruction(pCtx, pThis, _SG_VCDIFF_OP_ADD, addLength, 0)  );
				}

				// see if this copy addr is already in a cache
				if(! sg_vcdiff_instrcache__check_cache(&(pThis->CodeTable), bestPosition, &mode, &relativePosition))
				{
#if 0
					// see if it's shorter to encode this position relative to current position
					if((bufferPosition - bestPosition) < bestPosition)
					{
						mode = 1;
						relativePosition = bufferPosition - bestPosition;
					}
					// otherwise, just encode the address
					else
#endif
					{
						mode = 0;
						relativePosition = bestPosition;
					}
				}

				// write the copy instruction
				SG_ERR_CHECK(  sg_vcdiff_encoder__write_instruction(pCtx, pThis, _SG_VCDIFF_OP_COPY, bestSize, mode)  );

				// put the copy position into the copyaddr list, but only if the mode == 0
				// encode the number (unless it's in the same cache)
				if (mode < (NearSize + 2))
				{
					sg_vcdiff__encode_number(relativePosition, &dataLength, pw->CopyAddrData + pw->CopyAddrLength);
					pw->CopyAddrLength += dataLength;
					pw->DeltaLength += dataLength;
				}
				// in the same cache, we just encode a byte
				else
				{
					pw->CopyAddrData[pw->CopyAddrLength++] = (SG_byte)relativePosition;
					pw->DeltaLength++;
				}

				sg_vcdiff_instrcache__update_cache(&(pThis->CodeTable), bestPosition);

				/* There used to be an add to the hash here.  I don't think we needed it */

				bufferPosition += bestSize;
			}
			// no match for this key, insert it into the hash and make this the start (or continuation)
			// of an add instruction
			else
			{
#if 1
				// add to the hash
				sg_vcdiff__hash__add(pCtx, pThis->TargetHash, iBucket_Target, bufferPosition);
#endif

				if (!addMode)
				{
					addMode = SG_TRUE;
					addPosition = bufferPosition;
				}

				pw->AddRunData[pw->AddRunLength++] = (SG_byte)pw->WindowBuffer[bufferPosition];
				pw->DeltaLength++;

				bufferPosition++;
			}
		}
	}

fail:
	return;
}

void sg_vcdiff_encoder__create(SG_context* pCtx, sg_vcdiff_encoder* pThis, SG_uint64 targetPosition, SG_uint64 source_length, SG_uint32 window_size)
{
	sg_vcdiff_window* pw = pThis->Window;

    // set up the window header
    pw->TargetWinSize = window_size;

	// set up the window header for a source-encoded delta
    if(pThis->SourceAccessor == NULL)
	{
		/* we require a source.  this implementation doesn't do compression, just deltas */
		SG_ERR_THROW(  SG_ERR_VCDIFF_UNSUPPORTED  );
	}

	if (source_length < window_size)
	{
		// small file.  just read the whole thing into the window
		pw->SourcePosition = 0;
		pw->SourceSize = (SG_uint32)source_length;	// cast for VS2005
	}
	else
	{
		SG_uint64 source_avail;

		if (targetPosition >= source_length)
		{
			/*
			  We've run out of source but we
			  still have target that needs to
			  be delta-ed against something.
			  So we arbitrarily delta again
			  the window at the beginning of
			  the source file.
			*/
			pw->SourcePosition = 0;
		}
		else
		{
			/*
			  As long as we can, just choose a
			  source window that is at the same
			  position as the current target
			  window.  This optimizes for the
			  case where the target is a modified
			  version of the source.
			*/
			pw->SourcePosition = targetPosition;
		}

		source_avail = source_length - pw->SourcePosition;

		pw->SourceSize = (SG_uint32) (
			(pw->TargetWinSize > source_avail)
			? source_avail
			: pw->TargetWinSize
			);
	}

    SG_ASSERT(pw->SourceSize);

	pw->WindowBufferLength = pw->SourceSize + pw->TargetWinSize;

	// read the source and target data into the window buffer
	SG_ERR_CHECK(  sg_vcdiff_encoder__init_window_buffer(pCtx, pThis)  );

    // process the source and target
	SG_ERR_CHECK(  sg_vcdiff_encoder__process_window_buffer(pCtx, pThis)  );


fail:
	return;
}

void sg_vcdiff__create(SG_context* pCtx, SG_seekreader* SourceAccessor, SG_uint64 source_length, SG_readstream* TargetAccessor, SG_writestream* DeltaAccessor, SG_uint32 window_size, const sg_vcdiff_hashconfig* pHashConfig_Source, const sg_vcdiff_hashconfig* pHashConfig_Target)
{
    SG_uint64 size = 0;
	sg_vcdiff_window window;
	sg_vcdiff_hash* pSourceHash = NULL;
	sg_vcdiff_hash* pTargetHash = NULL;

    if (0 == source_length)
    {
		SG_ERR_THROW_RETURN( SG_ERR_VCDIFF_UNSUPPORTED );
    }

	SG_ERR_CHECK(  sg_vcdiff_window__init(pCtx, &window, window_size)  );

    // write the delta header
	SG_ERR_CHECK(  sg_vcdiff__write_header(pCtx, DeltaAccessor)  );

	SG_ERR_CHECK(  sg_vcdiff__hash__alloc(pCtx, pHashConfig_Source,&pSourceHash)  );
	SG_ERR_CHECK(  sg_vcdiff__hash__alloc(pCtx, pHashConfig_Target,&pTargetHash)  );

    // process each window
    while(SG_TRUE)
	{
		sg_vcdiff_window__reset(&window);

		{
			sg_vcdiff_encoder windowEncoder;
			sg_vcdiff_encoder__init(&windowEncoder, &window, DeltaAccessor, TargetAccessor, SourceAccessor, pSourceHash, pTargetHash);
			SG_ERR_CHECK(  sg_vcdiff_encoder__create(pCtx, &windowEncoder, size, source_length, window_size)  );
		}

		if(window.TargetWinSize == 0)
		{
			break;
		}

		size += window.TargetWinSize;

		SG_ERR_CHECK(  sg_vcdiff__write_window(pCtx, &window, DeltaAccessor)  );
	}

	sg_vcdiff__hash__free(pCtx, pSourceHash);
	sg_vcdiff__hash__free(pCtx, pTargetHash);

	sg_vcdiff_window__free_buffers(pCtx,&window);

    return;

fail:
	SG_ERR_IGNORE(  sg_vcdiff_window__free_buffers(pCtx,&window)  );

	if (pSourceHash)
	{
		SG_ERR_IGNORE(  sg_vcdiff__hash__free(pCtx, pSourceHash)  );
	}

	if (pTargetHash)
	{
		SG_ERR_IGNORE(  sg_vcdiff__hash__free(pCtx, pTargetHash)  );
	}

}

void sg_vcdiff__deltify(SG_context* pCtx, SG_seekreader* psrSource, SG_readstream* pstrmTarget, SG_writestream* pstrmDelta, SG_uint32 window_size, sg_vcdiff_hashconfig* pshc, sg_vcdiff_hashconfig* pthc)
{
	SG_uint64 source_length = 0;

	SG_ERR_CHECK(  SG_seekreader__length(pCtx, psrSource, &source_length)  );

	SG_ERR_CHECK(  sg_vcdiff__create(pCtx, psrSource, source_length, pstrmTarget, pstrmDelta, window_size, pshc, pthc)  );

fail:
	return;
}

void SG_vcdiff__deltify__streams(SG_context* pCtx, SG_seekreader* psrSource, SG_readstream* pstrmTarget, SG_writestream* pstrmDelta)
{
	sg_vcdiff_hashconfig shc;
	sg_vcdiff_hashconfig thc;
	SG_uint32 window_size = _SG_VCDIFF_DEFAULT_WINDOW_SIZE;

	shc.key_size = 4;
	shc.step_size = 1;
	shc.slots_per_bucket = 4;
	shc.num_buckets = window_size / shc.step_size / shc.slots_per_bucket;
	shc.enough = 1024;

	thc.key_size = 4;
	thc.step_size = 1;
	thc.slots_per_bucket = 4;
	thc.num_buckets = window_size / thc.step_size / thc.slots_per_bucket;
	thc.enough = 32;

	SG_ERR_CHECK(  sg_vcdiff__deltify(pCtx, psrSource, pstrmTarget, pstrmDelta, window_size, &shc, &thc)  );

fail:
	return;
}

void SG_vcdiff__deltify__files(SG_context* pCtx, SG_pathname* pPathSource, SG_pathname* pPathTarget, SG_pathname* pPathDelta)
{
	SG_seekreader* pFile_Source = NULL;
	SG_readstream* pFile_Target = NULL;
	SG_writestream* pFile_Delta = NULL;

    SG_ERR_CHECK(  SG_seekreader__alloc__for_file(pCtx, pPathSource, 0, &pFile_Source)  );

    SG_ERR_CHECK(  SG_readstream__alloc__for_file(pCtx, pPathTarget, &pFile_Target)  );

    SG_ERR_CHECK(  SG_writestream__alloc__for_file(pCtx, pPathDelta, &pFile_Delta)  );

	SG_ERR_CHECK(  SG_vcdiff__deltify__streams(pCtx, pFile_Source, pFile_Target, pFile_Delta)  );

	SG_ERR_CHECK(  SG_seekreader__close(pCtx, pFile_Source)  );
	pFile_Source = NULL;

	SG_ERR_CHECK(  SG_readstream__close(pCtx, pFile_Target)  );
	pFile_Target = NULL;

	SG_ERR_CHECK(  SG_writestream__close(pCtx, pFile_Delta)  );
	pFile_Delta = NULL;

	return;

fail:
	//TODO should we put nulls here?
	if (pFile_Source)
	{
		SG_ERR_IGNORE( SG_seekreader__close(pCtx, pFile_Source)  );
	}
	if (pFile_Target)
	{
		SG_ERR_IGNORE( SG_readstream__close(pCtx, pFile_Target)  );
	}
	if (pFile_Delta)
	{
		SG_ERR_IGNORE(  SG_writestream__close(pCtx, pFile_Delta)  );
		SG_ERR_IGNORE(  SG_fsobj__remove__pathname(pCtx, pPathDelta)  );
	}
}

void SG_vcdiff__undeltify__files(SG_context* pCtx, SG_pathname* pPathSource, SG_pathname* pPathTarget, SG_pathname* pPathDelta)
{
	SG_seekreader* pFile_Source = NULL;
	SG_writestream* pFile_Target = NULL;
	SG_readstream* pFile_Delta = NULL;

    SG_ERR_CHECK(  SG_seekreader__alloc__for_file(pCtx, pPathSource, 0, &pFile_Source)  );

    SG_ERR_CHECK(  SG_writestream__alloc__for_file(pCtx, pPathTarget, &pFile_Target)  );

    SG_ERR_CHECK(  SG_readstream__alloc__for_file(pCtx, pPathDelta, &pFile_Delta)  );

	SG_ERR_CHECK(  SG_vcdiff__undeltify__streams(pCtx, pFile_Source, pFile_Target, pFile_Delta)  );

	SG_ERR_CHECK(  SG_seekreader__close(pCtx, pFile_Source)  );
	pFile_Source = NULL;

	SG_ERR_CHECK(  SG_writestream__close(pCtx, pFile_Target)  );
	pFile_Target = NULL;

	SG_ERR_CHECK(  SG_readstream__close(pCtx, pFile_Delta)  );
	pFile_Delta = NULL;

	return;

fail:
	if (pFile_Source)
	{
		SG_ERR_IGNORE(SG_seekreader__close(pCtx, pFile_Source)  );
	}
	if (pFile_Target)
	{
		SG_ERR_IGNORE(  SG_writestream__close(pCtx, pFile_Target)  );
		SG_ERR_IGNORE(  SG_fsobj__remove__pathname(pCtx, pPathTarget)  );
	}
	if (pFile_Delta)
	{
		SG_ERR_IGNORE( SG_readstream__close(pCtx, pFile_Delta)  );
	}
}


