This directory contains reference code to compute the variation of
Unicode that Apple OS X uses for filenames.

Since the Apple-specific Unicode variation is frozen in their file
system implementation and since the ICU code is tracking Unicode 5.1
and beyond, we need this code to perform the various custom
transformations like Apple would in their filesystem.

________________________________________________________________

[0] Warning
________________________________________________________________

This directory contains the original sources that we got from Apple.
I have extracted the relevant parts and created files 
sg_apple_unicode*.[ch] in src/libraries/{include,ut}.

It is our current belief that the APSL is MPL-ish, rather than LGPL-ish
and that we don't need to isolate it in a private DLL or otherwise
publish the source of the containing library.

http://www.opensource.apple.com/apsl/
http://www.opensource.apple.com/darwinsource/

________________________________________________________________

[1] Apple Custom NFC/NFD
________________________________________________________________

Apple stores filenames in their custom version of NFD.
Prior to OS X 10.2, this was based upon Unicode 2.0 + their mods.
In 10.2 and beyond, this was based upon Unicode 3.2 + their mods.

http://www.unicode.org/unicode/reports/tr15/
http://developer.apple.com/technotes/tn/tn1150.html#CanonicalDecomposition
http://developer.apple.com/technotes/tn/tn1150.html#UnicodeSubtleties
http://developer.apple.com/technotes/tn/tn1150table.html
http://developer.apple.com/technotes/tn2002/tn2078.html#HOWENCODED

The only documentation that I could find to describe their
custom NFD was a summary in tn1150.html and the tn1150table.html.
The former just hints at what the differences are from "real" NFD.
The latter lists a table of substitutions, but does not indicate
which version it is.

I think the Darwin source code for this is in unicode_decompse()
and unicode_combine() in:

http://www.opensource.apple.com/darwinsource/10.5.6/xnu-1228.9.59/bsd/vfs/vfs_utfconvdata.h
http://www.opensource.apple.com/darwinsource/10.5.6/xnu-1228.9.59/bsd/vfs/vfs_utfconv.c
http://www.opensource.apple.com/darwinsource/10.5.6/xnu-1228.9.59/bsd/sys/utfconv.h

I also found the following links:

http://stuff.mit.edu/afs/sipb/project/darwin/src/modules/xnu/bsd/vfs/
http://stuff.mit.edu/afs/sipb/project/darwin/src/modules/xnu/bsd/vfs/vfs_utfconv.c
http://stuff.mit.edu/afs/sipb/project/darwin/src/modules/xnu/bsd/vfs/vfs_utfconvdata.h
http://src.gnu-darwin.org/DarwinSourceArchive/expanded/CF/CF-299/StringEncodings.subproj/CFUnicodeDecomposition.c
http://blog.tomtebo.org/wp-content/macfilename.py
http://www.opensource.apple.com/darwinsource/10.3.5/libiconv-9/libiconv/lib/utf8mac.h
http://developer.apple.com/qa/qa2001/qa1235.html
http://hfsplus.sourcearchive.com/documentation/1.0.4/FastUnicodeCompare_8c-source.html

________________________________________________________________

[2] Case Insensitivity and Ignorable Characters
________________________________________________________________

In addition to custom NFD transformations, Apple also has their own
method of ignoring case and ignoring Unicode "Ignorable" characters
in filenames.  Information for this may be found by searching for
"FastUnicodeCompare".  It is also discussed in:

http://developer.apple.com/technotes/tn/tn1150.html#HFSX
http://developer.apple.com/technotes/tn/tn1150.html#StringComparisonAlgorithm

I think the Darwin source code for this is in FastUnicodeCompare() in:

http://www.opensource.apple.com/darwinsource/10.5.6/xnu-1228.9.59/bsd/hfs/hfscommon/Unicode/UnicodeWrappers.c
http://www.opensource.apple.com/darwinsource/10.5.6/xnu-1228.9.59/bsd/hfs/hfscommon/Unicode/UCStringCompareData.h

Additional links:

http://www.koders.com/c/fidB652DB244B0E53A5559B1E1A77E7F32CA6D9BF6E.aspx
http://unicode.org/reports/tr21/tr21-5.html
http://hfsplus.sourcearchive.com/documentation/1.0.4/unicode_8c-source.html
http://hfsplus.sourcearchive.com/documentation/1.0.4/unicode_8h-source.html

________________________________________________________________

[3] Services for Macintosh (SFM) and NTFS
________________________________________________________________

When file names are created on the Mac onto a remotely mounted NTFS
partition, the Mac goes thru the SFM code and converts invalid NTFS
characters 0x00..0x1f, ", *, ... and final space/dot characters
into unicode private use characters 0xf0001..0xf029.

http://support.microsoft.com/kb/q117258/

________________________________________________________________

[4] Remarks
________________________________________________________________

This directory contains the original sources that I started with so
that we can diff against future versions of the Darwin source.

jeff
