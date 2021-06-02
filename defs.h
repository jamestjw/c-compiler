#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define TEXTLEN 512
#define NSYMBOLS 1024   // Num of entries in the symbol table
#define NOREG -1        // When AST generation functions have no registers to return
#define NOLABEL 0       // When we have no label to pass to genAST()

// For now, a token represents the 4 basic math operators and decimal whole numbers only.
struct token {
  int token;
  int intvalue;
};

// Token types
enum {
  T_EOF,
  // Binary operators
  T_ASSIGN, T_LOGOR, T_LOGAND,
  T_OR, T_XOR, T_AMPER,
  T_EQ, T_NE,
  T_LT, T_GT, T_LE, T_GE,
  T_LSHIFT, T_RSHIFT,
  T_PLUS, T_MINUS, T_STAR, T_SLASH,
  
  // Other operators
  T_INC, T_DEC, T_INVERT, T_LOGNOT,

  // Types
  T_VOID, T_CHAR, T_INT, T_LONG,

  // Structural tokens
  T_INTLIT, T_STRLIT, T_SEMI, T_IDENT,
  T_LBRACE, T_RBRACE, T_LPAREN, T_RPAREN,
  T_LBRACKET, T_RBRACKET,
  T_COMMA,

  // Other keywords
  T_IF, T_ELSE, T_WHILE,
  T_FOR, T_RETURN,
};

// AST node types
// Binary operators line up exactly with token types
enum {
  A_ASSIGN=1, A_LOGOR, A_LOGAND, A_OR, A_XOR, A_AND,
  A_EQ, A_NE, A_LT, A_GT, A_LE, A_GE, A_LSHIFT, A_RSHIFT,
  A_ADD, A_SUBTRACT, A_MULTIPLY, A_DIVIDE,
  A_INTLIT, A_STRLIT, A_IDENT, A_GLUE,
  A_IF, A_WHILE, A_FUNCTION, A_WIDEN, A_RETURN,
  A_FUNCCALL, A_ADDR, A_DEREF, A_SCALE,
  A_PREINC, A_PREDEC, A_POSTINC, A_POSTDEC,
  A_NEGATE, A_INVERT, A_LOGNOT, A_TOBOOL,
};

// AST structure
struct ASTnode {
  int op;                   // "Operation" to be performed on the tree
  int type;                 // Type of expression this tree generates
  int rvalue;               // True if the node should yield a r-value
  struct ASTnode *left;     // Left, middle and right child trees
  struct ASTnode *mid;
  struct ASTnode *right;
  union {
    int intvalue;           // Integer value for A_INTLIT
    int id;                 // Symbol slot number for A_IDENT
    int size;               // Size to scale by for A_SCALE
  } v;
};

// Symbol table entry
struct symtable {
  char *name;     // Name of a symbol
  int type;       // Primitive type of the symbol
  int stype;      // Structural type of the symbol
  int endlabel;   // End label for S_FUNCTIONs
  int size;       // Number of elements in the symbol
  int class;      // Storage class for the symbol
  int posn;       // Negative offset from the stack BP
                  // for locals
};

// Primitive types
enum {
  P_NONE, P_VOID, P_CHAR, P_INT, P_LONG,
  P_VOIDPTR, P_CHARPTR, P_INTPTR, P_LONGPTR,
};

// Structural types
enum {
  S_VARIABLE, S_FUNCTION, S_ARRAY,
};

enum {
  C_GLOBAL = 1, // Globally visible symbol
  C_LOCAL,      // Locally visible symbol
};

