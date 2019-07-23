DUKTAPE := duktape/src
DUKTAPE_EXTRAS := duktape/extras

CC := clang
AR := ar
CFLAGS := -std=c99 -Wall -pedantic -Wno-error=pedantic -I${DUKTAPE} -I${DUKTAPE_EXTRAS}/module-duktape -g
LDFLAGS :=

.PHONY: all
all: run

run: run.o dukev.o io.o duktape.a
	${CC} -o $@ $^ ${LDFLAGS} -lev -lm

dukev.so: dukev.o duktape.a
	${CC} -shared -o $@ $^ ${LDFLAGS} -lev -fPIC

%.o: %.c
	${CC} ${CFLAGS} -c -o $@ $<

duktape.a: ${DUKTAPE}/duktape.o ${DUKTAPE_EXTRAS}/module-duktape/duk_module_duktape.o
	${AR} rcs $@ $^

${DUKTAPE}/duktape.c: ${DUKTAPE}/duktape.h ${DUKTAPE}/duk_config.h
${DUKTAPE_EXTRAS}/module-duktape/duk_module_duktape.c: ${DUKTAPE_EXTRAS}/module-duktape/duk_module_duktape.h

.PHONY: clean
clean:
	rm -f run dukev.so
