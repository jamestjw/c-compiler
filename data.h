#include <stdio.h>
#include "defs.h"

extern int Line;
extern int Putback;
extern int Linestart;

extern FILE *Infile;
extern char *Infilename;

extern FILE *Outfile;
extern char *Outfilename;

extern struct token Token;        // Last token scanned
extern struct token Peektoken;    // A look-ahead token

extern char Text[TEXTLEN + 1];
extern struct symtable *Functionid;             // Current function symtable entry

extern int Looplevel;  // Depth of nested loops
extern int Switchlevel;  // Depth of switches

// Symbol table lists
extern struct symtable *Globhead, *Globtail;	    // Global variables and functions
extern struct symtable *Loclhead, *Locltail;	    // Local variables
extern struct symtable *Parmhead, *Parmtail;	    // Local parameters
extern struct symtable *Membhead, *Membtail;	    // Temp list of struct/union members
extern struct symtable *Structhead, *Structtail;  // List of struct types
extern struct symtable *Unionhead, *Uniontail;    // List of union types
extern struct symtable *Enumhead, *Enumtail;      // List of enum types
extern struct symtable *Typehead, *Typetail;      // List of typedefs

extern int O_dumpAST;     // Flag controlling debug output of AST trees
extern int O_keepasm;		  // Flag controlling whether we keep any assembly files
extern int O_assemble;		// Flag controlling whether we assemble the assembly files
extern int O_dolink;		  // Whether we should link the object files
extern int O_verbose;		  // Whether we should print info on compilation stages
extern int O_dumpsym;		  // Whether the symbol table should be dumped at the end of every source code file
