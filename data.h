#include <stdio.h>
#include "defs.h"

extern int Line;
extern int Putback;
extern FILE *Infile;
extern FILE *Outfile;
extern struct token Token;
extern char Text[TEXTLEN + 1];
extern int Globs;                               // Position of next free global symbol slot
extern int Locls;                               // Position of next free local symbol slot
extern struct symtable Symtable[NSYMBOLS];
extern int Functionid;                          // Symbol ID of current function
extern int O_dumpAST;                           // Flag controlling debug output of AST trees
