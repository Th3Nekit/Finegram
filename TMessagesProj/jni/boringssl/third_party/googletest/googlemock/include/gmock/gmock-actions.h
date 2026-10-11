// Copyright 2007, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Google Mock - a framework for writing C++ mock classes.
//
// The ACTION* family of macros can be used in a namespace scope to
// define custom actions easily.  The syntax:
//
//   ACTION(name) { statements; }
//
// will define an action with the given name that executes the
// statements.  The value returned by the statements will be used as
// the return value of the action.  Inside the statements, you can
// refer to the K-th (0-based) argument of the mock function by
// 'argK', and refer to its type by 'argK_type'.  For example:
//
//   ACTION(IncrementArg1) {
//     arg1_type temp = arg1;
//     return ++(*temp);
//   }
//
// allows you to write
//
//   ...WillOnce(IncrementArg1());
//
// You can also refer to the entire argument tuple and its type by
// 'args' and 'args_type', and refer to the mock function type and its
// return type by 'function_type' and 'return_type'.
//
// Note that you don't need to specify the types of the mock function
// arguments.  However rest assured that your code is still type-safe:
// you'll get a compiler error if *arg1 doesn't support the ++
// operator, or if the type of ++(*arg1) isn't compatible with the
// mock function's return type, for example.
//
// Sometimes you'll want to parameterize the action.   For that you can use
// another macro:
//
//   ACTION_P(name, param_name) { statements; }
//
// For example:
//
//   ACTION_P(Add, n) { return arg0 + n; }
//
// will allow you to write:
//
//   ...WillOnce(Add(5));
//
// Note that you don't need to provide the type of the parameter
// either.  If you need to reference the type of a parameter named
// 'foo', you can write 'foo_type'.  For example, in the body of
// ACTION_P(Add, n) above, you can write 'n_type' to refer to the type
// of 'n'.
//
// We also provide ACTION_P2, ACTION_P3, ..., up to ACTION_P10 to support
// multi-parameter actions.
//
// For the purpose of typing, you can view
//
//   ACTION_Pk(Foo, p1, ..., pk) { ... }
//
// as shorthand for
//
//   template <typename p1_type, ..., typename pk_type>
//   FooActionPk<p1_type, ..., pk_type> Foo(p1_type p1, ..., pk_type pk) { ... }
//
// In particular, you can provide the template type arguments
// explicitly when invoking Foo(), as in Foo<long, bool>(5, false);
// although usually you can rely on the compiler to infer the types
// for you automatically.  You can assign the result of expression
// Foo(p1, ..., pk) to a variable of type FooActionPk<p1_type, ...,
// pk_type>.  This can be useful when composing actions.
//
// You can also overload actions with different numbers of parameters:
//
//   ACTION_P(Plus, a) { ... }
//   ACTION_P2(Plus, a, b) { ... }
//
// While it's tempting to always use the ACTION* macros when defining
// a new action, you should also consider implementing ActionInterface
// or using MakePolymorphicAction() instead, especially if you need to
// use the action a lot.  While these approaches require more work,
// they give you more control on the types of the mock function
// arguments and the action parameters, which in general leads to
// better compiler error messages that pay off in the long run.  They
// also allow overloading actions based on parameter types (as opposed
// to just based on the number of parameters).
//
// CAVEAT:
//
// ACTION*() can only be used in a namespace scope as templates cannot be
// declared inside of a local class.
// Users can, however, define any local functors (e.g. a lambda) that
// can be used as actions.
//
// MORE INFORMATION:
//
// To learn more about using these macros, please search for 'ACTION' on
// https://github.com/google/googletest/blob/main/docs/gmock_cook_book.md

// IWYU pragma: private, include "gmock/gmock.h"
// IWYU pragma: friend gmock/.*

#ifndef GOOGLEMOCK_INCLUDE_GMOCK_GMOCK_ACTIONS_H_
#define GOOGLEMOCK_INCLUDE_GMOCK_GMOCK_ACTIONS_H_

#ifndef _WIN32_WCE
#include <errno.h>
#endif

#include <algorithm>
#include <exception>
#include <functional>
#include <memory>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

#include "gmock/internal/gmock-internal-utils.h"
#include "gmock/internal/gmock-port.h"
#include "gmock/internal/gmock-pp.h"

GTEST_DISABLE_MSC_WARNINGS_PUSH_(4100)

namespace testing {

namespace internal {

template <typename T, bool kDefaultConstructible>
struct BuiltInDefaultValueGetter {
  static T Get() { return T(); }
};
template <typename T>
struct BuiltInDefaultValueGetter<T, false> {
  static T Get() {
    Assert(false, __FILE__, __LINE__,
           "Default action undefined for the function return type.");
#if defined(__GNUC__) || defined(__clang__)
    __builtin_unreachable();
#elif defined(_MSC_VER)
    __assume(0);
#else
    return Invalid<T>();

#endif
  }
};

template <typename T>
class BuiltInDefaultValue {
 public:

  static bool Exists() { return ::std::is_default_constructible<T>::value; }

  static T Get() {
    return BuiltInDefaultValueGetter<
        T, ::std::is_default_constructible<T>::value>::Get();
  }
};

template <typename T>
class BuiltInDefaultValue<const T> {
 public:
  static bool Exists() { return BuiltInDefaultValue<T>::Exists(); }
  static T Get() { return BuiltInDefaultValue<T>::Get(); }
};

template <typename T>
class BuiltInDefaultValue<T*> {
 public:
  static bool Exists() { return true; }
  static T* Get() { return nullptr; }
};

#define GMOCK_DEFINE_DEFAULT_ACTION_FOR_RETURN_TYPE_(type, value) \
  template <>                                                     \
  class BuiltInDefaultValue<type> {                               \
   public:                                                        \
    static bool Exists() { return true; }                         \
    static type Get() { return value; }                           \
  }

GMOCK_DEFINE_DEFAULT_ACTION_FOR_RETURN_TYPE_(void, );
GMOCK_DEFINE_DEFAULT_ACTION_FOR_RETURN_TYPE_(::std::string, "");
GMOCK_DEFINE_DEFAULT_ACTION_FOR_RETURN_TYPE_(bool, false);
GMOCK_DEFINE_DEFAULT_ACTION_FOR_RETURN_TYPE_(unsigned char, '\0');
GMOCK_DEFINE_DEFAULT_ACTION_FOR_RETURN_TYPE_(signed char, '\0');
GMOCK_DEFINE_DEFAULT_ACTION_FOR_RETURN_TYPE_(char, '\0');

#if GMOCK_WCHAR_T_IS_NATIVE_
GMOCK_DEFINE_DEFAULT_ACTION_FOR_RETURN_TYPE_(wchar_t, 0U);
#endif

GMOCK_DEFINE_DEFAULT_ACTION_FOR_RETURN_TYPE_(unsigned short, 0U);
GMOCK_DEFINE_DEFAULT_ACTION_FOR_RETURN_TYPE_(signed short, 0);
GMOCK_DEFINE_DEFAULT_ACTION_FOR_RETURN_TYPE_(unsigned int, 0U);
GMOCK_DEFINE_DEFAULT_ACTION_FOR_RETURN_TYPE_(signed int, 0);
GMOCK_DEFINE_DEFAULT_ACTION_FOR_RETURN_TYPE_(unsigned long, 0UL);
GMOCK_DEFINE_DEFAULT_ACTION_FOR_RETURN_TYPE_(signed long, 0L);
GMOCK_DEFINE_DEFAULT_ACTION_FOR_RETURN_TYPE_(unsigned long long, 0);
GMOCK_DEFINE_DEFAULT_ACTION_FOR_RETURN_TYPE_(signed long long, 0);
GMOCK_DEFINE_DEFAULT_ACTION_FOR_RETURN_TYPE_(float, 0);
GMOCK_DEFINE_DEFAULT_ACTION_FOR_RETURN_TYPE_(double, 0);

#undef GMOCK_DEFINE_DEFAULT_ACTION_FOR_RETURN_TYPE_

template <typename P>
struct negation

    : std::integral_constant<bool, bool(!P::value)> {};

template <typename...>
struct conjunction : std::true_type {};

template <typename P1>
struct conjunction<P1> : P1 {};

template <typename P1, typename... Ps>
struct conjunction<P1, Ps...>
    : std::conditional<bool(P1::value), conjunction<Ps...>, P1>::type {};

template <typename...>
struct disjunction : std::false_type {};

template <typename P1>
struct disjunction<P1> : P1 {};

template <typename P1, typename... Ps>
struct disjunction<P1, Ps...>

    : std::conditional<!bool(P1::value), disjunction<Ps...>, P1>::type {};

template <typename...>
using void_t = void;

template <typename From, typename To>
struct is_implicitly_convertible {
 private:

  template <typename T>
  static void Accept(T);

  template <typename T>
  static T Make();

  template <typename T, typename = decltype(Accept<To>(Make<T>()))>
  static std::true_type TestImplicitConversion(int);

  template <typename T>
  static std::false_type TestImplicitConversion(...);

 public:
  using type = decltype(TestImplicitConversion<From>(0));
  static constexpr bool value = type::value;
};

template <typename F, typename... Args>
using call_result_t = decltype(std::declval<F>()(std::declval<Args>()...));

template <typename Void, typename R, typename F, typename... Args>
struct is_callable_r_impl : std::false_type {};

template <typename R, typename F, typename... Args>
struct is_callable_r_impl<void_t<call_result_t<F, Args...>>, R, F, Args...>
    : std::conditional<
          std::is_void<R>::value,
          std::true_type,
          is_implicitly_convertible<call_result_t<F, Args...>, R>>::type {};

template <typename R, typename F, typename... Args>
using is_callable_r = is_callable_r_impl<void, R, F, Args...>;

template <typename T>
typename std::add_const<T>::type& as_const(T& t) {
  return t;
}

}

template <typename F>
class OnceAction;

template <typename Result, typename... Args>
class OnceAction<Result(Args...)> final {
 private:

  template <typename Callable>
  using IsDirectlyCompatible = internal::conjunction<

      std::is_constructible<typename std::decay<Callable>::type, Callable>,

      internal::is_callable_r<Result, typename std::decay<Callable>::type,
                              Args...>>;

  template <typename Callable>
  using IsCompatibleAfterIgnoringArguments = internal::conjunction<

      std::is_constructible<typename std::decay<Callable>::type, Callable>,

      internal::is_callable_r<Result, typename std::decay<Callable>::type>>;

 public:

  template <typename Callable,
            typename std::enable_if<
                internal::conjunction<

                    internal::negation<std::is_same<
                        OnceAction, typename std::decay<Callable>::type>>,
                    IsDirectlyCompatible<Callable>>
                ::value,
                int>::type = 0>
  OnceAction(Callable&& callable)
      : function_(StdFunctionAdaptor<typename std::decay<Callable>::type>(
            {}, std::forward<Callable>(callable))) {}

  template <typename Callable,
            typename std::enable_if<
                internal::conjunction<

                    internal::negation<std::is_same<
                        OnceAction, typename std::decay<Callable>::type>>,

                    internal::negation<IsDirectlyCompatible<Callable>>,
                    IsCompatibleAfterIgnoringArguments<Callable>>::value,
                int>::type = 0>
  OnceAction(Callable&& callable)

      : OnceAction(IgnoreIncomingArguments<typename std::decay<Callable>::type>{
            std::forward<Callable>(callable)}) {}

  OnceAction(const OnceAction&) = delete;
  OnceAction& operator=(const OnceAction&) = delete;
  OnceAction(OnceAction&&) = default;

  Result Call(Args... args) && {
    return function_(std::forward<Args>(args)...);
  }

 private:

  template <typename Callable>
  class StdFunctionAdaptor final {
   public:

    struct CallableTag final {};

    template <typename F>
    explicit StdFunctionAdaptor(CallableTag, F&& callable)
        : callable_(std::make_shared<Callable>(std::forward<F>(callable))) {}

    template <typename... ArgRefs>
    internal::call_result_t<Callable, ArgRefs...> operator()(
        ArgRefs&&... args) const {
      return std::move(*callable_)(std::forward<ArgRefs>(args)...);
    }

   private:

    std::shared_ptr<Callable> callable_;
  };

  template <typename Callable>
  struct IgnoreIncomingArguments {
    internal::call_result_t<Callable> operator()(Args&&...) {
      return std::move(callable)();
    }

    Callable callable;
  };

  std::function<Result(Args...)> function_;
};

template <typename T>
class DefaultValue {
 public:

  static void Set(T x) {
    delete producer_;
    producer_ = new FixedValueProducer(x);
  }

  typedef T (*FactoryFunction)();
  static void SetFactory(FactoryFunction factory) {
    delete producer_;
    producer_ = new FactoryValueProducer(factory);
  }

  static void Clear() {
    delete producer_;
    producer_ = nullptr;
  }

  static bool IsSet() { return producer_ != nullptr; }

  static bool Exists() {
    return IsSet() || internal::BuiltInDefaultValue<T>::Exists();
  }

  static T Get() {
    return producer_ == nullptr ? internal::BuiltInDefaultValue<T>::Get()
                                : producer_->Produce();
  }

 private:
  class ValueProducer {
   public:
    virtual ~ValueProducer() = default;
    virtual T Produce() = 0;
  };

  class FixedValueProducer : public ValueProducer {
   public:
    explicit FixedValueProducer(T value) : value_(value) {}
    T Produce() override { return value_; }

   private:
    const T value_;
    FixedValueProducer(const FixedValueProducer&) = delete;
    FixedValueProducer& operator=(const FixedValueProducer&) = delete;
  };

  class FactoryValueProducer : public ValueProducer {
   public:
    explicit FactoryValueProducer(FactoryFunction factory)
        : factory_(factory) {}
    T Produce() override { return factory_(); }

   private:
    const FactoryFunction factory_;
    FactoryValueProducer(const FactoryValueProducer&) = delete;
    FactoryValueProducer& operator=(const FactoryValueProducer&) = delete;
  };

  static ValueProducer* producer_;
};

template <typename T>
class DefaultValue<T&> {
 public:

  static void Set(T& x) {
    address_ = &x;
  }

  static void Clear() { address_ = nullptr; }

  static bool IsSet() { return address_ != nullptr; }

  static bool Exists() {
    return IsSet() || internal::BuiltInDefaultValue<T&>::Exists();
  }

  static T& Get() {
    return address_ == nullptr ? internal::BuiltInDefaultValue<T&>::Get()
                               : *address_;
  }

 private:
  static T* address_;
};

template <>
class DefaultValue<void> {
 public:
  static bool Exists() { return true; }
  static void Get() {}
};

template <typename T>
typename DefaultValue<T>::ValueProducer* DefaultValue<T>::producer_ = nullptr;

template <typename T>
T* DefaultValue<T&>::address_ = nullptr;

template <typename F>
class ActionInterface {
 public:
  typedef typename internal::Function<F>::Result Result;
  typedef typename internal::Function<F>::ArgumentTuple ArgumentTuple;

  ActionInterface() = default;
  virtual ~ActionInterface() = default;

  virtual Result Perform(const ArgumentTuple& args) = 0;

 private:
  ActionInterface(const ActionInterface&) = delete;
  ActionInterface& operator=(const ActionInterface&) = delete;
};

template <typename F>
class Action;

template <typename R, typename... Args>
class Action<R(Args...)> {
 private:
  using F = R(Args...);

  struct ActionAdapter {

    ::std::shared_ptr<ActionInterface<F>> impl_;

    template <typename... InArgs>
    typename internal::Function<F>::Result operator()(InArgs&&... args) {
      return impl_->Perform(
          ::std::forward_as_tuple(::std::forward<InArgs>(args)...));
    }
  };

  template <typename G>
  using IsCompatibleFunctor = std::is_constructible<std::function<F>, G>;

 public:
  typedef typename internal::Function<F>::Result Result;
  typedef typename internal::Function<F>::ArgumentTuple ArgumentTuple;

  Action() = default;

  template <
      typename G,
      typename = typename std::enable_if<internal::disjunction<
          IsCompatibleFunctor<G>, std::is_constructible<std::function<Result()>,
                                                        G>>::value>::type>
  Action(G&& fun) {
    Init(::std::forward<G>(fun), IsCompatibleFunctor<G>());
  }

  explicit Action(ActionInterface<F>* impl)
      : fun_(ActionAdapter{::std::shared_ptr<ActionInterface<F>>(impl)}) {}

  template <typename Func>
  Action(const Action<Func>& action)
      : fun_(action.fun_) {}

  bool IsDoDefault() const { return fun_ == nullptr; }

  Result Perform(ArgumentTuple args) const {
    if (IsDoDefault()) {
      internal::IllegalDoDefault(__FILE__, __LINE__);
    }
    return internal::Apply(fun_, ::std::move(args));
  }

  operator OnceAction<F>() const {

    struct OA {
      Action<F> action;

      R operator()(Args... args) && {
        return action.Perform(
            std::forward_as_tuple(std::forward<Args>(args)...));
      }
    };

    return OA{*this};
  }

 private:
  template <typename G>
  friend class Action;

  template <typename G>
  void Init(G&& g, ::std::true_type) {
    fun_ = ::std::forward<G>(g);
  }

  template <typename G>
  void Init(G&& g, ::std::false_type) {
    fun_ = IgnoreArgs<typename ::std::decay<G>::type>{::std::forward<G>(g)};
  }

  template <typename FunctionImpl>
  struct IgnoreArgs {
    template <typename... InArgs>
    Result operator()(const InArgs&...) const {
      return function_impl();
    }

    FunctionImpl function_impl;
  };

  ::std::function<F> fun_;
};

template <typename Impl>
class PolymorphicAction {
 public:
  explicit PolymorphicAction(const Impl& impl) : impl_(impl) {}

  template <typename F>
  operator Action<F>() const {
    return Action<F>(new MonomorphicImpl<F>(impl_));
  }

 private:
  template <typename F>
  class MonomorphicImpl : public ActionInterface<F> {
   public:
    typedef typename internal::Function<F>::Result Result;
    typedef typename internal::Function<F>::ArgumentTuple ArgumentTuple;

    explicit MonomorphicImpl(const Impl& impl) : impl_(impl) {}

    Result Perform(const ArgumentTuple& args) override {
      return impl_.template Perform<Result>(args);
    }

   private:
    Impl impl_;
  };

  Impl impl_;
};

template <typename F>
Action<F> MakeAction(ActionInterface<F>* impl) {
  return Action<F>(impl);
}

template <typename Impl>
inline PolymorphicAction<Impl> MakePolymorphicAction(const Impl& impl) {
  return PolymorphicAction<Impl>(impl);
}

namespace internal {

template <typename T>
struct ByMoveWrapper {
  explicit ByMoveWrapper(T value) : payload(std::move(value)) {}
  T payload;
};

template <typename R>
class ReturnAction final {
 public:
  explicit ReturnAction(R value) : value_(std::move(value)) {}

  template <typename U, typename... Args,
            typename = typename std::enable_if<conjunction<

                negation<std::is_same<void, U>>,
                negation<std::is_reference<U>>,
                std::is_convertible<R, U>,
                std::is_move_constructible<U>>::value>::type>
  operator OnceAction<U(Args...)>() && {
    return Impl<U>(std::move(value_));
  }

  template <typename U, typename... Args,
            typename = typename std::enable_if<conjunction<

                negation<std::is_same<void, U>>,
                negation<std::is_reference<U>>,
                std::is_convertible<const R&, U>,
                std::is_copy_constructible<U>>::value>::type>
  operator Action<U(Args...)>() const {
    return Impl<U>(value_);
  }

 private:

  template <typename U>
  class Impl final {
   public:

    explicit Impl(R&& input_value)
        : state_(new State(std::move(input_value))) {}

    explicit Impl(const R& input_value) : state_(new State(input_value)) {}

    U operator()() && { return std::move(state_->value); }
    U operator()() const& { return state_->value; }

   private:

    struct State {
      explicit State(const R& input_value_in)
          : input_value(input_value_in),

            value(ImplicitCast_<U>(internal::as_const(input_value))) {}

      explicit State(R&& input_value_in)
          : input_value(std::move(input_value_in)),

            value(ImplicitCast_<U>(std::move(input_value))) {}

      R input_value;

      U value;
    };

    const std::shared_ptr<State> state_;
  };

  R value_;
};

template <typename T>
class ReturnAction<ByMoveWrapper<T>> final {
 public:
  explicit ReturnAction(ByMoveWrapper<T> wrapper)
      : state_(new State(std::move(wrapper.payload))) {}

  T operator()() const {
    GTEST_CHECK_(!state_->called)
        << "A ByMove() action must be performed at most once.";

    state_->called = true;
    return std::move(state_->value);
  }

 private:

  struct State {
    explicit State(T&& value_in) : value(std::move(value_in)) {}

    T value;
    bool called = false;
  };

  const std::shared_ptr<State> state_;
};

class ReturnNullAction {
 public:

  template <typename Result, typename ArgumentTuple>
  static Result Perform(const ArgumentTuple&) {
    return nullptr;
  }
};

class ReturnVoidAction {
 public:

  template <typename Result, typename ArgumentTuple>
  static void Perform(const ArgumentTuple&) {
    static_assert(std::is_void<Result>::value, "Result should be void.");
  }
};

template <typename T>
class ReturnRefAction {
 public:

  explicit ReturnRefAction(T& ref) : ref_(ref) {}

  template <typename F>
  operator Action<F>() const {
    typedef typename Function<F>::Result Result;

    static_assert(std::is_reference<Result>::value,
                  "use Return instead of ReturnRef to return a value");
    return Action<F>(new Impl<F>(ref_));
  }

 private:

  template <typename F>
  class Impl : public ActionInterface<F> {
   public:
    typedef typename Function<F>::Result Result;
    typedef typename Function<F>::ArgumentTuple ArgumentTuple;

    explicit Impl(T& ref) : ref_(ref) {}

    Result Perform(const ArgumentTuple&) override { return ref_; }

   private:
    T& ref_;
  };

  T& ref_;
};

template <typename T>
class ReturnRefOfCopyAction {
 public:

  explicit ReturnRefOfCopyAction(const T& value) : value_(value) {}

  template <typename F>
  operator Action<F>() const {
    typedef typename Function<F>::Result Result;

    static_assert(std::is_reference<Result>::value,
                  "use Return instead of ReturnRefOfCopy to return a value");
    return Action<F>(new Impl<F>(value_));
  }

 private:

  template <typename F>
  class Impl : public ActionInterface<F> {
   public:
    typedef typename Function<F>::Result Result;
    typedef typename Function<F>::ArgumentTuple ArgumentTuple;

    explicit Impl(const T& value) : value_(value) {}

    Result Perform(const ArgumentTuple&) override { return value_; }

   private:
    T value_;
  };

  const T value_;
};

template <typename T>
class ReturnRoundRobinAction {
 public:
  explicit ReturnRoundRobinAction(std::vector<T> values) {
    GTEST_CHECK_(!values.empty())
        << "ReturnRoundRobin requires at least one element.";
    state_->values = std::move(values);
  }

  template <typename... Args>
  T operator()(Args&&...) const {
    return state_->Next();
  }

 private:
  struct State {
    T Next() {
      T ret_val = values[i++];
      if (i == values.size()) i = 0;
      return ret_val;
    }

    std::vector<T> values;
    size_t i = 0;
  };
  std::shared_ptr<State> state_ = std::make_shared<State>();
};

class DoDefaultAction {
 public:

  template <typename F>
  operator Action<F>() const {
    return Action<F>();
  }
};

template <typename T1, typename T2>
class AssignAction {
 public:
  AssignAction(T1* ptr, T2 value) : ptr_(ptr), value_(value) {}

  template <typename Result, typename ArgumentTuple>
  void Perform(const ArgumentTuple&           ) const {
    *ptr_ = value_;
  }

 private:
  T1* const ptr_;
  const T2 value_;
};

#ifndef GTEST_OS_WINDOWS_MOBILE

template <typename T>
class SetErrnoAndReturnAction {
 public:
  SetErrnoAndReturnAction(int errno_value, T result)
      : errno_(errno_value), result_(result) {}
  template <typename Result, typename ArgumentTuple>
  Result Perform(const ArgumentTuple&           ) const {
    errno = errno_;
    return result_;
  }

 private:
  const int errno_;
  const T result_;
};

#endif

template <size_t N, typename A, typename = void>
struct SetArgumentPointeeAction {
  A value;

  template <typename... Args>
  void operator()(const Args&... args) const {
    *::std::get<N>(std::tie(args...)) = value;
  }
};

template <class Class, typename MethodPtr>
struct InvokeMethodAction {
  Class* const obj_ptr;
  const MethodPtr method_ptr;

  template <typename... Args>
  auto operator()(Args&&... args) const
      -> decltype((obj_ptr->*method_ptr)(std::forward<Args>(args)...)) {
    return (obj_ptr->*method_ptr)(std::forward<Args>(args)...);
  }
};

template <typename FunctionImpl>
struct InvokeWithoutArgsAction {
  FunctionImpl function_impl;

  template <typename... Args>
  auto operator()(const Args&...) -> decltype(function_impl()) {
    return function_impl();
  }
};

template <class Class, typename MethodPtr>
struct InvokeMethodWithoutArgsAction {
  Class* const obj_ptr;
  const MethodPtr method_ptr;

  using ReturnType =
      decltype((std::declval<Class*>()->*std::declval<MethodPtr>())());

  template <typename... Args>
  ReturnType operator()(const Args&...) const {
    return (obj_ptr->*method_ptr)();
  }
};

template <typename A>
class IgnoreResultAction {
 public:
  explicit IgnoreResultAction(const A& action) : action_(action) {}

  template <typename F>
  operator Action<F>() const {

    typedef typename internal::Function<F>::Result Result;

    static_assert(std::is_void<Result>::value, "Result type should be void.");

    return Action<F>(new Impl<F>(action_));
  }

 private:
  template <typename F>
  class Impl : public ActionInterface<F> {
   public:
    typedef typename internal::Function<F>::Result Result;
    typedef typename internal::Function<F>::ArgumentTuple ArgumentTuple;

    explicit Impl(const A& action) : action_(action) {}

    void Perform(const ArgumentTuple& args) override {

      action_.Perform(args);
    }

   private:

    typedef
        typename internal::Function<F>::MakeResultIgnoredValue OriginalFunction;

    const Action<OriginalFunction> action_;
  };

  const A action_;
};

template <typename InnerAction, size_t... I>
struct WithArgsAction {
  InnerAction inner_action;

  template <typename R, typename... Args>
  using InnerSignature =
      R(typename std::tuple_element<I, std::tuple<Args...>>::type...);

  template <
      typename R, typename... Args,
      typename std::enable_if<
          std::is_convertible<InnerAction,

                              OnceAction<R(internal::TupleElement<
                                           I, std::tuple<Args...>>...)>>::value,
          int>::type = 0>
  operator OnceAction<R(Args...)>() && {
    struct OA {
      OnceAction<InnerSignature<R, Args...>> inner_action;

      R operator()(Args&&... args) && {
        return std::move(inner_action)
            .Call(std::get<I>(
                std::forward_as_tuple(std::forward<Args>(args)...))...);
      }
    };

    return OA{std::move(inner_action)};
  }

  template <
      typename R, typename... Args,
      typename std::enable_if<
          std::is_convertible<const InnerAction&,

                              Action<R(internal::TupleElement<
                                       I, std::tuple<Args...>>...)>>::value,
          int>::type = 0>
  operator Action<R(Args...)>() const {
    Action<InnerSignature<R, Args...>> converted(inner_action);

    return [converted](Args&&... args) -> R {
      return converted.Perform(std::forward_as_tuple(
          std::get<I>(std::forward_as_tuple(std::forward<Args>(args)...))...));
    };
  }
};

template <typename... Actions>
class DoAllAction;

template <typename FinalAction>
class DoAllAction<FinalAction> {
 public:
  struct UserConstructorTag {};

  template <typename T>
  explicit DoAllAction(UserConstructorTag, T&& action)
      : final_action_(std::forward<T>(action)) {}

  template <typename R, typename... Args,
            typename std::enable_if<
                std::is_convertible<FinalAction, OnceAction<R(Args...)>>::value,
                int>::type = 0>
  operator OnceAction<R(Args...)>() && {
    return std::move(final_action_);
  }

  template <
      typename R, typename... Args,
      typename std::enable_if<
          conjunction<
              negation<
                  std::is_convertible<FinalAction, OnceAction<R(Args...)>>>,
              std::is_convertible<FinalAction, Action<R(Args...)>>>::value,
          int>::type = 0>
  operator OnceAction<R(Args...)>() && {
    return Action<R(Args...)>(std::move(final_action_));
  }

  template <
      typename R, typename... Args,
      typename std::enable_if<
          std::is_convertible<const FinalAction&, Action<R(Args...)>>::value,
          int>::type = 0>
  operator Action<R(Args...)>() const {
    return final_action_;
  }

 private:
  FinalAction final_action_;
};

template <typename InitialAction, typename... OtherActions>
class DoAllAction<InitialAction, OtherActions...>
    : private DoAllAction<OtherActions...> {
 private:
  using Base = DoAllAction<OtherActions...>;

  template <typename T>
  using InitialActionArgType =
      typename std::conditional<std::is_scalar<T>::value, T, const T&>::type;

 public:
  struct UserConstructorTag {};

  template <typename T, typename... U>
  explicit DoAllAction(UserConstructorTag, T&& initial_action,
                       U&&... other_actions)
      : Base({}, std::forward<U>(other_actions)...),
        initial_action_(std::forward<T>(initial_action)) {}

  template <
      typename R, typename... Args,
      typename std::enable_if<
          conjunction<std::is_convertible<
                          InitialAction,
                          OnceAction<void(InitialActionArgType<Args>...)>>,
                      std::is_convertible<Base, OnceAction<R(Args...)>>>::value,
          int>::type = 0>
  operator OnceAction<R(Args...)>() && {

    struct OA {
      OnceAction<void(InitialActionArgType<Args>...)> initial_action;
      OnceAction<R(Args...)> remaining_actions;

      R operator()(Args... args) && {
        std::move(initial_action)
            .Call(static_cast<InitialActionArgType<Args>>(args)...);

        return std::move(remaining_actions).Call(std::forward<Args>(args)...);
      }
    };

    return OA{
        std::move(initial_action_),
        std::move(static_cast<Base&>(*this)),
    };
  }

  template <
      typename R, typename... Args,
      typename std::enable_if<
          conjunction<
              negation<std::is_convertible<
                  InitialAction,
                  OnceAction<void(InitialActionArgType<Args>...)>>>,
              std::is_convertible<InitialAction,
                                  Action<void(InitialActionArgType<Args>...)>>,
              std::is_convertible<Base, OnceAction<R(Args...)>>>::value,
          int>::type = 0>
  operator OnceAction<R(Args...)>() && {
    return DoAll(
        Action<void(InitialActionArgType<Args>...)>(std::move(initial_action_)),
        std::move(static_cast<Base&>(*this)));
  }

  template <
      typename R, typename... Args,
      typename std::enable_if<
          conjunction<
              std::is_convertible<const InitialAction&,
                                  Action<void(InitialActionArgType<Args>...)>>,
              std::is_convertible<const Base&, Action<R(Args...)>>>::value,
          int>::type = 0>
  operator Action<R(Args...)>() const {

    struct OA {
      Action<void(InitialActionArgType<Args>...)> initial_action;
      Action<R(Args...)> remaining_actions;

      R operator()(Args... args) const {
        initial_action.Perform(std::forward_as_tuple(
            static_cast<InitialActionArgType<Args>>(args)...));

        return remaining_actions.Perform(
            std::forward_as_tuple(std::forward<Args>(args)...));
      }
    };

    return OA{
        initial_action_,
        static_cast<const Base&>(*this),
    };
  }

 private:
  InitialAction initial_action_;
};

template <typename T, typename... Params>
struct ReturnNewAction {
  T* operator()() const {
    return internal::Apply(
        [](const Params&... unpacked_params) {
          return new T(unpacked_params...);
        },
        params);
  }
  std::tuple<Params...> params;
};

template <size_t k>
struct ReturnArgAction {
  template <typename... Args,
            typename = typename std::enable_if<(k < sizeof...(Args))>::type>
  auto operator()(Args&&... args) const
      -> decltype(std::get<k>(
          std::forward_as_tuple(std::forward<Args>(args)...))) {
    return std::get<k>(std::forward_as_tuple(std::forward<Args>(args)...));
  }
};

template <size_t k, typename Ptr>
struct SaveArgAction {
  Ptr pointer;

  template <typename... Args>
  void operator()(const Args&... args) const {
    *pointer = std::get<k>(std::tie(args...));
  }
};

template <size_t k, typename Ptr>
struct SaveArgByMoveAction {
  Ptr pointer;

  template <typename... Args>
  void operator()(Args&&... args) const {
    *pointer = std::move(std::get<k>(std::tie(args...)));
  }
};

template <size_t k, typename Ptr>
struct SaveArgPointeeAction {
  Ptr pointer;

  template <typename... Args>
  void operator()(const Args&... args) const {
    *pointer = *std::get<k>(std::tie(args...));
  }
};

template <size_t k, typename T>
struct SetArgRefereeAction {
  T value;

  template <typename... Args>
  void operator()(Args&&... args) const {
    using argk_type =
        typename ::std::tuple_element<k, std::tuple<Args...>>::type;
    static_assert(std::is_lvalue_reference<argk_type>::value,
                  "Argument must be a reference type.");
    std::get<k>(std::tie(args...)) = value;
  }
};

template <size_t k, typename I1, typename I2>
struct SetArrayArgumentAction {
  I1 first;
  I2 last;

  template <typename... Args>
  void operator()(const Args&... args) const {
    auto value = std::get<k>(std::tie(args...));
    for (auto it = first; it != last; ++it, (void)++value) {
      *value = *it;
    }
  }
};

template <size_t k>
struct DeleteArgAction {
  template <typename... Args>
  void operator()(const Args&... args) const {
    delete std::get<k>(std::tie(args...));
  }
};

template <typename Ptr>
struct ReturnPointeeAction {
  Ptr pointer;
  template <typename... Args>
  auto operator()(const Args&...) const -> decltype(*pointer) {
    return *pointer;
  }
};

#if GTEST_HAS_EXCEPTIONS
template <typename T>
struct ThrowAction {
  T exception;

  template <typename R, typename... Args>
  operator Action<R(Args...)>() const {
    T copy = exception;
    return [copy](Args...) -> R { throw copy; };
  }
};
struct RethrowAction {
  std::exception_ptr exception;
  template <typename R, typename... Args>
  operator Action<R(Args...)>() const {
    return [ex = exception](Args...) -> R { std::rethrow_exception(ex); };
  }
};
#endif

}

typedef internal::IgnoredValue Unused;

template <typename... Action>
internal::DoAllAction<typename std::decay<Action>::type...> DoAll(
    Action&&... action) {
  return internal::DoAllAction<typename std::decay<Action>::type...>(
      {}, std::forward<Action>(action)...);
}

template <size_t k, typename InnerAction>
internal::WithArgsAction<typename std::decay<InnerAction>::type, k> WithArg(
    InnerAction&& action) {
  return {std::forward<InnerAction>(action)};
}

template <size_t k, size_t... ks, typename InnerAction>
internal::WithArgsAction<typename std::decay<InnerAction>::type, k, ks...>
WithArgs(InnerAction&& action) {
  return {std::forward<InnerAction>(action)};
}

template <typename InnerAction>
internal::WithArgsAction<typename std::decay<InnerAction>::type> WithoutArgs(
    InnerAction&& action) {
  return {std::forward<InnerAction>(action)};
}

template <typename R>
internal::ReturnAction<R> Return(R value) {
  return internal::ReturnAction<R>(std::move(value));
}

inline PolymorphicAction<internal::ReturnNullAction> ReturnNull() {
  return MakePolymorphicAction(internal::ReturnNullAction());
}

inline PolymorphicAction<internal::ReturnVoidAction> Return() {
  return MakePolymorphicAction(internal::ReturnVoidAction());
}

template <typename R>
inline internal::ReturnRefAction<R> ReturnRef(R& x) {
  return internal::ReturnRefAction<R>(x);
}

template <typename R, R* = nullptr>
internal::ReturnRefAction<R> ReturnRef(R&&) = delete;

template <typename R>
inline internal::ReturnRefOfCopyAction<R> ReturnRefOfCopy(const R& x) {
  return internal::ReturnRefOfCopyAction<R>(x);
}

template <typename R>
internal::ByMoveWrapper<R> ByMove(R x) {
  return internal::ByMoveWrapper<R>(std::move(x));
}

template <typename T>
internal::ReturnRoundRobinAction<T> ReturnRoundRobin(std::vector<T> vals) {
  return internal::ReturnRoundRobinAction<T>(std::move(vals));
}

template <typename T>
internal::ReturnRoundRobinAction<T> ReturnRoundRobin(
    std::initializer_list<T> vals) {
  return internal::ReturnRoundRobinAction<T>(std::vector<T>(vals));
}

inline internal::DoDefaultAction DoDefault() {
  return internal::DoDefaultAction();
}

template <size_t N, typename T>
internal::SetArgumentPointeeAction<N, T> SetArgPointee(T value) {
  return {std::move(value)};
}

template <size_t N, typename T>
internal::SetArgumentPointeeAction<N, T> SetArgumentPointee(T value) {
  return {std::move(value)};
}

template <typename T1, typename T2>
PolymorphicAction<internal::AssignAction<T1, T2>> Assign(T1* ptr, T2 val) {
  return MakePolymorphicAction(internal::AssignAction<T1, T2>(ptr, val));
}

#ifndef GTEST_OS_WINDOWS_MOBILE

template <typename T>
PolymorphicAction<internal::SetErrnoAndReturnAction<T>> SetErrnoAndReturn(
    int errval, T result) {
  return MakePolymorphicAction(
      internal::SetErrnoAndReturnAction<T>(errval, result));
}

#endif

template <typename FunctionImpl>
typename std::decay<FunctionImpl>::type Invoke(FunctionImpl&& function_impl) {
  return std::forward<FunctionImpl>(function_impl);
}

template <class Class, typename MethodPtr>
internal::InvokeMethodAction<Class, MethodPtr> Invoke(Class* obj_ptr,
                                                      MethodPtr method_ptr) {
  return {obj_ptr, method_ptr};
}

template <typename FunctionImpl>
internal::InvokeWithoutArgsAction<typename std::decay<FunctionImpl>::type>
InvokeWithoutArgs(FunctionImpl function_impl) {
  return {std::move(function_impl)};
}

template <class Class, typename MethodPtr>
internal::InvokeMethodWithoutArgsAction<Class, MethodPtr> InvokeWithoutArgs(
    Class* obj_ptr, MethodPtr method_ptr) {
  return {obj_ptr, method_ptr};
}

template <typename A>
inline internal::IgnoreResultAction<A> IgnoreResult(const A& an_action) {
  return internal::IgnoreResultAction<A>(an_action);
}

template <typename T>
inline ::std::reference_wrapper<T> ByRef(T& l_value) {
  return ::std::reference_wrapper<T>(l_value);
}

template <typename T, typename... Params>
internal::ReturnNewAction<T, typename std::decay<Params>::type...> ReturnNew(
    Params&&... params) {
  return {std::forward_as_tuple(std::forward<Params>(params)...)};
}

template <size_t k>
internal::ReturnArgAction<k> ReturnArg() {
  return {};
}

template <size_t k, typename Ptr>
internal::SaveArgAction<k, Ptr> SaveArg(Ptr pointer) {
  return {pointer};
}

template <size_t k, typename Ptr>
internal::SaveArgByMoveAction<k, Ptr> SaveArgByMove(Ptr pointer) {
  return {pointer};
}

template <size_t k, typename Ptr>
internal::SaveArgPointeeAction<k, Ptr> SaveArgPointee(Ptr pointer) {
  return {pointer};
}

template <size_t k, typename T>
internal::SetArgRefereeAction<k, typename std::decay<T>::type> SetArgReferee(
    T&& value) {
  return {std::forward<T>(value)};
}

template <size_t k, typename I1, typename I2>
internal::SetArrayArgumentAction<k, I1, I2> SetArrayArgument(I1 first,
                                                             I2 last) {
  return {first, last};
}

template <size_t k>
internal::DeleteArgAction<k> DeleteArg() {
  return {};
}

template <typename Ptr>
internal::ReturnPointeeAction<Ptr> ReturnPointee(Ptr pointer) {
  return {pointer};
}

#if GTEST_HAS_EXCEPTIONS

template <typename T>
typename std::enable_if<
    !std::is_base_of<std::exception_ptr, typename std::decay<T>::type>::value,
    internal::ThrowAction<typename std::decay<T>::type>>::type
Throw(T&& exception) {
  return {std::forward<T>(exception)};
}

inline internal::RethrowAction Rethrow(std::exception_ptr exception) {
  return {std::move(exception)};
}
#endif

namespace internal {

struct ExcessiveArg {};

template <typename F, typename Impl>
struct ActionImpl;

template <typename Impl>
struct ImplBase {
  struct Holder {

    explicit operator const Impl&() const { return *ptr; }
    std::shared_ptr<Impl> ptr;
  };
  using type = typename std::conditional<std::is_constructible<Impl>::value,
                                         Impl, Holder>::type;
};

template <typename R, typename... Args, typename Impl>
struct ActionImpl<R(Args...), Impl> : ImplBase<Impl>::type {
  using Base = typename ImplBase<Impl>::type;
  using function_type = R(Args...);
  using args_type = std::tuple<Args...>;

  ActionImpl() = default;
  explicit ActionImpl(std::shared_ptr<Impl> impl) : Base{std::move(impl)} {}

  R operator()(Args&&... arg) const {
    static constexpr size_t kMaxArgs =
        sizeof...(Args) <= 10 ? sizeof...(Args) : 10;
    return Apply(std::make_index_sequence<kMaxArgs>{},
                 std::make_index_sequence<10 - kMaxArgs>{},
                 args_type{std::forward<Args>(arg)...});
  }

  template <std::size_t... arg_id, std::size_t... excess_id>
  R Apply(std::index_sequence<arg_id...>, std::index_sequence<excess_id...>,
          const args_type& args) const {

    static constexpr ExcessiveArg kExcessArg{};
    return static_cast<const Impl&>(*this)
        .template gmock_PerformImpl<
                              function_type,                 R,
                          args_type,

            typename std::tuple_element<arg_id, args_type>::type...>(
                     args, std::get<arg_id>(args)...,
            ((void)excess_id, kExcessArg)...);
  }
};

template <typename F, typename Impl>
::testing::Action<F> MakeAction() {
  return ::testing::Action<F>(ActionImpl<F, Impl>());
}

template <typename F, typename Impl>
::testing::Action<F> MakeAction(std::shared_ptr<Impl> impl) {
  return ::testing::Action<F>(ActionImpl<F, Impl>(std::move(impl)));
}

#define GMOCK_INTERNAL_ARG_UNUSED(i, data, el) \
  , [[maybe_unused]] const arg##i##_type& arg##i
#define GMOCK_ACTION_ARG_TYPES_AND_NAMES_UNUSED_          \
  [[maybe_unused]] const args_type& args GMOCK_PP_REPEAT( \
      GMOCK_INTERNAL_ARG_UNUSED, , 10)

#define GMOCK_INTERNAL_ARG(i, data, el) , const arg##i##_type& arg##i
#define GMOCK_ACTION_ARG_TYPES_AND_NAMES_ \
  const args_type& args GMOCK_PP_REPEAT(GMOCK_INTERNAL_ARG, , 10)

#define GMOCK_INTERNAL_TEMPLATE_ARG(i, data, el) , typename arg##i##_type
#define GMOCK_ACTION_TEMPLATE_ARGS_NAMES_ \
  GMOCK_PP_TAIL(GMOCK_PP_REPEAT(GMOCK_INTERNAL_TEMPLATE_ARG, , 10))

#define GMOCK_INTERNAL_TYPENAME_PARAM(i, data, param) , typename param##_type
#define GMOCK_ACTION_TYPENAME_PARAMS_(params) \
  GMOCK_PP_TAIL(GMOCK_PP_FOR_EACH(GMOCK_INTERNAL_TYPENAME_PARAM, , params))

#define GMOCK_INTERNAL_TYPE_PARAM(i, data, param) , param##_type
#define GMOCK_ACTION_TYPE_PARAMS_(params) \
  GMOCK_PP_TAIL(GMOCK_PP_FOR_EACH(GMOCK_INTERNAL_TYPE_PARAM, , params))

#define GMOCK_INTERNAL_TYPE_GVALUE_PARAM(i, data, param) \
  , param##_type gmock_p##i
#define GMOCK_ACTION_TYPE_GVALUE_PARAMS_(params) \
  GMOCK_PP_TAIL(GMOCK_PP_FOR_EACH(GMOCK_INTERNAL_TYPE_GVALUE_PARAM, , params))

#define GMOCK_INTERNAL_GVALUE_PARAM(i, data, param) \
  , std::forward<param##_type>(gmock_p##i)
#define GMOCK_ACTION_GVALUE_PARAMS_(params) \
  GMOCK_PP_TAIL(GMOCK_PP_FOR_EACH(GMOCK_INTERNAL_GVALUE_PARAM, , params))

#define GMOCK_INTERNAL_INIT_PARAM(i, data, param) \
  , param(::std::forward<param##_type>(gmock_p##i))
#define GMOCK_ACTION_INIT_PARAMS_(params) \
  GMOCK_PP_TAIL(GMOCK_PP_FOR_EACH(GMOCK_INTERNAL_INIT_PARAM, , params))

#define GMOCK_INTERNAL_FIELD_PARAM(i, data, param) param##_type param;
#define GMOCK_ACTION_FIELD_PARAMS_(params) \
  GMOCK_PP_FOR_EACH(GMOCK_INTERNAL_FIELD_PARAM, , params)

#define GMOCK_INTERNAL_ACTION(name, full_name, params)                         \
  template <GMOCK_ACTION_TYPENAME_PARAMS_(params)>                             \
  class full_name {                                                            \
   public:                                                                     \
    explicit full_name(GMOCK_ACTION_TYPE_GVALUE_PARAMS_(params))               \
        : impl_(std::make_shared<gmock_Impl>(                                  \
              GMOCK_ACTION_GVALUE_PARAMS_(params))) {}                         \
    full_name(const full_name&) = default;                                     \
    full_name(full_name&&) noexcept = default;                                 \
    template <typename F>                                                      \
    operator ::testing::Action<F>() const {                                    \
      return ::testing::internal::MakeAction<F>(impl_);                        \
    }                                                                          \
                                                                               \
   private:                                                                    \
    class gmock_Impl {                                                         \
     public:                                                                   \
      explicit gmock_Impl(GMOCK_ACTION_TYPE_GVALUE_PARAMS_(params))            \
          : GMOCK_ACTION_INIT_PARAMS_(params) {}                               \
      template <typename function_type, typename return_type,                  \
                typename args_type, GMOCK_ACTION_TEMPLATE_ARGS_NAMES_>         \
      return_type gmock_PerformImpl(GMOCK_ACTION_ARG_TYPES_AND_NAMES_) const;  \
      GMOCK_ACTION_FIELD_PARAMS_(params)                                       \
    };                                                                         \
    std::shared_ptr<const gmock_Impl> impl_;                                   \
  };                                                                           \
  template <GMOCK_ACTION_TYPENAME_PARAMS_(params)>                             \
  [[nodiscard]] inline full_name<GMOCK_ACTION_TYPE_PARAMS_(params)> name(      \
      GMOCK_ACTION_TYPE_GVALUE_PARAMS_(params));                               \
  template <GMOCK_ACTION_TYPENAME_PARAMS_(params)>                             \
  inline full_name<GMOCK_ACTION_TYPE_PARAMS_(params)> name(                    \
      GMOCK_ACTION_TYPE_GVALUE_PARAMS_(params)) {                              \
    return full_name<GMOCK_ACTION_TYPE_PARAMS_(params)>(                       \
        GMOCK_ACTION_GVALUE_PARAMS_(params));                                  \
  }                                                                            \
  template <GMOCK_ACTION_TYPENAME_PARAMS_(params)>                             \
  template <typename function_type, typename return_type, typename args_type,  \
            GMOCK_ACTION_TEMPLATE_ARGS_NAMES_>                                 \
  return_type                                                                  \
  full_name<GMOCK_ACTION_TYPE_PARAMS_(params)>::gmock_Impl::gmock_PerformImpl( \
      GMOCK_ACTION_ARG_TYPES_AND_NAMES_UNUSED_) const

}

#define ACTION(name)                                                          \
  class name##Action {                                                        \
   public:                                                                    \
    explicit name##Action() noexcept {}                                       \
    name##Action(const name##Action&) noexcept {}                             \
    template <typename F>                                                     \
    operator ::testing::Action<F>() const {                                   \
      return ::testing::internal::MakeAction<F, gmock_Impl>();                \
    }                                                                         \
                                                                              \
   private:                                                                   \
    class gmock_Impl {                                                        \
     public:                                                                  \
      template <typename function_type, typename return_type,                 \
                typename args_type, GMOCK_ACTION_TEMPLATE_ARGS_NAMES_>        \
      return_type gmock_PerformImpl(GMOCK_ACTION_ARG_TYPES_AND_NAMES_) const; \
    };                                                                        \
  };                                                                          \
  [[nodiscard]] inline name##Action name();                                   \
  inline name##Action name() { return name##Action(); }                       \
  template <typename function_type, typename return_type, typename args_type, \
            GMOCK_ACTION_TEMPLATE_ARGS_NAMES_>                                \
  return_type name##Action::gmock_Impl::gmock_PerformImpl(                    \
      GMOCK_ACTION_ARG_TYPES_AND_NAMES_UNUSED_) const

#define ACTION_P(name, ...) \
  GMOCK_INTERNAL_ACTION(name, name##ActionP, (__VA_ARGS__))

#define ACTION_P2(name, ...) \
  GMOCK_INTERNAL_ACTION(name, name##ActionP2, (__VA_ARGS__))

#define ACTION_P3(name, ...) \
  GMOCK_INTERNAL_ACTION(name, name##ActionP3, (__VA_ARGS__))

#define ACTION_P4(name, ...) \
  GMOCK_INTERNAL_ACTION(name, name##ActionP4, (__VA_ARGS__))

#define ACTION_P5(name, ...) \
  GMOCK_INTERNAL_ACTION(name, name##ActionP5, (__VA_ARGS__))

#define ACTION_P6(name, ...) \
  GMOCK_INTERNAL_ACTION(name, name##ActionP6, (__VA_ARGS__))

#define ACTION_P7(name, ...) \
  GMOCK_INTERNAL_ACTION(name, name##ActionP7, (__VA_ARGS__))

#define ACTION_P8(name, ...) \
  GMOCK_INTERNAL_ACTION(name, name##ActionP8, (__VA_ARGS__))

#define ACTION_P9(name, ...) \
  GMOCK_INTERNAL_ACTION(name, name##ActionP9, (__VA_ARGS__))

#define ACTION_P10(name, ...) \
  GMOCK_INTERNAL_ACTION(name, name##ActionP10, (__VA_ARGS__))

}

GTEST_DISABLE_MSC_WARNINGS_POP_()

#endif
