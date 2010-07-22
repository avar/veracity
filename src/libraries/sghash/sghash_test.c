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
 * @file sghash_test.c
 *
 * @details
 *
 */

//////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sg_defines.h>
#include <sg_stdint.h>
#include <sg_error_typedefs.h>

#include "sghash.h"
#include "sghash__private.h"

//////////////////////////////////////////////////////////////////

void try_one__sz(const char * pszHashMethod, const char * pszInput, const char * pszOutput)
{
	SG_error err;
	SGHASH_handle * pHandle = NULL;
	char pBufResult[1000];

	err = SGHASH_init(pszHashMethod, &pHandle);
	if (err != SG_ERR_OK)
	{
		fprintf(stderr,"SGHASH_init(%s) failed\n",pszHashMethod);
		goto fail;
	}
	err = SGHASH_update(pHandle, (const unsigned char *)pszInput, strlen(pszInput));
	if (err != SG_ERR_OK)
	{
		fprintf(stderr,"SGHASH_update(%s,%s) failed\n",pszHashMethod,pszInput);
		goto fail;
	}
	err = SGHASH_final(&pHandle,pBufResult,sizeof(pBufResult));
	if (err != SG_ERR_OK)
	{
		fprintf(stderr,"SGHASH_final(%s,%s) failed\n",pszHashMethod,pszInput);
		goto fail;
	}
	if (strcmp(pBufResult,pszOutput) != 0)
	{
		fprintf(stderr,("SGHASH(%s,%s) result does not match:\n"
						"       [received %s]\n"
						"       [expected %s]\n"),
				pszHashMethod,pszInput,
				pBufResult,
				pszOutput);
		goto fail;
	}

	return;

fail:
	if (pHandle)
		free(pHandle);
	exit(1);
}

void try_one__raw(const char * pszHashMethod, const SG_byte * pRawInput, SG_uint32 lenRawInput, const char * pszOutput)
{
	SG_error err;
	SGHASH_handle * pHandle = NULL;
	char pBufResult[1000];

	err = SGHASH_init(pszHashMethod, &pHandle);
	if (err != SG_ERR_OK)
	{
		fprintf(stderr,"SGHASH_init(%s) failed\n",pszHashMethod);
		goto fail;
	}
	err = SGHASH_update(pHandle, (const unsigned char *)pRawInput, lenRawInput);
	if (err != SG_ERR_OK)
	{
		SG_uint32 k;

		fprintf(stderr,"SGHASH_update(%s,",pszHashMethod);
		for (k=0; k<lenRawInput; k++)
			fprintf(stderr,"%02x",pRawInput[k]);
		fprintf(stderr,") failed\n");
		goto fail;
	}
	err = SGHASH_final(&pHandle,pBufResult,sizeof(pBufResult));
	if (err != SG_ERR_OK)
	{
		SG_uint32 k;

		fprintf(stderr,"SGHASH_final(%s,",pszHashMethod);
		for (k=0; k<lenRawInput; k++)
			fprintf(stderr,"%02x",pRawInput[k]);
		fprintf(stderr,") failed\n");
		goto fail;
	}
	if (strcmp(pBufResult,pszOutput) != 0)
	{
		SG_uint32 k;

		fprintf(stderr,"SGHASH(%s,",pszHashMethod);
		for (k=0; k<lenRawInput; k++)
			fprintf(stderr,"%02x",pRawInput[k]);
		fprintf(stderr,(") result does not match:\n"
						"       [received %s]\n"
						"       [expected %s]\n"),
				pBufResult,
				pszOutput);
		goto fail;
	}

	return;

fail:
	if (pHandle)
		free(pHandle);
	exit(1);
}

int main(int argc, char ** argv)
{
	if (argc < 2)
	{
		fprintf(stderr,"Usage: sghash_test <hash-method>\n");
		exit(1);
	}

	if (strcmp(argv[1], "SHA1/160") == 0)
	{
		// (on the mac at least) we can generate these test values using:
		// echo -n "a" | shasum -a 1
		try_one__sz(argv[1], "a",
					"86f7e437faa5a7fce15d1ddcb9eaeaea377667b8");
		try_one__sz(argv[1],
					"abc",
					"a9993e364706816aba3e25717850c26c9cd0d89d");
		try_one__sz(argv[1],
					"abcdefg",
					"2fb5e13419fc89246865e7a324f476ec624e8740");
		try_one__sz(argv[1],
					"",
					"da39a3ee5e6b4b0d3255bfef95601890afd80709");
	}
	else if (strcmp(argv[1], "SHA2/256") == 0)
	{
		try_one__sz(argv[1],
					"a",
					"ca978112ca1bbdcafac231b39a23dc4da786eff8147c4e72b9807785afee48bb");
		try_one__sz(argv[1],
					"abc",
					"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
		try_one__sz(argv[1],
					"abcdefg",
					"7d1a54127b222502f5b79b5fb0803061152a44f92b37e23c6527baf665d4da9a");
		try_one__sz(argv[1],
					"",
					"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
	}
	else if (strcmp(argv[1], "SHA2/384") == 0)
	{
		try_one__sz(argv[1],
					"a",
					"54a59b9f22b0b80880d8427e548b7c23abd873486e1f035dce9cd697e85175033caa88e6d57bc35efae0b5afd3145f31");
		try_one__sz(argv[1],
					"abc",
					"cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed8086072ba1e7cc2358baeca134c825a7");
		try_one__sz(argv[1],
					"abcdefg",
					"9f11fc131123f844c1226f429b6a0a6af0525d9f40f056c7fc16cdf1b06bda08e302554417a59fa7dcf6247421959d22");
		try_one__sz(argv[1],
					"",
					"38b060a751ac96384cd9327eb1b1e36a21fdb71114be07434c0cc7bf63f6e1da274edebfe76f65fbd51ad2f14898b95b");
	}
	else if (strcmp(argv[1], "SHA2/512") == 0)
	{
		try_one__sz(argv[1],
					"a",
					"1f40fc92da241694750979ee6cf582f2d5d7d28e18335de05abc54d0560e0f5302860c652bf08d560252aa5e74210546f369fbbbce8c12cfc7957b2652fe9a75");
		try_one__sz(argv[1],
					"abc",
					"ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f");
		try_one__sz(argv[1],
					"abcdefg",
					"d716a4188569b68ab1b6dfac178e570114cdf0ea3a1cc0e31486c3e41241bc6a76424e8c37ab26f096fc85ef9886c8cb634187f4fddff645fb099f1ff54c6b8c");
		try_one__sz(argv[1],
					"",
					"cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e");
	}
	else if (strcmp(argv[1], "SKEIN/256") == 0)
	{
		// these are from Appendix C in skein1.2.pdf

		SG_byte b0[] = { 0xFF };
		SG_byte b1[] = { 0xFF, 0xFE, 0xFD, 0xFC, 0xFB, 0xFA, 0xF9, 0xF8, 0xF7, 0xF6, 0xF5, 0xF4, 0xF3, 0xF2, 0xF1, 0xF0,
						 0xEF, 0xEE, 0xED, 0xEC, 0xEB, 0xEA, 0xE9, 0xE8, 0xE7, 0xE6, 0xE5, 0xE4, 0xE3, 0xE2, 0xE1, 0xE0 };
		SG_byte b2[] = { 0xFF, 0xFE, 0xFD, 0xFC, 0xFB, 0xFA, 0xF9, 0xF8, 0xF7, 0xF6, 0xF5, 0xF4, 0xF3, 0xF2, 0xF1, 0xF0,
						 0xEF, 0xEE, 0xED, 0xEC, 0xEB, 0xEA, 0xE9, 0xE8, 0xE7, 0xE6, 0xE5, 0xE4, 0xE3, 0xE2, 0xE1, 0xE0,
						 0xDF, 0xDE, 0xDD, 0xDC, 0xDB, 0xDA, 0xD9, 0xD8, 0xD7, 0xD6, 0xD5, 0xD4, 0xD3, 0xD2, 0xD1, 0xD0,
						 0xCF, 0xCE, 0xCD, 0xCC, 0xCB, 0xCA, 0xC9, 0xC8, 0xC7, 0xC6, 0xC5, 0xC4, 0xC3, 0xC2, 0xC1, 0xC0 };

		try_one__raw(argv[1], b0, sizeof(b0), ("42c88237b63fc99c55098838a171d50b"
											   "fbffcaf340705c923bbb3745d14715e8"));

		try_one__raw(argv[1], b1, sizeof(b1), ("e33f483b1389ba9faeff25257e87cf76"
											   "008c35285e3bd562bdc1f3ea2a960635"));

		try_one__raw(argv[1], b2, sizeof(b2), ("90e50c4dcfc7490a09f3a1a79bf3b3df"
											   "21ea85447b0ff029c847d659856ec7a5"));

		// the Appendix did not give the answer for the zero-length input, so we computed it once and fixed the test.
		// (this is not as bogus as it sounds -- it does verify that we get the same answer on different platforms
		// and different endian machines.)

		try_one__raw(argv[1], NULL, 0, ("0b04103b828cddaebcf592ac845ecafd5887f61230a755406d38d85376e1ae08"));

	}
	else if (strcmp(argv[1], "SKEIN/512") == 0)
	{
		// these are from Appendix C in skein1.2.pdf

		SG_byte b0[] = { 0xFF };
		SG_byte b1[] = { 0xFF, 0xFE, 0xFD, 0xFC, 0xFB, 0xFA, 0xF9, 0xF8, 0xF7, 0xF6, 0xF5, 0xF4, 0xF3, 0xF2, 0xF1, 0xF0,
						 0xEF, 0xEE, 0xED, 0xEC, 0xEB, 0xEA, 0xE9, 0xE8, 0xE7, 0xE6, 0xE5, 0xE4, 0xE3, 0xE2, 0xE1, 0xE0,
						 0xDF, 0xDE, 0xDD, 0xDC, 0xDB, 0xDA, 0xD9, 0xD8, 0xD7, 0xD6, 0xD5, 0xD4, 0xD3, 0xD2, 0xD1, 0xD0,
						 0xCF, 0xCE, 0xCD, 0xCC, 0xCB, 0xCA, 0xC9, 0xC8, 0xC7, 0xC6, 0xC5, 0xC4, 0xC3, 0xC2, 0xC1, 0xC0 };
		SG_byte b2[] = { 0xFF, 0xFE, 0xFD, 0xFC, 0xFB, 0xFA, 0xF9, 0xF8, 0xF7, 0xF6, 0xF5, 0xF4, 0xF3, 0xF2, 0xF1, 0xF0,
						 0xEF, 0xEE, 0xED, 0xEC, 0xEB, 0xEA, 0xE9, 0xE8, 0xE7, 0xE6, 0xE5, 0xE4, 0xE3, 0xE2, 0xE1, 0xE0,
						 0xDF, 0xDE, 0xDD, 0xDC, 0xDB, 0xDA, 0xD9, 0xD8, 0xD7, 0xD6, 0xD5, 0xD4, 0xD3, 0xD2, 0xD1, 0xD0,
						 0xCF, 0xCE, 0xCD, 0xCC, 0xCB, 0xCA, 0xC9, 0xC8, 0xC7, 0xC6, 0xC5, 0xC4, 0xC3, 0xC2, 0xC1, 0xC0,
						 0xBF, 0xBE, 0xBD, 0xBC, 0xBB, 0xBA, 0xB9, 0xB8, 0xB7, 0xB6, 0xB5, 0xB4, 0xB3, 0xB2, 0xB1, 0xB0,
						 0xAF, 0xAE, 0xAD, 0xAC, 0xAB, 0xAA, 0xA9, 0xA8, 0xA7, 0xA6, 0xA5, 0xA4, 0xA3, 0xA2, 0xA1, 0xA0,
						 0x9F, 0x9E, 0x9D, 0x9C, 0x9B, 0x9A, 0x99, 0x98, 0x97, 0x96, 0x95, 0x94, 0x93, 0x92, 0x91, 0x90,
						 0x8F, 0x8E, 0x8D, 0x8C, 0x8B, 0x8A, 0x89, 0x88, 0x87, 0x86, 0x85, 0x84, 0x83, 0x82, 0x81, 0x80 };

		try_one__raw(argv[1], b0, sizeof(b0), ("42aa6bd9ca92e90ea28df6f6f2d0d9b8"
											   "5a2d1907ee4dc1b171ace7eb1159be3b"
											   "d1bc56586d92492b6eff9be03306994c"
											   "65a332c4c24160f46655040e558e8329"));

		try_one__raw(argv[1], b1, sizeof(b1), ("04f96c6f61b3e237a4fa7755ee4acf34"
											   "494222968954f495ad147a1a715f7a73"
											   "ebecfa1ef275bed87dc60bd1a0bc6021"
											   "06fa98f8e7237bd1ac0958e76d306678"));

		try_one__raw(argv[1], b2, sizeof(b2), ("b484ae9fb73e6620b10d52e49260ad26"
											   "620db2883ebafa210d701922aca85368"
											   "088144bdf4ef3d9898d47c34f130031b"
											   "0a0992f09f62dd78b329525a777daf7d"));

		// the Appendix did not give the answer for the zero-length input, so we computed it once and fixed the test.

		try_one__raw(argv[1], NULL, 0, ("5af68a4912e0a6187a004947a9d2a37d7a1f0873f0bdd9dc64838ece60da5535c2a55d039bd58e178948996b7a8336486ed969c894be658e47d595a5a9b86a8b"));

	}
	else if (strcmp(argv[1], "SKEIN/1024") == 0)
	{
		// these are from Appendix C in skein1.2.pdf

		SG_byte b0[] = { 0xFF };
		SG_byte b1[] = { 0xFF, 0xFE, 0xFD, 0xFC, 0xFB, 0xFA, 0xF9, 0xF8, 0xF7, 0xF6, 0xF5, 0xF4, 0xF3, 0xF2, 0xF1, 0xF0,
						 0xEF, 0xEE, 0xED, 0xEC, 0xEB, 0xEA, 0xE9, 0xE8, 0xE7, 0xE6, 0xE5, 0xE4, 0xE3, 0xE2, 0xE1, 0xE0,
						 0xDF, 0xDE, 0xDD, 0xDC, 0xDB, 0xDA, 0xD9, 0xD8, 0xD7, 0xD6, 0xD5, 0xD4, 0xD3, 0xD2, 0xD1, 0xD0,
						 0xCF, 0xCE, 0xCD, 0xCC, 0xCB, 0xCA, 0xC9, 0xC8, 0xC7, 0xC6, 0xC5, 0xC4, 0xC3, 0xC2, 0xC1, 0xC0,
						 0xBF, 0xBE, 0xBD, 0xBC, 0xBB, 0xBA, 0xB9, 0xB8, 0xB7, 0xB6, 0xB5, 0xB4, 0xB3, 0xB2, 0xB1, 0xB0,
						 0xAF, 0xAE, 0xAD, 0xAC, 0xAB, 0xAA, 0xA9, 0xA8, 0xA7, 0xA6, 0xA5, 0xA4, 0xA3, 0xA2, 0xA1, 0xA0,
						 0x9F, 0x9E, 0x9D, 0x9C, 0x9B, 0x9A, 0x99, 0x98, 0x97, 0x96, 0x95, 0x94, 0x93, 0x92, 0x91, 0x90,
						 0x8F, 0x8E, 0x8D, 0x8C, 0x8B, 0x8A, 0x89, 0x88, 0x87, 0x86, 0x85, 0x84, 0x83, 0x82, 0x81, 0x80 };
		SG_byte b2[] = { 0xFF, 0xFE, 0xFD, 0xFC, 0xFB, 0xFA, 0xF9, 0xF8, 0xF7, 0xF6, 0xF5, 0xF4, 0xF3, 0xF2, 0xF1, 0xF0,
						 0xEF, 0xEE, 0xED, 0xEC, 0xEB, 0xEA, 0xE9, 0xE8, 0xE7, 0xE6, 0xE5, 0xE4, 0xE3, 0xE2, 0xE1, 0xE0,
						 0xDF, 0xDE, 0xDD, 0xDC, 0xDB, 0xDA, 0xD9, 0xD8, 0xD7, 0xD6, 0xD5, 0xD4, 0xD3, 0xD2, 0xD1, 0xD0,
						 0xCF, 0xCE, 0xCD, 0xCC, 0xCB, 0xCA, 0xC9, 0xC8, 0xC7, 0xC6, 0xC5, 0xC4, 0xC3, 0xC2, 0xC1, 0xC0,
						 0xBF, 0xBE, 0xBD, 0xBC, 0xBB, 0xBA, 0xB9, 0xB8, 0xB7, 0xB6, 0xB5, 0xB4, 0xB3, 0xB2, 0xB1, 0xB0,
						 0xAF, 0xAE, 0xAD, 0xAC, 0xAB, 0xAA, 0xA9, 0xA8, 0xA7, 0xA6, 0xA5, 0xA4, 0xA3, 0xA2, 0xA1, 0xA0,
						 0x9F, 0x9E, 0x9D, 0x9C, 0x9B, 0x9A, 0x99, 0x98, 0x97, 0x96, 0x95, 0x94, 0x93, 0x92, 0x91, 0x90,
						 0x8F, 0x8E, 0x8D, 0x8C, 0x8B, 0x8A, 0x89, 0x88, 0x87, 0x86, 0x85, 0x84, 0x83, 0x82, 0x81, 0x80,
						 0x7F, 0x7E, 0x7D, 0x7C, 0x7B, 0x7A, 0x79, 0x78, 0x77, 0x76, 0x75, 0x74, 0x73, 0x72, 0x71, 0x70,
						 0x6F, 0x6E, 0x6D, 0x6C, 0x6B, 0x6A, 0x69, 0x68, 0x67, 0x66, 0x65, 0x64, 0x63, 0x62, 0x61, 0x60,
						 0x5F, 0x5E, 0x5D, 0x5C, 0x5B, 0x5A, 0x59, 0x58, 0x57, 0x56, 0x55, 0x54, 0x53, 0x52, 0x51, 0x50,
						 0x4F, 0x4E, 0x4D, 0x4C, 0x4B, 0x4A, 0x49, 0x48, 0x47, 0x46, 0x45, 0x44, 0x43, 0x42, 0x41, 0x40,
						 0x3F, 0x3E, 0x3D, 0x3C, 0x3B, 0x3A, 0x39, 0x38, 0x37, 0x36, 0x35, 0x34, 0x33, 0x32, 0x31, 0x30,
						 0x2F, 0x2E, 0x2D, 0x2C, 0x2B, 0x2A, 0x29, 0x28, 0x27, 0x26, 0x25, 0x24, 0x23, 0x22, 0x21, 0x20,
						 0x1F, 0x1E, 0x1D, 0x1C, 0x1B, 0x1A, 0x19, 0x18, 0x17, 0x16, 0x15, 0x14, 0x13, 0x12, 0x11, 0x10,
						 0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00 };

		try_one__raw(argv[1], b0, sizeof(b0), ("a204c394923416b820686e1b449ca427"
											   "7a477ca9db084dd931715b102a43f045"
											   "bc3483aee0288608916710f875df8d79"
											   "424997465328b517bac30996fbfa642b"
											   "cc446e157a45d02f0878daa3556139a7"
											   "6c2cfec283072196b33a128354cbdb6d"
											   "b9dba879c71cb10c77e929786d2fbafd"
											   "e6a05529e26b20ff3bbb7b475caa7115"));

		try_one__raw(argv[1], b1, sizeof(b1), ("c2e6b6fc042f86f2e31738641db60295"
											   "f74204ab525895a5dec5c806ac4786ec"
											   "1c982920095b7129fe3d8bd451f67ea3"
											   "1320c78b11575ea6dde394e75dc5f5c9"
											   "6a5104386dd55016d494dffac5ad119b"
											   "22c960dc46b658cf2ceb7d73af0fd0e1"
											   "9c7e21344aad06af39fcbef6c6c5d00d"
											   "e896b888d95456dedba6e5377b0cc572"));

		try_one__raw(argv[1], b2, sizeof(b2), ("64661f7dc4abbb50003eaea69242018d"
											   "27aeb415ca7b891fbddbc1c69040d4c4"
											   "a9822133e60e222af7ee09349c3f1c4c"
											   "c8f38111a505e86393e7284180602db6"
											   "30ae763d9a620ae4932a823415b59515"
											   "aeec556418ed2ef0827189d54e6576d4"
											   "1d961eddcbbd871e6f1b6ba625dc684c"
											   "bbc204888b18687ed627506070006b82"));

		// the Appendix did not give the answer for the zero-length input, so we computed it once and fixed the test.

		try_one__raw(argv[1], NULL, 0, ("5c88c7faeed294c36d955dd01ece99ec852bb8738a499743d8d93e64dc00ed3e0b3ee42774172e10ba2109634771e5c4443b651d6755e73937c0f57f6259dc0a2fd66048718c2cc65e789ea7fdf32baf0fc63509cf448faebe7aea765b51ee5f2852e5d244d7c0211fd1a17f5ca50f94d500e6eeec6f88d93637484420a2d1fe"));

	}
	else
	{
		fprintf(stderr,"Unknown <hash-method>\n");
		exit(1);
	}

	return 0;
}

