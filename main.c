#include <errno.h>
#include "defs.h"
#include "decl.h"
#include "data.h"

int Line;
int Putback;
FILE *Infile;

// String version of tokens to be used for debugging purposes.
char *tokstr[] = { "+", "-", "*", "/", "intlit" };

static void init() {
  Line = 1;
  Putback = '\n';
}

static void usage(char *prog) {
  fprintf(stderr, "Usage: %s <input-file>\n", prog);
  exit(1);
}

static void scanfile() {
  struct token T;
  while (scan(&T)) {
    printf("Token %s", tokstr[T.token]);
    if (T.token == T_INTLIT) {
      printf(", value %d", T.intvalue);
    }
    printf("\n");
  }
}

int main(int argc, char* argv[]) {
  if (argc != 2) usage(argv[0]); 

  init();

  if((Infile = fopen(argv[1], "r")) == NULL) {
    fprintf(stderr, "Unable to open %s: %s\n", argv[1], strerror(errno));
    exit(1);
  }

  scanfile();

  exit(0);
}
