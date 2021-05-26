#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define TEXTLEN 512
#define NSYMBOLS 1024 // Num of entries in the symbol table
#define NOREG -1      // When AST generation functions have no registers to return

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
  T_INTLIT, T_SEMI, T_ASSIGN, T_IDENT,
  T_LBRACE, T_RBRACE, T_LPAREN, T_RPAREN,

  // Keywords
  T_PRINT, T_INT, T_IF, T_ELSE, T_WHILE,
  T_FOR, T_VOID, T_CHAR,
};

// AST node types
// Binary operators line up exactly with token types
enum {
  A_ADD=1, A_SUBTRACT, A_MULTIPLY, A_DIVIDE, 
  A_EQ, A_NE, A_LT, A_GT, A_LE, A_GE,
  A_INTLIT,
  A_IDENT, A_LVIDENT, A_ASSIGN, A_PRINT, A_GLUE, 
  A_IF, A_WHILE, A_FUNCTION, A_WIDEN,
};

// AST structure
struct ASTnode {
  int op;                   // "Operation" to be performed on the tree
  int type;                 // Type of expression this tree generates
  struct ASTnode *left;     // Left, middle and right child trees
  struct ASTnode *mid;
  struct ASTnode *right;
  union {
    int intvalue; // Integer value for A_INTLIT
    int id;       // Symbol slot number for A_IDENT
  } v;
};

// Symbol table entry
struct symtable {
  char *name; // Name of a symbol
  int type;   // Primitive type of the symbol
  int stype;  // Structural type of the symbol
};

// Primitive types
enum {
  P_NONE, P_VOID, P_CHAR, P_INT,
};

// Structural types
enum {
  S_VARIABLE, S_FUNCTION,
};

