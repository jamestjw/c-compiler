// Determine if a symbol is in the global symbol table
// and returning the symtable entry corresponding to it.
struct symtable *findglob(char *s);

// Add a global symbol to the symbol table and returning
// its slot number.
// + type: P_CHAR, P_VOID etc
// + stype: Structural type S_FUNCTION, S_ARRAY etc
// + size: Number of elements in array or endlabel for function
struct symtable *addglob(char *name, int type, int stype, int class, int size);
// Determine if a symbol with name s is in the local
// symbol table and return the corresponding symtable entry 
struct symtable *findlocl(char *s);
struct symtable *addlocl(char *name, int type, int stype, int class, int size);
struct symtable *addparm(char *name, int type, int stype, int class, int size);
// Find a symbol within the symbol table, return
//its slot position or -1 if not found
struct symtable *findsymbol(char *s);
void freeloclsyms(void);
void copyfuncparams(int slot);
void clear_symtable(void);
