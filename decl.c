#include "data.h"
#include "gen.h"
#include "misc.h"
#include "scan.h"
#include "stmt.h"
#include "sym.h"
#include "tree.h"
#include "types.h"

struct symtable *Functionid;

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
struct symtable *var_declaration(int type, int class) {
  struct symtable *sym = NULL;

  switch (class) {
    case C_GLOBAL:
      if (findglob(Text)!= NULL)
        fatals("Duplicate global variable declaration", Text);
    case C_LOCAL:
    case C_PARAM:
      if (findlocl(Text) != NULL)
        fatals("Duplicate local variable declaration", Text);
  }

  if (Token.token == T_LBRACKET) {
    // Consume the '[' 
    scan(&Token);

    // Check for array size
    // TODO: Must provide size for now
    if (Token.token == T_INTLIT) {
      switch (class) {
        case C_GLOBAL:
          sym = addglob(Text, pointer_to(type), S_ARRAY, class, Token.intvalue);
          break;
        case C_LOCAL:
        case C_PARAM:
          fatal("TODO: Implement support for local arrays.");
      }
    }
    // Consume int literal
    scan(&Token);
    match(T_RBRACKET, "]");
  } else {
    switch (class) {
      case C_GLOBAL:
        sym = addglob(Text, type, S_VARIABLE, class, 1);
        break;
      case C_LOCAL:
        sym = addlocl(Text, type, S_VARIABLE, class, 1);
        break;
      case C_PARAM:
        sym = addparm(Text, type, S_VARIABLE, class, 1);
        break;
    }
  }

  return sym;
}

// Parse the parameters in the parentheses after the function
// name. Add them as symbols to the symbol table and return the
// number of parameters. If funcsym is not NULL, then there is an
// existing function prototype and the function has this symtable
// entry.
static int param_declaration(struct symtable *funcsym) {
  int type;
  int paramcnt = 0;
  struct symtable *protoptr = NULL;

  if (funcsym != NULL)
    protoptr = funcsym->member;

  // Loop until we hit the right parenthesis
  while (Token.token != T_RPAREN) {
    type = parse_type();
    ident();

    // If prototype already exists, check that the types match
    if (protoptr != NULL) {
      if (type != protoptr->type)
        fatald("Type doesn't match prototype for parameter", paramcnt + 1);
      protoptr = protoptr->next;
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

  if ((funcsym != NULL) && (paramcnt != funcsym->nelems))
    fatals("Parameter count mismatch for function", funcsym->name);

  return paramcnt;
}

// The identifier should already have been consumed
// prior to calling this function.
struct ASTnode *function_declaration(int type) {
  struct ASTnode *tree, *finalstmt;
  struct symtable *oldfuncsym, *newfuncsym = NULL;
  int endlabel, paramcnt;

  // Fetch the function symbol if it exists
  if ((oldfuncsym = findsymbol(Text)) != NULL)
    if (oldfuncsym->stype != S_FUNCTION)
      oldfuncsym = NULL;

  // If this is a new function declaration, get a label ID
  // for the end label and add the fucntion to the symbol table
  if (oldfuncsym == NULL) {
    endlabel = genlabel();
    newfuncsym = addglob(Text, type, S_FUNCTION, C_GLOBAL, endlabel);
  }

  lparen();
  paramcnt = param_declaration(oldfuncsym);
  rparen();

  // If this is a new function declaration, update the
  // entry with the number of parameters and the parameter
  // list.
  if (newfuncsym) {
    newfuncsym->nelems = paramcnt;
    newfuncsym->member = Parmhead;
    oldfuncsym = newfuncsym;
  }

  // Clear our the param list
  Parmhead = Parmtail = NULL;

  // Check if this is a full declaration or just a prototype
  if (Token.token == T_SEMI) {
    scan(&Token);
    return NULL;
  }

  // If we get to this point, it means that we handling
  // a full function declaration and not just a prototype
  Functionid = oldfuncsym;

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

  return mkastunary(A_FUNCTION, type, tree, oldfuncsym, endlabel);
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
