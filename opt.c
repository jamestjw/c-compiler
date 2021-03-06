#include "data.h"
#include "opt.h"
#include "tree.h"

// Fold an AST tree with a binary operator and
// two A_INTLIT children. Return either the
// original tree or a new leaf node.
static struct ASTnode *fold2(struct ASTnode *n) {
  int val, leftval, rightval;

  leftval = n->left->a_intvalue;
  rightval = n->right->a_intvalue;

  switch (n->op) {
    case A_ADD:
      val = leftval + rightval;
      break;
    case A_SUBTRACT:
      val = leftval - rightval;
      break;
    case A_MULTIPLY:
      val = leftval * rightval;
      break;
    case A_DIVIDE:
      // Let the crash occur during execution
      if (rightval == 0)
        return n;
      val = leftval / rightval;
      break;
    default:
      return n;
  }

  return mkastleaf(A_INTLIT, n->type, NULL,NULL, val);
}

// Fold on AST tree with a unary operator and one
// INTLIT child. Return either the original tree
// or a new leaf node.
static struct ASTnode *fold1(struct ASTnode *n) {
  int val;

  val = n->left->a_intvalue;
  switch (n->op) {
    case A_WIDEN:
      // We have the literal value copied so that
      // we can directly return a node with the
      // target type
      break;
    case A_INVERT:
      val = ~val;
      break;
    case A_LOGNOT:
      val = !val;
      break;
    default:
      return n;
  }

  return mkastleaf(A_INTLIT, n->type, NULL, NULL, val);
}

static struct ASTnode *fold(struct ASTnode *n) {
  if (n == NULL)
    return NULL;

  n->left = fold(n->left);
  n->right = fold(n->right);

  if (n->left && n->left->op == A_INTLIT) {
    if (n->right && n->right->op == A_INTLIT)
      n = fold2(n);
    else
      n = fold1(n);
  }

  return n;
}

struct ASTnode *optimise(struct ASTnode *n) {
  n = fold(n);

  return n;
}
