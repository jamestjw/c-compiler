#include "data.h"
#include "decl.h"
#include "expr.h"
#include "gen.h"
#include "misc.h"
#include "scan.h"
#include "sym.h"
#include "tree.h"
#include "types.h"

// Contains most recently scanned token from input
struct token Token;
struct token Peektoken;    // A look-ahead token

// Operator precedence for each token
// A bigger number indicates a higher precedence
static int OpPrec[] = {
  0, 10, 10,			// T_EOF, T_ASSIGN, T_ASPLUS,
  10, 10,			    // T_ASMINUS, T_ASSTAR,
  10, 10,			    // T_ASSLASH, T_ASMOD,
  15,				      // T_QUESTION,
  20, 30,			    // T_LOGOR, T_LOGAND
  40, 50, 60,			// T_OR, T_XOR, T_AMPER
  70, 70,			    // T_EQ, T_NE
  80, 80, 80, 80,	// T_LT, T_GT, T_LE, T_GE
  90, 90,			    // T_LSHIFT, T_RSHIFT
  100, 100,			  // T_PLUS, T_MINUS
  110, 110, 110		// T_STAR, T_SLASH, T_MOD
};

// Convert a token type into an AST operation (AST node type)
int binastop(int tokentype) {
  // For tokens in this range, there is a 1-1 mapping between
  // token type and node type
  if (tokentype > T_EOF && tokentype <= T_MOD)
    return tokentype;

  fatald("Syntax error, token", tokentype);

  return -1;
}

// Check that we have a binary operator and return
// its precedence.
// This function serves to enforce valid syntax, when
// we ask for a token's precedence, we expect the token
// to be some operator, this function throws an error when
// that expectation is not met.
static int op_precedence(int tokentype) {
  int prec = OpPrec[tokentype];

  if (tokentype > T_MOD)
    fatald("Token with no precedence in op_precedence", tokentype);

  if (prec == 0) {
    fprintf(stderr, "syntax error on line %d token %d\n", Line, tokentype);
    exit(1);
  }

  return prec;
}

// Returns true if token is a right-associative operator
static int rightassoc(int tokentype) {
  if (tokentype == T_ASSIGN && tokentype <= T_ASSLASH)
    return 1;
  return 0;
}

// Before invoking this function, the identifier
// should have already been consumed. The current
// token should be pointing at a '['
static struct ASTnode *array_access(struct ASTnode *left) {
  struct ASTnode *right;

  // Check that the sub-tree is a pointer
  if (!ptrtype(left->type))
    fatal("Not an array or pointer");

  // Consume the '['
  scan(&Token);

  right = binexpr(0);

  match(T_RBRACKET, "]");

  if (!inttype(right->type))
    fatal("Array index is not of integer type");

  left->rvalue = 1;

  // Scale the index by the size of the element's type
  right = modify_type(right, left->type, left->ctype, A_ADD);

  // Add offset to base of the array and dereference it
  left = mkastnode(A_ADD, left->type, left->ctype, left, NULL, right, NULL, 0);
  left = mkastunary(A_DEREF, value_at(left->type), left->ctype, left, NULL, 0);
  return left;
}

static struct ASTnode *member_access(struct ASTnode *left, int withpointer) {
  struct ASTnode *right;
  struct symtable *typeptr;
  struct symtable *m;

  // Verify that the left AST tree is a pointer to a struct or union
  if (withpointer &&
      left->type != pointer_to(P_STRUCT) &&
      left->type != pointer_to(P_UNION))
    fatal("Expression is not a pointer to a struct/union");

  // Or, check that the left AST tree is a struct or union.
  // If so, change it from A_IDENT to A_ADDR so that we get
  // the base address instead of the value at the address
  if (!withpointer) {
    if (left->type == P_STRUCT || left->type == P_UNION)
      left->op = A_ADDR;
    else
      fatal("Expression is not a struct/union");
  }

  typeptr = left->ctype;
  // Skip the '.' or '->' token and get the member's name
  scan(&Token);
  ident();

  for (m = typeptr->member; m != NULL; m = m->next)
    if (!strcmp(m->name, Text))
      break;
  if (m == NULL)
    fatals("No member found in struct/union", Text);

  left->rvalue = 1;
  // Build a node with the offset of the member from the base address
  right = mkastleaf(A_INTLIT, P_INT, NULL, NULL, m->st_posn);
  // Add the member's offset to the base of the struct and dereference it
  left = mkastnode(A_ADD, pointer_to(m->type), m->ctype, left, NULL, right, NULL, 0);
  left = mkastunary(A_DEREF, m->type, m->ctype, left, NULL, 0);

  return left;
}

// Parse a parenthesised expression and return
// an AST node representing it.
static struct ASTnode *paren_expression(int ptp) {
  struct ASTnode *n;
  int type = 0;
  struct symtable *ctype = NULL;

  // Consume the '('
  scan(&Token);

  switch (Token.token) {
    case T_IDENT:
      if (findtypedef(Text) == NULL) {
        n = binexpr(0); // ptp is zero as expression is inside parentheses
        break;
      }
    case T_VOID:
    case T_CHAR:
    case T_INT:
    case T_LONG:
    case T_STRUCT:
    case T_UNION:
    case T_ENUM:
      // Handle type casts
      type = parse_cast(&ctype);
      rparen();
    default:
      n = binexpr(ptp); // We pass in the ptp from before as a typecast
                        // doesn't change the precedence of the expr
  }

  // We now have an expression, and a non-zero type if
  // there was a type cast.
  // Consume the closing ')' if there was no cast
  if (type == 0)
    rparen();
  else
    // Otherwise make a unary AST node for the cast
    n = mkastunary(A_CAST, type, ctype, n, NULL, 0);

  return n;
}

// Parse a primary factor and return a node representing it
static struct ASTnode *primary(int ptp) {
  struct ASTnode *n = NULL;
  struct symtable *enumptr;
  struct symtable *varptr;
  int id;
  int type = 0;
  int size, class;
  struct symtable *ctype;

  switch (Token.token) {
    case T_STATIC:
    case T_EXTERN:
      fatal("Compiler doesn't support static or extern local declarations");
    case T_SIZEOF:
      scan(&Token);
      if (Token.token != T_LPAREN)
        fatal("Left parenthesis expected after sizeof");
      scan(&Token);

      type = parse_stars(parse_type(&ctype, &class));
      size = typesize(type, ctype);
      rparen();

      return mkastleaf(A_INTLIT, P_INT, NULL, NULL, size);
    case T_INTLIT:
      // Create AST node with P_CHAR type if within
      // P_CHAR range
      if ((Token.intvalue) >= 0 && (Token.intvalue < 256))
        n = mkastleaf(A_INTLIT, P_CHAR, NULL, NULL, Token.intvalue);
      else
        n = mkastleaf(A_INTLIT, P_INT, NULL, NULL, Token.intvalue);
      break;
    case T_STRLIT:
      id = genglobstr(Text, 0); // 0 indicates that we should generate a label here

      // To support the following syntax
      // char *c = "Hello " "world" " !";
      while (1) {
        scan(&Peektoken);
        // Stop looping if there are no more upcoming strlits
        if (Peektoken.token != T_STRLIT) break;
        genglobstr(Text, 1);  // 1 indicates that we should not generate a label here
        scan(&Token);
      }

      genglobstrend();
      n = mkastleaf(A_STRLIT, pointer_to(P_CHAR), NULL, NULL, id);
      break;
    case T_IDENT:
      // If identifier matches an enum, return an A_INTLIT node
      // corresponding to the enum val.
      if ((enumptr = findenumval(Text)) != NULL) {
        n = mkastleaf(A_INTLIT, P_INT, NULL, NULL, enumptr->st_posn);
        break;
      }
      // See if this identifier exists as a symbol. For arrays,
      // we set the rvalue to 1.
      if ((varptr = findsymbol(Text)) == NULL) {
        fatals("Unknown variable or function", Text);
      }
      switch (varptr->stype) {
        case S_VARIABLE:
          n = mkastleaf(A_IDENT, varptr->type, varptr->ctype, varptr, 0);
          break;
        case S_ARRAY:
          n = mkastleaf(A_ADDR, varptr->type, varptr->ctype, varptr, 0);
          n->rvalue = 1;
          break;
        case S_FUNCTION:
          // Function call, check that if we have a left parenthesis
          scan(&Token);
          if (Token.token != T_LPAREN)
            fatals("Function name used without parentheses", Text);
          return funccall();
        default:
          fatals("Identifier not a scalar or array variable", Text);
      }
      break;
    case T_LPAREN:
      return paren_expression(ptp);
    default:
      fatals("Syntax error, token", Token.tokstr);
  }

  scan(&Token);
  return n;
}

// Parse a postfix expression and return an AST
// node representing it. The identifier is already
// in Text
static struct ASTnode *postfix(int ptp) {
  struct ASTnode *n;

  // Get the primary expression
  n = primary(ptp);

  while(1) {
    switch (Token.token) {
      case T_LBRACKET:
        n = array_access(n);
        break;
      case T_DOT:
        n = member_access(n, 0);
        break;
      case T_ARROW:
        n = member_access(n, 1);
        break;
      case T_INC:
        if (n->rvalue == 1)
          fatals("Cannot ++ on rvalue", Text);
        scan(&Token);
        if (n->op == A_POSTINC || n->op == A_POSTDEC)
          fatal("Cannot ++ and/or -- more than once");

        n->op = A_POSTINC;
        break;
      case T_DEC:
        if (n->rvalue == 1)
          fatal("Cannot -- on rvalue");
        scan(&Token);
        if (n->op == A_POSTINC || n->op == A_POSTDEC)
          fatal("Cannot ++ and/or -- more than once");

        n->op = A_POSTDEC;
        break;
      default:
        return n;
    }
  }

  return NULL;
}

struct ASTnode *prefix(int ptp) {
  struct ASTnode *tree;

  switch (Token.token) {
    case T_AMPER:
      scan(&Token);
      tree = prefix(ptp);

      if (tree->op != A_IDENT) {
        fatal("& operator must be followed by an identifier");
      }

      // Prevent '&' being performed on an array
      if (tree->sym->stype == S_ARRAY)
        fatal("& operator cannot be performed on an array");

      // Change the operator to A_ADDR, and change the type
      tree->op = A_ADDR;
      tree->type = pointer_to(tree->type);
      break;
    case T_STAR:
      scan(&Token);
      tree = prefix(ptp);
      tree->rvalue= 1;

      if (!ptrtype(tree->type)) {
        fatal("* operator must be followed by an identifier or *");
      }

      tree = mkastunary(A_DEREF, value_at(tree->type), tree->ctype, tree, NULL, 0);
      break;
    case T_MINUS:
      scan(&Token);
      tree = prefix(ptp);

      tree->rvalue = 1;
      // Widen to int in case we are operating on
      // chars that are unsigned
      if (tree->type == P_CHAR)
        tree->type = P_INT;

      tree = mkastunary(A_NEGATE, tree->type, tree->ctype, tree, NULL, 0);
      break;
    case T_INVERT:
      scan(&Token);
      tree = prefix(ptp);
      tree->rvalue = 1;
      tree = mkastunary(A_INVERT, tree->type, tree->ctype, tree, NULL, 0);
      break;
    case T_LOGNOT:
      scan(&Token);
      tree = prefix(ptp);
      tree->rvalue = 1;
      tree = mkastunary(A_LOGNOT, tree->type, tree->ctype, tree, NULL, 0);
      break;
    case T_INC:
      scan(&Token);
      tree = prefix(ptp);
      // TODO: Support other node types, e.g. addresses
      if (tree->op != A_IDENT)
        fatal("++ operator must be followed by an identifier");

      tree = mkastunary(A_PREINC, tree->type, tree->ctype, tree, NULL, 0);
      break;
    case T_DEC:
      scan(&Token);
      tree = prefix(ptp);
      // TODO: Support other node types, e.g. addresses
      if (tree->op != A_IDENT)
        fatal("-- operator must be followed by an identifier");

      tree = mkastunary(A_PREDEC, tree->type, tree->ctype, tree, NULL, 0);
      break;
    default:
      tree = postfix(ptp);
  }

  return tree;
}

// Return a AST node whose root is a binary operator
// Parameter ptp is the precedence of the previous token
struct ASTnode *binexpr(int ptp) {
  struct ASTnode *left, *right;
  struct ASTnode *ltemp, *rtemp;
  int ASTop;
  int tokentype;

  // Build the left node using a prefix
  left = prefix(ptp);

  tokentype = Token.token;

  // If we hit a semi colon, return the left node
  // Ensure that we have a binary operator
  if ((tokentype == T_SEMI) || (tokentype < T_ASSIGN) || (tokentype > T_MOD)) {
    left->rvalue = 1;
    return left;
  }

  // While the current token's precedence is
  // higher than that of the previous token,
  // or if the token is a right-associative
  // operator whose precedence is equal to
  // that of the previous token.
  while((op_precedence(tokentype) > ptp) ||
      (rightassoc(tokentype) && op_precedence(tokentype) == ptp)) {
    scan(&Token);

    // Recursively call this function to build a
    // subtree while passing in the precedence of the
    // current token
    right = binexpr(OpPrec[tokentype]);

    // Ensure that the two types are compatible by
    // trying to modify each tree to match the other
    // type.
    ASTop = binastop(tokentype);

    switch (ASTop) {
      case A_TERNARY:
        match(T_COLON, ":");
        ltemp = binexpr(0);
        // TODO: For the type, we should choose the wider of the TRUE and FALSE expressions
        return mkastnode(A_TERNARY, right->type, right->ctype, left, right, ltemp, NULL, 0);
      case A_ASSIGN:
        right->rvalue = 1;
        // Ensure that right expression matches the left
        right = modify_type(right, left->type, left->ctype, 0);

        if (right == NULL)
          fatal("Incompatible expression in assignment");
        // Swap the left and right trees so that code for the
        // right expression will be generated before the left
        ltemp = left; left = right; right = ltemp;
        break;
      default:
        // Since we are not doing an assignment or ternary,
        // both trees should be rvalues
        left->rvalue = right->rvalue = 1;

        ltemp = modify_type(left, right->type, right->ctype, ASTop);
        rtemp = modify_type(right, left->type, left->ctype, ASTop);
        if (ltemp == NULL && rtemp == NULL) {
          fatal("Incompatible types in binary expression");
        }
        // When we successfully modify one tree to fit the
        // other, we leave the other one as it is.
        if (ltemp != NULL)
          left = ltemp;
        if (rtemp != NULL)
          right = rtemp;
    }

    // Join that subtree with the left node
    // The node contains an expression of the same type
    // as the left node.
    left = mkastnode(binastop(tokentype), left->type, left->ctype, left, NULL, right, NULL, 0);

    tokentype = Token.token;

    // If we hit a semi colon, return the left node
    // Ensure that we have a binary operator
    if ((tokentype == T_SEMI) || (tokentype < T_ASSIGN) || (tokentype > T_MOD)) {
      left->rvalue = 1;
      return left;
    }
  }

  // When the next operator has a precedence equal or lower,
  // we return the tree
  left->rvalue = 1;
  return left;
}

struct ASTnode *expression_list(int endtoken) {
  struct ASTnode *tree = NULL;
  struct ASTnode *child = NULL;
  int exprcount = 0;

  // Loop until we hit right parenthesis
  while (Token.token != endtoken) {
    child = binexpr(0);
    exprcount++;

    // Build an A_GLUE node with the previous tree as the left child
    // and the new expression as the right child
    tree = mkastnode(A_GLUE, P_NONE, NULL, tree, NULL, child, NULL, exprcount);

    if (Token.token == endtoken) break;

    match(T_COMMA, ",");
  }

  return tree;
}

struct ASTnode *funccall(void) {
  struct ASTnode *tree;
  struct symtable *funcptr;

  // Check that the function has been declared
  if ((funcptr = findsymbol(Text)) == NULL || funcptr->stype != S_FUNCTION) {
    fatals("Undeclared function", Text);
  }

  lparen();
  // Parse the argument expression list
  tree = expression_list(T_RPAREN);


  // TODO: Check argument type against function prototype

  // Store the function's return type as this node's type
  // along with the symbol ID
  tree = mkastunary(A_FUNCCALL, funcptr->type, funcptr->ctype, tree, funcptr, 0);

  rparen();

  return tree;
}
