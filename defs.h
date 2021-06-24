#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "incdir.h"

#define TEXTLEN 512
#define NSYMBOLS 1024   // Num of entries in the symbol table

#define AOUT "a.out"
#define ASCMD "as -o"
#define LDCMD "cc -o"
#define CPPCMD "cpp -nostdinc -isystem"

struct token {
  int token;
  int intvalue;
  char *tokstr;   // String version of the token
};

// Token types
enum {
  T_EOF,

  // Binary operators
  T_ASSIGN, T_ASPLUS, T_ASMINUS,
  T_ASSTAR, T_ASSLASH,
  T_QUESTION, T_LOGOR, T_LOGAND,
  T_OR, T_XOR, T_AMPER,
  T_EQ, T_NE,
  T_LT, T_GT, T_LE, T_GE,
  T_LSHIFT, T_RSHIFT,
  T_PLUS, T_MINUS, T_STAR, T_SLASH,

  // Other operators
  T_INC, T_DEC, T_INVERT, T_LOGNOT,

  // Type keywords
  T_VOID, T_CHAR, T_INT, T_LONG,

  // Other keywords
  T_IF, T_ELSE, T_WHILE, T_FOR, T_RETURN,
  T_STRUCT, T_UNION, T_ENUM, T_TYPEDEF,
  T_EXTERN, T_BREAK, T_CONTINUE, T_SWITCH,
  T_CASE, T_DEFAULT, T_SIZEOF, T_STATIC,

  // Structural tokens
  T_INTLIT, T_STRLIT, T_SEMI, T_IDENT,
  T_LBRACE, T_RBRACE, T_LPAREN, T_RPAREN,
  T_LBRACKET, T_RBRACKET, T_COMMA, T_DOT,
  T_ARROW, T_COLON
};

// AST node types
// Binary operators line up exactly with token types
enum {
  A_ASSIGN=1, A_ASPLUS, A_ASMINUS, A_ASSTAR, A_ASSLASH,
  A_TERNARY, A_LOGOR, A_LOGAND, A_OR, A_XOR, A_AND,
  A_EQ, A_NE, A_LT, A_GT, A_LE, A_GE, A_LSHIFT, A_RSHIFT,
  A_ADD, A_SUBTRACT, A_MULTIPLY, A_DIVIDE,
  A_INTLIT, A_STRLIT, A_IDENT, A_GLUE,
  A_IF, A_WHILE, A_FUNCTION, A_WIDEN, A_RETURN,
  A_FUNCCALL, A_ADDR, A_DEREF, A_SCALE,
  A_PREINC, A_PREDEC, A_POSTINC, A_POSTDEC,
  A_NEGATE, A_INVERT, A_LOGNOT, A_TOBOOL, A_BREAK,
  A_CONTINUE, A_SWITCH, A_CASE, A_DEFAULT, A_CAST
};

// Symbol table entry
struct symtable {
  char *name;                  // Name of a symbol
  int type;                    // Primitive type of the symbol
  struct symtable *ctype;      // Pointer to the composite type if type == P_STRUCT
  int stype;                   // Structural type of the symbol
  int class;                   // Storage class for the symbol
                               // i.e. C_GLOBAL, C_LOCAL or C_PARAM
                               //
  int size;                    // Number of elements in the symbol
  int nelems;                  // For functions, # of params
                               // For structs, # of fields
                               // For arrays, # of elements
#define st_endlabel st_posn    // End label for S_FUNCTIONs
  int st_posn;                 // Negative offset from the stack BP
                               // for locals
  int *initlist;               // List of initial values
  struct symtable *next;       // Next symbol on the list
  struct symtable *member;     // First member of a function, struct, union or enum
};

// AST structure
struct ASTnode {
  int op;                   // "Operation" to be performed on the tree
  int type;                 // Type of expression this tree generates
  struct symtable *ctype;   // If struct/union, ptr to that type
  int rvalue;               // True if the node should yield a r-value
  struct ASTnode *left;     // Left, middle and right child trees
  struct ASTnode *mid;
  struct ASTnode *right;
  struct symtable *sym;     // The pointer to the symbol in the symtable
#define a_intvalue a_size	  // For A_INTLIT, the integer value
  int a_size;			          // For A_SCALE, the size to scale by
};

// Primitive types
// Lower 4 btes encode the level of indirection,
// e.g. 0b110000 is an int, whereas 0b110001 is an *int
// 0b110010 is an **int and so on
enum {
  P_NONE, P_VOID=16, P_CHAR=32, P_INT=48, P_LONG=64,
  P_STRUCT=80, P_UNION=96
};

// Structural types
enum {
  S_VARIABLE, S_FUNCTION, S_ARRAY
};

enum {
  C_GLOBAL = 1, // Globally visible symbol
  C_LOCAL,      // Locally visible symbol
  C_PARAM,      // Locally visible function parameter
  C_EXTERN,     // External globally visible symbol
  C_STRUCT,
  C_UNION,
  C_MEMBER,     // Member of a struct or a union
  C_ENUMTYPE,   // A named enumeration type
  C_ENUMVAL,    // A named enumeration value
  C_TYPEDEF,    // A named typedef
  C_STATIC
};

enum {
  NOREG = -1,   // Use NOREG when AST generation functions
                // have no registers to return
  NOLABEL = 0   // Use NOLABEL when we have no label to
                // pass to genAST
};
