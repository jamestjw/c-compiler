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
void var_declaration(int type, int class) {
  // Handle `int list[5];`
  if (Token.token == T_LBRACKET) {
    // Consume the '[' 
    scan(&Token);

    // Check for array size
    // TODO: Must provide size for now
    if (Token.token == T_INTLIT) {
      if (class == C_LOCAL) {
        fatal("TODO: Implement support for local arrays.");
      } else {
        addglob(Text, pointer_to(type), S_ARRAY, class, Token.intvalue);
      }
    }
    // Consume int literal
    scan(&Token);
    match(T_RBRACKET, "]");
  } else {
    if (class == C_LOCAL) {
      if (addlocl(Text, type, S_VARIABLE, class, 1) == -1)
       fatals("Duplicate local variable declaration", Text);
    } else {
      addglob(Text, type, S_VARIABLE, class, 1);
    }
  }
}

// Parse the parameters in the parentheses after the function
// name. Add them as symbols to the symbol table and return the
// number of parameters. If ID is not -1, then there is an
// existing function prototype and the function has this slot
// number in the symbol table.
static int param_declaration(int id) {
  int type, param_id;
  int orig_paramcnt;
  int paramcnt = 0;

  // This holds either 0 if there is no prototype defined
  // yet, or the position of the zeroth parameter in the
  // symbol table
  param_id = id + 1;

  if (param_id)
    orig_paramcnt = Symtable[id].nelems;

  // Loop until we hit the right parenthesis
  while (Token.token != T_RPAREN) {
    type = parse_type();
    ident();

    // If prototype already exists, check that the types match
    if (param_id) {
      if (type != Symtable[param_id].type)
        fatald("Type doesn't match prototype for parameter", param_id - id);
      param_id++;
    } else {
      // Add a new parameter to the prototype
      var_declaration(type, C_PARAM);
    }

    paramcnt++;

    switch (Token.token) {
      case T_COMMA: 
        scan(&Token); 
        break;
      case T_RPAREN: 
        break;
      default:
        fatald("Unexpected token in parameter list", Token.token);
    }
  }

  if ((id != -1) && (paramcnt != orig_paramcnt))
    fatals("Parameter count mismatch for function", Symtable[id].name);

  return paramcnt;
}

// The identifier should already have been consumed
// prior to calling this function.
struct ASTnode *function_declaration(int type) {
  struct ASTnode *tree, *finalstmt;
  int id;
  int nameslot, endlabel, paramcnt;

  // Check if a function with this name has already been
  // defined and set the ID.
  if ((id = findsymbol(Text)) != -1)
    if (Symtable[id].stype != S_FUNCTION)
      id = -1;

  // If this is a new function declaration, get a label ID
  // for the end label and add it to the symbol table
  if (id == -1) {
    endlabel = genlabel();
    nameslot = addglob(Text, type, S_FUNCTION, C_GLOBAL, endlabel);
  }

  lparen();
  paramcnt = param_declaration(id);
  rparen();

  // If this is a new function declaration, update the
  // entry with the number of parameters
  if (id == -1)
    Symtable[nameslot].nelems = paramcnt;

  // Check if this is a full declaration or just a prototype
  if (Token.token == T_SEMI) {
    scan(&Token);
    return NULL;
  }

  if (id == -1) id = nameslot;

  // Copy global parameters to local parameters
  copyfuncparams(id);
  Functionid = id;

  // Parse function body
  tree = compound_statement();

  // If the function type is not P_VOID,
  // check that the last AST operation in the
  // compound statement was a return statement
  if (type != P_VOID) {
    if (tree == NULL)
      fatal("No statements in function with non-void type");

    finalstmt = (tree->op == A_GLUE ? tree->right : tree);
    if (finalstmt == NULL || finalstmt->op != A_RETURN)
      fatal("No return for function with non-void type");
  }

  return mkastunary(A_FUNCTION, type, tree, id);
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

      // Only a function prototype, i.e. no code
      if (tree == NULL)
        continue;

      if (O_dumpAST) {
        dumpAST(tree, NOLABEL, 0);
        fprintf(stdout, "\n\n");
      }

      genAST(tree, NOREG, 0);

      freeloclsyms();
    } else {
      // Dealing with variable declaration
      var_declaration(type, C_GLOBAL);
      semi();
    }

    if (Token.token == T_EOF)
      break;
  }
}
