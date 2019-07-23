#pragma once
// A header for some stuff that's useful in any C duktape module

#include <duktape.h>

#define DUKH_FUNC_LIST(name) static duk_function_list_entry name##_funcs[] =

#define DUKH_ENTRY(class, name, nargs) {#name, _##class##_##name, nargs}

#define DUKH_REGISTER(constructor_js, constructor_c, constr_nargs) do { \
	duk_push_c_function(ctx, _##constructor_c##_constructor, constr_nargs); \
	duk_push_object(ctx); \
	\
	duk_put_function_list(ctx, -1, constructor_c##_funcs); \
	\
	duk_dup_top(ctx); \
	duk_set_prototype(ctx, -3); \
	duk_put_prop_literal(ctx, -2, "prototype"); \
	duk_put_prop_literal(ctx, -2, #constructor_js); \
} while (0)

#define DUKH_STATIC(constructor_js, constructor_c) do { \
	duk_get_prop_literal(ctx, -1, #constructor_js); \
	duk_put_function_list(ctx, -1, constructor_c##_static_funcs); \
	duk_pop(ctx); \
} while (0)
