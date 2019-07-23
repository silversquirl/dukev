// vim: fdm=marker noet
// Some IO stuff that's not related to libev

#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include <duktape.h>

#include "duk_helper.h"
#include "ct_assert.h"

// File API {{{
// The File API is entirely synchronous. For asynchronous file I/O use the AIO API in dukev

static duk_ret_t _file_constructor(duk_context *ctx) {
	duk_push_this(ctx);
	duk_idx_t thisI = duk_get_top_index(ctx);

	int fd = duk_require_int(ctx, 0);
	if (fd < 0) return duk_generic_error(ctx, "Invalid file descriptor");

	duk_push_literal(ctx, "fd");
	duk_dup(ctx, 0);
	duk_def_prop(ctx, thisI, DUK_DEFPROP_HAVE_VALUE|DUK_DEFPROP_CLEAR_WRITABLE);

	// Set close as the finalizer
	// var close = this.close;
	duk_get_prop_literal(ctx, thisI, "close");
	// var finalizer = close.bind(this);
	duk_push_literal(ctx, "bind");
	duk_dup(ctx, thisI);
	duk_call_prop(ctx, -3, 1);
	// Duktape.fin(this, finalizer);
	duk_set_finalizer(ctx, thisI);

	return 0;
}

static duk_ret_t _file_static_openFile(duk_context *ctx) {
	// Arguments
	const char *path = duk_require_string(ctx, 0);
	duk_int_t flags = duk_require_int(ctx, 1);
	duk_int_t mode = duk_opt_int(ctx, 1, 0666);

	// Open the file
	int fd = open(path, flags, mode);
	if (fd < 0) return duk_generic_error(ctx, "Failed to open file '%s': %s", path, strerror(errno));

	duk_push_this(ctx);
	duk_push_int(ctx, fd);
	duk_new(ctx, 1);
	return 1;
}

static duk_ret_t _file_static_open(duk_context *ctx) {
	duk_set_top(ctx, 1);
	duk_push_int(ctx, O_RDONLY);
	return _file_static_openFile(ctx);
}

static duk_ret_t _file_static_create(duk_context *ctx) {
	duk_set_top(ctx, 2);
	duk_push_int(ctx, O_RDWR|O_CREAT|O_TRUNC);
	duk_swap(ctx, -1, -2);
	return _file_static_openFile(ctx);
}

static duk_ret_t _file_static_read(duk_context *ctx) {
	// var file = this.open(arg1);
	duk_push_this(ctx);
	duk_push_literal(ctx, "open");
	duk_dup(ctx, 0); // The path is in arg 0
	duk_call_prop(ctx, -3, 1);
	duk_idx_t fileI = duk_get_top_index(ctx);

	// try {
	//   var data = file.readAll();
	duk_push_literal(ctx, "readAll");
	int err = duk_pcall_prop(ctx, fileI, 0);
	// } finally {
	//   file.close();
	duk_push_literal(ctx, "close");
	duk_call_prop(ctx, fileI, 0);
	duk_pop(ctx);
	if (err) return duk_throw(ctx);
	// }

	// return data;
	return 1;
}

static duk_ret_t _file_close(duk_context *ctx) {
	duk_push_this(ctx);
	duk_idx_t thisI = duk_get_top_index(ctx);

	// Get the fd to close
	duk_get_prop_literal(ctx, thisI, "fd");
	int fd = duk_require_int(ctx, -1);

	// Close the file
	if (close(fd)) return duk_generic_error(ctx, "Failed to close file: %s", strerror(errno));

	// Remove the finalizer
	duk_push_undefined(ctx);
	duk_set_finalizer(ctx, thisI);

	return 0;
}

static duk_ret_t _file_read(duk_context *ctx) {
	// Get the fd to read from
	duk_push_this(ctx);
	duk_get_prop_literal(ctx, -1, "fd");
	int fd = duk_require_int(ctx, -1);

	// Get the buffer to write to
	duk_size_t buflen;
	void *buf = duk_require_buffer_data(ctx, 0, &buflen);

	// Read the data
	ssize_t num_read = read(fd, buf, buflen);
	if (num_read < 0) return duk_generic_error(ctx, "Failed to read file: %s", strerror(errno));

	// Return the number of bytes read
	duk_push_number(ctx, num_read);
	return 1;
}

static duk_ret_t _file_readAll(duk_context *ctx) {
	enum {READALL_START_LEN = 1024};
	CT_ASSERT(READALL_START_LEN > 0);

	// Get the fd to read from
	duk_push_this(ctx);
	duk_get_prop_literal(ctx, -1, "fd");
	int fd = duk_require_int(ctx, -1);

	// Create a buffer
	duk_size_t buflen = READALL_START_LEN;
	char *buf = duk_push_dynamic_buffer(ctx, 0);
	duk_idx_t bufI = duk_get_top_index(ctx);

	// Read the file data
	duk_size_t bytes_read = 0;
	do {
		buflen += bytes_read;
		buf = duk_resize_buffer(ctx, bufI, buflen);

		ssize_t ret = read(fd, buf + bytes_read, buflen - bytes_read);
		if (ret < 0) return duk_generic_error(ctx, "Failed to read file: %s", strerror(errno));
		bytes_read += ret;
	} while (bytes_read == buflen);

	// If bytes_read < buflen, we've reached EOF
	// bytes_read > buflen should never happen

	// Shrink the buffer to the actual data size
	duk_resize_buffer(ctx, bufI, bytes_read);

	// Return the buffer
	duk_dup(ctx, bufI);
	return 1;
}

DUKH_FUNC_LIST(file_static) {
	DUKH_ENTRY(file_static, openFile, 3),
	DUKH_ENTRY(file_static, open, 1),
	DUKH_ENTRY(file_static, create, 1),

	DUKH_ENTRY(file_static, read, 1),

	{0}
};

DUKH_FUNC_LIST(file) {
	DUKH_ENTRY(file, close, 0),
	DUKH_ENTRY(file, read, 1),
	DUKH_ENTRY(file, readAll, 0),
	{0}
};
// }}}

duk_ret_t dukopen_io(duk_context *ctx) {
	duk_push_object(ctx);

	DUKH_REGISTER(File, file, 1);
	DUKH_STATIC(File, file);

	duk_get_prop_literal(ctx, -1, "File");
#define IO_FILE_CONST(name) do { \
	duk_push_int(ctx, name); \
	duk_put_prop_literal(ctx, -2, #name); \
} while (0)

#ifdef O_EXEC
	IO_FILE_CONST(O_EXEC);
#endif
	IO_FILE_CONST(O_RDONLY);
	IO_FILE_CONST(O_RDWR);
#ifdef O_SEARCH
	IO_FILE_CONST(O_SEARCH);
#endif
	IO_FILE_CONST(O_WRONLY);

	IO_FILE_CONST(O_APPEND);
	IO_FILE_CONST(O_CLOEXEC);
	IO_FILE_CONST(O_CREAT);
	IO_FILE_CONST(O_DIRECTORY);
	IO_FILE_CONST(O_DSYNC);
	IO_FILE_CONST(O_EXCL);
	IO_FILE_CONST(O_NOCTTY);
	IO_FILE_CONST(O_NOFOLLOW);
	IO_FILE_CONST(O_NONBLOCK);
	IO_FILE_CONST(O_RSYNC);
	IO_FILE_CONST(O_SYNC);
	IO_FILE_CONST(O_TRUNC);
#ifdef O_TTY_INIT
	IO_FILE_CONST(O_TTY_INIT);
#endif

#undef IO_FILE_CONST
	duk_pop(ctx);

	return 1;
}
