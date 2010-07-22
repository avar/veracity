#include <wchar.h>
#include <stdio.h>

int main(int argc, char ** argv)
{
	int k, j;

	// printable 1-byte range (US-ASCII) -- 0x00 -- 0x7f

	for (k=0x20; k<0x80; k+=0x10)
	{
		printf("%04x:",k);
		for(j=0; j<0x10; j++)
			printf(" %c",k+j);
		printf("\n");
	}
	
	// 2-byte range -- 0x80 -- 0x07ff

	for (k=0x80; k<0x800; k+=0x10)
	{
		printf("%04x:",k);
		for(j=0; j<0x10; j++)
		{
			int sum = k+j;
			int b0 = 0x80 + ((sum & 0x003f)     );
			int b1 = 0xc0 + ((sum & 0x07c0) >> 6);
			printf(" %c%c",b1,b0);
		}
		printf("\n");
	}

	// 3-byte range -- 0x0800 -- 0xffff

	for (k=0x800; k<0x10000; k+=0x10)
	{
		if (k >= 0xd7a0 && k < 0xe000) /* have some undefined areas */
			continue;
		if (k >= 0xfdd0 && k < 0xfdf0) /* have some undefined areas */
			continue;
		if (k >= 0xfff0 && k < 0x10000) /* have some undefined areas */
			continue;
		
		printf("%04x:",k);
		for(j=0; j<0x10; j++)
		{
			int sum = k+j;
			int b0 = 0x80 + ((sum & 0x003f)     );
			int b1 = 0x80 + ((sum & 0x0fc0) >> 6);
			int b2 = 0xe0 + ((sum & 0xf000) >>12);
			printf(" %c%c%c",b2,b1,b0);
		}
		printf("\n");
	}
	
}

