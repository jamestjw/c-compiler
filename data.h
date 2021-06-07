#include <stdio.h>
#include "defs.h"

extern int Line;
extern int Putback;
extern FILE *Infile;

extern FILE *Outfile;
extern char *Outfilename;

extern struct token Token;
extern char Text[TEXTLEN + 1];
extern struct symtable *Functionid;             // Current function symtable entry

// Symbol table lists
extern struct symtable *Globhead, *Globtail;	// Global variables and functions
extern struct symtable *Loclhead, *Locltail;	// Local variables
extern struct symtable *Parmhead, *Parmtail;	// Local parameters
extern struct symtable *Comphead, *Comptail;	// Composite types

extern int O_dumpAST;     // Flag controlling debug output of AST trees
extern int O_keepasm;		  // Flag controlling whether we keep any assembly files
extern int O_assemble;		// Flag controlling whether we assemble the assembly files
extern int O_dolink;		  // Whether we should link the object files
extern int O_verbose;		  // Whether we should print info on compilation stages
