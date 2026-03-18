#ifndef ZSONET_UTILS_H
#define ZSONET_UTILS_H

#include <linux/build_bug.h>

#ifndef SHOW_MACRO_DEBUG
    #define __NL__ 
    #define __T
    #define __NLT__
#endif

#define __resolve(x) x
#define _str(x) __resolve(#x)


#define assert_offset(type, member, offset) \
    static_assert(offsetof(type, member) == (offset), "Wrong offset of member " #member "! Expected: " _str(offset))

#define assert_size(type, expected) \
    static_assert(sizeof(type) == (expected), "Wrong size of " #type "! Expected: " #expected)


#define SWITCH_0(func) \
case  0: {__NLT__ func( 0); __NLT__ break; __NLT__ }
#define SWITCH_1(func) \
case  1: {__NLT__ func( 1); __NLT__ break; __NLT__ } __NLT__ SWITCH_0(func)
#define SWITCH_2(func) \
case  2: {__NLT__ func( 2); __NLT__ break; __NLT__ } __NLT__ SWITCH_1(func)
#define SWITCH_3(func) \
case  3: {__NLT__ func( 3); __NLT__ break; __NLT__ } __NLT__ SWITCH_2(func)
#define SWITCH_4(func) \
case  4: {__NLT__ func( 4); __NLT__ break; __NLT__ } __NLT__ SWITCH_3(func)
#define SWITCH_5(func) \
case  5: {__NLT__ func( 5); __NLT__ break; __NLT__ } __NLT__ SWITCH_4(func)
#define SWITCH_6(func) \
case  6: {__NLT__ func( 6); __NLT__ break; __NLT__ } __NLT__ SWITCH_5(func)
#define SWITCH_7(func) \
case  7: {__NLT__ func( 7); __NLT__ break; __NLT__ } __NLT__ SWITCH_6(func)
#define SWITCH_8(func) \
case  8: {__NLT__ func( 8); __NLT__ break; __NLT__ } __NLT__ SWITCH_7(func)
#define SWITCH_9(func) \
case  9: {__NLT__ func( 9); __NLT__ break; __NLT__ } __NLT__ SWITCH_8(func)
#define SWITCH_10(func)\
case 10: {__NLT__ func(10); __NLT__ break; __NLT__ } __NLT__ SWITCH_9(func)

#define CONST_SWITCH(N, x, func) switch(x) {__NLT__ SWITCH_##N(func) }

#define CONST_SWITCH_RANGE(N, start, expr, func) \
    switch ((expr) + 1 - (start)) {__NLT__ SWITCH_##N(func)}

#endif /* ZSONET_UTILS_H */