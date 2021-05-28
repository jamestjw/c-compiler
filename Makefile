SRCS= cg.c decl.c expr.c gen.c main.c misc.c scan.c stmt.c sym.c tree.c types.c
ARMSRCS= cg_arm.c decl.c expr.c gen.c main.c misc.c scan.c stmt.c \
	sym.c tree.c types.c

comp1: $(SRCS)
	cc -o comp1 -g -Wall $(SRCS)

comp1arm: $(ARMSRCS)
	cc -o comp1arm -g -Wall $(ARMSRCS)
	cp comp1arm comp1
clean:
	rm -rf comp1* comp1arm* *.o *.s out

test: comp1 tests/runtests
	(cd tests; chmod +x runtests; ./runtests)

armtest: comp1arm tests/runtests
	(cd tests; chmod +x runtests; ./runtests)

test14: comp1 tests/input14 lib/printint.c
	./comp1 tests/input14
	cc -o out out.s lib/printint.c
	./out

armtest14: comp1arm tests/input14 lib/printint.c
	./comp1 tests/input14
	cc -o out out.s lib/printint.c
	./out
