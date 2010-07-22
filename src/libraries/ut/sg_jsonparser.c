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

/*
  TODO

  remove the unused utf convert code

  indent to move the curly to the next line

  make the stack always static

*/

#include <sg.h>

/*
 * Copyright 2001-2004 Unicode, Inc.
 *
 * Disclaimer
 *
 * This source code is provided as is by Unicode, Inc. No claims are
 * made as to fitness for any particular purpose. No warranties of any
 * kind are expressed or implied. The recipient agrees to determine
 * applicability of information provided. If this file has been
 * purchased on magnetic or optical media from Unicode, Inc., the
 * sole remedy for any claim will be exchange of defective media
 * within 90 days of receipt.
 *
 * Limitations on Rights to Redistribute This Code
 *
 * Unicode, Inc. hereby grants the right to freely use the information
 * supplied in this file in the creation of products supporting the
 * Unicode Standard, and to make copies of this file in any form
 * for internal or external distribution as long as this notice
 * remains attached.
 */

/* ---------------------------------------------------------------------

   Conversions between UTF32, UTF-16, and UTF-8.  Header file.

   Several funtions are included here, forming a complete set of
   conversions between the three formats.  UTF-7 is not included
   here, but is handled in a separate source file.

   Each of these routines takes pointers to input buffers and output
   buffers.  The input buffers are const.

   Each routine converts the text between *sourceStart and sourceEnd,
   putting the result into the buffer between *targetStart and
   targetEnd. Note: the end pointers are *after* the last item: e.g.
   *(sourceEnd - 1) is the last item.

   The return result indicates whether the conversion was successful,
   and if not, whether the problem was in the source or target buffers.
   (Only the first encountered problem is indicated.)

   After the conversion, *sourceStart and *targetStart are both
   updated to point to the end of last text successfully converted in
   the respective buffers.

   Input parameters:
   sourceStart - pointer to a pointer to the source buffer.
   The contents of this are modified on return so that
   it points at the next thing to be converted.
   targetStart - similarly, pointer to pointer to the target buffer.
   sourceEnd, targetEnd - respectively pointers to the ends of the
   two buffers, for overflow checking only.

   These conversion functions take a ConversionFlags argument. When this
   flag is set to strict, both irregular sequences and isolated surrogates
   will cause an error.  When the flag is set to lenient, both irregular
   sequences and isolated surrogates are converted.

   Whether the flag is strict or lenient, all illegal sequences will cause
   an error return. This includes sequences such as: <F4 90 80 80>, <C0 80>,
   or <A0> in UTF-8, and values above 0x10FFFF in UTF-32. Conformant code
   must check for illegal sequences.

   When the flag is set to lenient, characters over 0x10FFFF are converted
   to the replacement character; otherwise (when the flag is set to strict)
   they constitute an error.

   Output parameters:
   The value "sourceIllegal" is returned from some routines if the input
   sequence is malformed.  When "sourceIllegal" is returned, the source
   value will point to the illegal value that caused the problem. E.g.,
   in UTF-8 when a sequence is malformed, it points to the start of the
   malformed sequence.

   Author: Mark E. Davis, 1994.
   Rev History: Rick McGowan, fixes & updates May 2001.
   Fixes & updates, Sept 2001.

   ------------------------------------------------------------------------ */

/* ---------------------------------------------------------------------
   The following 4 definitions are compiler-specific.
   The C standard does not guarantee that wchar_t has at least
   16 bits, so wchar_t is no less portable than unsigned short!
   All should be unsigned values to avoid sign extension during
   bit mask & shift operations.
   ------------------------------------------------------------------------ */


typedef SG_uint32	UTF32;	/* at least 32 bits */
typedef SG_uint16	UTF16;	/* at least 16 bits */
typedef SG_uint8	UTF8;	/* typically 8 bits */
typedef SG_uint8	Boolean; /* 0 or 1 */

/* Some fundamental constants */
#define UNI_REPLACEMENT_CHAR (UTF32)0x0000FFFD
#define UNI_MAX_BMP (UTF32)0x0000FFFF
#define UNI_MAX_UTF16 (UTF32)0x0010FFFF
#define UNI_MAX_UTF32 (UTF32)0x7FFFFFFF
#define UNI_MAX_LEGAL_UTF32 (UTF32)0x0010FFFF

typedef enum {
	conversionOK, 		/* conversion successful */
	sourceExhausted,	/* partial character in source, but hit end */
	targetExhausted,	/* insuff. room in target for conversion */
	sourceIllegal		/* source sequence is illegal/malformed */
} ConversionResult;

typedef enum {
	strictConversion = 0,
	lenientConversion
} ConversionFlags;

ConversionResult ConvertUTF8toUTF16 (
	const UTF8** sourceStart, const UTF8* sourceEnd,
	UTF16** targetStart, UTF16* targetEnd, ConversionFlags flags);

ConversionResult ConvertUTF16toUTF8 (
	const UTF16** sourceStart, const UTF16* sourceEnd,
	UTF8** targetStart, UTF8* targetEnd, ConversionFlags flags);

ConversionResult ConvertUTF8toUTF32 (
	const UTF8** sourceStart, const UTF8* sourceEnd,
	UTF32** targetStart, UTF32* targetEnd, ConversionFlags flags);

ConversionResult ConvertUTF32toUTF8 (
	const UTF32** sourceStart, const UTF32* sourceEnd,
	UTF8** targetStart, UTF8* targetEnd, ConversionFlags flags);

ConversionResult ConvertUTF16toUTF32 (
	const UTF16** sourceStart, const UTF16* sourceEnd,
	UTF32** targetStart, UTF32* targetEnd, ConversionFlags flags);

ConversionResult ConvertUTF32toUTF16 (
	const UTF32** sourceStart, const UTF32* sourceEnd,
	UTF16** targetStart, UTF16* targetEnd, ConversionFlags flags);

Boolean isLegalUTF8Sequence(const UTF8 *source, const UTF8 *sourceEnd);


/* --------------------------------------------------------------------- */

/* JSON_parser.h */


/*
 * Copyright 2001-2004 Unicode, Inc.
 *
 * Disclaimer
 *
 * This source code is provided as is by Unicode, Inc. No claims are
 * made as to fitness for any particular purpose. No warranties of any
 * kind are expressed or implied. The recipient agrees to determine
 * applicability of information provided. If this file has been
 * purchased on magnetic or optical media from Unicode, Inc., the
 * sole remedy for any claim will be exchange of defective media
 * within 90 days of receipt.
 *
 * Limitations on Rights to Redistribute This Code
 *
 * Unicode, Inc. hereby grants the right to freely use the information
 * supplied in this file in the creation of products supporting the
 * Unicode Standard, and to make copies of this file in any form
 * for internal or external distribution as long as this notice
 * remains attached.
 */

/* ---------------------------------------------------------------------

   Conversions between UTF32, UTF-16, and UTF-8. Source code file.
   Author: Mark E. Davis, 1994.
   Rev History: Rick McGowan, fixes & updates May 2001.
   Sept 2001: fixed const & error conditions per
   mods suggested by S. Parent & A. Lillich.
   June 2002: Tim Dodd added detection and handling of incomplete
   source sequences, enhanced error detection, added casts
   to eliminate compiler warnings.
   July 2003: slight mods to back out aggressive FFFE detection.
   Jan 2004: updated switches in from-UTF8 conversions.
   Oct 2004: updated to use UNI_MAX_LEGAL_UTF32 in UTF-32 conversions.

   See the header file "ConvertUTF.h" for complete documentation.

   ------------------------------------------------------------------------ */


static const int halfShift  = 10; /* used for shifting by 10 bits */

static const UTF32 halfBase = 0x0010000UL;
static const UTF32 halfMask = 0x3FFUL;

#define UNI_SUR_HIGH_START  (UTF32)0xD800
#define UNI_SUR_HIGH_END    (UTF32)0xDBFF
#define UNI_SUR_LOW_START   (UTF32)0xDC00
#define UNI_SUR_LOW_END     (UTF32)0xDFFF

/* --------------------------------------------------------------------- */

ConversionResult ConvertUTF32toUTF16 (
	const UTF32** sourceStart, const UTF32* sourceEnd,
	UTF16** targetStart, UTF16* targetEnd, ConversionFlags flags) {
    ConversionResult result = conversionOK;
    const UTF32* source = *sourceStart;
    UTF16* target = *targetStart;
    while (source < sourceEnd) {
		UTF32 ch;
		if (target >= targetEnd) {
			result = targetExhausted; break;
		}
		ch = *source++;
		if (ch <= UNI_MAX_BMP) { /* Target is a character <= 0xFFFF */
			/* UTF-16 surrogate values are illegal in UTF-32; 0xffff or 0xfffe are both reserved values */
			if (ch >= UNI_SUR_HIGH_START && ch <= UNI_SUR_LOW_END) {
				if (flags == strictConversion) {
					--source; /* return to the illegal value itself */
					result = sourceIllegal;
					break;
				} else {
					*target++ = UNI_REPLACEMENT_CHAR;
				}
			} else {
				*target++ = (UTF16)ch; /* normal case */
			}
		} else if (ch > UNI_MAX_LEGAL_UTF32) {
			if (flags == strictConversion) {
				result = sourceIllegal;
			} else {
				*target++ = UNI_REPLACEMENT_CHAR;
			}
		} else {
			/* target is a character in range 0xFFFF - 0x10FFFF. */
			if (target + 1 >= targetEnd) {
				--source; /* Back up source pointer! */
				result = targetExhausted; break;
			}
			ch -= halfBase;
			*target++ = (UTF16)((ch >> halfShift) + UNI_SUR_HIGH_START);
			*target++ = (UTF16)((ch & halfMask) + UNI_SUR_LOW_START);
		}
    }
    *sourceStart = source;
    *targetStart = target;
    return result;
}

/* --------------------------------------------------------------------- */

ConversionResult ConvertUTF16toUTF32 (
	const UTF16** sourceStart, const UTF16* sourceEnd,
	UTF32** targetStart, UTF32* targetEnd, ConversionFlags flags) {
    ConversionResult result = conversionOK;
    const UTF16* source = *sourceStart;
    UTF32* target = *targetStart;
    UTF32 ch, ch2;
    while (source < sourceEnd) {
		const UTF16* oldSource = source; /*  In case we have to back up because of target overflow. */
		ch = *source++;
		/* If we have a surrogate pair, convert to UTF32 first. */
		if (ch >= UNI_SUR_HIGH_START && ch <= UNI_SUR_HIGH_END) {
			/* If the 16 bits following the high surrogate are in the source buffer... */
			if (source < sourceEnd) {
				ch2 = *source;
				/* If it's a low surrogate, convert to UTF32. */
				if (ch2 >= UNI_SUR_LOW_START && ch2 <= UNI_SUR_LOW_END) {
					ch = ((ch - UNI_SUR_HIGH_START) << halfShift)
						+ (ch2 - UNI_SUR_LOW_START) + halfBase;
					++source;
				} else if (flags == strictConversion) { /* it's an unpaired high surrogate */
					--source; /* return to the illegal value itself */
					result = sourceIllegal;
					break;
				}
			} else { /* We don't have the 16 bits following the high surrogate. */
				--source; /* return to the high surrogate */
				result = sourceExhausted;
				break;
			}
		} else if (flags == strictConversion) {
			/* UTF-16 surrogate values are illegal in UTF-32 */
			if (ch >= UNI_SUR_LOW_START && ch <= UNI_SUR_LOW_END) {
				--source; /* return to the illegal value itself */
				result = sourceIllegal;
				break;
			}
		}
		if (target >= targetEnd) {
			source = oldSource; /* Back up source pointer! */
			result = targetExhausted; break;
		}
		*target++ = ch;
    }
    *sourceStart = source;
    *targetStart = target;
#ifdef CVTUTF_DEBUG
	if (result == sourceIllegal) {
		fprintf(stderr, "ConvertUTF16toUTF32 illegal seq 0x%04x,%04x\n", ch, ch2);
		fflush(stderr);
	}
#endif
    return result;
}

/* --------------------------------------------------------------------- */

/*
 * Index into the table below with the first byte of a UTF-8 sequence to
 * get the number of trailing bytes that are supposed to follow it.
 * Note that *legal* UTF-8 values can't have 4 or 5-bytes. The table is
 * left as-is for anyone who may want to do such conversion, which was
 * allowed in earlier algorithms.
 */
static const char trailingBytesForUTF8[256] = {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, 3,3,3,3,3,3,3,3,4,4,4,4,5,5,5,5
};

/*
 * Magic values subtracted from a buffer value during UTF8 conversion.
 * This table contains as many values as there might be trailing bytes
 * in a UTF-8 sequence.
 */
static const UTF32 offsetsFromUTF8[6] = { 0x00000000UL, 0x00003080UL, 0x000E2080UL,
										  0x03C82080UL, 0xFA082080UL, 0x82082080UL };

/*
 * Once the bits are split out into bytes of UTF-8, this is a mask OR-ed
 * into the first byte, depending on how many bytes follow.  There are
 * as many entries in this table as there are UTF-8 sequence types.
 * (I.e., one byte sequence, two byte... etc.). Remember that sequencs
 * for *legal* UTF-8 will be 4 or fewer bytes total.
 */
static const UTF8 firstByteMark[7] = { 0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC };

/* --------------------------------------------------------------------- */

/* The interface converts a whole buffer to avoid function-call overhead.
 * Constants have been gathered. Loops & conditionals have been removed as
 * much as possible for efficiency, in favor of drop-through switches.
 * (See "Note A" at the bottom of the file for equivalent code.)
 * If your compiler supports it, the "isLegalUTF8" call can be turned
 * into an inline function.
 */

/* --------------------------------------------------------------------- */

ConversionResult ConvertUTF16toUTF8 (
	const UTF16** sourceStart, const UTF16* sourceEnd,
	UTF8** targetStart, UTF8* targetEnd, ConversionFlags flags) {
    ConversionResult result = conversionOK;
    const UTF16* source = *sourceStart;
    UTF8* target = *targetStart;
    while (source < sourceEnd) {
		UTF32 ch;
		unsigned short bytesToWrite = 0;
		const UTF32 byteMask = 0xBF;
		const UTF32 byteMark = 0x80;
		const UTF16* oldSource = source; /* In case we have to back up because of target overflow. */
		ch = *source++;
		/* If we have a surrogate pair, convert to UTF32 first. */
		if (ch >= UNI_SUR_HIGH_START && ch <= UNI_SUR_HIGH_END) {
			/* If the 16 bits following the high surrogate are in the source buffer... */
			if (source < sourceEnd) {
				UTF32 ch2 = *source;
				/* If it's a low surrogate, convert to UTF32. */
				if (ch2 >= UNI_SUR_LOW_START && ch2 <= UNI_SUR_LOW_END) {
					ch = ((ch - UNI_SUR_HIGH_START) << halfShift)
						+ (ch2 - UNI_SUR_LOW_START) + halfBase;
					++source;
				} else if (flags == strictConversion) { /* it's an unpaired high surrogate */
					--source; /* return to the illegal value itself */
					result = sourceIllegal;
					break;
				}
			} else { /* We don't have the 16 bits following the high surrogate. */
				--source; /* return to the high surrogate */
				result = sourceExhausted;
				break;
			}
		} else if (flags == strictConversion) {
			/* UTF-16 surrogate values are illegal in UTF-32 */
			if (ch >= UNI_SUR_LOW_START && ch <= UNI_SUR_LOW_END) {
				--source; /* return to the illegal value itself */
				result = sourceIllegal;
				break;
			}
		}
		/* Figure out how many bytes the result will require */
		if (ch < (UTF32)0x80) {	     bytesToWrite = 1;
		} else if (ch < (UTF32)0x800) {     bytesToWrite = 2;
		} else if (ch < (UTF32)0x10000) {   bytesToWrite = 3;
		} else if (ch < (UTF32)0x110000) {  bytesToWrite = 4;
		} else {			    bytesToWrite = 3;
			ch = UNI_REPLACEMENT_CHAR;
		}

		target += bytesToWrite;
		if (target > targetEnd) {
			source = oldSource; /* Back up source pointer! */
			target -= bytesToWrite; result = targetExhausted; break;
		}
		switch (bytesToWrite) { /* note: everything falls through. */
	    case 4: *--target = (UTF8)((ch | byteMark) & byteMask); ch >>= 6;
	    case 3: *--target = (UTF8)((ch | byteMark) & byteMask); ch >>= 6;
	    case 2: *--target = (UTF8)((ch | byteMark) & byteMask); ch >>= 6;
	    case 1: *--target =  (UTF8)(ch | firstByteMark[bytesToWrite]);
		}
		target += bytesToWrite;
    }
    *sourceStart = source;
    *targetStart = target;
    return result;
}

/* --------------------------------------------------------------------- */

/*
 * Utility routine to tell whether a sequence of bytes is legal UTF-8.
 * This must be called with the length pre-determined by the first byte.
 * If not calling this from ConvertUTF8to*, then the length can be set by:
 *  length = trailingBytesForUTF8[*source]+1;
 * and the sequence is illegal right away if there aren't that many bytes
 * available.
 * If presented with a length > 4, this returns false.  The Unicode
 * definition of UTF-8 goes up to 4-byte sequences.
 */

static Boolean isLegalUTF8(const UTF8 *source, int length) {
    UTF8 a;
    const UTF8 *srcptr = source+length;
    switch (length) {
    default: return SG_FALSE;
		/* Everything else falls through when "true"... */
    case 4: if ((a = (*--srcptr)) < 0x80 || a > 0xBF) return SG_FALSE;
    case 3: if ((a = (*--srcptr)) < 0x80 || a > 0xBF) return SG_FALSE;
    case 2: if ((a = (*--srcptr)) > 0xBF) return SG_FALSE;

		switch (*source) {
			/* no fall-through in this inner switch */
	    case 0xE0: if (a < 0xA0) return SG_FALSE; break;
	    case 0xED: if (a > 0x9F) return SG_FALSE; break;
	    case 0xF0: if (a < 0x90) return SG_FALSE; break;
	    case 0xF4: if (a > 0x8F) return SG_FALSE; break;
	    default:   if (a < 0x80) return SG_FALSE;
		}

    case 1: if (*source >= 0x80 && *source < 0xC2) return SG_FALSE;
    }
    if (*source > 0xF4) return SG_FALSE;
    return SG_TRUE;
}

/* --------------------------------------------------------------------- */

/*
 * Exported function to return whether a UTF-8 sequence is legal or not.
 * This is not used here; it's just exported.
 */
Boolean isLegalUTF8Sequence(const UTF8 *source, const UTF8 *sourceEnd) {
    int length = trailingBytesForUTF8[*source]+1;
    if (source+length > sourceEnd) {
		return SG_FALSE;
    }
    return isLegalUTF8(source, length);
}

/* --------------------------------------------------------------------- */

ConversionResult ConvertUTF8toUTF16 (
	const UTF8** sourceStart, const UTF8* sourceEnd,
	UTF16** targetStart, UTF16* targetEnd, ConversionFlags flags) {
    ConversionResult result = conversionOK;
    const UTF8* source = *sourceStart;
    UTF16* target = *targetStart;
    while (source < sourceEnd) {
		UTF32 ch = 0;
		unsigned short extraBytesToRead = trailingBytesForUTF8[*source];
		if (source + extraBytesToRead >= sourceEnd) {
			result = sourceExhausted; break;
		}
		/* Do this check whether lenient or strict */
		if (! isLegalUTF8(source, extraBytesToRead+1)) {
			result = sourceIllegal;
			break;
		}
		/*
		 * The cases all fall through. See "Note A" below.
		 */
		switch (extraBytesToRead) {
	    case 5: ch += *source++; ch <<= 6; /* remember, illegal UTF-8 */
	    case 4: ch += *source++; ch <<= 6; /* remember, illegal UTF-8 */
	    case 3: ch += *source++; ch <<= 6;
	    case 2: ch += *source++; ch <<= 6;
	    case 1: ch += *source++; ch <<= 6;
	    case 0: ch += *source++;
		}
		ch -= offsetsFromUTF8[extraBytesToRead];

		if (target >= targetEnd) {
			source -= (extraBytesToRead+1); /* Back up source pointer! */
			result = targetExhausted; break;
		}
		if (ch <= UNI_MAX_BMP) { /* Target is a character <= 0xFFFF */
			/* UTF-16 surrogate values are illegal in UTF-32 */
			if (ch >= UNI_SUR_HIGH_START && ch <= UNI_SUR_LOW_END) {
				if (flags == strictConversion) {
					source -= (extraBytesToRead+1); /* return to the illegal value itself */
					result = sourceIllegal;
					break;
				} else {
					*target++ = UNI_REPLACEMENT_CHAR;
				}
			} else {
				*target++ = (UTF16)ch; /* normal case */
			}
		} else if (ch > UNI_MAX_UTF16) {
			if (flags == strictConversion) {
				result = sourceIllegal;
				source -= (extraBytesToRead+1); /* return to the start */
				break; /* Bail out; shouldn't continue */
			} else {
				*target++ = UNI_REPLACEMENT_CHAR;
			}
		} else {
			/* target is a character in range 0xFFFF - 0x10FFFF. */
			if (target + 1 >= targetEnd) {
				source -= (extraBytesToRead+1); /* Back up source pointer! */
				result = targetExhausted; break;
			}
			ch -= halfBase;
			*target++ = (UTF16)((ch >> halfShift) + UNI_SUR_HIGH_START);
			*target++ = (UTF16)((ch & halfMask) + UNI_SUR_LOW_START);
		}
    }
    *sourceStart = source;
    *targetStart = target;
    return result;
}

/* --------------------------------------------------------------------- */

ConversionResult ConvertUTF32toUTF8 (
	const UTF32** sourceStart, const UTF32* sourceEnd,
	UTF8** targetStart, UTF8* targetEnd, ConversionFlags flags) {
    ConversionResult result = conversionOK;
    const UTF32* source = *sourceStart;
    UTF8* target = *targetStart;
    while (source < sourceEnd) {
		UTF32 ch;
		unsigned short bytesToWrite = 0;
		const UTF32 byteMask = 0xBF;
		const UTF32 byteMark = 0x80;
		ch = *source++;
		if (flags == strictConversion ) {
			/* UTF-16 surrogate values are illegal in UTF-32 */
			if (ch >= UNI_SUR_HIGH_START && ch <= UNI_SUR_LOW_END) {
				--source; /* return to the illegal value itself */
				result = sourceIllegal;
				break;
			}
		}
		/*
		 * Figure out how many bytes the result will require. Turn any
		 * illegally large UTF32 things (> Plane 17) into replacement chars.
		 */
		if (ch < (UTF32)0x80) {	     bytesToWrite = 1;
		} else if (ch < (UTF32)0x800) {     bytesToWrite = 2;
		} else if (ch < (UTF32)0x10000) {   bytesToWrite = 3;
		} else if (ch <= UNI_MAX_LEGAL_UTF32) {  bytesToWrite = 4;
		} else {			    bytesToWrite = 3;
			ch = UNI_REPLACEMENT_CHAR;
			result = sourceIllegal;
		}

		target += bytesToWrite;
		if (target > targetEnd) {
			--source; /* Back up source pointer! */
			target -= bytesToWrite; result = targetExhausted; break;
		}
		switch (bytesToWrite) { /* note: everything falls through. */
	    case 4: *--target = (UTF8)((ch | byteMark) & byteMask); ch >>= 6;
	    case 3: *--target = (UTF8)((ch | byteMark) & byteMask); ch >>= 6;
	    case 2: *--target = (UTF8)((ch | byteMark) & byteMask); ch >>= 6;
	    case 1: *--target = (UTF8) (ch | firstByteMark[bytesToWrite]);
		}
		target += bytesToWrite;
    }
    *sourceStart = source;
    *targetStart = target;
    return result;
}

/* --------------------------------------------------------------------- */

ConversionResult ConvertUTF8toUTF32 (
	const UTF8** sourceStart, const UTF8* sourceEnd,
	UTF32** targetStart, UTF32* targetEnd, ConversionFlags flags) {
    ConversionResult result = conversionOK;
    const UTF8* source = *sourceStart;
    UTF32* target = *targetStart;
    while (source < sourceEnd) {
		UTF32 ch = 0;
		unsigned short extraBytesToRead = trailingBytesForUTF8[*source];
		if (source + extraBytesToRead >= sourceEnd) {
			result = sourceExhausted; break;
		}
		/* Do this check whether lenient or strict */
		if (! isLegalUTF8(source, extraBytesToRead+1)) {
			result = sourceIllegal;
			break;
		}
		/*
		 * The cases all fall through. See "Note A" below.
		 */
		switch (extraBytesToRead) {
	    case 5: ch += *source++; ch <<= 6;
	    case 4: ch += *source++; ch <<= 6;
	    case 3: ch += *source++; ch <<= 6;
	    case 2: ch += *source++; ch <<= 6;
	    case 1: ch += *source++; ch <<= 6;
	    case 0: ch += *source++;
		}
		ch -= offsetsFromUTF8[extraBytesToRead];

		if (target >= targetEnd) {
			source -= (extraBytesToRead+1); /* Back up the source pointer! */
			result = targetExhausted; break;
		}
		if (ch <= UNI_MAX_LEGAL_UTF32) {
			/*
			 * UTF-16 surrogate values are illegal in UTF-32, and anything
			 * over Plane 17 (> 0x10FFFF) is illegal.
			 */
			if (ch >= UNI_SUR_HIGH_START && ch <= UNI_SUR_LOW_END) {
				if (flags == strictConversion) {
					source -= (extraBytesToRead+1); /* return to the illegal value itself */
					result = sourceIllegal;
					break;
				} else {
					*target++ = UNI_REPLACEMENT_CHAR;
				}
			} else {
				*target++ = ch;
			}
		} else { /* i.e., ch > UNI_MAX_LEGAL_UTF32 */
			result = sourceIllegal;
			*target++ = UNI_REPLACEMENT_CHAR;
		}
    }
    *sourceStart = source;
    *targetStart = target;
    return result;
}

/* ---------------------------------------------------------------------

   Note A.
   The fall-through switches in UTF-8 reading code save a
   temp variable, some decrements & conditionals.  The switches
   are equivalent to the following loop:
   {
   int tmpBytesToRead = extraBytesToRead+1;
   do {
   ch += *source++;
   --tmpBytesToRead;
   if (tmpBytesToRead) ch <<= 6;
   } while (tmpBytesToRead > 0);
   }
   In UTF-8 writing code, the switches on "bytesToWrite" are
   similarly unrolled loops.

   --------------------------------------------------------------------- */

/* JSON_parser.c */

/* 2007-08-24 */

/*
  Copyright (c) 2005 JSON.org

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  The Software shall be used for Good, not Evil.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

/*
  Callbacks, comments, Unicode handling by Jean Gressmann (jean@0x42.de), 2007-2008.

  For the added features the license above applies also.

  Changelog:

  2008/05/28
  - Made JSON_value structure ansi C compliant. This bug was report by
  trisk@acm.jhu.edu

  2008/05/20
  - Fixed bug reported by Charles.Kerr@noaa.gov where the switching
  from static to dynamic parse buffer did not copy the static parse
  buffer's content.
*/

#define __   -1     /* the universal error code */

#define JSON_PARSER_STACK_SIZE 128
#define JSON_PARSER_PARSE_BUFFER_SIZE 3500

struct _SG_jsonparser
{
    SG_jsonparser_callback callback;
    void* pVoidCallbackData;
    signed char state, before_comment_state, type, escaped, comment;
    UTF16 utf16_decode_buffer[2];
    long top;
    signed char* stack;
    long stack_capacity;
    signed char static_stack[JSON_PARSER_STACK_SIZE];
    char* parse_buffer;
    size_t parse_buffer_capacity;
    size_t parse_buffer_count;
    size_t comment_begin_offset;
    char static_parse_buffer[JSON_PARSER_PARSE_BUFFER_SIZE];
};

#define COUNTOF(x) (sizeof(x)/sizeof(x[0]))

/*
  Characters are mapped into these 31 character classes. This allows for
  a significant reduction in the size of the state transition table.
*/

enum classes {
    C_SPACE,  /* space */
    C_WHITE,  /* other whitespace */
    C_LCURB,  /* {  */
    C_RCURB,  /* } */
    C_LSQRB,  /* [ */
    C_RSQRB,  /* ] */
    C_COLON,  /* : */
    C_COMMA,  /* , */
    C_QUOTE,  /* " */
    C_BACKS,  /* \ */
    C_SLASH,  /* / */
    C_PLUS,   /* + */
    C_MINUS,  /* - */
    C_POINT,  /* . */
    C_ZERO ,  /* 0 */
    C_DIGIT,  /* 123456789 */
    C_LOW_A,  /* a */
    C_LOW_B,  /* b */
    C_LOW_C,  /* c */
    C_LOW_D,  /* d */
    C_LOW_E,  /* e */
    C_LOW_F,  /* f */
    C_LOW_L,  /* l */
    C_LOW_N,  /* n */
    C_LOW_R,  /* r */
    C_LOW_S,  /* s */
    C_LOW_T,  /* t */
    C_LOW_U,  /* u */
    C_ABCDF,  /* ABCDF */
    C_E,      /* E */
    C_ETC,    /* everything else */
    C_STAR,   /* * */
    NR_CLASSES
};

static int ascii_class[128] = {
/*
  This array maps the 128 ASCII characters into character classes.
  The remaining Unicode characters should be mapped to C_ETC.
  Non-whitespace control characters are errors.
*/
    __,      __,      __,      __,      __,      __,      __,      __,
    __,      C_WHITE, C_WHITE, __,      __,      C_WHITE, __,      __,
    __,      __,      __,      __,      __,      __,      __,      __,
    __,      __,      __,      __,      __,      __,      __,      __,

    C_SPACE, C_ETC,   C_QUOTE, C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,
    C_ETC,   C_ETC,   C_STAR,   C_PLUS,  C_COMMA, C_MINUS, C_POINT, C_SLASH,
    C_ZERO,  C_DIGIT, C_DIGIT, C_DIGIT, C_DIGIT, C_DIGIT, C_DIGIT, C_DIGIT,
    C_DIGIT, C_DIGIT, C_COLON, C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,

    C_ETC,   C_ABCDF, C_ABCDF, C_ABCDF, C_ABCDF, C_E,     C_ABCDF, C_ETC,
    C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,
    C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,
    C_ETC,   C_ETC,   C_ETC,   C_LSQRB, C_BACKS, C_RSQRB, C_ETC,   C_ETC,

    C_ETC,   C_LOW_A, C_LOW_B, C_LOW_C, C_LOW_D, C_LOW_E, C_LOW_F, C_ETC,
    C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_LOW_L, C_ETC,   C_LOW_N, C_ETC,
    C_ETC,   C_ETC,   C_LOW_R, C_LOW_S, C_LOW_T, C_LOW_U, C_ETC,   C_ETC,
    C_ETC,   C_ETC,   C_ETC,   C_LCURB, C_ETC,   C_RCURB, C_ETC,   C_ETC
};


#if defined(IN)		// WinDef.h in PlatformSDK does "#define IN" for some odd reason.
#undef IN
#endif

/*
  The state codes.
*/
enum states {
    GO,  /* start    */
    OK,  /* ok       */
    OB,  /* object   */
    KE,  /* key      */
    CO,  /* colon    */
    VA,  /* value    */
    AR,  /* array    */
    ST,  /* string   */
    ES,  /* escape   */
    U1,  /* u1       */
    U2,  /* u2       */
    U3,  /* u3       */
    U4,  /* u4       */
    MI,  /* minus    */
    ZE,  /* zero     */
    IN,  /* integer  */
    FR,  /* fraction */
    E1,  /* e        */
    E2,  /* ex       */
    E3,  /* exp      */
    T1,  /* tr       */
    T2,  /* tru      */
    T3,  /* true     */
    F1,  /* fa       */
    F2,  /* fal      */
    F3,  /* fals     */
    F4,  /* false    */
    N1,  /* nu       */
    N2,  /* nul      */
    N3,  /* null     */
    C1,  /* /        */
    C2,  /* / *     */
    C3,  /* *        */
    FX,  /* *.* *eE* */
    D1,  /* second UTF-16 character decoding started by \ */
    D2,  /* second UTF-16 character proceeded by u */
    NR_STATES
};

enum actions
{
    CB = -10, /* comment begin */
    CE = -11, /* comment end */
    FA = -12, /* false */
    TR = -13, /* false */
    NU = -14, /* null */
    DE = -15, /* double detected by exponent e E */
    DF = -16, /* double detected by fraction . */
    SB = -17, /* string begin */
    MX = -18, /* integer detected by minus */
    ZX = -19, /* integer detected by zero */
    IX = -20, /* integer detected by 1-9 */
    EX = -21, /* next char is escaped */
    UC = -22, /* Unicode character read */
};


static int state_transition_table[NR_STATES][NR_CLASSES] = {
/*
  The state transition table takes the current state and the current symbol,
  and returns either a new state or an action. An action is represented as a
  negative number. A JSON text is accepted if at the end of the text the
  state is OK and if the mode is MODE_DONE.

  white                                      1-9                                   ABCDF  etc
  space |  {  }  [  ]  :  ,  "  \  /  +  -  .  0  |  a  b  c  d  e  f  l  n  r  s  t  u  |  E  |  * */
	/*start  GO*/ {GO,GO,-6,__,-5,__,__,__,__,__,CB,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__},
	/*ok     OK*/ {OK,OK,__,-8,__,-7,__,-3,__,__,CB,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__},
	/*object OB*/ {OB,OB,__,-9,__,__,__,__,SB,__,CB,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__},
	/*key    KE*/ {KE,KE,__,__,__,__,__,__,SB,__,CB,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__},
	/*colon  CO*/ {CO,CO,__,__,__,__,-2,__,__,__,CB,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__},
	/*value  VA*/ {VA,VA,-6,__,-5,__,__,__,SB,__,CB,__,MX,__,ZX,IX,__,__,__,__,__,FA,__,NU,__,__,TR,__,__,__,__,__},
	/*array  AR*/ {AR,AR,-6,__,-5,-7,__,__,SB,__,CB,__,MX,__,ZX,IX,__,__,__,__,__,FA,__,NU,__,__,TR,__,__,__,__,__},
	/*string ST*/ {ST,__,ST,ST,ST,ST,ST,ST,-4,EX,ST,ST,ST,ST,ST,ST,ST,ST,ST,ST,ST,ST,ST,ST,ST,ST,ST,ST,ST,ST,ST,ST},
	/*escape ES*/ {__,__,__,__,__,__,__,__,ST,ST,ST,__,__,__,__,__,__,ST,__,__,__,ST,__,ST,ST,__,ST,U1,__,__,__,__},
	/*u1     U1*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,U2,U2,U2,U2,U2,U2,U2,U2,__,__,__,__,__,__,U2,U2,__,__},
	/*u2     U2*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,U3,U3,U3,U3,U3,U3,U3,U3,__,__,__,__,__,__,U3,U3,__,__},
	/*u3     U3*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,U4,U4,U4,U4,U4,U4,U4,U4,__,__,__,__,__,__,U4,U4,__,__},
	/*u4     U4*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,UC,UC,UC,UC,UC,UC,UC,UC,__,__,__,__,__,__,UC,UC,__,__},
	/*minus  MI*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,ZE,IN,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__},
	/*zero   ZE*/ {OK,OK,__,-8,__,-7,__,-3,__,__,CB,__,__,DF,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__},
	/*int    IN*/ {OK,OK,__,-8,__,-7,__,-3,__,__,CB,__,__,DF,IN,IN,__,__,__,__,DE,__,__,__,__,__,__,__,__,DE,__,__},
	/*frac   FR*/ {OK,OK,__,-8,__,-7,__,-3,__,__,CB,__,__,__,FR,FR,__,__,__,__,E1,__,__,__,__,__,__,__,__,E1,__,__},
	/*e      E1*/ {__,__,__,__,__,__,__,__,__,__,__,E2,E2,__,E3,E3,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__},
	/*ex     E2*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,E3,E3,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__},
	/*exp    E3*/ {OK,OK,__,-8,__,-7,__,-3,__,__,__,__,__,__,E3,E3,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__},
	/*tr     T1*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,T2,__,__,__,__,__,__,__},
	/*tru    T2*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,T3,__,__,__,__},
	/*true   T3*/ {__,__,__,__,__,__,__,__,__,__,CB,__,__,__,__,__,__,__,__,__,OK,__,__,__,__,__,__,__,__,__,__,__},
	/*fa     F1*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,F2,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__},
	/*fal    F2*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,F3,__,__,__,__,__,__,__,__,__},
	/*fals   F3*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,F4,__,__,__,__,__,__},
	/*false  F4*/ {__,__,__,__,__,__,__,__,__,__,CB,__,__,__,__,__,__,__,__,__,OK,__,__,__,__,__,__,__,__,__,__,__},
	/*nu     N1*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,N2,__,__,__,__},
	/*nul    N2*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,N3,__,__,__,__,__,__,__,__,__},
	/*null   N3*/ {__,__,__,__,__,__,__,__,__,__,CB,__,__,__,__,__,__,__,__,__,__,__,OK,__,__,__,__,__,__,__,__,__},
	/*/      C1*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,C2},
	/*/*     C2*/ {C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C3},
	/**      C3*/ {C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,CE,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C2,C3},
	/*_.     FX*/ {OK,OK,__,-8,__,-7,__,-3,__,__,__,__,__,__,FR,FR,__,__,__,__,E1,__,__,__,__,__,__,__,__,E1,__,__},
	/*\      D1*/ {__,__,__,__,__,__,__,__,__,D2,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__},
	/*\      D2*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,U1,__,__,__,__},
};


/*
  These modes can be pushed on the stack.
*/
enum modes {
    MODE_ARRAY = 1,
    MODE_DONE = 2,
    MODE_KEY = 3,
    MODE_OBJECT = 4
};

static int push(SG_jsonparser* jc, int mode)
{
/*
  Push a mode onto the stack. return SG_FALSE if there is overflow.
*/
    jc->top += 1;
	if (jc->top >= JSON_PARSER_STACK_SIZE)
	{
		return SG_FALSE;
	}

    jc->stack[jc->top] = (signed char)mode;	// cast for VS2005
    return SG_TRUE;
}


static int pop(SG_jsonparser* jc, int mode)
{
/*
  Pop the stack, assuring that the current mode matches the expectation.
  return SG_FALSE if there is underflow or if the modes mismatch.
*/
    if (jc->top < 0 || jc->stack[jc->top] != mode)
	{
        return SG_FALSE;
    }
    jc->top -= 1;
    return SG_TRUE;
}


#define parse_buffer_clear(jc)					\
    do {										\
        jc->parse_buffer_count = 0;				\
        jc->parse_buffer[0] = 0;				\
    } while (0)

#define parse_buffer_pop_back_char(jc)					\
    do {												\
        SG_ASSERT(jc->parse_buffer_count >= 1);			\
        --jc->parse_buffer_count;						\
        jc->parse_buffer[jc->parse_buffer_count] = 0;	\
    } while (0)

void SG_jsonparser__free(SG_context * pCtx, SG_jsonparser* jc)
{
    if (jc)
	{
        if (jc->stack != &jc->static_stack[0])
		{
            SG_NULLFREE(pCtx, jc->stack);
        }
        if (jc->parse_buffer != &jc->static_parse_buffer[0])
		{
            SG_NULLFREE(pCtx, jc->parse_buffer);
        }
        SG_NULLFREE(pCtx, jc);
	}
}


/*
  SG_jsonparser__alloc starts the checking process by constructing a JSON_parser
  object.

  To continue the process, call JSON_parser_char for each character in the
  JSON text, and then call JSON_parser_done to obtain the final result.
  These functions are fully reentrant.
*/
void SG_jsonparser__alloc(SG_context * pCtx,
						  SG_jsonparser** ppNew,
						  SG_jsonparser_callback callback, void* pVoidCallbackData)
{
    SG_jsonparser* jc = NULL;

	SG_ERR_CHECK_RETURN(  SG_alloc1(pCtx,jc)  );

    jc->state = GO;
    jc->top = -1;

	jc->stack_capacity = COUNTOF(jc->static_stack);
	jc->stack = &jc->static_stack[0];

    /* set parser to start */
    push(jc, MODE_DONE);

    /* set up the parse buffer */
    jc->parse_buffer = &jc->static_parse_buffer[0];
    jc->parse_buffer_capacity = COUNTOF(jc->static_parse_buffer);
    parse_buffer_clear(jc);

    /* set up callback, comment & float handling */
    jc->callback = callback;
    jc->pVoidCallbackData = pVoidCallbackData;

	*ppNew = jc;
}

static void grow_parse_buffer(SG_context * pCtx, SG_jsonparser* jc)
{
	char* old_parse_buffer = jc->parse_buffer;
    jc->parse_buffer_capacity *= 2;

	// TODO convert this to SG_alloc()
	jc->parse_buffer = (char*)SG_malloc((SG_uint32)jc->parse_buffer_capacity);	// SG_uint32 cast for VS2005
	memcpy(jc->parse_buffer, old_parse_buffer, jc->parse_buffer_count);

    if (old_parse_buffer != &jc->static_parse_buffer[0])
	{
		SG_NULLFREE(pCtx, old_parse_buffer);
    }
}

#define parse_buffer_push_back_char(pCtx, jc, c)						\
    do {																\
        if (jc->parse_buffer_count + 1 >= jc->parse_buffer_capacity) grow_parse_buffer(pCtx, jc); \
        jc->parse_buffer[jc->parse_buffer_count++] = c;					\
        jc->parse_buffer[jc->parse_buffer_count] = 0;					\
    } while (0)


static void parse_parse_buffer(SG_context * pCtx, SG_jsonparser* jc)
{
    if (jc->callback)
	{
        int result = 1;
        SG_jsonparser_value value, *arg = NULL;

        if (jc->type != SG_JSONPARSER_TYPE_NONE)
		{
            SG_ASSERT(
                jc->type == SG_JSONPARSER_TYPE_NULL ||
                jc->type == SG_JSONPARSER_TYPE_FALSE ||
                jc->type == SG_JSONPARSER_TYPE_TRUE ||
                jc->type == SG_JSONPARSER_TYPE_FLOAT ||
                jc->type == SG_JSONPARSER_TYPE_INTEGER ||
                jc->type == SG_JSONPARSER_TYPE_STRING);


            switch(jc->type)
			{
			case SG_JSONPARSER_TYPE_FLOAT:
				arg = &value;
#if defined(WINDOWS)
				result = sscanf_s(jc->parse_buffer, "%Lf", &value.vu.float_value);
#else
				result = sscanf(jc->parse_buffer, "%Lf", &value.vu.float_value);
#endif
				break;
			case SG_JSONPARSER_TYPE_INTEGER:
				arg = &value;
				SG_ERR_CHECK_RETURN(  SG_int64__parse__stop_on_nondigit(pCtx, &value.vu.integer_value, jc->parse_buffer)  );
				break;
			case SG_JSONPARSER_TYPE_STRING:
				arg = &value;
				value.vu.str.value = jc->parse_buffer;
				value.vu.str.length = jc->parse_buffer_count;
				break;
            }

			SG_ERR_CHECK_RETURN(  (*jc->callback)(pCtx, jc->pVoidCallbackData, jc->type, arg)  );
        }
    }

    parse_buffer_clear(jc);
}

static int decode_unicode_char(SG_jsonparser* jc)
{
    const unsigned chars = jc->utf16_decode_buffer[0] ? 2 : 1;
    int i;
    UTF16 *uc = chars == 1 ? &jc->utf16_decode_buffer[0] : &jc->utf16_decode_buffer[1];
    UTF16 x;
    char* p;

    SG_ASSERT(jc->parse_buffer_count >= 6);

    p = &jc->parse_buffer[jc->parse_buffer_count - 4];

    for (i = 0; i < 4; ++i, ++p)
	{
        x = *p;

        if (x >= 'a') {
            x -= ('a' - 10);
        } else if (x >= 'A') {
            x -= ('A' - 10);
        } else {
            x &= ~((UTF16) 0x30);
        }

        SG_ASSERT(x < 16);

        *uc |= x << ((3u - i) << 2);
    }

    /* clear UTF-16 char form buffer */
    jc->parse_buffer_count -= 6;
    jc->parse_buffer[jc->parse_buffer_count] = 0;

    /* attempt decoding ... */
    {
        UTF8* dec_start = (UTF8*)&jc->parse_buffer[jc->parse_buffer_count];
        UTF8* dec_start_dup = dec_start;
        UTF8* dec_end = dec_start + 6;

        const UTF16* enc_start = &jc->utf16_decode_buffer[0];
        const UTF16* enc_end = enc_start + chars;

        const ConversionResult result = ConvertUTF16toUTF8(
            &enc_start, enc_end, &dec_start, dec_end, strictConversion);

        const size_t new_chars = dec_start - dec_start_dup;

        /* was it a surrogate UTF-16 char? */
        if (chars == 1 && result == sourceExhausted) {
            return SG_TRUE;
        }

        if (result != conversionOK) {
            return SG_FALSE;
        }

        /* NOTE: clear decode buffer to resume string reading,
           otherwise we continue to read UTF-16 */
        jc->utf16_decode_buffer[0] = 0;

        SG_ASSERT(new_chars <= 6);

        jc->parse_buffer_count += new_chars;
        jc->parse_buffer[jc->parse_buffer_count] = 0;
    }

    return SG_TRUE;
}


/*
  After calling new_JSON_parser, call this function for each character (or
  partial character) in your JSON text. It can accept UTF-8, UTF-16, or
  UTF-32. It returns true if things are looking ok so far. If it rejects the
  text, it returns false.
*/
void SG_jsonparser__chars(SG_context * pCtx, SG_jsonparser* jc, const char* psz)
{
    int next_class, next_state;
    const unsigned char* p = NULL;

    p = (const unsigned char*) psz;
    while (*p)
    {
        int next_char = (int) *p++;
    /*
      Determine the character's class.
    */
        if (next_char < 0)
        {
            SG_ERR_THROW_RETURN( SG_ERR_JSONPARSER_SYNTAX );
        }
        if (next_char >= 128)
        {
            next_class = C_ETC;
        }
        else
        {
            next_class = ascii_class[next_char];
            if (next_class <= __)
            {
                SG_ERR_THROW_RETURN( SG_ERR_JSONPARSER_SYNTAX );
            }
        }

        if (jc->escaped)
        {
            jc->escaped = 0;
            /* remove the backslash */
            parse_buffer_pop_back_char(jc);
            switch(next_char)
            {
            case 'b':
                parse_buffer_push_back_char(pCtx, jc, '\b');
                break;
            case 'f':
                parse_buffer_push_back_char(pCtx, jc, '\f');
                break;
            case 'n':
                parse_buffer_push_back_char(pCtx, jc, '\n');
                break;
            case 'r':
                parse_buffer_push_back_char(pCtx, jc, '\r');
                break;
            case 't':
                parse_buffer_push_back_char(pCtx, jc, '\t');
                break;
            case '"':
                parse_buffer_push_back_char(pCtx, jc, '"');
                break;
            case '\\':
                parse_buffer_push_back_char(pCtx, jc, '\\');
                break;
            case '/':
                parse_buffer_push_back_char(pCtx, jc, '/');
                break;
            case 'u':
                parse_buffer_push_back_char(pCtx, jc, '\\');
                parse_buffer_push_back_char(pCtx, jc, 'u');
                break;
            default:
                SG_ERR_THROW_RETURN( SG_ERR_JSONPARSER_SYNTAX );
            }
        }
        else if (!jc->comment)
        {
            if (jc->type != SG_JSONPARSER_TYPE_NONE || !(next_class == C_SPACE || next_class == C_WHITE) /* non-white-space */)
            {
                parse_buffer_push_back_char(pCtx, jc, (char)next_char);
            }
        }



    /*
      Get the next state from the state transition table.
    */
        next_state = state_transition_table[jc->state][next_class];
        if (next_state >= 0)
        {
    /*
      Change the state.
    */
            jc->state = (signed char)next_state;		// cast for VS2005
        }
        else
        {
    /*
      Or perform one of the actions.
    */
            switch (next_state)
            {
    /* Unicode character */
            case UC:
                if(!decode_unicode_char(jc))
                {
                    SG_ERR_THROW_RETURN( SG_ERR_JSONPARSER_SYNTAX );
                }
                /* check if we need to read a second UTF-16 char */
                if (jc->utf16_decode_buffer[0])
                {
                    jc->state = D1;
                }
                else
                {
                    jc->state = ST;
                }
                break;
    /* escaped char */
            case EX:
                jc->escaped = 1;
                jc->state = ES;
                break;
    /* integer detected by minus */
            case MX:
                jc->type = SG_JSONPARSER_TYPE_INTEGER;
                jc->state = MI;
                break;
    /* integer detected by zero */
            case ZX:
                jc->type = SG_JSONPARSER_TYPE_INTEGER;
                jc->state = ZE;
                break;
    /* integer detected by 1-9 */
            case IX:
                jc->type = SG_JSONPARSER_TYPE_INTEGER;
                jc->state = IN;
                break;

    /* floating point number detected by exponent*/
            case DE:
                SG_ASSERT(jc->type != SG_JSONPARSER_TYPE_FALSE);
                SG_ASSERT(jc->type != SG_JSONPARSER_TYPE_TRUE);
                SG_ASSERT(jc->type != SG_JSONPARSER_TYPE_NULL);
                SG_ASSERT(jc->type != SG_JSONPARSER_TYPE_STRING);
                jc->type = SG_JSONPARSER_TYPE_FLOAT;
                jc->state = E1;
                break;

    /* floating point number detected by fraction */
            case DF:
                SG_ASSERT(jc->type != SG_JSONPARSER_TYPE_FALSE);
                SG_ASSERT(jc->type != SG_JSONPARSER_TYPE_TRUE);
                SG_ASSERT(jc->type != SG_JSONPARSER_TYPE_NULL);
                SG_ASSERT(jc->type != SG_JSONPARSER_TYPE_STRING);
                jc->type = SG_JSONPARSER_TYPE_FLOAT;
                jc->state = FX;
                break;
    /* string begin " */
            case SB:
                parse_buffer_clear(jc);
                SG_ASSERT(jc->type == SG_JSONPARSER_TYPE_NONE);
                jc->type = SG_JSONPARSER_TYPE_STRING;
                jc->state = ST;
                break;

    /* n */
            case NU:
                SG_ASSERT(jc->type == SG_JSONPARSER_TYPE_NONE);
                jc->type = SG_JSONPARSER_TYPE_NULL;
                jc->state = N1;
                break;
    /* f */
            case FA:
                SG_ASSERT(jc->type == SG_JSONPARSER_TYPE_NONE);
                jc->type = SG_JSONPARSER_TYPE_FALSE;
                jc->state = F1;
                break;
    /* t */
            case TR:
                SG_ASSERT(jc->type == SG_JSONPARSER_TYPE_NONE);
                jc->type = SG_JSONPARSER_TYPE_TRUE;
                jc->state = T1;
                break;

    /* closing comment */
            case CE:
                jc->comment = 0;
                SG_ASSERT(jc->parse_buffer_count == 0);
                SG_ASSERT(jc->type == SG_JSONPARSER_TYPE_NONE);
                jc->state = jc->before_comment_state;
                break;

    /* opening comment  */
            case CB:
                parse_buffer_pop_back_char(jc);
                SG_ERR_CHECK_RETURN(  parse_parse_buffer(pCtx, jc)  );
                SG_ASSERT(jc->parse_buffer_count == 0);
                SG_ASSERT(jc->type != SG_JSONPARSER_TYPE_STRING);
                switch (jc->stack[jc->top]) {
                case MODE_ARRAY:
                case MODE_OBJECT:
                    switch(jc->state) {
                    case VA:
                    case AR:
                        jc->before_comment_state = jc->state;
                        break;
                    default:
                        jc->before_comment_state = OK;
                        break;
                    }
                    break;
                default:
                    jc->before_comment_state = jc->state;
                    break;
                }
                jc->type = SG_JSONPARSER_TYPE_NONE;
                jc->state = C1;
                jc->comment = 1;
                break;
    /* empty } */
            case -9:
                parse_buffer_clear(jc);
                if (jc->callback)
                {
                    SG_ERR_CHECK_RETURN(  (*jc->callback)(pCtx, jc->pVoidCallbackData, SG_JSONPARSER_TYPE_OBJECT_END, NULL)  );
                }
                if (!pop(jc, MODE_KEY))
                {
                    SG_ERR_THROW_RETURN( SG_ERR_JSONPARSER_SYNTAX );
                }
                jc->state = OK;
                break;

                /* } */ case -8:
                parse_buffer_pop_back_char(jc);
                SG_ERR_CHECK_RETURN( parse_parse_buffer(pCtx, jc) );
                if (jc->callback)
                {
                    SG_ERR_CHECK_RETURN( (*jc->callback)(pCtx, jc->pVoidCallbackData, SG_JSONPARSER_TYPE_OBJECT_END, NULL) );
                }
                if (!pop(jc, MODE_OBJECT))
                {
                    SG_ERR_THROW_RETURN( SG_ERR_JSONPARSER_SYNTAX );
                }
                jc->type = SG_JSONPARSER_TYPE_NONE;
                jc->state = OK;
                break;

                /* ] */ case -7:
                parse_buffer_pop_back_char(jc);
                SG_ERR_CHECK_RETURN( parse_parse_buffer(pCtx, jc) );
                if (jc->callback)
                {
                    SG_ERR_CHECK_RETURN( (*jc->callback)(pCtx, jc->pVoidCallbackData, SG_JSONPARSER_TYPE_ARRAY_END, NULL) );
                }
                if (!pop(jc, MODE_ARRAY))
                {
                    SG_ERR_THROW_RETURN( SG_ERR_JSONPARSER_SYNTAX );
                }

                jc->type = SG_JSONPARSER_TYPE_NONE;
                jc->state = OK;
                break;

                /* { */ case -6:
                parse_buffer_pop_back_char(jc);
                if (jc->callback)
                {
                    SG_ERR_CHECK_RETURN( (*jc->callback)(pCtx, jc->pVoidCallbackData, SG_JSONPARSER_TYPE_OBJECT_BEGIN, NULL) );
                }
                if (!push(jc, MODE_KEY))
                {
                    SG_ERR_THROW_RETURN( SG_ERR_JSONPARSER_SYNTAX );
                }
                SG_ASSERT(jc->type == SG_JSONPARSER_TYPE_NONE);
                jc->state = OB;
                break;

                /* [ */ case -5:
                parse_buffer_pop_back_char(jc);
                if (jc->callback)
                {
                    SG_ERR_CHECK_RETURN( (*jc->callback)(pCtx, jc->pVoidCallbackData, SG_JSONPARSER_TYPE_ARRAY_BEGIN, NULL) );
                }
                if (!push(jc, MODE_ARRAY))
                {
                    SG_ERR_THROW_RETURN( SG_ERR_JSONPARSER_SYNTAX );
                }
                SG_ASSERT(jc->type == SG_JSONPARSER_TYPE_NONE);
                jc->state = AR;
                break;

                /* string end " */ case -4:
                parse_buffer_pop_back_char(jc);
                switch (jc->stack[jc->top])
                {
                case MODE_KEY:
                    SG_ASSERT(jc->type == SG_JSONPARSER_TYPE_STRING);
                    jc->type = SG_JSONPARSER_TYPE_NONE;
                    jc->state = CO;

                    if (jc->callback)
                    {
                        SG_jsonparser_value value;

                        value.vu.str.value = jc->parse_buffer;
                        value.vu.str.length = jc->parse_buffer_count;

                        SG_ERR_CHECK_RETURN( (*jc->callback)(pCtx, jc->pVoidCallbackData, SG_JSONPARSER_TYPE_KEY, &value) );
                    }
                    parse_buffer_clear(jc);
                    break;
                case MODE_ARRAY:
                case MODE_OBJECT:
                    SG_ASSERT(jc->type == SG_JSONPARSER_TYPE_STRING);
                    SG_ERR_CHECK_RETURN( parse_parse_buffer(pCtx, jc) );
                    jc->type = SG_JSONPARSER_TYPE_NONE;
                    jc->state = OK;
                    break;
                default:
                    SG_ERR_THROW_RETURN( SG_ERR_JSONPARSER_SYNTAX );
                }
                break;

                /* , */ case -3:
                parse_buffer_pop_back_char(jc);
                SG_ERR_CHECK_RETURN( parse_parse_buffer(pCtx, jc) );
                switch (jc->stack[jc->top]) {
                case MODE_OBJECT:
    /*
      A comma causes a flip from object mode to key mode.
    */
                    if (!pop(jc, MODE_OBJECT) || !push(jc, MODE_KEY)) {
                        SG_ERR_THROW_RETURN( SG_ERR_JSONPARSER_SYNTAX );
                    }
                    SG_ASSERT(jc->type != SG_JSONPARSER_TYPE_STRING);
                    jc->type = SG_JSONPARSER_TYPE_NONE;
                    jc->state = KE;
                    break;
                case MODE_ARRAY:
                    SG_ASSERT(jc->type != SG_JSONPARSER_TYPE_STRING);
                    jc->type = SG_JSONPARSER_TYPE_NONE;
                    jc->state = VA;
                    break;
                default:
                    SG_ERR_THROW_RETURN( SG_ERR_JSONPARSER_SYNTAX );
                }
                break;

                /* : */ case -2:
    /*
      A colon causes a flip from key mode to object mode.
    */
                parse_buffer_pop_back_char(jc);
                if (!pop(jc, MODE_KEY) || !push(jc, MODE_OBJECT)) {
                    SG_ERR_THROW_RETURN( SG_ERR_JSONPARSER_SYNTAX );
                }
                SG_ASSERT(jc->type == SG_JSONPARSER_TYPE_NONE);
                jc->state = VA;
                break;
    /*
      Bad action.
    */
            default:
                SG_ERR_THROW_RETURN( SG_ERR_JSONPARSER_SYNTAX );
            }
        }
    }
}


void SG_jsonparser__done(SG_context * pCtx, SG_jsonparser* jc)
{
    const int result = jc->state == OK && pop(jc, MODE_DONE);

	if (!result)
		SG_ERR_THROW_RETURN( SG_ERR_UNSPECIFIED );
}
