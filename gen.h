void genpreamble();
void genpostamble();
void genfreeregs();
void genprintint(int reg);
// Generate code for global symbol declaration
void genglobsym(int id);
int genAST(struct ASTnode *n, int reg, int parentASTop);
// Return the size of a particular primitive
int genprimsize(int type);
// Generate an ID for a label
int genlabel(void);
// Generate code for a string literal and return the
// label for it
int genglobstr(char *strvalue);
