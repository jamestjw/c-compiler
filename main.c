#include <errno.h>
#include <unistd.h>
#include "data.h"
#include "decl.h"
#include "expr.h"
#include "gen.h"
#include "scan.h"
#include "stmt.h"
#include "sym.h"
#include "interp.h"

#define MAXOBJ 100 // Max num of objs we can link

int Line;
int Putback;
FILE *Infile;
FILE *Outfile;
char *Infilename;
char *Outfilename;

int O_dumpAST;
int O_keepasm;	
int O_assemble;
int O_dolink;	
int O_verbose;	

// String version of tokens to be used for debugging purposes.
char *tokstr[] = { "+", "-", "*", "/", "intlit" };

static void init() {
  Line = 1;
  Putback = '\n';
  O_dumpAST = 0;
}

static void usage(char *prog) {
  fprintf(stderr, "Usage: %s [-vcST] [-o outfile] file [file ...]\n", prog);
  fprintf(stderr, "       -v give verbose output of the compilation stages\n");
  fprintf(stderr, "       -c generate object files but don't link them\n");
  fprintf(stderr, "       -S generate assembly files but don't link them\n");
  fprintf(stderr, "       -T dump the AST trees for each input file\n");
  fprintf(stderr, "       -o outfile, produce the outfile executable file\n");
  exit(1);
}

static char *alter_suffix(char *str, char suffix) {
  char *posn;
  char *newstr;

  if ((newstr = strdup(str)) == NULL)
    return NULL;

  // Find the '.'
  if ((posn = strchr(newstr, '.')) == NULL)
    return NULL;

  // Ensure that a suffix exists
  posn++;
  if (*posn == '\0')
    return NULL;

  // Change the suffix and null-terminate it
  *posn++ = suffix;
  *posn = '\0';

  return newstr;
}

// Given an input filename, compile that file
// down to assembly code and return the resulting
// file's name
static char *do_compile(char *filename) {
  char cmd[TEXTLEN];

  Outfilename = alter_suffix(filename, 's');
  if (Outfilename == NULL) {
    fprintf(stderr, "Error: %s has no suffix, try appending .c at the end the input filename.\n", filename);
    exit(1);
  }

  // Preprocessor command
  // INCDIR defined in Makefile
  snprintf(cmd, TEXTLEN, "%s %s %s", CPPCMD, INCDIR, filename);

  if ((Infile = popen(cmd, "r")) == NULL) {
    fprintf(stderr, "Unable to open %s: %s\n", filename, strerror(errno));
    exit(1);
  }
  Infilename = filename;

  if ((Outfile = fopen(Outfilename, "w")) == NULL) {
    fprintf(stderr, "Unable to create %s: %s\n", Outfilename, strerror(errno));
    exit(1);
  }

  Line = 1;
  Putback = '\n';
  clear_symtable();

  if (O_verbose)
    printf("compiling %s\n", filename);

  scan(&Token);
  genpreamble();
  global_declarations(); 
  genpostamble();

  fclose(Outfile);

  return Outfilename;
}

static char *do_assemble(char *filename) {
  char cmd[TEXTLEN];
  int err;

  char *outfilename = alter_suffix(filename, 'o');
  if (outfilename == NULL) {
    fprintf(stderr, "Error: %s has no suffix, try appending .s at the end of the filename\n", filename);
    exit(1);
  }
  
  // Build the assembly command and execute it
  snprintf(cmd, TEXTLEN, "%s %s %s", ASCMD, outfilename, filename);

  if (O_verbose)
    printf("%s\n", cmd);

  err = system(cmd);
  if (err != 0) {
    fprintf(stderr, "Assembly of %s failed\n", filename);
    exit(1);
  }

  return outfilename;
}

void do_link(char *outfilename, char *objlist[]) {
  int cnt, size = TEXTLEN;
  char cmd[TEXTLEN], *cptr;
  int err;

  // Start with the linker command and the output file
  cptr = cmd;
  cnt = snprintf(cptr, size, "%s %s ", LDCMD, outfilename);
  cptr += cnt; size -= cnt;

  // Append each object filename
  while (*objlist != NULL) {
    cnt = snprintf(cptr, size, "%s ", *objlist);
    cptr += cnt; size -= cnt; objlist++;
  }

  if (O_verbose)
    printf("%s\n", cmd);

  err = system(cmd);

  if (err != 0) {
    fprintf(stderr, "Linking failed\n");
    exit(1);
  }
}

int main(int argc, char* argv[]) {
  int i, objcount = 0;
  char *asmfile, *objfile;
  char *objlist[MAXOBJ];
  char *outfilename = AOUT;

  // Initialise flags
  O_dumpAST = 0;        // If true, dump the AST trees
  O_keepasm = 0;        // If true, keep any assembly files
  O_assemble = 0;       // If true, assemble the assembly files
  O_dolink = 1;         // If true, link the object files
  O_verbose = 0;        // If true, print info on compilation stages

  init();

  for (i = 1; i < argc; i++) {
    if (*argv[i] != '-') break;

    for (int j = 1; (*argv[i] == '-') && argv[i][j]; j++) {
      switch(argv[i][j]) {
        case 'o': 
          outfilename = argv[++i];
          break;
        case 'T': 
          O_dumpAST = 1; 
          break;
        case 'c':
          O_assemble = 1;
          O_keepasm = 0;
          O_dolink = 0;
          break;
        case 'S':
          O_keepasm = 1;
          O_assemble = 0;
          O_dolink = 0;
          break;
        case 'v':
          O_verbose = 1;
          break;
        default: 
          usage(argv[0]);
      }
    }
  }

  // Ensure that we have an input file argument
  if (i >= argc) usage(argv[0]);

  // Work on each input file
  while (i < argc) {
    asmfile = do_compile(argv[i]);

    if (O_dolink || O_assemble) {
      // Assemble to object form
      objfile = do_assemble(asmfile);

      if (objcount == (MAXOBJ - 2)) {
        fprintf(stderr, "Too many object files for the compiler to handle\n");
        exit(1);
      }

      // Add the object file's name to the list
      objlist[objcount++] = objfile; 
      objlist[objcount] = NULL;
    }

    // Remove the assembly file if we do not
    // keep to retain it
    if (!O_keepasm)
      unlink(asmfile);

    i++;
  }

  if (O_dolink) {
    do_link(outfilename, objlist);

    // Remove the object files if we do not need
    // to keep them
    if (!O_assemble) {
      for (i = 0; objlist[i] != NULL; i++)
        unlink(objlist[i]);
    }
  }
 
  return 0;
}
