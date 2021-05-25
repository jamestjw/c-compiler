#include "data.h"
#include "gen.h"
#include "misc.h"
#include "stmt.h"
#include "sym.h"
#include "tree.h"

// Parse variable declarations
void var_declaration(void) {
  match(T_INT, "int");
  ident();
  addglob(Text);
  genglobsym(Text);
  semi();
}

struct ASTnode *function_declaration(void) {
  struct ASTnode *tree;
  int nameslot;

  // Match 'void' 'identifier '(' ')'
  // TODO: Support other return types
  match(T_VOID, "void");
  ident();
  nameslot = addglob(Text);
  // TODO: Support function with parameters
  lparen();
  rparen();

  // Parse function body
  tree = compound_statement();

  return mkastunary(A_FUNCTION, tree, nameslot);
}
