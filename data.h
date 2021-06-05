#include <stdio.h>
#include "defs.h"

extern int Line;
extern int Putback;
extern FILE *Infile;

extern FILE *Outfile;
extern char *Outfilename;

extern struct token Token;
extern char Text[TEXTLEN + 1];
extern int Globs;                               // Position of next free global symbol slot
extern int Locls;                               // Position of next free local symbol slot
extern struct symtable Symtable[NSYMBOLS];
extern int Functionid;                          // Symbol ID of current function

extern int O_dumpAST;     // Flag controlling debug output of AST trees
extern int O_keepasm;		  // Flag controlling whether we keep any assembly files
extern int O_assemble;		// Flag controlling whether we assemble the assembly files
extern int O_dolink;		  // Whether we should link the object files
extern int O_verbose;		  // Whether we should print info on compilation stages
