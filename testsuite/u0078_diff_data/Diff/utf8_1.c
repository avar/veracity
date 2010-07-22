#include <wchar.h>
#include <stdio.h>

#	define NrElements(a)	((sizeof(a))/(sizeof(a[0])))

int main(int argc, char ** argv)
{
	struct _x { unsigned long l,u; };
	struct _x x[] = { {0xd700, 0xd810}, {0xfdc0,0xfdf0}, {0xfff0,0xfff0} };
	unsigned long k, j, i;

	// printable 1-byte range (US-ASCII) -- 0x00 -- 0x7f

	for (k=0x20; k<0x80; k+=0x10)
	{
		printf("%04x:",k);
		for(j=0; j<0x10; j++)
			printf(" %c",k+j);
		printf("\n");
	}
	
	// 3-byte range -- 0x0800 -- 0xffff

	for (i=0; i<NrElements(x); i++)
	{
		printf("Block[%d]: %x -- %x\n", i,x[i].l,x[i].u );
		for (k=x[i].l; k<=x[i].u; k+=0x10)
		{
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
}

