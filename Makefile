SRCS= cg.c decl.c expr.c gen.c main.c misc.c scan.c stmt.c sym.c tree.c types.c
ARMSRCS= cg_arm.c decl.c expr.c gen.c main.c misc.c scan.c stmt.c \
	sym.c tree.c types.c

ccc: $(SRCS)
	cc -o ccc -g -Wall $(SRCS)

cccarm: $(ARMSRCS)
	cc -o cccarm -g -Wall $(ARMSRCS)
	cp cccarm ccc
clean:
	rm -rf ccc* cccarm* *.o *.s tests/*.o tests/*.s *.out tests/*.out

test: ccc tests/runtests
	(cd tests; chmod +x runtests; ./runtests)

armtest: cccarm tests/runtests
	(cd tests; chmod +x runtests; ./runtests)

