void genpreamble();
void genpostamble();
void genfreeregs();
void genprintint(int reg);
// Generate code for global symbol declaration
void genglobsym(int id);
int genAST(struct ASTnode *n, int reg, int parentASTop);
