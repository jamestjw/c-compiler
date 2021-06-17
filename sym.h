// Add a global symbol to the symbol table and returning
// its slot number.
// + type: P_CHAR, P_VOID etc
// + stype: Structural type S_FUNCTION, S_ARRAY etc
// + size: Number of elements in array or endlabel for function
struct symtable *addglob(char *name, int type, struct symtable *ctype, int stype, int class, int nelems, int posn);
struct symtable *addlocl(char *name, int type, struct symtable *ctype, int stype, int nelems);
struct symtable *addparm(char *name, int type, struct symtable *ctype, int stype);
// Add a symbol to the temporary member list
struct symtable *addmemb(char *name, int type, struct symtable *ctype, int stype, int nelems);
// Add a struct to the struct list
struct symtable *addstruct(char *name);
struct symtable *addunion(char *name);
struct symtable *addenum(char *name, int class, int value);
struct symtable *addtypedef(char *name, int type, struct symtable *ctype);

// Determine if a symbol is in the global symbol table
// and returning the symtable entry corresponding to it.
struct symtable *findglob(char *s);
// Determine if a symbol with name s is in the local
// symbol table and return the corresponding symtable entry
struct symtable *findlocl(char *s);
struct symtable *findstruct(char *s);
struct symtable *findunion(char *s);
struct symtable *findenumtype(char *s);
struct symtable *findenumval(char *s);
struct symtable *findtypedef(char *s);
struct symtable *findmember(char *s);

// Find a symbol within the symbol table, return
//its slot position or -1 if not found
struct symtable *findsymbol(char *s);
void freeloclsyms(void);
void copyfuncparams(int slot);
void clear_symtable(void);
void freestaticsyms(void);
