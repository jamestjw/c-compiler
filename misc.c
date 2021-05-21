#include <stdio.h>
#include <stdlib.h>
#include "data.h"
#include "defs.h"
#include "scan.h"

// Ensures that the current token has type t,
// and fetches the next token. Throws an error
// otherwise.
void match(int t, char *what) {
  if (Token.token == t) {
    scan(&Token);
  } else {
    printf("%s expected on line %d\n", what, Line);
    exit(1);
  }
}

void semi(void) {
  match(T_SEMI, ";");
}
