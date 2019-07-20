DUKTAPE := duktape/src-noline
DUKTAPE_EXTRAS := duktape/extras

CC := clang
CFLAGS := -std=c99 -Wall -pedantic -Wno-error=pedantic -I${DUKTAPE} -I${DUKTAPE_EXTRAS}/module-duktape -g

.PHONY: all
all: example dukev.so

example: example.c ${DUKTAPE}/duktape.c ${DUKTAPE_EXTRAS}/module-duktape/duk_module_duktape.c
	${CC} ${CFLAGS} -o $@ $^ -ldl -lm

dukev.so: dukev.c ${DUKTAPE}/duktape.c
	${CC} ${CFLAGS} -shared -o $@ $^ -lev -fPIC

${DUKTAPE}/duktape.c: ${DUKTAPE}/duktape.h ${DUKTAPE}/duk_config.h
${DUKTAPE_EXTRAS}/module-duktape/duk_module_duktape.c: ${DUKTAPE_EXTRAS}/module-duktape/duk_module_duktape.h

.PHONY: clean
clean:
	rm -f example dukev.so
