#include <stdio.h>
#include "defs.h"

extern int Line;
extern int Putback;
extern FILE *Infile;
extern FILE *Outfile;
extern struct token Token;
extern char Text[TEXTLEN + 1];
extern struct symtable Gsym[NSYMBOLS];
extern int Functionid; // Symbol ID of current function 
