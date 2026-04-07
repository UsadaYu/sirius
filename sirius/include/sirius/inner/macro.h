#pragma once

#ifdef __cplusplus
#  if __cplusplus >= 201103L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201103L)
#    define _SS_INNER_THROW_SPEC noexcept(false)
#  else
#    define _SS_INNER_THROW_SPEC throw()
#  endif
#else
#  define _SS_INNER_THROW_SPEC
#endif
