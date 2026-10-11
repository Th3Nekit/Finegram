#ifndef GOOGLEMOCK_INCLUDE_GMOCK_INTERNAL_GMOCK_PP_H_
#define GOOGLEMOCK_INCLUDE_GMOCK_INTERNAL_GMOCK_PP_H_

#define GMOCK_PP_CAT(_1, _2) GMOCK_PP_INTERNAL_CAT(_1, _2)

#define GMOCK_PP_STRINGIZE(...) GMOCK_PP_INTERNAL_STRINGIZE(__VA_ARGS__)

#define GMOCK_PP_EMPTY(...)

#define GMOCK_PP_COMMA(...) ,

#define GMOCK_PP_IDENTITY(_1) _1

#define GMOCK_PP_NARG(...) \
  GMOCK_PP_INTERNAL_16TH(  \
      (__VA_ARGS__, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0))

#define GMOCK_PP_HAS_COMMA(...) \
  GMOCK_PP_INTERNAL_16TH(       \
      (__VA_ARGS__, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0))

#define GMOCK_PP_HEAD(...) GMOCK_PP_INTERNAL_HEAD((__VA_ARGS__, unusedArg))

#define GMOCK_PP_TAIL(...) GMOCK_PP_INTERNAL_TAIL((__VA_ARGS__))

#define GMOCK_PP_VARIADIC_CALL(_Macro, ...) \
  GMOCK_PP_IDENTITY(                        \
      GMOCK_PP_CAT(_Macro, GMOCK_PP_NARG(__VA_ARGS__))(__VA_ARGS__))

#define GMOCK_PP_IS_EMPTY(...)                                               \
  GMOCK_PP_INTERNAL_IS_EMPTY(GMOCK_PP_HAS_COMMA(__VA_ARGS__),                \
                             GMOCK_PP_HAS_COMMA(GMOCK_PP_COMMA __VA_ARGS__), \
                             GMOCK_PP_HAS_COMMA(__VA_ARGS__()),              \
                             GMOCK_PP_HAS_COMMA(GMOCK_PP_COMMA __VA_ARGS__()))

#define GMOCK_PP_IF(_Cond, _Then, _Else) \
  GMOCK_PP_CAT(GMOCK_PP_INTERNAL_IF_, _Cond)(_Then, _Else)

#define GMOCK_PP_GENERIC_IF(_Cond, _Then, _Else) \
  GMOCK_PP_REMOVE_PARENS(GMOCK_PP_IF(_Cond, _Then, _Else))

#define GMOCK_PP_NARG0(...) \
  GMOCK_PP_IF(GMOCK_PP_IS_EMPTY(__VA_ARGS__), 0, GMOCK_PP_NARG(__VA_ARGS__))

#define GMOCK_PP_IS_BEGIN_PARENS(...)                              \
  GMOCK_PP_HEAD(GMOCK_PP_CAT(GMOCK_PP_INTERNAL_IBP_IS_VARIADIC_R_, \
                             GMOCK_PP_INTERNAL_IBP_IS_VARIADIC_C __VA_ARGS__))

#define GMOCK_PP_IS_ENCLOSED_PARENS(...)             \
  GMOCK_PP_IF(GMOCK_PP_IS_BEGIN_PARENS(__VA_ARGS__), \
              GMOCK_PP_IS_EMPTY(GMOCK_PP_EMPTY __VA_ARGS__), 0)

#define GMOCK_PP_REMOVE_PARENS(...) GMOCK_PP_INTERNAL_REMOVE_PARENS __VA_ARGS__

#define GMOCK_PP_FOR_EACH(_Macro, _Data, _Tuple)                        \
  GMOCK_PP_CAT(GMOCK_PP_INTERNAL_FOR_EACH_IMPL_, GMOCK_PP_NARG0 _Tuple) \
  (0, _Macro, _Data, _Tuple)

#define GMOCK_PP_REPEAT(_Macro, _Data, _N)           \
  GMOCK_PP_CAT(GMOCK_PP_INTERNAL_FOR_EACH_IMPL_, _N) \
  (0, _Macro, _Data, GMOCK_PP_INTENRAL_EMPTY_TUPLE)

#define GMOCK_PP_INC(_i) GMOCK_PP_CAT(GMOCK_PP_INTERNAL_INC_, _i)

#define GMOCK_PP_COMMA_IF(_i) GMOCK_PP_CAT(GMOCK_PP_INTERNAL_COMMA_IF_, _i)

#define GMOCK_PP_INTENRAL_EMPTY_TUPLE (, , , , , , , , , , , , , , , )
#define GMOCK_PP_INTERNAL_CAT(_1, _2) _1##_2
#define GMOCK_PP_INTERNAL_STRINGIZE(...) #__VA_ARGS__
#define GMOCK_PP_INTERNAL_CAT_5(_1, _2, _3, _4, _5) _1##_2##_3##_4##_5
#define GMOCK_PP_INTERNAL_IS_EMPTY(_1, _2, _3, _4)                             \
  GMOCK_PP_HAS_COMMA(GMOCK_PP_INTERNAL_CAT_5(GMOCK_PP_INTERNAL_IS_EMPTY_CASE_, \
                                             _1, _2, _3, _4))
#define GMOCK_PP_INTERNAL_IS_EMPTY_CASE_0001 ,
#define GMOCK_PP_INTERNAL_IF_1(_Then, _Else) _Then
#define GMOCK_PP_INTERNAL_IF_0(_Then, _Else) _Else

#define GMOCK_PP_INTERNAL_INTERNAL_16TH(_1, _2, _3, _4, _5, _6, _7, _8, _9, \
                                        _10, _11, _12, _13, _14, _15, _16,  \
                                        ...)                                \
  _16
#define GMOCK_PP_INTERNAL_16TH(_Args) \
  GMOCK_PP_IDENTITY(GMOCK_PP_INTERNAL_INTERNAL_16TH _Args)
#define GMOCK_PP_INTERNAL_INTERNAL_HEAD(_1, ...) _1
#define GMOCK_PP_INTERNAL_HEAD(_Args) \
  GMOCK_PP_IDENTITY(GMOCK_PP_INTERNAL_INTERNAL_HEAD _Args)
#define GMOCK_PP_INTERNAL_INTERNAL_TAIL(_1, ...) __VA_ARGS__
#define GMOCK_PP_INTERNAL_TAIL(_Args) \
  GMOCK_PP_IDENTITY(GMOCK_PP_INTERNAL_INTERNAL_TAIL _Args)

#define GMOCK_PP_INTERNAL_IBP_IS_VARIADIC_C(...) 1 _
#define GMOCK_PP_INTERNAL_IBP_IS_VARIADIC_R_1 1,
#define GMOCK_PP_INTERNAL_IBP_IS_VARIADIC_R_GMOCK_PP_INTERNAL_IBP_IS_VARIADIC_C \
  0,
#define GMOCK_PP_INTERNAL_REMOVE_PARENS(...) __VA_ARGS__
#define GMOCK_PP_INTERNAL_INC_0 1
#define GMOCK_PP_INTERNAL_INC_1 2
#define GMOCK_PP_INTERNAL_INC_2 3
#define GMOCK_PP_INTERNAL_INC_3 4
#define GMOCK_PP_INTERNAL_INC_4 5
#define GMOCK_PP_INTERNAL_INC_5 6
#define GMOCK_PP_INTERNAL_INC_6 7
#define GMOCK_PP_INTERNAL_INC_7 8
#define GMOCK_PP_INTERNAL_INC_8 9
#define GMOCK_PP_INTERNAL_INC_9 10
#define GMOCK_PP_INTERNAL_INC_10 11
#define GMOCK_PP_INTERNAL_INC_11 12
#define GMOCK_PP_INTERNAL_INC_12 13
#define GMOCK_PP_INTERNAL_INC_13 14
#define GMOCK_PP_INTERNAL_INC_14 15
#define GMOCK_PP_INTERNAL_INC_15 16
#define GMOCK_PP_INTERNAL_COMMA_IF_0
#define GMOCK_PP_INTERNAL_COMMA_IF_1 ,
#define GMOCK_PP_INTERNAL_COMMA_IF_2 ,
#define GMOCK_PP_INTERNAL_COMMA_IF_3 ,
#define GMOCK_PP_INTERNAL_COMMA_IF_4 ,
#define GMOCK_PP_INTERNAL_COMMA_IF_5 ,
#define GMOCK_PP_INTERNAL_COMMA_IF_6 ,
#define GMOCK_PP_INTERNAL_COMMA_IF_7 ,
#define GMOCK_PP_INTERNAL_COMMA_IF_8 ,
#define GMOCK_PP_INTERNAL_COMMA_IF_9 ,
#define GMOCK_PP_INTERNAL_COMMA_IF_10 ,
#define GMOCK_PP_INTERNAL_COMMA_IF_11 ,
#define GMOCK_PP_INTERNAL_COMMA_IF_12 ,
#define GMOCK_PP_INTERNAL_COMMA_IF_13 ,
#define GMOCK_PP_INTERNAL_COMMA_IF_14 ,
#define GMOCK_PP_INTERNAL_COMMA_IF_15 ,
#define GMOCK_PP_INTERNAL_CALL_MACRO(_Macro, _i, _Data, _element) \
  _Macro(_i, _Data, _element)
#define GMOCK_PP_INTERNAL_FOR_EACH_IMPL_0(_i, _Macro, _Data, _Tuple)
#define GMOCK_PP_INTERNAL_FOR_EACH_IMPL_1(_i, _Macro, _Data, _Tuple) \
  GMOCK_PP_INTERNAL_CALL_MACRO(_Macro, _i, _Data, GMOCK_PP_HEAD _Tuple)
#define GMOCK_PP_INTERNAL_FOR_EACH_IMPL_2(_i, _Macro, _Data, _Tuple)    \
  GMOCK_PP_INTERNAL_CALL_MACRO(_Macro, _i, _Data, GMOCK_PP_HEAD _Tuple) \
  GMOCK_PP_INTERNAL_FOR_EACH_IMPL_1(GMOCK_PP_INC(_i), _Macro, _Data,    \
                                    (GMOCK_PP_TAIL _Tuple))
#define GMOCK_PP_INTERNAL_FOR_EACH_IMPL_3(_i, _Macro, _Data, _Tuple)    \
  GMOCK_PP_INTERNAL_CALL_MACRO(_Macro, _i, _Data, GMOCK_PP_HEAD _Tuple) \
  GMOCK_PP_INTERNAL_FOR_EACH_IMPL_2(GMOCK_PP_INC(_i), _Macro, _Data,    \
                                    (GMOCK_PP_TAIL _Tuple))
#define GMOCK_PP_INTERNAL_FOR_EACH_IMPL_4(_i, _Macro, _Data, _Tuple)    \
  GMOCK_PP_INTERNAL_CALL_MACRO(_Macro, _i, _Data, GMOCK_PP_HEAD _Tuple) \
  GMOCK_PP_INTERNAL_FOR_EACH_IMPL_3(GMOCK_PP_INC(_i), _Macro, _Data,    \
                                    (GMOCK_PP_TAIL _Tuple))
#define GMOCK_PP_INTERNAL_FOR_EACH_IMPL_5(_i, _Macro, _Data, _Tuple)    \
  GMOCK_PP_INTERNAL_CALL_MACRO(_Macro, _i, _Data, GMOCK_PP_HEAD _Tuple) \
  GMOCK_PP_INTERNAL_FOR_EACH_IMPL_4(GMOCK_PP_INC(_i), _Macro, _Data,    \
                                    (GMOCK_PP_TAIL _Tuple))
#define GMOCK_PP_INTERNAL_FOR_EACH_IMPL_6(_i, _Macro, _Data, _Tuple)    \
  GMOCK_PP_INTERNAL_CALL_MACRO(_Macro, _i, _Data, GMOCK_PP_HEAD _Tuple) \
  GMOCK_PP_INTERNAL_FOR_EACH_IMPL_5(GMOCK_PP_INC(_i), _Macro, _Data,    \
                                    (GMOCK_PP_TAIL _Tuple))
#define GMOCK_PP_INTERNAL_FOR_EACH_IMPL_7(_i, _Macro, _Data, _Tuple)    \
  GMOCK_PP_INTERNAL_CALL_MACRO(_Macro, _i, _Data, GMOCK_PP_HEAD _Tuple) \
  GMOCK_PP_INTERNAL_FOR_EACH_IMPL_6(GMOCK_PP_INC(_i), _Macro, _Data,    \
                                    (GMOCK_PP_TAIL _Tuple))
#define GMOCK_PP_INTERNAL_FOR_EACH_IMPL_8(_i, _Macro, _Data, _Tuple)    \
  GMOCK_PP_INTERNAL_CALL_MACRO(_Macro, _i, _Data, GMOCK_PP_HEAD _Tuple) \
  GMOCK_PP_INTERNAL_FOR_EACH_IMPL_7(GMOCK_PP_INC(_i), _Macro, _Data,    \
                                    (GMOCK_PP_TAIL _Tuple))
#define GMOCK_PP_INTERNAL_FOR_EACH_IMPL_9(_i, _Macro, _Data, _Tuple)    \
  GMOCK_PP_INTERNAL_CALL_MACRO(_Macro, _i, _Data, GMOCK_PP_HEAD _Tuple) \
  GMOCK_PP_INTERNAL_FOR_EACH_IMPL_8(GMOCK_PP_INC(_i), _Macro, _Data,    \
                                    (GMOCK_PP_TAIL _Tuple))
#define GMOCK_PP_INTERNAL_FOR_EACH_IMPL_10(_i, _Macro, _Data, _Tuple)   \
  GMOCK_PP_INTERNAL_CALL_MACRO(_Macro, _i, _Data, GMOCK_PP_HEAD _Tuple) \
  GMOCK_PP_INTERNAL_FOR_EACH_IMPL_9(GMOCK_PP_INC(_i), _Macro, _Data,    \
                                    (GMOCK_PP_TAIL _Tuple))
#define GMOCK_PP_INTERNAL_FOR_EACH_IMPL_11(_i, _Macro, _Data, _Tuple)   \
  GMOCK_PP_INTERNAL_CALL_MACRO(_Macro, _i, _Data, GMOCK_PP_HEAD _Tuple) \
  GMOCK_PP_INTERNAL_FOR_EACH_IMPL_10(GMOCK_PP_INC(_i), _Macro, _Data,   \
                                     (GMOCK_PP_TAIL _Tuple))
#define GMOCK_PP_INTERNAL_FOR_EACH_IMPL_12(_i, _Macro, _Data, _Tuple)   \
  GMOCK_PP_INTERNAL_CALL_MACRO(_Macro, _i, _Data, GMOCK_PP_HEAD _Tuple) \
  GMOCK_PP_INTERNAL_FOR_EACH_IMPL_11(GMOCK_PP_INC(_i), _Macro, _Data,   \
                                     (GMOCK_PP_TAIL _Tuple))
#define GMOCK_PP_INTERNAL_FOR_EACH_IMPL_13(_i, _Macro, _Data, _Tuple)   \
  GMOCK_PP_INTERNAL_CALL_MACRO(_Macro, _i, _Data, GMOCK_PP_HEAD _Tuple) \
  GMOCK_PP_INTERNAL_FOR_EACH_IMPL_12(GMOCK_PP_INC(_i), _Macro, _Data,   \
                                     (GMOCK_PP_TAIL _Tuple))
#define GMOCK_PP_INTERNAL_FOR_EACH_IMPL_14(_i, _Macro, _Data, _Tuple)   \
  GMOCK_PP_INTERNAL_CALL_MACRO(_Macro, _i, _Data, GMOCK_PP_HEAD _Tuple) \
  GMOCK_PP_INTERNAL_FOR_EACH_IMPL_13(GMOCK_PP_INC(_i), _Macro, _Data,   \
                                     (GMOCK_PP_TAIL _Tuple))
#define GMOCK_PP_INTERNAL_FOR_EACH_IMPL_15(_i, _Macro, _Data, _Tuple)   \
  GMOCK_PP_INTERNAL_CALL_MACRO(_Macro, _i, _Data, GMOCK_PP_HEAD _Tuple) \
  GMOCK_PP_INTERNAL_FOR_EACH_IMPL_14(GMOCK_PP_INC(_i), _Macro, _Data,   \
                                     (GMOCK_PP_TAIL _Tuple))

#endif
