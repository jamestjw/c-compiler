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
