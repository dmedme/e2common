#include <stdio.h>
main(argc,argv)
int argc;
char ** argv;
{
static char buf[32768];
int i;
int rcnt;
FILE * fpr, *fpw;
    if (argc < 2)
    {
        fprintf(stderr,
            "Provide an output file and a number of input files\n");
        exit(1);
    }
    if ((fpw = fopen(argv[1],"wb")) == (FILE *) NULL)
    {
        perror("Cannot open output file");
        exit(1);
    }
    for (i= 2; i < argc; i++)
    {
        if ((fpr = fopen(argv[i],"rb")) == (FILE *) NULL)
        {
            perror("Cannot open input file");
            exit(1);
        }
        while ((rcnt = fread(&buf[0],sizeof(char), sizeof(buf), fpr)) > 0)
            (void) fwrite(&buf[0],sizeof(char),rcnt,fpw);
        (void) fclose(fpr);
    }
    (void) fclose(fpw);
    exit(0);
}
