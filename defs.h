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
  T_EOF,
  T_PLUS, T_MINUS, 
  T_STAR, T_SLASH,
  T_EQ, T_NE,
  T_LT, T_GT, T_LE, T_GE,
  T_INTLIT, // Int literal
  T_SEMI,
  T_ASSIGN,
  T_IDENT,

  // Keywords
  T_PRINT,
  T_INT
};

// AST node types
// Binary operators line up exactly with token types
enum {
  A_ADD=1, A_SUBTRACT, A_MULTIPLY, A_DIVIDE, 
  A_EQ, A_NE, A_LT, A_GT, A_LE, A_GE,
  A_INTLIT,
  A_IDENT, A_LVIDENT, A_ASSIGN
};

// AST structure
struct ASTnode {
  int op;                   // "Operation" to be performed on the tree
  struct ASTnode *left;     // Left and right child trees
  struct ASTnode *right;
  union {
    int intvalue; // Integer value for A_INTLIT
    int id;       // Symbol slot number for A_IDENT
  } v;
};

// Symbol table entry
struct symtable {
  char *name; // Name of a symbol
};
