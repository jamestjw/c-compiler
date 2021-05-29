#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "data.h"
#include "misc.h"
#include "scan.h"

char Text[TEXTLEN + 1];

// Pointer to a rejected token
static struct token *Rejtoken = NULL;

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

// Scan an identifier from the input file and store it
// in buf[] and return its length
//
// c - First letter of identifier
// buf - Buffer to store resulting identifier
// lim - Max identifier length
static int scanident(int c, char *buf, int lim) {
  int i = 0;

  // Identifiers contain letters, digits and underscores only
  while (isalpha(c) || isdigit(c) || c == '_') {
    if (lim - 1 == i) {
      printf("identifier too long on line %d\n", Line);
      exit(1);
    } else if (i < lim - 1) {
      buf[i++] = c;
    }
    c = next();
  }

  // The while loop terminates when we hit an invalid character,
  // which we put back
  putback(c);

  // NULL terminate the string
  buf[i] = '\0';
  return i;
}

// Given a word, returns the matching keyword token number
// or 0 if it is not a keyword.
//
// This implementation switches on the first char to save
// time calling strcmp against all keywords.
static int keyword(char *s) {
  switch (*s) {
    case 'c':
      if (!strcmp(s, "char"))
        return T_CHAR;
      break;
    case 'e':
      if (!strcmp(s, "else"))
        return T_ELSE;
      break;
    case 'f':
      if (!strcmp(s, "for"))
        return T_FOR;
      break;
    case 'i':
      if (!strcmp(s, "int"))
        return T_INT;
      if (!strcmp(s, "if"))
        return T_IF;
      break;
    case 'l':
      if (!strcmp(s, "long"))
        return T_LONG;
      break;
    case 'p':
      if (!strcmp(s, "print")) 
        return T_PRINT;
      break;
    case 'r':
      if (!strcmp(s, "return")) 
        return T_RETURN;
      break;
    case 'v':
      if (!strcmp(s, "void")) 
        return T_VOID;
      break;
    case 'w':
      if (!strcmp(s, "while"))
        return T_WHILE;
      break;
  }

  // Return 0 if no keywords were identified
  return 0;
}

void reject_token(struct token *t) {
  if (Rejtoken != NULL)
    fatal("Can't reject token twice");
  Rejtoken = t;  
}

int scan(struct token *t) {
  int c, tokentype;

  // Return a rejected token if possible
  if (Rejtoken != NULL) {
    t = Rejtoken;
    Rejtoken = NULL;
    return 1;
  }
  
  // Skip whitespace
  c = skip();

  // Set up the token based on the input character
  switch (c) {
    case EOF:
      t->token = T_EOF;
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
    case ';':
      t->token = T_SEMI;
      break;
    case '=':
      if ((c = next()) == '=') {
        t->token = T_EQ;
      } else {
        putback(c);
        t->token = T_ASSIGN;
      }
      break;
    case '!':
      if ((c = next()) == '=') {
        t->token = T_NE;
      } else {
        fatalc("Unrecognised character", c);
      }
      break;
    case '<':
      if ((c = next()) == '=') {
        t->token = T_LE;
      } else {
        putback(c);
        t->token = T_LT;
      }
      break;
    case '>':
      if ((c = next()) == '=') {
        t->token = T_GE;
      } else {
        putback(c);
        t->token = T_GT;
      }
      break;
    case '{':
      t->token = T_LBRACE;
      break;
    case '}':
      t->token = T_RBRACE;
      break;
    case '(':
      t->token = T_LPAREN;
      break;
    case ')':
      t->token = T_RPAREN;
      break;
    case '&':
      if ((c = next()) == '&') {
        t->token = T_LOGAND;
      } else {
        putback(c);
        t->token = T_AMPER;
      }
      break;
    default:
      // If a digit was encountered, scan the entire number
      // in and store it in the token.
      if (isdigit(c)) {
        t->intvalue = scanint(c);
        t->token = T_INTLIT;
        break;
      } else if (isalpha(c) || c == '_') {
        // Read in keyword or identifier
        scanident(c, Text, TEXTLEN);

        if ((tokentype = keyword(Text))) {
          t->token = tokentype;
          break;
        }

        // If it is not a recognised keyword, we assume
        // it is an identifier
        t->token = T_IDENT;
        break;
      }

      fatalc("Unrecognised character", c);
  }

  return 1;
}

