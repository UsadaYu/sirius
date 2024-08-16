#ifndef __INTERNAL_THREAD__
#define __INTERNAL_THREAD__

typedef enum {
  /* invalid state */
  internal_thread_state_invalid = 0,

  /* thread exiting */
  internal_thread_state_exiting,

  /* thread exited */
  internal_thread_state_exited,

  /* thread is running */
  internal_thread_state_running,

  /* thread self-termination */
  internal_thread_state_termination,
} internal_thread_state_t;

#endif  // __INTERNAL_THREAD__
