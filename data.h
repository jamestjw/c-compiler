#include <stdio.h>
#include "defs.h"

#define TEXTLEN 512
#define NSYMBOLS 1024 // Num of entries in the symbol table

extern int Line;
extern int Putback;
extern FILE *Infile;
extern FILE *Outfile;
extern struct token Token;
extern char Text[TEXTLEN + 1];
extern struct symtable Gsym[NSYMBOLS];
