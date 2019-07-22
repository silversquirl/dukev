#include <errno.h>
#include <stdio.h>

#include <duktape.h>
#include <duk_module_duktape.h>

#include "dukev.h"

static duk_ret_t read_file_sync(duk_context *ctx) {
	duk_size_t buflen = 1024, bufi = 0;
	char *buf = duk_push_dynamic_buffer(ctx, buflen);

	const char *filename = duk_get_string(ctx, 0);
	FILE *f = fopen(filename, "r");
	if (!f) return duk_generic_error(ctx, "Failed to open file '%s': %s", filename, strerror(errno));

	for (;;) {
		bufi += fread(buf + bufi, 1, buflen - bufi, f);
		if (bufi < buflen) break;
		buf = duk_resize_buffer(ctx, -1, buflen *= 2);
	}

	if (ferror(f)) {
		fclose(f);
		return duk_generic_error(ctx, "Failed to read file '%s': %s", filename, strerror(errno));
	}

	duk_resize_buffer(ctx, -1, bufi);
	duk_buffer_to_string(ctx, -1);
	fclose(f);
	return 1;
}

static duk_ret_t mod_search(duk_context *ctx) {
	// Arguments
	enum {
		nameI,
		requireI,
		exportsI,
		moduleI,
	};

	// Special case for dukev
	duk_push_literal(ctx, "dukev");
	if (duk_strict_equals(ctx, nameI, -1)) {
		// module.exports = <heap stash>.dukev;
		duk_push_heap_stash(ctx);
		duk_get_prop_literal(ctx, -1, "dukev");
		duk_put_prop_literal(ctx, moduleI, "exports");

		// module.filename = "dukev.so";
		duk_push_literal(ctx, "dukev.so");
		duk_put_prop_literal(ctx, moduleI, "filename");

		return 0;
	}
	duk_pop(ctx);

	// We need the file reader function later on
	duk_push_c_function(ctx, read_file_sync, 1);

	// Construct the filename to read
	duk_dup(ctx, nameI);
	duk_push_literal(ctx, ".js");
	duk_concat(ctx, 2);

	// Save the filename in module.filename
	duk_dup_top(ctx);
	duk_put_prop_literal(ctx, moduleI, "filename");

	// Read the file
	duk_call(ctx, 1);
	return 1;
}

static duk_ret_t duk_print(duk_context *ctx) {
	puts(duk_to_string(ctx, 0));
	return 0;
}

// print_repr is designed so it can be called directly from C, or from JS as an external C function taking 1 argument
// If called directly, it will print the item on top of the stack without removing it
static duk_ret_t print_repr(duk_context *ctx) {
	duk_idx_t objI = duk_get_top_index(ctx);

	// Duktape.enc(obj);
	duk_get_global_literal(ctx, "Duktape");
	duk_push_literal(ctx, "enc");
	duk_push_literal(ctx, "jx");
	duk_dup(ctx, objI);
	duk_call_prop(ctx, -4, 2);
	puts(duk_to_string(ctx, -1));

	duk_set_top(ctx, objI + 1);

	return 0;
}

static void fatal_handler(void *data, const char *msg) {
	fprintf(stderr, "fatal error: %s\n", msg);
	fflush(stderr);
	abort();
}

int main(int argc, char *argv[]) {
	if (argc != 2) {
		printf("Usage: %s <filename>\n", argv[0]);
		return 1;
	}
	const char *filename = argv[1];

	duk_context *ctx = duk_create_heap(NULL, NULL, NULL, NULL, fatal_handler);
	duk_module_duktape_init(ctx);

	duk_get_global_literal(ctx, "Duktape");
	duk_push_c_function(ctx, mod_search, 4);
	duk_put_prop_literal(ctx, -2, "modSearch");

	// <heap stash>.dukev = dukopen_dukev();
	duk_push_heap_stash(ctx);
	duk_push_c_function(ctx, dukopen_dukev, 0);
	duk_call(ctx, 0);
	duk_put_prop_literal(ctx, -2, "dukev");
	duk_pop(ctx);

	duk_push_c_function(ctx, duk_print, 1);
	duk_put_global_literal(ctx, "print");

	duk_push_c_function(ctx, print_repr, 1);
	duk_put_global_literal(ctx, "print_repr");

	// Load source code
	duk_push_c_function(ctx, read_file_sync, 1);
	duk_push_string(ctx, filename);
	duk_call(ctx, 1);

	// Compile and run
	duk_push_string(ctx, filename);
	if (duk_pcompile(ctx, DUK_COMPILE_SHEBANG)) {
		print_repr(ctx);
		fprintf(stderr, "%s: %s\n", filename, duk_safe_to_string(ctx, -1));
		return 1;
	}
	if (duk_pcall(ctx, 0)) {
		duk_get_prop_literal(ctx, -1, "stack");
		fprintf(stderr, "%s\n", duk_require_string(ctx, -1));
		return 1;
	}

	return 0;
}
