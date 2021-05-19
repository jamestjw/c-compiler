#include "defs.h"
#include "data.h"
#include "expr.h"
#include "scan.h"
#include "tree.h"

// Contains most recently scanned token from input
struct token Token;

// Operator precedence for each token
// A bigger number indicates a higher precedence
static int OpPrec[] = {
  0,  // EOF
  10, // +
  10, // -
  20, // *
  20, // /
  0,  // INTLIT
};

// Convert a token type into an AST operation (AST node type)
int arithop(int tok) {
  switch (tok) {
    case T_PLUS:
      return A_ADD;
    case T_MINUS:
      return A_SUBTRACT;
    case T_STAR:
      return A_MULTIPLY;
    case T_SLASH:
      return A_DIVIDE;
    default:
      fprintf(stderr, "unknown arithmetic operator detected on line %d\n", Line);
      exit(1);
  }
}

// Check that we have a binary operator and return
// its precedence.
// This function serves to enforce valid syntax, when
// we ask for a token's precedence, we expect the token
// to be some operator, this function throws an error when
// that expectation is not met.
static int op_precedence(int tokentype) {
  int prec = OpPrec[tokentype];
  if (prec == 0) {
    fprintf(stderr, "syntax error on line %d token %d\n", Line, tokentype);
    exit(1);
  }

  return prec;
}

// Parse a primary factor and return a node representing it
static struct ASTnode *primary(void) {
  struct ASTnode *n;

  switch (Token.token) {
    case T_INTLIT:
      n = mkastleaf(A_INTLIT, Token.intvalue);
      scan(&Token);
      return n;
    default:
      fprintf(stderr, "syntax error on line %d, expected an integer literal\n", Line);
      exit(1);
  }
}

// Return a AST node whose root is a binary operator
// Parameter ptp is the precedence of the previous token
struct ASTnode *binexpr(int ptp) {
  struct ASTnode *left, *right;
  int tokentype;

  // Build the left node using an integer literal
  left = primary();

  tokentype = Token.token;
  // If no tokens are left, return the left node
  if (tokentype == T_EOF) return left;

  // While the current token's precedence is
  // higher than that of the previous token
  while(op_precedence(tokentype) > ptp) {
    scan(&Token);

    // Recursively call this function to build a
    // subtree while passing in the precedence of the
    // current token
    right = binexpr(OpPrec[tokentype]);

    // Join that subtree with the left node
    left  = mkastnode(arithop(tokentype), left, right, 0);

    tokentype = Token.token;
    if (tokentype == T_EOF) return left;
  }

  // When the next operator has a precedence equal or lower,
  // we return the tree
  return left;
}
