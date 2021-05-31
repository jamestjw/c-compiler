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
void var_declaration(int type) {
  int id;
 
  // Handle `int list[5];`
  if (Token.token == T_LBRACKET) {
    // Consume the '[' 
    scan(&Token);

    // Check for array size
    // TODO: Must provide size for now
    if (Token.token == T_INTLIT) {
      id = addglob(Text, pointer_to(type), S_ARRAY, 0, Token.intvalue);
      genglobsym(id);

      // Consume int literal
      scan(&Token);
      match(T_RBRACKET, "]");
      semi();
      return;
    } else {
      fatal("Missing array size");
    }
  } else { // Handle `int a,b,c;`
    while (1) {
      id = addglob(Text, type, S_VARIABLE, 0, 1);
      genglobsym(id);
  
      // If the next token is a semicolon, we 
      // consume it and we have come to the end
      // of the declaration.
      if (Token.token == T_SEMI) {
        scan(&Token);
        return;
      }
  
      // If the next token is a comma, consume it
      // and proceed to the next iteration of the
      // loop to continue parsing the declaration.
      if (Token.token == T_COMMA) {
        scan(&Token);
        ident();
        continue;
      }
  
      fatal("Missing ',' or ';' after identifier in variable declaration");
    }
  }
}

// The identifier should already have been consumed
// prior to calling this function.
struct ASTnode *function_declaration(int type) {
  struct ASTnode *tree, *finalstmt;
  int nameslot, endlabel;

  // Get a label for the label that we place at the 
  // end of the function
  endlabel = genlabel();
  nameslot = addglob(Text, type, S_FUNCTION, endlabel, 0);
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
    } else {
      // Dealing with variable declaration
      var_declaration(type);
    }

    if (Token.token == T_EOF)
      break;
  }
}
