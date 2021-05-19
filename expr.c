#include "defs.h"
#include "data.h"
#include "expr.h"
#include "scan.h"
#include "tree.h"

// Contains most recently scanned token from input
struct token Token;

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

struct ASTnode *multiplicative_expr(void) {
  struct ASTnode *left, *right;
  int tokentype;

  // Get integer literal on the left
  left = primary();

  tokentype = Token.token;
  // Return the left node if no tokens are left
  if (tokentype == T_EOF) return left;

  // While we encounter the '*' or '/' operators
  while ((tokentype == T_STAR || tokentype == T_SLASH)) {
    // Fetch next integer literal
    scan(&Token);
    right = primary();

    left = mkastnode(arithop(tokentype), left, right, 0);

    tokentype = Token.token;
    if (tokentype == T_EOF) break;
  }

  return left;
}

// Return an AST tree node whose root is a '+' or '-' binary operator
struct ASTnode *additive_expr(void) {
  struct ASTnode *left, *right;
  int tokentype;

  // Get left sub-tree at a higher precedence than us.
  // This function only returns when it encounters an
  // operator with lower precedence than itself,
  // i.e. a '+' or '-'
  left = multiplicative_expr();
  
  tokentype = Token.token;
  // If no tokens left, return the node as the expression
  // If a token was found, we know we have a '+' or '-'
  if (tokentype == T_EOF) return left;

  while (1) {
    scan(&Token);

    // Get the right sub-tree at higher precedence than us
    // in case there operators with higher precedence ahead of
    // where we are now.
    right = multiplicative_expr();

    left = mkastnode(arithop(tokentype), left, right, 0);

    tokentype = Token.token;
    if (tokentype == T_EOF) break;
  }

  return left;
}

struct ASTnode *binexpr(void) {
  struct ASTnode *n, *left, *right;
  int nodetype;

  // Build the left node using an integer literal
  left = primary();

  // If no tokens are left, return the left node
  if (Token.token == T_EOF) return left;
  
  // Convert operator token to node type
  nodetype = arithop(Token.token);

  scan(&Token);

  right = binexpr();

  n = mkastnode(nodetype, left, right, 0);

  return n;
}
