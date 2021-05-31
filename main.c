#include <errno.h>
#include "data.h"
#include "decl.h"
#include "expr.h"
#include "gen.h"
#include "scan.h"
#include "stmt.h"
#include "sym.h"
#include "interp.h"

int Line;
int Putback;
FILE *Infile;
FILE *Outfile;
int O_dumpAST;

// String version of tokens to be used for debugging purposes.
char *tokstr[] = { "+", "-", "*", "/", "intlit" };

static void init() {
  Line = 1;
  Putback = '\n';
  O_dumpAST = 0;
}

static void usage(char *prog) {
  fprintf(stderr, "Usage: %s <input-file>\n", prog);
  exit(1);
}

int main(int argc, char* argv[]) {
  int i;

  init();

  for (i = 1; i < argc; i++) {
    if (*argv[i] != '-') break;
   for (int j = 1; argv[i][j]; j++) {
      switch(argv[i][j]) {
        case 'T': O_dumpAST = 1; break;
        default: usage(argv[0]);
      }
    }
  }

  // Ensure that we have an input file argument
  if (i >= argc) usage(argv[0]);

  if ((Infile = fopen(argv[i], "r")) == NULL) {
    fprintf(stderr, "Unable to open %s: %s\n", argv[i], strerror(errno));
    exit(1);
  }

  if ((Outfile = fopen("out.s", "w")) == NULL) {
    fprintf(stderr, "Unable to open out.s: %s\n", strerror(errno));
    exit(1);
  }

  // TODO: For now, use this hack to ensure that printint()
  // is defined
  addglob("printint", P_CHAR, S_FUNCTION, 0, 0);

  scan(&Token);
  genpreamble();
  global_declarations(); 
  genpostamble();

  fclose(Outfile);
  exit(0);
}
