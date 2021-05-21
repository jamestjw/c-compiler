#include <stdio.h>

#define TEXTLEN 512

extern int Line;
extern int Putback;
extern FILE *Infile;
extern FILE *Outfile;
extern struct token Token;
extern char Text[TEXTLEN + 1];
