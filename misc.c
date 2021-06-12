#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "data.h"
#include "scan.h"

// Ensures that the current token has type t,
// and fetches the next token. Throws an error
// otherwise.
void match(int t, char *what) {
  if (Token.token == t) {
    scan(&Token);
  } else {
    fprintf(stderr, "'%s' expected on line %d, found '%s' instead\n", what, Line, Token.tokstr);
    exit(1);
  }
}

void semi(void) {
  match(T_SEMI, ";");
}

void lbrace(void) {
  match(T_LBRACE, "{");
}

void rbrace(void) {
  match(T_RBRACE, "}");
}

void lparen(void) {
  match(T_LPAREN, "(");
}

void rparen(void) {
  match(T_RPAREN, ")");
}

void ident(void) {
  match(T_IDENT, "identifier");
}

void comma(void) {
  match(T_COMMA, ",");
}

void fatal(char *s) {
  fprintf(stderr, "%s on line %d of %s\n", s, Line, Infilename);
  fclose(Outfile);
  unlink(Outfilename);
  exit(1);
}

void fatals(char *s1, char* s2) {
  fprintf(stderr, "%s:%s on line %d of %s\n", s1, s2, Line, Infilename);
  fclose(Outfile);
  unlink(Outfilename);
  exit(1);
}

void fatald(char *s, int d) {
  fprintf(stderr, "%s:%d on line %d of %s\n", s, d, Line, Infilename);
  fclose(Outfile);
  unlink(Outfilename);
  exit(1);

}
void fatalc(char *s, int c) {
  fprintf(stderr, "%s:%c on line %d\n", s, c, Line);
  fclose(Outfile);
  unlink(Outfilename);
  exit(1);
}
