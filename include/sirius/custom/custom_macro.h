#ifndef __CUSTOM_MACRO_H__
#define __CUSTOM_MACRO_H__

#define _custom_swallow(...) ((void)sizeof(#__VA_ARGS__))

#endif  // __CUSTOM_MACRO_H__
