struct symtable *var_declaration(int type, struct symtable *ctype, int class);
void global_declarations(void);
// Consumes a type token and returns a primitive type
int parse_type(struct symtable **ctype, int *class);
int declaration_list(struct symtable **ctype, int class, int et1, int et2);
