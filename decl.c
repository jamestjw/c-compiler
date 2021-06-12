#include "data.h"
#include "decl.h"
#include "gen.h"
#include "misc.h"
#include "scan.h"
#include "stmt.h"
#include "sym.h"
#include "tree.h"
#include "types.h"

struct symtable *Functionid;
int Looplevel;
int Switchlevel;

int parse_type(struct symtable **ctype, int *class);
static int parse_stars(int type);
static int param_declaration_list(struct symtable *oldfuncsym, struct symtable *newfuncsym);

// When this function is called, the current token
// should be T_STRUCT
static struct symtable *composite_declaration(int type) {
  struct symtable *ctype = NULL;
  struct symtable *m;
  int offset;
  int t;

  scan(&Token);

  if (Token.token == T_IDENT) {
    if (type == P_STRUCT)
      ctype = findstruct(Text);
    else if (type == P_UNION)
      ctype = findunion(Text);
    else
      fatald("Unsupported composite type", type);
    scan(&Token);
  }

  // If we don't see a left brace, it means
  // that we using an existing struct type
  // instead of defining a new one.
  if (Token.token != T_LBRACE) {
    if (ctype == NULL)
      // Using an undefined struct
      fatals("Unknown struct/union type", Text); 

    return ctype;
  }

  // Ensure that this struct has not already been
  // defined
  if (ctype)
    fatals("Previously defined struct/union", Text);

  // Build the composite and skip the left brace
  if (type == P_STRUCT)
    ctype = addstruct(Text, P_STRUCT, NULL, 0, 0);
  else
    ctype = addunion(Text, P_UNION, NULL, 0, 0); 
  scan(&Token);

  while (1) {
    t = declaration_list(&m ,C_MEMBER, T_SEMI, T_RBRACE);
    if (t == -1)
      fatal("Bad type in member list");

    if (Token.token == T_SEMI)
      scan(&Token);

    if (Token.token == T_RBRACE)
      break;
  }

  rbrace();

  if (Membhead == NULL)
    fatals("No members in struct", ctype->name);

  ctype->member = Membhead;
  Membhead = Membtail = NULL;

  m = ctype->member;
  m->posn = 0;
  offset = typesize(m->type, m->ctype);

  for (m = m->next; m != NULL; m = m->next) {
    if (type == P_STRUCT) {
      // Set the offset for the next member
      m->posn = genalign(m->type, offset, 1);

      // Get the offset of the next free byte after this member
      // Is this right implementation??
      offset = m->posn + typesize(m->type, m->ctype);
      // Below is incorrect?
      // offset += typesize(m->type, m->ctype);
    } else {
      m->posn = 0;
      // Incorrect? Union size should be the size of largest member
      offset += typesize(m->type, m->ctype);
    }
  }

  // Set the overall size of the struct
  ctype->size = offset;

  return ctype;
}

static void enum_declaration(void) {
  struct symtable *etype = NULL;
  char *name;
  int intval = 0;

  // Consume the enum keyword
  scan(&Token);

  if (Token.token == T_IDENT) {
    etype = findenumtype(Text);
    // Cache the identifier lexeme before we lose it
    name = strdup(Text);
    scan(&Token);
  }

  // If we do not hit a left brace, check that this
  // enum has previously been defined.
  // e.g. `enum foo var1` instead of `enum foo { x, y, z }`
  if (Token.token != T_LBRACE) {
    if (etype == NULL)
      fatals("Undeclared enum type", name);
    return;
  }

  // Consume the LBRACE
  scan(&Token);

  if (etype != NULL)
    fatals("Enum type redeclared", etype->name);
  else
    etype = addenum(name, C_ENUMTYPE, 0);

  while (1) {
    // Ensure we have an identifier and cache it
    ident();
    name = strdup(Text);

    etype = findenumval(name);
    if (etype != NULL)
      fatals("Enum value redeclared", name);

    if (Token.token == T_ASSIGN) {
      scan(&Token);
      if (Token.token != T_INTLIT)
        fatal("Expected int literal after '=' in enum declaration");
      intval = Token.intvalue;
      scan(&Token);
    }

    etype = addenum(name, C_ENUMVAL, intval++);

    if (Token.token == T_RBRACE)
      break;
    comma();
  }

  // Consume RBRACE
  scan(&Token); 
}

// Parse a typedef definition and return the type
// and ctype it represents
static int typedef_declaration(struct symtable **ctype) {
  int type, class=0;

  // Consume the typedef keyword
  scan(&Token);

  // Get the actual type following the `typedef` keyword
  type = parse_type(ctype, &class);
  if (class != 0) {
    fatal("Can't have extern in a typedef declaration");
  }

  if (findtypedef(Text) != NULL)
    fatals("Redefinition of typedef", Text);

  type = parse_stars(type);

  addtypedef(Text, type, *ctype, 0, 0);
  // Consume the new type name identifier
  scan(&Token); 

  return type;
}

static int type_of_typedef(char *name, struct symtable **ctype) {
  struct symtable *t;

  t = findtypedef(name);
  if (t == NULL)
    fatals("Unknown type", name);
  scan(&Token);
  *ctype = t->ctype;
  return t->type;
}

// Parse the current token and 
// return a primitive type enum value
int parse_type(struct symtable **ctype, int *class) {
  int type, exstatic = 1;

  // See if the class has been changed to extern (later, static)
  while (exstatic) {
    switch (Token.token) {
      case T_EXTERN: 
        *class = C_EXTERN;
        scan(&Token);
        break;
      default:
        exstatic = 0;
    }
  }

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
      *ctype = composite_declaration(P_STRUCT);
      if (Token.token == T_SEMI)
        type = -1;
      break;
    case T_UNION:
      type = P_UNION;
      *ctype = composite_declaration(P_UNION);
      if (Token.token == T_SEMI)
        type = -1;
      break;
    case T_ENUM:
      // Enums are just ints
      type = P_INT;
      enum_declaration();
      if (Token.token == T_SEMI)
        type = -1;
      break;
    case T_TYPEDEF:
      type = typedef_declaration(ctype);
      if (Token.token == T_SEMI)
        type = -1;
      break;
    case T_IDENT:
      // Encountered a type we don't recognise
      // hence we check if there was a typedef
      type = type_of_typedef(Text, ctype);
      break;
    default:
      // Fix -Wall warning
      type = -1;
      fatals("Illegal type, token", Token.tokstr);
  }

  return type;
}

// Given a type returned by parse_type(),
// scan in any subsequent asterisks and
// return the new type. The return type
// could be the original type that was
// passed in, or a pointer to it (with
// support for multiple levels of
// indirection)
static int parse_stars(int type) {
  while (1) {
    if (Token.token != T_STAR)
      break;
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
    case C_EXTERN:
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
        case C_EXTERN:
        case C_GLOBAL:
          sym = addglob(Text, pointer_to(type), ctype, S_ARRAY, class, Token.intvalue);
          break;
        case C_LOCAL:
        case C_PARAM:
        case C_MEMBER:
          fatal("TODO: Implement support for local arrays");
      }
    }
    // Consume int literal
    scan(&Token);
    match(T_RBRACKET, "]");
  } else {
    switch (class) {
      case C_EXTERN:
      case C_GLOBAL:
        sym = addglob(Text, type, ctype, S_VARIABLE, class, 1);
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

// The identifier should already have been consumed
// prior to calling this function.
static struct symtable *function_declaration(char *funcname, int type,
    struct symtable *ctype, int class) {
  struct ASTnode *tree, *finalstmt;
  struct symtable *oldfuncsym, *newfuncsym = NULL;
  int endlabel, paramcnt;

  // Fetch the function symbol if it exists
  if ((oldfuncsym = findsymbol(funcname)) != NULL)
    if (oldfuncsym->stype != S_FUNCTION)
      oldfuncsym = NULL;

  // If this is a new function declaration, get a label ID
  // for the end label and add the fucntion to the symbol table
  if (oldfuncsym == NULL) {
    endlabel = genlabel();
    newfuncsym = addglob(funcname, type, NULL, S_FUNCTION, C_GLOBAL, endlabel);
  }

  lparen();
  paramcnt = param_declaration_list(oldfuncsym, newfuncsym);
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
    return oldfuncsym;
  }

  // If we get to this point, it means that we handling
  // a full function declaration and not just a prototype
  Functionid = oldfuncsym;

  // Parse function body
  Looplevel = 0;
  Switchlevel = 0;
  lbrace();
  tree = compound_statement(0);
  rbrace();

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

  tree = mkastunary(A_FUNCTION, type, tree, oldfuncsym, endlabel);

  if (O_dumpAST) {
    dumpAST(tree, NOLABEL, 0);
    fprintf(stdout, "\n\n");
  }

  genAST(tree, NOLABEL, NOLABEL, NOLABEL, 0);

  freeloclsyms();

  return oldfuncsym;
}

static struct symtable *array_declaration(char *varname, int type,
    struct symtable *ctype, int class) {
  struct symtable *sym = NULL;
  // Skip past '['
  scan(&Token);

  // Check for array size
  if (Token.token == T_INTLIT) {
    switch(class) {
      case C_EXTERN:
      case C_GLOBAL:
        sym = addglob(varname, pointer_to(type), ctype, S_ARRAY, class, Token.intvalue);
        break;
      case C_LOCAL:
      case C_PARAM:
      case C_MEMBER:
        fatal("For now, declaration of non-global arrays is not supported");
    }
  }

  // Ensure we have a following ']'
  scan(&Token);
  match(T_RBRACKET, "]");

  return sym;
}

static struct symtable *scalar_declaration(char *varname, int type,
    struct symtable *ctype, int class) {
  switch (class) {
    case C_EXTERN:
    case C_GLOBAL:
      return addglob(varname, type, ctype, S_VARIABLE, class, 1);
    case C_LOCAL:
      return addlocl(varname, type, ctype, S_VARIABLE, 1);
    case C_PARAM:
      return addparm(varname, type, ctype, S_VARIABLE, 1);
    case C_MEMBER:
      return addmemb(varname, type, ctype, S_VARIABLE, 1);
  }

  return NULL;
}

static void array_initialisation(struct symtable *sym, int type,
    struct symtable *ctype, int class) {
  fatal("No array initialisation yet");
}

static struct symtable *symbol_declaration(int type, struct symtable *ctype, int class) {
  struct symtable *sym = NULL;
  // We might scan in more identifiers for assignment expressions,
  // hence we copy the name just in case.
  char *varname = strdup(Text);
  int stype = S_VARIABLE;

  // Assume that it will be a scalar variable
  ident();

  if (Token.token == T_LPAREN) {
    return function_declaration(varname, type, ctype, class);
  }

  switch (class) {
    case C_EXTERN:
    case C_GLOBAL:
      if (findglob(varname) != NULL)
        fatals("Duplicate global variable declaration", varname);
    case C_LOCAL:
    case C_PARAM:
      if (findlocl(varname) != NULL)
        fatals("Duplicate local variable declaration", varname);
    case C_MEMBER:
      if (findmember(varname) != NULL)
        fatals("Duplicate struct/union member declaration", varname);
  }

  if (Token.token == T_LBRACKET) {
    sym = array_declaration(varname, type, ctype, class);
    stype = S_ARRAY;
  } else {
    sym = scalar_declaration(varname, type, ctype, class);
  }

  if (Token.token == T_ASSIGN) {
    if (class == C_PARAM)
      fatals("Initialisation of a parameter not permitted", varname);
    if (class == C_MEMBER)
      fatals("Initialisation of a member not permitted", varname);
    scan(&Token);

    if (stype == S_ARRAY)
      array_initialisation(sym, type, ctype, class);
    else {
      fatal("Scalar variable initialisation not done yet");
      // Var initialisation
    }
  }

  return sym;
}

// Parse a list of symbols where there is an initial type
// Return the type of the symbols, et1 and et2 are end tokens
int declaration_list(struct symtable **ctype, int class, int et1, int et2) {
  int inittype, type;
  struct symtable *sym;

  // Get the initial type, if we get -1, i.e. it
  // was a composite type definition, return it
  // right away
  if ((inittype = parse_type(ctype, &class)) == -1)
    return inittype;

  while (1) {
    // Check if this symbol is a pointer
    type = parse_stars(inittype);

    // Parse this symbol
    sym = symbol_declaration(type, *ctype, class);

    // If we parsed a function, there is no list
    // hence we return right away
    if (sym->stype == S_FUNCTION) {
      if (class != C_GLOBAL)
        fatal("Function definition not at global level");
      return type;
    }

    // If we hit an endtoken, return
    if (Token.token == et1 || Token.token == et2)
      return type;

    comma();
  }
}

static int param_declaration_list(struct symtable *oldfuncsym, struct symtable *newfuncsym) {
  int type, paramcnt = 0;
  struct symtable *ctype;
  struct symtable *protoptr = NULL;

  if (oldfuncsym != NULL)
    protoptr = oldfuncsym->member;

  while (Token.token != T_RPAREN) {
    type = declaration_list(&ctype, C_PARAM, T_COMMA, T_RPAREN);
    if (type == -1)
      fatal("Bad type in parameter list");

    if (protoptr != NULL) {
      if (type != protoptr->type)
        fatald("Type doesn't match prototype for parameter", paramcnt + 1);
      protoptr = protoptr->next;
    }
    paramcnt++;

    if (Token.token == T_RPAREN)
      break;
    comma();
  }

  if (oldfuncsym != NULL && paramcnt != oldfuncsym->nelems)
    fatals("Parameter count mismatch for function", oldfuncsym->name);

  return paramcnt;
}


void global_declarations(void) {
   struct symtable *ctype;
   while (Token.token != T_EOF) {
     declaration_list(&ctype, C_GLOBAL, T_SEMI, T_EOF);

     if (Token.token == T_SEMI)
       scan(&Token);
   }
}
