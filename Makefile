SRCS= cg.c decl.c expr.c gen.c main.c misc.c scan.c stmt.c sym.c tree.c
comp1: $(SRCS)
	cc -o comp1 -g $(SRCS)

clean:
	rm -rf comp1* *.o *.s out

test: comp1 tests/runtests
	(cd tests; chmod +x runtests; ./runtests)

test8: comp1 tests/input08
	./comp1 tests/input08
	cc -o out out.s
	./out
