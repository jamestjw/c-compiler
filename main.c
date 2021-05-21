#include <errno.h>
#include "data.h"
#include "defs.h"
#include "expr.h"
#include "gen.h"
#include "scan.h"
#include "stmt.h"
#include "interp.h"

int Line;
int Putback;
FILE *Infile;
FILE *Outfile;

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

int main(int argc, char* argv[]) {
  struct ASTnode *n;

  // Print help message with proper way to call
  // this program and exit.
  if (argc != 2) usage(argv[0]); 

  init();

  if ((Infile = fopen(argv[1], "r")) == NULL) {
    fprintf(stderr, "Unable to open %s: %s\n", argv[1], strerror(errno));
    exit(1);
  }

  if ((Outfile = fopen("out.s", "w")) == NULL) {
    fprintf(stderr, "Unable to open out.s: %s\n", strerror(errno));
    exit(1);
  }

  scan(&Token);
  genpreamble();
  statements();
  genpostamble();
  fclose(Outfile);
  exit(0);
}
