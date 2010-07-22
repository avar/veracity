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
  sg_hexdump
*/

#include <stdio.h>

int main(int argc, char** argv)
{
    FILE* fp_in = NULL;
    FILE* fp_out = NULL;
    char buf_name[1024];
    int b;
    int count = 0;
    char* p = NULL;
    char* q = NULL;
    int done = 0;

    if (argc != 3)
    {
        printf("Usage: %s infile outfile\n", argv[0]);
        return -1;
    }

    // find the end of the infile string
    p = argv[1];
    while (*p)
    {
        p++;
    }

    // now go backwards until I find a slash, or the beginning
    while (
            (p >= argv[1])
            && (*p != '/')
          )
    {
        p--;
    }

    // if I found a slash, move forward one char
    if (*p == '/')
    {
        p++;
    }

    // now copy the filename over to buf_name, fixing stuff as we go
    q = buf_name;
    while (*p)
    {
        if (
                ('.' == *p)
                || (' ' == *p)
           )
        {
            *q = '_';
        }
        else
        {
            *q = *p;
        }
        q++;
        p++;
    }
    *q = 0;

#ifdef WINDOWS
    fopen_s(&fp_in, argv[1], "r");
    fopen_s(&fp_out, argv[2], "w");
#else
    fp_in = fopen(argv[1], "r");
    fp_out = fopen(argv[2], "w");
#endif
    fprintf(fp_out, "\n");
    fprintf(fp_out, "unsigned char %s[] =\n", buf_name);
    fprintf(fp_out, "{\n");

    while (!done)
    {
        b = fgetc(fp_in);
        if (EOF == b)
        {
            done = 1;
            // TODO check feof
            break;
        }
        if (0 == count)
        {
            fprintf(fp_out, "    ");
        }
        fprintf(fp_out, "0x%02x, ", b);
        count++;
        if (8 == count)
        {
            count = 0;
            fprintf(fp_out, "\n");
        }
    }
    fclose(fp_in);
    if (count)
    {
        fprintf(fp_out, "\n");
    }
    fprintf(fp_out, "    0x00\n};\n\n");
    fclose(fp_out);

    return 0;
}

