#include "data.h"
#include "expr.h"
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
    case T_IDENT:
      // In order to know if this is a variable or a function
      // call, we need to look at the subsequent token
      scan(&Token);

      if (Token.token == T_LPAREN)
        return funccall();

      // Reject the new token if we did not encounter a left parenthesis
      reject_token(&Token);

      id = findglob(Text);
      if (id == -1)
        fatals("Unknown variable", Text);
      n = mkastleaf(A_IDENT, Gsym[id].type, id);
      break;
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

    // Ensure that the two types are compatible by
    // trying to modify each tree to match the other
    // type.
    ASTop = arithop(tokentype);
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

    // Join that subtree with the left node
    // The node contains an expression of the same type
    // as the left node.
    left = mkastnode(arithop(tokentype), left->type, left, NULL, right, 0);

    tokentype = Token.token;
    if (tokentype == T_SEMI) return left;
    if (tokentype < T_PLUS || tokentype > T_GE) return left;
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

