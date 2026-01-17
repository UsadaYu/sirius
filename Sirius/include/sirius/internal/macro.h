#ifndef SIRIUS_INTERNAL_MACRO_H
#define SIRIUS_INTERNAL_MACRO_H

#ifdef __cplusplus

#  if __cplusplus >= 201103L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201103L)
#    define SIRIUS_INTERNAL_THROW_SPEC noexcept(false)
#  else
#    define SIRIUS_INTERNAL_THROW_SPEC throw()
#  endif

#else

#  define SIRIUS_INTERNAL_THROW_SPEC

#endif

#endif
