#ifndef __INTERNAL_THREAD__
#define __INTERNAL_THREAD__

typedef enum {
  /* Invalid state. */
  internal_thread_invalid = 0,

  /* Thread exiting. */
  internal_thread_exiting,

  /* Thread exited. */
  internal_thread_exited,

  /* Thread is running. */
  internal_thread_running,

  /* Thread self-termination. */
  internal_thread_termination,
} internal_thread_t;

#endif  // __INTERNAL_THREAD__
