#include "data.h"
#include "decl.h"
#include "expr.h"
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
  struct ASTnode *unused;
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
    ctype = addstruct(Text);
  else
    ctype = addunion(Text);
  scan(&Token);

  while (1) {
    t = declaration_list(&m ,C_MEMBER, T_SEMI, T_RBRACE, &unused);
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

  addtypedef(Text, type, *ctype);
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

// Parsing of '(' and ')' should be done
// by the caller of this function
int parse_cast(void) {
  int type, class;
  struct symtable *ctype;

  type = parse_stars(parse_type(&ctype, &class));

  if (type == P_STRUCT || type == P_UNION || type == P_VOID)
    fatal("Cannot cast to a struct, union or void type");

  return type;
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
    newfuncsym = addglob(funcname, type, NULL, S_FUNCTION, C_GLOBAL, 0, endlabel);
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
  int nelems = -1;    // Assume number of elements is not given
  int maxelems;       // Max number of elems in the init list
  int *initlist;      // List of initial elements
  int i = 0, j;
  int casttype, newtype;

  // Skip past '['
  scan(&Token);

  if (Token.token == T_INTLIT) {
    if (Token.intvalue <= 0)
      fatald("Array size is illegal", Token.intvalue);
    nelems = Token.intvalue;
    scan(&Token);
  }

  match(T_RBRACKET, "]");

  switch(class) {
    case C_EXTERN:
    case C_GLOBAL:
      sym = addglob(varname, pointer_to(type), ctype, S_ARRAY, class, 0, 0);
      break;
    case C_LOCAL:
    case C_PARAM:
    case C_MEMBER:
      fatal("For now, declaration of non-global arrays is not supported");
  }

  if (Token.token == T_ASSIGN) {
    if (class != C_GLOBAL)
      fatals("Variable can not be initialised", varname);
    scan(&Token);

    match(T_LBRACE, "{");

#define TABLE_INCREMENT 10

    // If the array already has nelems, allocate that many elements
    // Otherwise, start with TABLE_INCREMENT
    if (nelems != -1)
      maxelems = nelems;
    else
      maxelems = TABLE_INCREMENT;

    initlist = (int *) malloc(maxelems * sizeof(int));

    // Loop getting new literal value and adding to initlist
    while (1) {
      // Get original type
      newtype = type;

      // If the number of elems in the list exceeds the
      // size that was specified before, raise an error
      if (nelems != -1 && i == maxelems)
        fatal("Too many values in initialisation list");

      if (Token.token == T_LPAREN) {
        scan(&Token);
        casttype = parse_cast();
        rparen();

        // Check that the two types are compatible, change the newtype so
        // that parse_literal works since a void * can be assigned to any
        // ptr type
        if (casttype == type || (casttype == pointer_to(P_VOID) && ptrtype(type)))
          newtype = P_NONE;
        else
          fatal("Type mismatch");
        newtype = P_NONE;
      }

      initlist[i++] = parse_literal(newtype);
      scan(&Token);

      // If original size was not specified, we increase
      // the list size
      if (nelems == -1 && i == maxelems) {
        maxelems += TABLE_INCREMENT;
        initlist = (int *) realloc(initlist, maxelems * sizeof(int));
      }

      if (Token.token == T_RBRACE) {
        scan(&Token);
        break;
      }

      comma();
    }

    for (j = i; j < maxelems; j++)
      initlist[j] = 0;
    if (i > nelems)
      nelems = i;
    sym->initlist = initlist;
  }

  sym->nelems = nelems;
  sym->size = sym->nelems * typesize(type, ctype);
  if (class == C_GLOBAL)
    genglobsym(sym);

  return sym;
}

static struct symtable *scalar_declaration(char *varname, int type,
    struct symtable *ctype, int class, struct ASTnode **tree) {
  struct symtable *sym = NULL;
  struct ASTnode *varnode, *exprnode;
  int casttype;
  *tree = NULL;

  switch (class) {
    case C_EXTERN:
    case C_GLOBAL:
      sym = addglob(varname, type, ctype, S_VARIABLE, class, 1, 0);
      break;
    case C_LOCAL:
      sym = addlocl(varname, type, ctype, S_VARIABLE, 1);
      break;
    case C_PARAM:
      sym = addparm(varname, type, ctype, S_VARIABLE);
      break;
    case C_MEMBER:
      sym = addmemb(varname, type, ctype, S_VARIABLE, 1);
      break;
  }

  if (Token.token == T_ASSIGN) {
    // Our compiler only allows this for global and local vars
    if (class != C_GLOBAL && class != C_LOCAL)
      fatals("Variable can not be initialised", varname);
    scan(&Token);

    if (class == C_GLOBAL) {
      if (Token.token == T_LPAREN) {
        scan(&Token);
        casttype = parse_cast();
        rparen();

        // A 'void *' cast type can be assigned to any ptr type
        if (casttype == type || (casttype == pointer_to(P_VOID) && ptrtype(type)))
          // Relax parse type restrictions when we pass this to parse_literal
          // We can do this since either the cast type matches the var type
          // or we have a void*
          type = P_NONE;
        else
          fatal("Type mismatch");
      }
      sym->initlist = (int *) malloc(sizeof(int));
      sym->initlist[0] = parse_literal(type);
      // Consume the literal
      scan(&Token);
    } else if (class == C_LOCAL) {
      varnode = mkastleaf(A_IDENT, sym->type, sym, 0);
      // Get the expression for the assignment as an r-value
      exprnode = binexpr(0);
      exprnode->rvalue = 1;

      exprnode = modify_type(exprnode, varnode->type, 0);
      if (exprnode == NULL)
        fatal("Incompatible expression in assignment");

      *tree = mkastnode(A_ASSIGN, exprnode->type, exprnode, NULL, varnode, NULL, 0);
    }
  }

  if (class == C_GLOBAL)
    genglobsym(sym);

  return sym;
}

// Given a type, check that the current token is a literal of
// that type. If it's an integer literal, return this value. If
// it's a string literal, return the label number of the string.
// It does not consume the current token. Relax parsing restrictions
// if type is P_NONE.
int parse_literal(int type) {
  if (Token.token == T_STRLIT) {
    if (type == pointer_to(P_CHAR) || type == P_NONE)
      return genglobstr(Text);
  }

  if (Token.token == T_INTLIT) {
    switch (type) {
      case P_CHAR:
        if (Token.intvalue < 0 || Token.intvalue > 255)
          fatal("Integer literal value too big for char type");
      case P_NONE:
      case P_INT:
      case P_LONG:
        break;
      default:
        fatal("Type mismatch: integer literal vs. variable");
    }
  } else {
    fatal("Expecting an integer literal value");
  }

  return Token.intvalue;
}

static struct symtable *symbol_declaration(int type, struct symtable *ctype, int class,
    struct ASTnode **tree) {
  struct symtable *sym = NULL;
  // We might scan in more identifiers for assignment expressions,
  // hence we copy the name just in case.
  char *varname = strdup(Text);

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
  } else {
    sym = scalar_declaration(varname, type, ctype, class, tree);
  }

  return sym;
}

// Parse a list of symbols where there is an initial type
// Return the type of the symbols, et1 and et2 are end tokens
int declaration_list(struct symtable **ctype, int class, int et1, int et2,
    struct ASTnode **gluetree) {
  int inittype, type;
  struct symtable *sym;
  struct ASTnode *tree = NULL;
  *gluetree = NULL;

  // Get the initial type, if we get -1, i.e. it
  // was a composite type definition, return it
  // right away
  if ((inittype = parse_type(ctype, &class)) == -1)
    return inittype;

  while (1) {
    // Check if this symbol is a pointer
    type = parse_stars(inittype);
   
    // Parse this symbol
    sym = symbol_declaration(type, *ctype, class, &tree);
    
    // If we parsed a function, there is no list
    // hence we return right away
    if (sym->stype == S_FUNCTION) {
      if (class != C_GLOBAL)
        fatal("Function definition not at global level");
      return type;
    }
  
    if (tree != NULL) {
      if (*gluetree == NULL) {
        *gluetree = tree;
      } else {
        *gluetree = mkastnode(A_GLUE, P_NONE, *gluetree, NULL, tree, NULL, 0);
      }
      tree = NULL;
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
  struct ASTnode *unused;

  if (oldfuncsym != NULL)
    protoptr = oldfuncsym->member;

  while (Token.token != T_RPAREN) {
    type = declaration_list(&ctype, C_PARAM, T_COMMA, T_RPAREN, &unused);
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
   struct ASTnode *unused;
   while (Token.token != T_EOF) {
     declaration_list(&ctype, C_GLOBAL, T_SEMI, T_EOF, &unused);

     if (Token.token == T_SEMI)
       scan(&Token);
   }
}
