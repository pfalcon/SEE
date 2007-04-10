/*
 * Copyright (c) 2003
 *      David Leonard.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Mr Leonard nor the names of the contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY DAVID LEONARD AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL DAVID LEONARD OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/* $Id$ */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#if HAVE_LIMITS_H
# include <limits.h>
#else
# define UINT_MAX (~(unsigned int)0)
#endif

#include <string.h>

#include <see/mem.h>
#include <see/system.h>
#include <see/error.h>
#include <see/string.h>

#include "stringdefs.h"
#include "dprint.h"

#ifndef NDEBUG
int SEE_mem_debug = 0;
#endif

#undef SEE_malloc
#undef SEE_malloc_finalize
#undef SEE_malloc_string
#undef SEE_free
#undef SEE_grow_to

/*------------------------------------------------------------
 * Wrappers around memory allocators that check for failure
 */

/*
 * Allocates size bytes of garbage-collected storage.
 */
void *
SEE_malloc(interp, size)
	struct SEE_interpreter *interp;
	SEE_size_t size;
{
	void *data;

	if (size == 0)
		return NULL;
	data = (*SEE_system.malloc)(interp, size);
	if (data == NULL) 
		(*SEE_system.mem_exhausted)(interp);
	return data;
}

/*
 * Allocates size bytes of garbage-collected storage, attaching
 * a finalizer function that will be called when the storage becomes
 * unreachable.
 */
void *
SEE_malloc_finalize(interp, size, finalizefn, closure)
	struct SEE_interpreter *interp;
	SEE_size_t size;
	void (*finalizefn)(struct SEE_interpreter *, void *, void *);
	void *closure;
{
	void *data;

	if (size == 0)
		return NULL;
	data = (*SEE_system.malloc_finalize)(interp, size, finalizefn, closure);
	if (data == NULL) 
		(*SEE_system.mem_exhausted)(interp);
	return data;
}

/*
 * Allocates size bytes of garbage-collected, string storage.
 * This function is just like SEE_malloc(), except that the caller
 * guarantees that no pointers will be stored in the data. This
 * improves performance with strings.
 */
void *
SEE_malloc_string(interp, size)
	struct SEE_interpreter *interp;
	SEE_size_t size;
{
	void *data;

	if (size == 0)
		return NULL;
	if (SEE_system.malloc_string)
		data = (*SEE_system.malloc_string)(interp, size);
	else
		data = (*SEE_system.malloc)(interp, size);
	if (data == NULL) 
		(*SEE_system.mem_exhausted)(interp);
	return data;
}

/*
 * Releases memory that the caller *knows* is unreachable.
 */
void
SEE_free(interp, memp)
	struct SEE_interpreter *interp;
	void **memp;
{
	if (*memp) {
		(*SEE_system.free)(interp, *memp);
		*memp = NULL;
	}
}

/*
 * Forces a garbage collection, or does nothing if unsupported.
 */
void
SEE_gcollect(interp)
	struct SEE_interpreter *interp;
{
	if (SEE_system.gcollect)
		(*SEE_system.gcollect)(interp);
}

/*
 * The debug variants must not be protected by NDEBUG.
 * This is for the case when the library is compiled with NDEBUG,
 * but the library-user application is not.
 */

void *
_SEE_malloc_debug(interp, size, file, line, arg)
	struct SEE_interpreter *interp;
	SEE_size_t size;
	const char *file;
	int line;
	const char *arg;
{
	void *data;

#ifndef NDEBUG
	if (SEE_mem_debug)
		dprintf("malloc %u (%s:%d '%s')", size, file, line, arg);
#endif
	data = SEE_malloc(interp, size);
#ifndef NDEBUG
	if (SEE_mem_debug)
		dprintf(" -> %p\n", data);
#endif
	return data;
}

void *
_SEE_malloc_finalize_debug(interp, size, finalizefn, closure, file, line, arg)
	struct SEE_interpreter *interp;
	SEE_size_t size;
	void (*finalizefn)(struct SEE_interpreter *, void *, void *);
	void *closure;
	const char *file;
	int line;
	const char *arg;
{
	void *data;

#ifndef NDEBUG
	if (SEE_mem_debug)
		dprintf("malloc %u (%s:%d '%s')", size, file, line, arg);
#endif
	data = SEE_malloc_finalize(interp, size, finalizefn, closure);
#ifndef NDEBUG
	if (SEE_mem_debug)
		dprintf(" -> %p\n", data);
#endif
	return data;
}

void *
_SEE_malloc_string_debug(interp, size, file, line, arg)
	struct SEE_interpreter *interp;
	SEE_size_t size;
	const char *file;
	int line;
	const char *arg;
{
	void *data;

#ifndef NDEBUG
	if (SEE_mem_debug)
		dprintf("malloc_string %u (%s:%d '%s')", size, file, 
			line, arg);
#endif
	data = SEE_malloc_string(interp, size);
#ifndef NDEBUG
	if (SEE_mem_debug)
		dprintf(" -> %p\n", data);
#endif
	return data;
}

void
_SEE_free_debug(interp, memp, file, line, arg)
	struct SEE_interpreter *interp;
	void **memp;
	const char *file;
	int line;
	const char *arg;
{
#ifndef NDEBUG
	if (SEE_mem_debug)
		dprintf("free %p (%s:%d '%s')", *memp, file, line, arg);
#endif
	SEE_free(interp, memp);
}

/* 
 * Memory segments start at GROW_INITIAL_SIZE and double until
 * they would surpass GROW_MAXIMUM_SIZE. That should be slightly
 * less than the maximum hardware segment size (to account for
 * malloc overhead
 */
#ifndef GROW_INITIAL_SIZE
# define GROW_INITIAL_SIZE   1024		/* bytes */
#endif
#ifndef GROW_MAXIMUM_SIZE
# define GROW_MAXIMUM_SIZE   (UINT_MAX - 128)	/* bytes */
#endif

void
SEE_grow_to(interp, grow, new_len)
	struct SEE_interpreter *interp;
	struct SEE_growable *grow;
	unsigned int new_len;
{
	SEE_size_t new_alloc;
	void *new_ptr;

	if (new_len > GROW_MAXIMUM_SIZE / grow->element_size)
	    SEE_error_throw_string(interp, interp->Error,
		STR(string_limit_reached));
	new_alloc = grow->allocated;
	while (new_len > new_alloc / grow->element_size)
	    if (new_alloc == 0)
		new_alloc = GROW_INITIAL_SIZE;
	    else if (new_alloc >= GROW_MAXIMUM_SIZE / 2)
		new_alloc = GROW_MAXIMUM_SIZE;
	    else
		new_alloc *= 2;

	if (new_alloc != grow->allocated) {
	    if (grow->is_string)
		new_ptr = SEE_malloc(interp, new_alloc);
	    else
		new_ptr = SEE_malloc_string(interp, new_alloc);
	    if (*grow->length_ptr)
		memcpy(new_ptr, *grow->data_ptr, 
		    *grow->length_ptr * grow->element_size);
	    if (*grow->data_ptr)
		SEE_free(interp, *grow->data_ptr);
#ifndef NDEBUG
	    if (SEE_mem_debug)
		dprintf("grow from %p/%u/%u -> %p/%u/%u%s\n", 
		    *grow->data_ptr, *grow->length_ptr, grow->allocated,
		    new_ptr, new_len, new_alloc, 
		    grow->is_string ? " [string]":"");
#endif
	    *grow->data_ptr = new_ptr;
	    grow->allocated = new_alloc;
	}
	*grow->length_ptr = new_len;
}

void
_SEE_grow_to_debug(interp, grow, new_len, file, line)
	struct SEE_interpreter *interp;
	struct SEE_growable *grow;
	unsigned int new_len;
	const char *file;
	int line;
{
#ifndef NDEBUG
	if (SEE_mem_debug)
		dprintf("grow %p %d->%d (%s:%d)", 
		    grow, 
		    grow && grow->length_ptr ? *grow->length_ptr : -1,
		    new_len,
		    file, line);
#endif
	SEE_grow_to(interp, grow, new_len);
}
