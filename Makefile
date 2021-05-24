SRCS= cg.c decl.c expr.c gen.c main.c misc.c scan.c stmt.c sym.c tree.c
comp1: $(SRCS)
	cc -o comp1 -g $(SRCS)

clean:
	rm -rf comp1* *.o *.s out

test: comp1 input01 input02 input04 input05
	./comp1 input05
	cc -o out out.s
	./out
