#include "data.h"
#include "defs.h"
#include "expr.h"
#include "gen.h"
#include "misc.h"

void statements(void) {
  struct ASTnode *tree;
  int reg;

  while (1) {
    // Match a 'print' as the first token of a statement
    match(T_PRINT, "print");

    // Parse the following expression and 
    // generate the assembly code
    tree = binexpr(0);
    reg = genAST(tree);
    genprintint(reg);
    genfreeregs();

    // Match the semicolon at the end of statements
    semi();
    
    // If we have reached the EOF, it means that there
    // are no further statements
    if (Token.token == T_EOF) return;
  }
}
