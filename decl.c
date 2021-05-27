#include "data.h"
#include "gen.h"
#include "misc.h"
#include "scan.h"
#include "stmt.h"
#include "sym.h"
#include "tree.h"

int Functionid;

// Parse the current token and 
// return a primitive type enum value
int parse_type(int t) {
  if (t == T_CHAR) return P_CHAR;
  if (t == T_INT) return P_INT;
  if (t == T_VOID) return P_VOID;
  if (t == T_LONG) return P_LONG;

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
  id = addglob(Text, type, S_VARIABLE, 0);
  genglobsym(id);
  semi();
}

struct ASTnode *function_declaration(void) {
  struct ASTnode *tree, *finalstmt;
  int nameslot, type, endlabel;

  // Get the type of the variable and the identifier
  type = parse_type(Token.token);

  scan(&Token);
  ident();

  // Get a label for the label that we place at the 
  // end of the function
  endlabel = genlabel();

  nameslot = addglob(Text, type, S_FUNCTION, endlabel);
  Functionid = nameslot;
  // TODO: Support function with parameters
  lparen();
  rparen();

  // Parse function body
  tree = compound_statement();

  // If the function type is not P_VOID,
  // check that the last AST operation in the
  // compound statement was a return statement
  if (type != P_VOID) {
    finalstmt = (tree->op == A_GLUE ? tree->right : tree);
    if (finalstmt == NULL || finalstmt->op != A_RETURN)
      fatal("No return for function with non-void type");
  }

  return mkastunary(A_FUNCTION, P_VOID, tree, nameslot);
}
