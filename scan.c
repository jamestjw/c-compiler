#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "data.h"
#include "misc.h"
#include "scan.h"

char Text[TEXTLEN + 1];

// Pointer to a rejected token
static struct token *Rejtoken = NULL;

// List of token strings, for debugging purposes
char *Tstring[] = {
  "EOF", "=", "+=", "-=", "*=", "/=",
  "||", "&&", "|", "^", "&",
  "==", "!=", ",", ">", "<=", ">=", "<<", ">>",
  "+", "-", "*", "/", "++", "--", "~", "!",
  "void", "char", "int", "long",
  "if", "else", "while", "for", "return",
  "struct", "union", "enum", "typedef",
  "extern", "break", "continue", "switch",
  "case", "default",
  "intlit", "strlit", ";", "identifier",
  "{", "}", "(", ")", "[", "]", ",", ".",
  "->", ":"
};

// Get the next char from the input file.
static int next(void) {
  int c, l;

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

  while (c == '#') { // Hit a preprocessor statement
    scan(&Token);

    // This is the line number of the following line
    if (Token.token != T_INTLIT)
      fatals("Expecting pre-processor line number, got", Text);
    l = Token.intvalue;

    scan(&Token);
    // The file from which the following line is from
    if (Token.token != T_STRLIT)
      fatals("Expecting pre-processor filename, got", Text);

    // Check if this is a real filename
    if (Text[0] != '<') {
      // Update the filename if it does not match
      // the current one we have
      if (strcmp(Text, Infilename))
        Infilename = strdup(Text);
      // Update the line number
      Line = l;
    }

    // Skip to EOL
    while ((c = fgetc(Infile)) != '\n');
    c = fgetc(Infile);
  }

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
  int k, val = 0, radix = 10;

  // Assume the radix is 10 at the beginning,
  // but if it starts with 0 then we try to
  // figure out the actual radix.
  if (c == '0') {
    // If it begins with 0x, its radix is 16
    if ((c = next()) == 'x') {
      radix = 16;
      c = next();
    } else {
      // Otherwise the radix is 8
      radix = 8;
    }
  }

  // Convert each char into an int value
  while ((k = chrpos("0123456789abcdef", tolower(c))) >= 0) {
    if (k >= radix)
      fatalc("Invalid digit in integer literal", c);
    val = val * radix + k;
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
    case 'b':
      if (!strcmp(s, "break"))
        return T_BREAK;
      break;
    case 'c':
      if (!strcmp(s, "case"))
        return T_CASE;
      if (!strcmp(s, "char"))
        return T_CHAR;
      if (!strcmp(s, "continue"))
        return T_CONTINUE;
      break;
    case 'd':
      if (!strcmp(s, "default"))
        return T_DEFAULT;
      break;
    case 'e':
      if (!strcmp(s, "else"))
        return T_ELSE;
      else if (!strcmp(s, "enum"))
        return T_ENUM;
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
    case 'r':
      if (!strcmp(s, "return"))
        return T_RETURN;
      break;
    case 's':
      if (!strcmp(s, "struct"))
        return T_STRUCT;
      if (!strcmp(s, "switch"))
        return T_SWITCH;
      break;
    case 't':
      if (!strcmp(s, "typedef"))
        return T_TYPEDEF;
      break;
    case 'u':
      if (!strcmp(s, "union"))
        return T_UNION;
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

// Read in a hexadecimal constant from the input
// destined to be a single char
static int hexchar(void) {
  int c, h, n = 0, f = 0;

  while (isxdigit(c = next())) {
    // Convert from char to int value
    h = chrpos("0123456789abcdef", tolower(c));
    // Add to running hex value
    n = n * 16 + h;
    f = 1;
  }

  // Hit a non-hex character
  putback(c);
  
  if (!f)
    fatal("Missing digits after '\\x'");
  if (n > 255)
    fatal("Value out of range after '\\x'");
  return n;
}

// Return the next character from a character
// or string literal
static int scanch(void) {
  int i, c, c2;

  c = next();

  // Handle escape sequences
  if (c == '\\') {
    switch (c = next()) {
        case 'a':  return '\a';
        case 'b':  return '\b';
        case 'f':  return '\f';
        case 'n':  return '\n';
        case 'r':  return '\r';
        case 't':  return '\t';
        case 'v':  return '\v';
        case '\\': return '\\';
        case '"':  return '"' ;
        case '\'': return '\'';
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
          // Handling octals
          for (i = c2 = 0; isdigit(c) && c < '8'; c = next()) {
            if (++i > 3)
              break;
            c2 = c2 * 8 + (c - '0');
          }
          putback(c);
          return c2;
        case 'x':
          return hexchar();
        default:
          fatalc("unknown escape sequence", c);
    }
  }
  return c;
}

static int scanstr(char *buf) {
  int i, c;

  for (i = 0; i < TEXTLEN - 1; i++) {
    if ((c = scanch()) == '"') {
      // Null terminate the string
      buf[i] = 0;
      return i;
    }
    buf[i] = c;
  }

  fatal("String literal too long");
  return 0;
}

void reject_token(struct token *t) {
  if (Rejtoken != NULL)
    fatal("Can't reject token twice");
  Rejtoken = t;
}

int scan(struct token *t) {
  int c, tokentype;

  if (Peektoken.token != 0) {
    t->token = Peektoken.token;
    t->tokstr = Peektoken.tokstr;
    t->intvalue = Peektoken.intvalue;
    Peektoken.token = 0;
    return 1;
  }

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
      if ((c = next()) == '+') {
	      t->token = T_INC;
      } else if (c == '=') {
        t->token = T_ASPLUS;
      } else {
	      putback(c);
	      t->token = T_PLUS;
      }
      break;
    case '-':
      if ((c = next()) == '-') {
	      t->token = T_DEC;
      } else if (c == '>') {
        t->token = T_ARROW;
      } else if (c == '=') {
        t->token = T_ASMINUS;
      } else if (isdigit(c)) {
        // Negative int literal
        // TODO: Scanner greedily forces -1 as 
        // T_INTLIT, e.g. `1-1` without spaces
        // is parsed as 1 -1
        t->intvalue = -scanint(c);
        t->token = T_INTLIT;
      } else {
	      putback(c);
	      t->token = T_MINUS;
      }
      break;
    case '*':
      if ((c = next()) == '=') {
        t->token = T_ASSTAR;
      } else {
        putback(c);
        t->token = T_STAR;
      }
      break;
    case '/':
      if ((c = next()) == '=') {
        t->token = T_ASSLASH;
      } else {
        putback(c);
        t->token = T_SLASH;
      }
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
        putback(c);
        t->token = T_LOGNOT;
      }
      break;
    case '<':
      if ((c = next()) == '=') {
        t->token = T_LE;
      } else if (c == '<') {
        t->token = T_LSHIFT;
      } else {
        putback(c);
        t->token = T_LT;
      }
      break;
    case '>':
      if ((c = next()) == '=') {
        t->token = T_GE;
      } else if (c == '>') {
        t->token = T_RSHIFT;
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
    case ',':
      t->token = T_COMMA;
      break;
    case '[':
      t->token = T_LBRACKET;
      break;
    case ']':
      t->token = T_RBRACKET;
      break;
    case '|':
      if ((c = next()) == '|') {
        t->token = T_LOGOR;
      } else {
        putback(c);
        t->token = T_OR;
      }
      break;
    case '^':
      t->token = T_XOR;
      break;
    case '~':
      t->token = T_INVERT;
      break;
    // Handle character literals
    case '\'':
      t->intvalue = scanch();
      t->token = T_INTLIT;
      if (next() != '\'')
        fatal("Expected \"'\" at the end of char literal");
      break;
    case '"':
      scanstr(Text);
      t->token = T_STRLIT;
      break;
    case '.':
      t->token = T_DOT;
      break;
    case ':':
      t->token = T_COLON;
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

  t->tokstr = Tstring[t->token];
  return 1;
}

