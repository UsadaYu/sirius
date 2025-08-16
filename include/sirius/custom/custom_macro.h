#ifndef CUSTOM_MACRO_H
#define CUSTOM_MACRO_H

#define _custom_swallow(...) ((void)sizeof(#__VA_ARGS__))

#endif  // CUSTOM_MACRO_H
