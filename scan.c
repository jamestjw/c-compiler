#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "defs.h"
#include "decl.h"
#include "data.h"

// Get the next char from the input file.
static int next(void) {
  int c;

  // If a character was previously asked to be put back,
  // we now return that character and reset the Putback
  // variable.
  if (Putback) {
    c = Putback;
    Putback = 0;
    return c;
  }

  // If no character was asked to be putback, we get a new
  // character from the input stream.
  c = fgetc(Infile);

  // Track what line we are on for the purposes
  // of printing debug messages.
  if ('\n' == c) Line++;

  return c;
}

// Put back an unwanted character to the input stream
static void putback(int c) {
  Putback = c;
}

// This function returns the next character in the input
// stream that is not a whitespace.
static int skip(void) {
  int c;

  c = next();
  
  while (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' ) {
    c = next();
  }
  
  return c;
}

static int chrpos(char *s, int c) {
  char *p;

  p = strchr(s, c);
  return (p ? p - s : -1);
}

// Scan and return an integer literal.
static int scanint(int c) {
  int k, val = 0;

  // We use chrpos instead of substracting the ASCII value of 
  // '0' from c so that we maintain the possibility of using this
  // with hexadecimals in the future.
  while ((k = chrpos("0123456789", c)) >= 0) {
    val = val * 10 + k;
    c = next();
  }

  // The above while loop exits when we hit a char
  // that is not a digit, which we put back into the
  // stream.
  putback(c);

  return val;
}

int scan(struct token *t) {
  int c;
  
  // Skip whitespace
  c = skip();

  // Set up the token based on the input character
  switch (c) {
    case EOF:
      return 0;
    case '+':
      t->token = T_PLUS;
      break;
    case '-':
      t->token = T_MINUS;
      break;
    case '*':
      t->token = T_STAR;
      break;
    case '/':
      t->token = T_SLASH;
      break;
    default:
      // If a digit was encountered, scan the entire number
      // in and store it in the token.
      if (isdigit(c)) {
        t->intvalue = scanint(c);
        t->token = T_INTLIT;
        break;
      }

      printf("Unrecognised character %c on line %d\n", c, Line);
      exit(1);
  }

  return 1;
}
