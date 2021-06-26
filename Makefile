INCDIR=/tmp/include
BINDIR=/tmp

SRCS= cg.c expr.c gen.c main.c misc.c scan.c stmt.c sym.c tree.c types.c opt.c decl.c incdir.h
ARMSRCS= cg_arm.c decl.c expr.c gen.c main.c misc.c scan.c stmt.c \
	sym.c tree.c types.c

ccc: $(SRCS)
	cc -o ccc -g -Wall $(SRCS)

cccarm: $(ARMSRCS)
	cc -o cccarm -g -Wall $(ARMSRCS)
	cp cccarm ccc

incdir.h:
	echo "#define INCDIR \"$(INCDIR)\"" > incdir.h

install: ccc
	mkdir -p $(INCDIR)
	rsync -a include/. $(INCDIR)
	cp ccc $(BINDIR)
	chmod +x $(BINDIR)/ccc

clean:
	rm -rf ccc* cccarm* *.o *.s tests/*.o tests/*.s *.out tests/*.out incdir.h

test: ccc tests/runtests
	(cd tests; chmod +x runtests; ./runtests)

# Run the tests with the
# compiler that compiled itself
test0: install tests/runtests0 ccc0
	(cd tests; chmod +x runtests0; ./runtests0)

armtest: cccarm tests/runtests
	(cd tests; chmod +x runtests; ./runtests)

# Try to do the triple test
triple: ccc1
	size ccc[01]

# Paranoid: quadruple test
quad: ccc2
	size ccc[012]

ccc2: ccc1 $(SRCS)
	./ccc1 -o ccc2 $(SRCS)

ccc1: ccc0 $(SRCS)
	./ccc0 -o ccc1 $(SRCS)

ccc0: install $(SRCS)
	./ccc  -o ccc0 $(SRCS)
