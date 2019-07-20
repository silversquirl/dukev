#include <dlfcn.h>

#include <duktape.h>
#include <duk_module_duktape.h>

static int read_file(duk_context *ctx, duk_idx_t idx, const char *filename) {
	duk_size_t buflen, bufi = 0;
	char *buf = duk_get_buffer(ctx, idx, &buflen);
	if (!buf) return 1;

	FILE *f = fopen(filename, "r");
	if (!f) return 1;

	for (;;) {
		bufi += fread(buf + bufi, 1, buflen - bufi, f);
		if (bufi < buflen) break;
		duk_resize_buffer(ctx, idx, buflen *= 2);
	}

	if (ferror(f)) {
		fclose(f);
		return 1;
	}

	duk_resize_buffer(ctx, idx, bufi);

	fclose(f);
	return 0;
}

static duk_ret_t mod_finalize(duk_context *ctx) {
	duk_get_prop_literal(ctx, 0, "_dl");
	void *dl = duk_get_pointer(ctx, -1);
	dlclose(dl);
	return 0;
}

#define CT_MAX(a, b) ((a) > (b) ? (a) : (b))
#define MOD_BUFLEN(namelen) (sizeof "./" - 1 + CT_MAX(sizeof "dukopen_", CT_MAX(sizeof ".js", sizeof ".so")) + namelen)
enum {
	MOD_IDX_ID,
	MOD_IDX_REQUIRE,
	MOD_IDX_EXPORTS,
	MOD_IDX_MODULE,
};
static duk_ret_t mod_search(duk_context *ctx) {
	const char *name = duk_to_string(ctx, MOD_IDX_ID);
	size_t namelen = strlen(name);
	char *buf = duk_alloc(ctx, MOD_BUFLEN(namelen));
	strcpy(buf, "./");
	strcpy(buf+2, name);

	// DLL loading
	strcpy(buf +2+ namelen, ".so");
	void *dl = dlopen(buf, RTLD_LAZY);
	if (dl) {
		strcpy(buf, "dukopen_");
		strcpy(buf + sizeof "dukopen_" - 1, name);

		duk_c_function dukopen = (duk_c_function)dlsym(dl, buf);
		duk_free(ctx, buf);

		duk_push_c_function(ctx, dukopen, 0);
		duk_call(ctx, 0);

		duk_push_pointer(ctx, dl);
		duk_put_prop_literal(ctx, -2, "_dl");
		
		duk_push_c_function(ctx, mod_finalize, 1);
		duk_set_finalizer(ctx, -2);

		duk_put_prop_literal(ctx, MOD_IDX_MODULE, "exports");

		return 0;
	} else {
		// ECMA module loading
		strcpy(buf +2+ namelen, ".js");
		duk_push_dynamic_buffer(ctx, 1024);

		if (read_file(ctx, -1, buf)) {
			duk_free(ctx, buf);
			return duk_generic_error(ctx, "Could not find module %s", name);
		}

		duk_buffer_to_string(ctx, -1);
		duk_free(ctx, buf);
		return 1;
	}
}

static duk_ret_t duk_print(duk_context *ctx) {
	puts(duk_to_string(ctx, 0));
	return 0;
}

void fatal_handler(void *data, const char *msg) {
	fprintf(stderr, "%s\n", msg);
	fflush(stderr);
	abort();
}

int main() {
	duk_context *ctx = duk_create_heap(NULL, NULL, NULL, NULL, fatal_handler);
	duk_module_duktape_init(ctx);

	duk_get_global_literal(ctx, "Duktape");
	duk_push_c_function(ctx, mod_search, 4);
	duk_put_prop_literal(ctx, -2, "modSearch");

	duk_push_c_function(ctx, duk_print, 1);
	duk_put_global_literal(ctx, "print");

	duk_eval_string_noresult(ctx, "require('example');");

	return 0;
}
