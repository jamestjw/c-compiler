// Determine if a symbol is in the global symbol table
// and returning its slot position. Returns -1 if the
// symbol is not found.
int findglob(char *s);
// Add a global symbol to the symbol table and returning
// its slot number.
int addglob(char *name, int type, int stype);
