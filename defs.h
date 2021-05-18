#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

// For now, a token represents the 4 basic math operators and decimal whole numbers only.
struct token {
  int token;
  int intvalue;
};

// Token types
enum {
  T_PLUS, 
  T_MINUS, 
  T_STAR, 
  T_SLASH,
  // Represents an integer literal, in which case 
  // the value will be held in the +intvalue+ field
  // of the token.
  T_INTLIT,
  T_EOF,
};

// AST node types
enum {
  A_ADD, A_SUBTRACT, A_MULTIPLY, A_DIVIDE, A_INTLIT
};

// AST structure
struct ASTnode {
  int op;                   // "Operation" to be performed on the tree
  struct ASTnode *left;     // Left and right child trees
  struct ASTnode *right;
  int intvalue;             // Value for A_INTLIT
};

