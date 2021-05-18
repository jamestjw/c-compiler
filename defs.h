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
  T_INTLIT
};
