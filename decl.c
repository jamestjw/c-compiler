#include "data.h"
#include "gen.h"
#include "misc.h"
#include "scan.h"
#include "stmt.h"
#include "sym.h"
#include "tree.h"

// Parse the current token and 
// return a primitive type enum value
int parse_type(int t) {
  if (t == T_CHAR) return P_CHAR;
  if (t == T_INT) return P_INT;
  if (t == T_VOID) return P_VOID;

  fatald("Illegal type, token", t);

  // Unreachable (to silence compiler warnings)
  return -1;
}

// Parse variable declarations
void var_declaration(void) {
  int id, type;

  type = parse_type(Token.token);
  scan(&Token);

  ident();
  id = addglob(Text, type, S_VARIABLE);
  genglobsym(id);
  semi();
}

struct ASTnode *function_declaration(void) {
  struct ASTnode *tree;
  int nameslot;

  // Match 'void' 'identifier '(' ')'
  // TODO: Support other return types
  match(T_VOID, "void");
  ident();
  nameslot = addglob(Text, P_VOID, S_FUNCTION);
  // TODO: Support function with parameters
  lparen();
  rparen();

  // Parse function body
  tree = compound_statement();

  return mkastunary(A_FUNCTION, P_VOID, tree, nameslot);
}
