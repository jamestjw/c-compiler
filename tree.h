struct ASTnode *mkastnode(int op, int type, struct symtable *ctype, struct ASTnode *left,
			  struct ASTnode *mid, struct ASTnode *right, struct symtable *sym, int intvalue);
struct ASTnode *mkastleaf(int op, int type, struct symtable *ctype, struct symtable *sym, int intvalue);
struct ASTnode *mkastunary(int op, int type, struct symtable *ctype, struct ASTnode *left, struct symtable *sym, int intvalue);
void dumpAST(struct ASTnode *n, int label, int level);
