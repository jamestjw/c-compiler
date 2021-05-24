#include "data.h"
#include "expr.h"
#include "misc.h"
#include "scan.h"
#include "sym.h"
#include "tree.h"

// Contains most recently scanned token from input
struct token Token;

// Operator precedence for each token
// A bigger number indicates a higher precedence
static int OpPrec[] = {
  0, 60, 60,      // T_EOF, T_PLUS, T_MINUS
  70, 70,         // T_STAR, T_SLASH
  30, 30,         // T_EQ, T_NE
  40, 40, 40, 40  // T_LT, T_GT, T_LE, T_GE
};

// Convert a token type into an AST operation (AST node type)
int arithop(int tokentype) {
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
  if (prec == 0) {
    fprintf(stderr, "syntax error on line %d token %d\n", Line, tokentype);
    exit(1);
  }

  return prec;
}

// Parse a primary factor and return a node representing it
static struct ASTnode *primary(void) {
  struct ASTnode *n;
  int id;

  switch (Token.token) {
    case T_INTLIT:
      n = mkastleaf(A_INTLIT, Token.intvalue);
      break;
    case T_IDENT:
      id = findglob(Text);
      if (id == -1)
        fatals("Unknown variable", Text);
      n = mkastleaf(A_IDENT, id);
      break;
    default:
      fatald("Syntax error, token", Token.token);
  }
  
  scan(&Token);
  return n;
}

// Return a AST node whose root is a binary operator
// Parameter ptp is the precedence of the previous token
struct ASTnode *binexpr(int ptp) {
  struct ASTnode *left, *right;
  int tokentype;

  // Build the left node using an integer literal
  left = primary();

  tokentype = Token.token;
  // If we hit a semi colon, return the left node
  if (tokentype == T_SEMI) return left;
  // Ensure that we have a binary operator
  if (tokentype < T_PLUS || tokentype > T_GE) return left;

  // While the current token's precedence is
  // higher than that of the previous token
  while(op_precedence(tokentype) > ptp) {
    scan(&Token);

    // Recursively call this function to build a
    // subtree while passing in the precedence of the
    // current token
    right = binexpr(OpPrec[tokentype]);

    // Join that subtree with the left node
    left  = mkastnode(arithop(tokentype), left, NULL, right, 0);

    tokentype = Token.token;
    if (tokentype == T_SEMI) return left;
    if (tokentype < T_PLUS || tokentype > T_GE) return left;
  }

  // When the next operator has a precedence equal or lower,
  // we return the tree
  return left;
}
