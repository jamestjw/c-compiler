void global_declarations(void);
// Consumes a type token and returns a primitive type
int parse_type(struct symtable **ctype, int *class);
int parse_cast(struct symtable **ctype);
int parse_literal(int type);
int parse_stars(int type);
int declaration_list(struct symtable **ctype, int class, int et1, int et2, struct ASTnode **gluetree);
