#include "defs.h"
#include "tree.h"

// Build and return a generic AST node
struct ASTnode *mkastnode(int op, 
                          struct ASTnode *left, 
                          struct ASTnode *mid, 
                          struct ASTnode *right, 
                          int intvalue) {
  struct ASTnode *n;
  
  n = (struct ASTnode *) malloc(sizeof(struct ASTnode));

  if (n == NULL) {
    fprintf(stderr, "Out of memory to allocate AST node\n");
    exit(1);
  }

  n->op = op;
  n->left = left;
  n->mid = mid;
  n->right = right;
  n->v.intvalue = intvalue;

  return n;
}

// Make an AST leaf node
struct ASTnode *mkastleaf(int op, int intvalue) {
  return mkastnode(op, NULL, NULL, NULL, intvalue);
}

// Make a unary AST node
struct ASTnode *mkastunary(int op, struct ASTnode *left, int intvalue) {
  return mkastnode(op, left, NULL, NULL, intvalue);
}
