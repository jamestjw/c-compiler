#include "data.h"
#include "gen.h"
#include "misc.h"
#include "scan.h"
#include "stmt.h"
#include "sym.h"
#include "tree.h"
#include "types.h"

int Functionid;

// Parse the current token and 
// return a primitive type enum value
int parse_type() {
  int type;

  switch (Token.token) {
    case T_VOID: type = P_VOID; break;
    case T_CHAR: type = P_CHAR; break;
    case T_INT:  type = P_INT;  break;
    case T_LONG: type = P_LONG; break;
    default:
       fatald("Illegal type, token", Token.token);
  }

  while (1) {
    scan(&Token);
    if (Token.token != T_STAR) break;
    type = pointer_to(type);
  }

  return type;
}

// Parse variable declarations
void var_declaration(void) {
  int id, type;

  type = parse_type();

  ident();
  id = addglob(Text, type, S_VARIABLE, 0);
  genglobsym(id);
  semi();
}

struct ASTnode *function_declaration(void) {
  struct ASTnode *tree, *finalstmt;
  int nameslot, type, endlabel;

  // Get the type of the variable and the identifier
  type = parse_type();

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
