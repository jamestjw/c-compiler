void var_declaration(int type, int islocal, int isparam);
struct ASTnode *function_declaration(int type);
void global_declarations(void);
// Consumes a type token and returns a primitive type
int parse_type();
