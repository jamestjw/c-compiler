void genpreamble();
void genpostamble();
void genfreeregs();
void genprintint(int reg);
// Generate code for global symbol declaration
void genglobsym(char *s);
int genAST(struct ASTnode *n, int reg);
