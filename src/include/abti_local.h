/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef ABTI_LOCAL_H_INCLUDED
#define ABTI_LOCAL_H_INCLUDED

/*
 * An inlined getter function for ES Local Data.  This function is more
 * efficient than ABTI_local_get_local_uninlined, but it can be used once only
 * at the beginning of the function to avoid TLS caching across context switch.
 *
 * Consider the following case:
 *
 * int ABT_any_func()
 * {
 *     ABTI_xstream *p_xstream = ABTI_local_get_local()->p_xstream;
 *     [context switch (e.g., ABTI_thread_yield())];
 *     ABTI_xstream *p_xstream2 = ABTI_local_get_local()->p_xstream;
 * }
 *
 * p_xstream and p_xstream2 can be always the same although context switch
 * changes the running execution stream because a compiler assumes that the
 * running Pthreads is the same across the function call
 * (i.e., ABTI_thread_yield()) and caches a thread local value as a compiler
 * optimization.  To avoid this, we need to assure that the second
 * ABTI_local_get_local() really reads the thread local value again.
 *
 * See https://github.com/pmodels/argobots/issues/55 for details.
 *
 * ABTI_local_get_local_uninlined() guarantees that it truly reads the thread
 * local value of the current Pthreads, but it is slow.
 * ABTI_local_get_local_uninlined() should be used only after context switch
 * happens, and in other cases, ABTI_local_get_local() should be called for
 * performance.
 *
 * If you don't understand this problem well and it is not in the critical path,
 * use the uninlined version for correctness.
 */
static inline ABTI_local *ABTI_local_get_local(void)
{
    return lp_ABTI_local;
}

/*
 * A safe getter function for ES Local Data, which guarantees that it reads
 * the thread local value without referring to the cached TLS.  This is slower
 * than ABTI_local_get_local(), so use ABTI_local_get_local() if possible.
 */
static inline ABTI_local *ABTI_local_get_local_uninlined(void)
{
    return gp_ABTI_local_func.get_local_f();
}

/*
 * A setter function for ES Local Data.  This function is rarely called, so it
 * uses a slow version for correctness.
 */
static inline void ABTI_local_set_local(ABTI_local *p_local)
{
    gp_ABTI_local_func.set_local_f(p_local);
}

/*
 * A safe getter function for a pointer to an ES Local Data, which is useful to
 * identify a native thread (i.e., execution streams and external threads).
 */
static inline void *ABTI_local_get_local_ptr(void)
{
	return gp_ABTI_local_func.get_local_ptr_f();
}

#endif /* ABTI_LOCAL_H_INCLUDED */

