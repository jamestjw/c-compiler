// Given a primitive type, return the type which is a
// pointer to it.
int pointer_to(int type);
// Given a primitive pointer type, return the type
// which it points to
int value_at(int type); 
struct ASTnode *modify_type(struct ASTnode *tree, int rtype, int op);
// Return true if the type is an integer compatible 
// type.
int inttype(int type);
int ptrtype(int type);
