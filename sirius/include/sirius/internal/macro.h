#pragma once

#ifdef __cplusplus

#  if __cplusplus >= 201103L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201103L)
#    define _sirius_throw_spec noexcept(false)
#  else
#    define _sirius_throw_spec throw()
#  endif

#else

#  define _sirius_throw_spec

#endif
