#include "data.h"
#include "expr.h"
#include "gen.h"
#include "misc.h"
#include "scan.h"
#include "sym.h"
#include "tree.h"
#include "types.h"

// Contains most recently scanned token from input
struct token Token;

// Operator precedence for each token
// A bigger number indicates a higher precedence
static int OpPrec[] = {
  0, 10,          // T_EOF, T_ASSIGN
  60, 60,         // T_PLUS, T_MINUS
  70, 70,         // T_STAR, T_SLASH
  30, 30,         // T_EQ, T_NE
  40, 40, 40, 40  // T_LT, T_GT, T_LE, T_GE
};

// Convert a token type into an AST operation (AST node type)
int binastop(int tokentype) {
  // For tokens in this range, there is a 1-1 mapping between
  // token type and node type
  if (tokentype > T_EOF && tokentype < T_INTLIT) 
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
  
  if (tokentype >= T_VOID)
    fatald("Token with no precedence in op_precedence", tokentype);

  if (prec == 0) {
    fprintf(stderr, "syntax error on line %d token %d\n", Line, tokentype);
    exit(1);
  }

  return prec;
}

// Returns true if token is a right-associative operator
static int rightassoc(int tokentype) {
  if (tokentype == T_ASSIGN)
    return 1;
  return 0;
}

// Before invoking this function, the identifier
// should have already been consumed. The current
// token should be pointing at a '['
static struct ASTnode *array_access(void) {
  struct ASTnode *left, *right;
  int id;

  // Ensure that such an identifier exists and points to
  // an array
  if ((id = findglob(Text)) == -1 || Gsym[id].stype != S_ARRAY) {
    fatals("Undeclared array", Text);
  }

  left = mkastleaf(A_ADDR, Gsym[id].type, id);

  // Consume the '['
  scan(&Token);

  right = binexpr(0);

  match(T_RBRACKET, "]");

  if (!inttype(right->type))
    fatal("Array index is not of integer type");

  // Scale the index by the size of the element's type
  right = modify_type(right, left->type, A_ADD);

  // Add offset to base of the array and dereference it
  left = mkastnode(A_ADD, Gsym[id].type, left, NULL, right, 0);
  left = mkastunary(A_DEREF, value_at(left->type), left, 0);
  return left;
}

// Parse a primary factor and return a node representing it
static struct ASTnode *primary(void) {
  struct ASTnode *n = NULL;
  int id;

  switch (Token.token) {
    case T_INTLIT:
      // Create AST node with P_CHAR type if within
      // P_CHAR range
      if ((Token.intvalue) >= 0 && (Token.intvalue < 256))
        n = mkastleaf(A_INTLIT, P_CHAR, Token.intvalue);
      else
        n = mkastleaf(A_INTLIT, P_INT, Token.intvalue);
      break;
    case T_STRLIT:
      id = genglobstr(Text);
      n = mkastleaf(A_STRLIT, P_CHARPTR, id);
      break;
    case T_IDENT:
      // In order to know if this is a variable, a function
      // call or an array index so  we need to look at 
      // the subsequent token
      scan(&Token);

      if (Token.token == T_LPAREN)
        return funccall();

      if (Token.token == T_LBRACKET) {
        return array_access();
      }

      // Reject the new token if we did not encounter a left parenthesis
      reject_token(&Token);

      id = findglob(Text);
      if (id == -1)
        fatals("Unknown variable", Text);
      n = mkastleaf(A_IDENT, Gsym[id].type, id);
      break;
    case T_LPAREN:
      // Consume the '('
      scan(&Token);
      // Parse the expression
      n = binexpr(0);
      // Match a ')'
      rparen();

      return n;
    default:
      fatald("Syntax error, token", Token.token);
  }
  
  scan(&Token);
  return n;
}

struct ASTnode *prefix(void) {
  struct ASTnode *tree;

  switch (Token.token) {
    case T_AMPER:
      scan(&Token);
      tree = prefix();

      if (tree->op != A_IDENT)
        fatal("& operator must be followed by an identifier");

      // Change the operator to A_ADDR, and change the type
      tree->op = A_ADDR;
      tree->type = pointer_to(tree->type);
      break;
    case T_STAR:
      scan(&Token);
      tree = prefix();

      if (tree->op != A_IDENT && tree->op != A_DEREF)
        fatal("* operator must be followed by an identifier or *");
      tree = mkastunary(A_DEREF, value_at(tree->type), tree, 0);
      break;
    default:
      tree = primary();
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
  left = prefix();

  tokentype = Token.token;

  // If we hit a semi colon, return the left node
  // Ensure that we have a binary operator
  if ((tokentype == T_SEMI) || (tokentype < T_ASSIGN) || (tokentype > T_GE)) {
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

    // Assignment expressions are a special case
    if (ASTop == A_ASSIGN) {
      right->rvalue = 1;
      // Ensure that right expression matches the left
      right = modify_type(right, left->type, 0);
      if (right == NULL)
        fatal("Incompatible expression in assignment");

     // Swap the left and right trees so that code for the
     // right expression will be generated before the left
     ltemp = left; left = right; right = ltemp;
    } else {
      // Since we are not doing an assignment, both trees should
      // be rvalues
      left->rvalue = right->rvalue = 1;

      ltemp = modify_type(left, right->type, ASTop);
      rtemp = modify_type(right, left->type, ASTop);
      if (ltemp == NULL && rtemp == NULL)
        fatal("Incompatible types in binary expression");
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
    left = mkastnode(binastop(tokentype), left->type, left, NULL, right, 0);

    tokentype = Token.token;

    // If we hit a semi colon, return the left node
    // Ensure that we have a binary operator
    if ((tokentype == T_SEMI) || (tokentype < T_ASSIGN) || (tokentype > T_GE)) {
      left->rvalue = 1;
      return left;
    }
  }

  // When the next operator has a precedence equal or lower,
  // we return the tree
  return left;
}

struct ASTnode *funccall(void) {
  struct ASTnode *tree;
  int id;
  
  // Check that the function has been declared
  // TODO: Check if stype == S_FUNCTION
  if ((id = findglob(Text)) == -1) {
    fatals("Undeclared function", Text);
  }

  lparen();
  tree = binexpr(0);

  // Store the function's return type as this node's type
  // along with the symbol ID
  tree = mkastunary(A_FUNCCALL, Gsym[id].type, tree, id);

  rparen();

  return tree;
}

