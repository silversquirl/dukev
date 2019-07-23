// vim: fdm=marker noet
// TODO: event loop flags
// TODO: loop_run flags

#include <inttypes.h>
#include <stdint.h>

#include <duktape.h>
#include <ev.h>

#include "duk_helper.h"

// Generic bits that are used across all watchers {{{
struct dukev_data {
	duk_context *ctx;
	void *callback;
	void *watcher;
};

static void _watcher_callback(struct dukev_data *data, int events) {
	duk_push_heapptr(data->ctx, data->callback);
	duk_push_heapptr(data->ctx, data->watcher);
	duk_push_int(data->ctx, events);
	duk_call(data->ctx, 2);
}

#define DUKEV_WATCHER_CALLBACK(watcher_name) \
	static void _##watcher_name##_callback(struct ev_loop *loop, ev_##watcher_name *watcher, int events) { \
		_watcher_callback(watcher->data, events); \
	}

static duk_ret_t _watcher_finalize(duk_context *ctx) {
	// If the finalizer is being called, we're not referenced from anywhere which means stop() has already been called
	// Therefore, all we have to do is clean up the memory we're using
	duk_get_prop_literal(ctx, 0, "_ptr");
	ev_check *ptr = duk_require_pointer(ctx, -1);
	// Delete userdata
	duk_free(ctx, ptr->data);
	// Delete watcher
	duk_free(ctx, ptr);
	return 0;
}

// If parameters are defined, they must be preceded by two commas, eg:
//   DUKEV_WATCHER_CONSTRUCTOR(timer,, duk_require_number(ctx, 1), duk_get_number_default(ctx, 2, 0.))
#define DUKEV_WATCHER_CONSTRUCTOR(watcher_name, ...) \
	static duk_ret_t _##watcher_name##_constructor(duk_context *ctx) { \
		duk_push_this(ctx); \
		duk_idx_t thisI = duk_get_top_index(ctx); \
		\
		duk_get_prototype(ctx, thisI); \
		duk_put_prop_literal(ctx, thisI, "prototype"); \
		\
		/* Watcher data */ \
		struct dukev_data data; \
		data.ctx = ctx; \
		data.watcher = duk_get_heapptr(ctx, thisI); \
		duk_require_function(ctx, 0); \
		data.callback = duk_require_heapptr(ctx, 0); \
		\
		/* Allocate watcher */ \
		ev_##watcher_name *this = duk_alloc(ctx, sizeof *this); \
		duk_push_pointer(ctx, this); \
		duk_put_prop_literal(ctx, thisI, "_ptr"); \
		\
		/* Copy data */ \
		this->data = duk_alloc(ctx, sizeof data); \
		*(struct dukev_data *)this->data = data; \
		\
		/* Initialize watcher */ \
		ev_##watcher_name##_init(this, _##watcher_name##_callback __VA_ARGS__); \
		\
		/* Set finalizer */ \
		duk_push_c_function(ctx, _watcher_finalize, 1); \
		duk_set_finalizer(ctx, thisI); \
		\
		return 0; \
	}

#define DUKEV_WATCHER_HEADER() \
		duk_push_this(ctx); \
		duk_idx_t thisI = duk_get_top_index(ctx)

#define DUKEV_WATCHER_GET_THIS(watcher_name) \
		/* Get watcher */ \
		duk_get_prop_literal(ctx, thisI, "_ptr"); \
		ev_##watcher_name *this = duk_require_pointer(ctx, -1)

#define DUKEV_WATCHER_GET_LOOP() \
		/* Get loop */ \
		duk_get_prop_literal(ctx, thisI, "loop"); \
		duk_idx_t loopI = duk_get_top_index(ctx); \
		duk_get_prop_literal(ctx, loopI, "_ptr"); \
		struct ev_loop *loop = duk_require_pointer(ctx, -1)

#define DUKEV_WATCHER_START(watcher_name) \
	static duk_ret_t _##watcher_name##_start(duk_context *ctx) { \
		DUKEV_WATCHER_HEADER(); \
		duk_idx_t loopI = 0; \
		\
		/* Get loop */ \
		duk_get_prop_literal(ctx, loopI, "_ptr"); \
		struct ev_loop *loop = duk_require_pointer(ctx, -1); \
		\
		/* Get watcher */ \
		DUKEV_WATCHER_GET_THIS(watcher_name); \
		\
		/* If we're already active, throw an error */ \
		if (ev_is_active(this)) return duk_generic_error(ctx, "Cannot start an active watcher"); \
		\
		/* Link the watcher and loop objects to ensure GCing happens correctly */ \
		/* Watcher -> loop */ \
		duk_dup(ctx, loopI); \
		duk_put_prop_literal(ctx, thisI, "loop"); \
		/* Loop -> watcher */ \
		duk_get_prop_literal(ctx, loopI, "_watchers"); \
		duk_dup(ctx, thisI); \
		duk_put_prop_index(ctx, -2, (uintptr_t)this); \
		\
		/* Start watcher in loop */ \
		ev_##watcher_name##_start(loop, this); \
		\
		/* Return this so we can chain */ \
		duk_dup(ctx, thisI); \
		return 1; \
	}

static duk_bool_t _watcher_cleanup(duk_context *ctx, void *watcher, int loopI, int thisI) {
	if (ev_is_active(watcher) || ev_is_pending(watcher)) return 0;
	// Unlink loop and watcher
	// Loop -> watcher
	duk_get_prop_literal(ctx, loopI, "_watchers");
	duk_del_prop_index(ctx, -1, (uintptr_t)watcher);
	duk_pop(ctx);
	// Watcher -> loop
	duk_dup(ctx, thisI);
	duk_del_prop_literal(ctx, -1, "loop");
	duk_pop(ctx);
	return 1;
}

#define DUKEV_WATCHER_STOP(watcher_name) \
	static duk_ret_t _##watcher_name##_stop(duk_context *ctx) { \
		DUKEV_WATCHER_HEADER(); \
		DUKEV_WATCHER_GET_THIS(watcher_name); \
		DUKEV_WATCHER_GET_LOOP(); \
		\
		/* Stop watcher in loop */ \
		ev_##watcher_name##_stop(loop, this); \
		\
		/* Unlink loop and watcher */ \
		_watcher_cleanup(ctx, this, loopI, thisI); \
		\
		/* Return this so we can chain */ \
		duk_dup(ctx, thisI); \
		return 1; \
	}

#define DUKEV_WATCHER_BASE(watcher_name) \
	DUKEV_WATCHER_CALLBACK(watcher_name) \
	DUKEV_WATCHER_START(watcher_name) \
	DUKEV_WATCHER_STOP(watcher_name)

#define DUKEV_WATCHER_BASE_ENTRIES(watcher_name) \
	DUKH_ENTRY(watcher_name, start, 1), \
	DUKH_ENTRY(watcher_name, stop, 1)
// }}}

// Loop API {{{
static duk_ret_t _loop_finalize(duk_context *ctx) {
	// Destroy the event loop
	duk_get_prop_literal(ctx, 0, "_ptr");
	struct ev_loop *loop = duk_get_pointer(ctx, -1);
	ev_loop_destroy(loop);

	// Remove all references to our watchers so they can be GCed
	duk_del_prop_literal(ctx, 0, "_watchers");

	return 0;
}

static void _collect_watchers(struct ev_loop *loop, ev_prepare *watcher, int events) {
	struct dukev_data *data = watcher->data;
	duk_context *ctx = data->ctx;
	duk_idx_t base = duk_get_top(ctx);

	duk_push_heapptr(ctx, data->watcher); // In this dukev_data, watcher is actually pointing to the loop object
	duk_idx_t loopI = duk_get_top_index(ctx);
	duk_get_prop_literal(ctx, loopI, "_watchers");
	duk_enum(ctx, -1, 0);

	duk_bool_t empty = 1;
	while (duk_next(ctx, -1, 1)) {
		duk_idx_t thisI = duk_get_top_index(ctx);
		duk_get_prop_literal(ctx, thisI, "_ptr");
		void *watch = duk_require_pointer(ctx, -1);

		// We need to know if there are any items left in the list so we can delete ourselves if not
		if (!_watcher_cleanup(ctx, watch, loopI, thisI)) empty = 0;

		duk_pop_3(ctx);
	}

	if (empty) {
		// Stop event processing
		ev_prepare_stop(loop, watcher);

		// Delete user data
		duk_free(ctx, watcher->data);

		// Delete watcher
		duk_free(ctx, watcher);
	}

	duk_set_top(ctx, base);
}

static duk_ret_t _loop_construct_jsobj(duk_context *ctx, struct ev_loop *loop) {
	duk_push_this(ctx);
	duk_idx_t thisI = duk_get_top_index(ctx);

	// Set prototype
	duk_get_prototype(ctx, thisI);
	duk_put_prop_literal(ctx, thisI, "prototype");

	// Store pointer to C object
	duk_push_pointer(ctx, loop);
	duk_put_prop_literal(ctx, thisI, "_ptr");

	// Create watcher array
	duk_push_array(ctx);
	duk_put_prop_literal(ctx, -2, "_watchers");

	// Set finalizer
	duk_push_c_function(ctx, _loop_finalize, 1);
	duk_set_finalizer(ctx, -2);

	return 0;
}

static duk_ret_t _loop_constructor(duk_context *ctx) {
	return _loop_construct_jsobj(ctx, ev_loop_new(0));
}

static duk_ret_t _loop_default(duk_context *ctx) {
	return _loop_construct_jsobj(ctx, ev_default_loop(0));
}

static duk_ret_t _loop_run(duk_context *ctx) {
	duk_push_this(ctx);
	duk_idx_t thisI = duk_get_top_index(ctx);

	duk_get_prop_literal(ctx, -1, "_ptr");
	struct ev_loop *loop = duk_require_pointer(ctx, -1);

	ev_prepare *prepare = duk_alloc(ctx, sizeof *prepare);
	struct dukev_data data = {
		.watcher = duk_get_heapptr(ctx, thisI),
		.ctx = ctx,
	};
	prepare->data = duk_alloc(ctx, sizeof data);
	*(struct dukev_data *)prepare->data = data;
	ev_prepare_init(prepare, _collect_watchers);
	ev_prepare_start(loop, prepare);

	duk_bool_t ret = ev_run(loop, 0);
	duk_push_boolean(ctx, ret);
	return 1;
}

static duk_ret_t _loop_list_watchers(duk_context *ctx) {
	duk_push_this(ctx);
	duk_get_prop_literal(ctx, -1, "_watchers");
	duk_enum(ctx, -1, 0);
	while (duk_next(ctx, -1, 0)) {
		printf("0x%" PRIxPTR "\n", (uintptr_t)duk_to_number(ctx, -1));
		duk_pop(ctx);
	}
	return 0;
}

DUKH_FUNC_LIST(loop) {
	DUKH_ENTRY(loop, run, 0),
	DUKH_ENTRY(loop, list_watchers, 0),
	{0}
};
// }}}

// Timer API {{{
DUKEV_WATCHER_BASE(timer)
DUKEV_WATCHER_CONSTRUCTOR(timer,, duk_require_number(ctx, 1), duk_get_number_default(ctx, 2, 0.))
DUKH_FUNC_LIST(timer) {
	DUKEV_WATCHER_BASE_ENTRIES(timer),
	{0}
};
// }}}

// Async API {{{
DUKEV_WATCHER_BASE(async)
DUKEV_WATCHER_CONSTRUCTOR(async,)

static duk_ret_t _async_send(duk_context *ctx) {
	DUKEV_WATCHER_HEADER();
	DUKEV_WATCHER_GET_THIS(async);
	DUKEV_WATCHER_GET_LOOP();

	// Send the event
	ev_async_send(loop, this);

	// Return this so we can chain
	duk_dup(ctx, thisI);
	return 1;
}

DUKH_FUNC_LIST(async) {
	DUKEV_WATCHER_BASE_ENTRIES(async),
	DUKH_ENTRY(async, send, 0),
	{0}
};
// }}}

duk_ret_t dukopen_dukev(duk_context *ctx) {
	duk_push_object(ctx);

	DUKH_REGISTER(Loop, loop, 0);
	DUKH_REGISTER(Timer, timer, 3);
	DUKH_REGISTER(Async, async, 1);

	// Set dukev.default
	duk_push_c_function(ctx, _loop_default, 0);
	// _loop_default.prototype = exports.loop.prototype
	duk_get_prop_literal(ctx, -2, "Loop");
	duk_get_prop_literal(ctx, -1, "prototype");
	duk_put_prop_literal(ctx, -3, "prototype");
	duk_pop(ctx);
	// exports.default = new _loop_default();
	duk_new(ctx, 0);
	duk_put_prop_literal(ctx, -2, "default");

	return 1;
}
