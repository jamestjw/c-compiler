struct ASTnode *binexpr(int ptp);
// Parse a function call with a single expression
struct ASTnode *funccall(void);
// Parse a prefix expression and return a sub-tree
// representing it
struct ASTnode *prefix(int ptp);
struct ASTnode *expression_list(int endtoken);
