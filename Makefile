INCDIR=/tmp/include
BINDIR=/tmp

SRCS= cg.c decl.c expr.c gen.c main.c misc.c scan.c stmt.c sym.c tree.c types.c opt.c
ARMSRCS= cg_arm.c decl.c expr.c gen.c main.c misc.c scan.c stmt.c \
	sym.c tree.c types.c

ccc: $(SRCS)
	cc -o ccc -g -Wall -DINCDIR=\"$(INCDIR)\" $(SRCS) 

cccarm: $(ARMSRCS)
	cc -o cccarm -g -Wall $(ARMSRCS)
	cp cccarm ccc

install: ccc
	mkdir -p $(INCDIR)
	rsync -a include/. $(INCDIR)
	cp ccc $(BINDIR)
	chmod +x $(BINDIR)/ccc

clean:
	rm -rf ccc* cccarm* *.o *.s tests/*.o tests/*.s *.out tests/*.out

test: ccc tests/runtests
	(cd tests; chmod +x runtests; ./runtests)

armtest: cccarm tests/runtests
	(cd tests; chmod +x runtests; ./runtests)

