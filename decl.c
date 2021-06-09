#include "data.h"
#include "gen.h"
#include "misc.h"
#include "scan.h"
#include "stmt.h"
#include "sym.h"
#include "tree.h"
#include "types.h"

struct symtable *Functionid;

static int var_declaration_list(struct symtable *funcsym, int class, 
    int separate_token, int end_token);

// When this function is called, the current token
// should be T_STRUCT
static struct symtable *struct_declaration(void) {
  struct symtable *ctype = NULL;
  struct symtable *m;
  int offset;

  scan(&Token);

  if (Token.token == T_IDENT) {
    ctype = findstruct(Text);
    scan(&Token);
  }

  // If we don't see a left brace, it means
  // that we using an existing struct type
  // instead of defining a new one.
  if (Token.token != T_LBRACE) {
    if (ctype == NULL)
      // Using an undefined struct
      fatals("Unknown struct type", Text); 

    return ctype;
  }

  // Ensure that this struct has not already been
  // defined
  if (ctype)
    fatals("Previously defined struct", Text);

  // Build the struct node and skip the left brace
  ctype = addstruct(Text, P_STRUCT, NULL, 0, 0);
  scan(&Token);
  // Parse the members and populate the Memb symtable list
  var_declaration_list(NULL, C_MEMBER, T_SEMI, T_RBRACE);
  rbrace();

  ctype->member = Membhead;
  Membhead = Membtail = NULL;

  m = ctype->member;
  m->posn = 0;
  offset = typesize(m->type, m->ctype);

  for (m = m->next; m != NULL; m = m->next) {
    // Set the offset for the next member
    m->posn = genalign(m->type, offset, 1);

    // Get the offset of the next free byte after this member
    // Is this right implementation??
    offset = m->posn + typesize(m->type, m->ctype);
    // Below is incorrect?
    // offset += typesize(m->type, m->ctype);
  }

  // Set the overall size of the struct
  ctype->size = offset;

  return ctype;
}

// Parse the current token and 
// return a primitive type enum value
int parse_type(struct symtable **ctype) {
  int type;

  switch (Token.token) {
    case T_VOID: 
      type = P_VOID;
      scan(&Token);
      break;
    case T_CHAR: 
      type = P_CHAR;
      scan(&Token);
      break;
    case T_INT:  
      type = P_INT;
      scan(&Token);
      break;
    case T_LONG: 
      type = P_LONG;
      scan(&Token);
      break;
    case T_STRUCT:
      type = P_STRUCT;
      // Look up an existing struct type, or parse the
      // declaration of a new struct type
      *ctype = struct_declaration();
      break;
    case T_UNION:
      type = P_UNION;
      *ctype = struct_declaration();
      break;
    default:
       fatald("Illegal type, token", Token.token);
  }

  while (1) {
    if (Token.token != T_STAR) break;
    type = pointer_to(type);
    scan(&Token);
  }

  return type;
}

// Parse the declaration of a list of
// variables. The identifier should have
// been scanned prior to calling this function.
struct symtable *var_declaration(int type, struct symtable *ctype, int class) {
  struct symtable *sym = NULL;

  switch (class) {
    case C_GLOBAL:
      if (findglob(Text) != NULL)
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
          sym = addglob(Text, pointer_to(type), ctype, S_ARRAY, Token.intvalue);
          break;
        case C_LOCAL:
        case C_PARAM:
        case C_MEMBER:
          fatal("TODO: Implement support for local arrays.");
      }
    }
    // Consume int literal
    scan(&Token);
    match(T_RBRACKET, "]");
  } else {
    switch (class) {
      case C_GLOBAL:
        sym = addglob(Text, type, ctype, S_VARIABLE, 1);
        break;
      case C_LOCAL:
        sym = addlocl(Text, type, ctype, S_VARIABLE, 1);
        break;
      case C_PARAM:
        sym = addparm(Text, type, ctype, S_VARIABLE, 1);
        break;
      case C_MEMBER:
        sym = addmemb(Text, type, ctype, S_VARIABLE, 1);
        break;
    }
  }

  return sym;
}

// When called to parse function params, separate_token token is T_COMMA
// When called to parse members of a struct/union, separate_token token is T_SEMI
//
// Parse a list of variables.
// Add them as symbols to the symbol table and return the
// number of parameters. If funcsym is not NULL, then there is an
// existing function prototype and the function has this symtable
// entry and each variable type is verified against the prototype
static int var_declaration_list(struct symtable *funcsym, int class, 
    int separate_token, int end_token) {
  int type;
  int paramcnt = 0;
  struct symtable *protoptr = NULL;
  struct symtable *ctype;

  if (funcsym != NULL)
    protoptr = funcsym->member;

  // Loop until we hit the right parenthesis
  while (Token.token != end_token) {
    type = parse_type(&ctype);
    ident();

    // If prototype already exists, check that the types match
    if (protoptr != NULL) {
      if (type != protoptr->type)
        fatald("Type doesn't match prototype for parameter", paramcnt + 1);
      protoptr = protoptr->next;
    } else {
      // Add a new parameter to the right symbol table list
      // based on the class
      var_declaration(type, ctype, class);
    }

    paramcnt++;

    if ((Token.token != separate_token) && (Token.token != end_token))
      fatald("Unexpected token in parameter list", Token.token);
    if (Token.token == separate_token)
      scan(&Token);
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
    newfuncsym = addglob(Text, type, NULL, S_FUNCTION, endlabel);
  }

  lparen();
  paramcnt = var_declaration_list(oldfuncsym, C_PARAM, T_COMMA, T_RPAREN);
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
  struct symtable *ctype;
  int type;

  while (1) {
    // We have to first parse the type and identifier
    // into order to tell if we are dealing with a 
    // function declaration or a variable declaration.

    if (Token.token == T_EOF)
      break;
    
    // Get the type
    type = parse_type(&ctype);

    // We might have parsed a struct declaration with no
    // associated variable.
    if (type == P_STRUCT && Token.token == T_SEMI) {
      scan(&Token);
      continue;
    }

    // We have to read past the identifier to see either
    // a '(' for function declaration, or a ',' or ';' for
    // a variable declaration.
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
      var_declaration(type, ctype, C_GLOBAL);
      semi();
    }
  }
}
