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

// Parse the declaration of a list of
// variables. The identifier should have
// been scanned prior to calling this function.
void var_declaration(int type, int islocal, int isparam) {
  // Handle `int list[5];`
  if (Token.token == T_LBRACKET) {
    // Consume the '[' 
    scan(&Token);

    // Check for array size
    // TODO: Must provide size for now
    if (Token.token == T_INTLIT) {
      if (islocal) {
        addlocl(Text, pointer_to(type), S_ARRAY, 0, Token.intvalue);
      } else {
        addglob(Text, pointer_to(type), S_ARRAY, 0, Token.intvalue);
      }
    }
    // Consume int literal
    scan(&Token);
    match(T_RBRACKET, "]");
  } else {
    if (islocal) {
      if (addlocl(Text, type, S_VARIABLE, isparam, 1) == -1)
       fatals("Duplicate local variable declaration", Text);
    } else {
      addglob(Text, type, S_VARIABLE, 0, 1);
    }
  }
}

static int param_declaration(void) {
  int type;
  int paramcnt = 0;

  // Loop until we hit the right parenthesis
  while (Token.token != T_RPAREN) {
    type = parse_type();
    ident();
    var_declaration(type, 1, 1);
    paramcnt++;

    switch (Token.token) {
      case T_COMMA: scan(&Token); break;
      case T_RPAREN: break;
      default:
        fatald("Unexpected token in parameter list", Token.token);
    }
  }

  return paramcnt;
}

// The identifier should already have been consumed
// prior to calling this function.
struct ASTnode *function_declaration(int type) {
  struct ASTnode *tree, *finalstmt;
  int nameslot, endlabel, paramcnt;

  // Get a label for the label that we place at the 
  // end of the function
  endlabel = genlabel();
  nameslot = addglob(Text, type, S_FUNCTION, endlabel, 0);
  Functionid = nameslot;

  lparen();
  paramcnt = param_declaration();
  Symtable[nameslot].nelems = paramcnt;
  rparen();

  // Parse function body
  tree = compound_statement();

  // If the function type is not P_VOID,
  // check that the last AST operation in the
  // compound statement was a return statement
  if (type != P_VOID) {
    if (tree == NULL)
      fatal("No statements in fucntion with non-void type");

    finalstmt = (tree->op == A_GLUE ? tree->right : tree);
    if (finalstmt == NULL || finalstmt->op != A_RETURN)
      fatal("No return for function with non-void type");
  }

  return mkastunary(A_FUNCTION, P_VOID, tree, nameslot);
}

void global_declarations(void) {
  struct ASTnode *tree;
  int type;

  while (1) {
    // We have to first parse the type and identifier
    // into order to tell if we are dealing with a 
    // function declaration or a variable declaration.
    
    type = parse_type();
    ident();

    if (Token.token == T_LPAREN) {
      // Dealing with a function declaration
      tree = function_declaration(type);

      if (O_dumpAST) {
        dumpAST(tree, NOLABEL, 0);
        fprintf(stdout, "\n\n");
      }

      genAST(tree, NOREG, 0);

      freeloclsyms();
    } else {
      // Dealing with variable declaration
      var_declaration(type, 0, 0);
      semi();
    }

    if (Token.token == T_EOF)
      break;
  }
}
