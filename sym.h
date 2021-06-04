// Determine if a symbol is in the global symbol table
// and returning its slot position. Returns -1 if the
// symbol is not found.
int findglob(char *s);
// Add a global symbol to the symbol table and returning
// its slot number.
int addglob(char *name, int type, int stype, int class, int endlabel, int size);
// Determine if a symbol with name s is in the local
// symbol table. Return its slot position or -1 if not
// found
int findlocl(char *s);
int addlocl(char *name, int type, int stype, int endlabel, int size);
// Find a symbol within the symbol table, return
//its slot position or -1 if not found
int findsymbol(char *s);
void freeloclsyms(void);
void copyfuncparams(int slot);
