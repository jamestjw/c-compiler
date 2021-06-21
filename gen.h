void genpreamble();
void genpostamble();
void genfreeregs();
void genprintint(int reg);
// Generate code for global symbol declaration
void genglobsym(struct symtable *node);
int genAST(struct ASTnode *n, int iflabel, int looptoplabel,
	   int loopendlabel, int parentASTop);
// Return the size of a particular primitive
int genprimsize(int type);
// Generate an ID for a label
int genlabel(void);
// Generate code for a string literal and return the
// label for it
int genglobstr(char *strvalue, int append);
void genglobstrend(void);
int genalign(int type, int offset, int direction);
