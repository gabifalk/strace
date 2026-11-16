/*
 * Copyright (c) 2013 Luca Clementi <luca.clementi@gmail.com>
 * Copyright (c) 2013-2026 The strace developers.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "defs.h"
#include "unwind.h"

#ifdef USE_DEMANGLE
/* Avoids including libiberty.h that has several undesirable definitions */
# define LIBIBERTY_H

# if defined HAVE_DEMANGLE_H
#  include <demangle.h>
# elif defined HAVE_LIBIBERTY_DEMANGLE_H
#  include <libiberty/demangle.h>
# endif /* HAVE_DEMANGLE_H */
#endif /* USE_DEMANGLE */

/*
 * Type used in stacktrace capturing
 */
struct call_t {
	struct call_t *next;
	char *binary_filename;
	char *symbol_name;
	unwind_function_offset_t function_offset;
	unsigned long true_offset;
	char *source_filename;
	int source_line;
	char *error;
};

struct unwind_queue_t {
	struct call_t *tail;
	struct call_t *head;
};

static void queue_drain(struct unwind_queue_t *queue, bool print);

void
unwind_init(bool with_srcinfo, int stack_trace_limit)
{
	if (unwinder.init)
		unwinder.init(with_srcinfo, stack_trace_limit);
}

void
unwind_tcb_init(struct tcb *tcp)
{
	if (tcp->unwind_queue)
		return;

	tcp->unwind_queue = xmalloc(sizeof(*tcp->unwind_queue));
	tcp->unwind_queue->head = NULL;
	tcp->unwind_queue->tail = NULL;

	tcp->unwind_ctx = unwinder.tcb_init(tcp);
}

void
unwind_tcb_fin(struct tcb *tcp)
{
	if (!tcp->unwind_queue)
		return;

	queue_drain(tcp->unwind_queue, true);
	free(tcp->unwind_queue);
	tcp->unwind_queue = NULL;

	unwinder.tcb_fin(tcp);
	tcp->unwind_ctx = NULL;
}

/*
 * printing an entry in stack
 */
static char *
demangle_symbol(const char *symbol_name)
{
#ifdef USE_DEMANGLE
	char *demangled_name = cplus_demangle(symbol_name,
					      DMGL_AUTO | DMGL_PARAMS);
	if (demangled_name)
		return demangled_name;
#endif
	return xstrdup(symbol_name);
}

static void
print_call(const char *binary_filename,
	   const char *symbol_name,
	   unwind_function_offset_t function_offset,
	   unsigned long true_offset,
	   const char *source_filename,
	   int source_line,
	   const char *func)
{
	if (symbol_name || binary_filename) {
		tprint_stack_frame(binary_filename, symbol_name,
				   function_offset, true_offset,
				   source_filename, source_line);
	} else {
		char *bug = xasprintf("BUG IN %s", func);
		tprint_stack_error(bug, 0);
		free(bug);
	}
}

static void
print_call_cb(void *dummy,
	      const char *binary_filename,
	      const char *symbol_name,
	      unwind_function_offset_t function_offset,
	      unsigned long true_offset,
	      const char *source_filename,
	      int source_line)
{
	char *symbol = NULL;

	if (symbol_name && (symbol_name[0] != '\0'))
		symbol = demangle_symbol(symbol_name);
	print_call(binary_filename, symbol, function_offset, true_offset,
		   source_filename, source_line, __func__);
	free(symbol);
}

static void
print_error_cb(void *dummy,
	       const char *error,
	       unsigned long true_offset)
{
	tprint_stack_error(error, true_offset);
}

/*
 * queue manipulators
 */
static void
queue_put(struct unwind_queue_t *queue,
	  const char *binary_filename,
	  const char *symbol_name,
	  unwind_function_offset_t function_offset,
	  unsigned long true_offset,
	  const char *source_filename,
	  int source_line,
	  const char *error)
{
	struct call_t *call;

	call = xmalloc(sizeof(*call));
	call->binary_filename = xstrdup(binary_filename);
	call->symbol_name = symbol_name ? demangle_symbol(symbol_name) : NULL;
	call->function_offset = function_offset;
	call->true_offset = true_offset;
	call->source_filename = xstrdup(source_filename);
	call->source_line = source_line;
	call->error = xstrdup(error);
	call->next = NULL;

	if (!queue->head) {
		queue->head = call;
		queue->tail = call;
	} else {
		queue->tail->next = call;
		queue->tail = call;
	}
}

static void
queue_put_call(void *queue,
	       const char *binary_filename,
	       const char *symbol_name,
	       unwind_function_offset_t function_offset,
	       unsigned long true_offset,
	       const char *source_filename,
	       int source_line)
{
	queue_put(queue,
		  binary_filename,
		  symbol_name,
		  function_offset,
		  true_offset,
		  source_filename,
		  source_line,
		  NULL);
}

static void
queue_put_error(void *queue,
		const char *error,
		unsigned long ip)
{
	queue_put(queue, NULL, NULL, 0, ip, NULL, 0, error);
}

static void
queue_drain(struct unwind_queue_t *queue, bool print)
{
	struct call_t *call, *tmp;

	queue->tail = NULL;
	call = queue->head;
	queue->head = NULL;
	while (call) {
		tmp = call;
		call = call->next;

		if (print) {
			if (tmp->symbol_name || tmp->binary_filename
			    || !tmp->error)
				print_call(tmp->binary_filename,
					   tmp->symbol_name,
					   tmp->function_offset,
					   tmp->true_offset,
					   tmp->source_filename,
					   tmp->source_line,
					   "sprint_call_or_error");
			else
				tprint_stack_error(tmp->error,
						   tmp->true_offset);
		}

		free(tmp->binary_filename);
		free(tmp->symbol_name);
		free(tmp->source_filename);
		free(tmp->error);
		free(tmp);
	}
}

/*
 * printing stack
 */
void
unwind_tcb_print(struct tcb *tcp)
{
#if defined(USE_LIBUNWIND) && (SUPPORTED_PERSONALITIES > 1)
	if (tcp->currpers != DEFAULT_PERSONALITY) {
		/* disable stack trace */
		return;
	}
#endif
	tprint_stack_begin();
	if (tcp->unwind_queue->head) {
		debug_func_msg("head: tcp=%p, queue=%p",
			       tcp, tcp->unwind_queue->head);
		queue_drain(tcp->unwind_queue, true);
	} else
		unwinder.tcb_walk(tcp, print_call_cb, print_error_cb, NULL);
	tprint_stack_end();
}

void
unwind_tcb_discard(struct tcb *tcp)
{
	queue_drain(tcp->unwind_queue, false);
}

/*
 * capturing stack
 */
void
unwind_tcb_capture(struct tcb *tcp)
{
#if defined(USE_LIBUNWIND) && (SUPPORTED_PERSONALITIES > 1)
	if (tcp->currpers != DEFAULT_PERSONALITY) {
		/* disable stack trace */
		return;
	}
#endif
	if (tcp->unwind_queue->head)
		error_msg_and_die("bug: unprinted entries in queue");
	else {
		debug_func_msg("walk: tcp=%p, queue=%p",
			       tcp, tcp->unwind_queue->head);
		unwinder.tcb_walk(tcp, queue_put_call, queue_put_error,
				  tcp->unwind_queue);
	}
}
