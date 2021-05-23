#include "data.h"
#include "decl.h"
#include "expr.h"
#include "gen.h"
#include "misc.h"
#include "sym.h"
#include "tree.h"

void print_statement(void) {
  struct ASTnode *tree;
  int reg;

  // Match a 'print' as the first token of a statement
  match(T_PRINT, "print");

  // Parse the following expression and 
  // generate the assembly code
  tree = binexpr(0);
  reg = genAST(tree, -1);
  genprintint(reg);
  genfreeregs();

  // Match the semicolon at the end of statements
  semi();
}

void assignment_statement(void) {
  struct ASTnode *left, *right, *tree;
  int id;

  // Match an identifier
  ident();

  if ((id = findglob(Text)) == -1) {
    fatals("Assigning to undeclared variable", Text);
  }

  right = mkastleaf(A_LVIDENT, id);

  match(T_ASSIGN, "=");

  left = binexpr(0);

  tree = mkastnode(A_ASSIGN, left, right, 0);
  genAST(tree, -1);
  genfreeregs();

  semi();
}

void statements(void) {
  while (1) {
    switch (Token.token) {  
      case T_PRINT:
        print_statement();
        break;
      case T_INT:
        var_declaration();
        break;
      case T_IDENT:
        assignment_statement();
        break;
      case T_EOF:
        return;
      default:
        fatald("Syntax error stmt, token", Token.token);
    }
  }
}
