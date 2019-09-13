#pragma once
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/detail/handler_alloc_helpers.hpp>
#include <boost/asio/detail/handler_cont_helpers.hpp>
#include <boost/asio/detail/handler_invoke_helpers.hpp>
#include <boost/outcome/outcome.hpp>
#include <boost/system/error_code.hpp>
#include <type_traits>
#include <utility>

template <typename CompletionToken>
struct as_result_t {
  CompletionToken token_;
};

template <typename CompletionToken>
inline as_result_t<std::decay_t<CompletionToken>> as_result(CompletionToken&& completion_token) {
  return as_result_t<std::decay_t<CompletionToken>>{ std::forward<CompletionToken>(completion_token) };
}

namespace detail {

// Class to adapt as_outcome_t as a completion handler
template <typename Handler>
struct outcome_result_handler {
  void operator()(const boost::system::error_code& ec) {
    using Result = boost::outcome_v2::result<void, boost::system::error_code>;

    if (ec)
      handler_(Result{ ec });
    else
      handler_(Result{ boost::outcome_v2::success() });
  }

  void operator()(std::exception_ptr ex) {
    using Result = boost::outcome_v2::result<void, std::exception_ptr>;

    if (ex)
      handler_(Result{ ex });
    else
      handler_(Result{ boost::outcome_v2::success() });
  }

  template <typename T>
  void operator()(const boost::system::error_code& ec, T t) {
    using Result = boost::outcome_v2::result<T, boost::system::error_code>;

    if (ec)
      handler_(Result{ ec });
    else
      handler_(Result{ std::move(t) });
  }

  template <typename T>
  void operator()(std::exception_ptr ex, T t) {
    using Result = boost::outcome_v2::result<T, std::exception_ptr>;
    if (ex)
      handler_(Result{ ex });
    else
      handler_(Result{ std::move(t) });
  }

  Handler handler_;
};

template <typename Handler>
inline void* asio_handler_allocate(std::size_t size, outcome_result_handler<Handler>* this_handler) {
  return boost_asio_handler_alloc_helpers::allocate(size, this_handler->handler_);
}

template <typename Handler>
inline void asio_handler_deallocate(void* pointer, std::size_t size, outcome_result_handler<Handler>* this_handler) {
  boost_asio_handler_alloc_helpers::deallocate(pointer, size, this_handler->handler_);
}

template <typename Handler>
inline bool asio_handler_is_continuation(outcome_result_handler<Handler>* this_handler) {
  return boost_asio_handler_cont_helpers::is_continuation(this_handler->handler_);
}

template <typename Function, typename Handler>
inline void asio_handler_invoke(Function& function, outcome_result_handler<Handler>* this_handler) {
  boost_asio_handler_invoke_helpers::invoke(function, this_handler->handler_);
}

template <typename Function, typename Handler>
inline void asio_handler_invoke(const Function& function, outcome_result_handler<Handler>* this_handler) {
  boost_asio_handler_invoke_helpers::invoke(function, this_handler->handler_);
}

template <typename Signature>
struct result_signature;

template <>
struct result_signature<void(boost::system::error_code)> {
  using type = void(boost::outcome_v2::result<void, boost::system::error_code>);
};

template <>
struct result_signature<void(const boost::system::error_code&)> : result_signature<void(boost::system::error_code)> {};

template <>
struct result_signature<void(std::exception_ptr)> {
  using type = void(boost::outcome_v2::result<void, std::exception_ptr>);
};

template <typename T>
struct result_signature<void(boost::system::error_code, T)> {
  using type = void(boost::outcome_v2::result<T, boost::system::error_code>);
};

template <typename T>
struct result_signature<void(const boost::system::error_code&, T)> :
  result_signature<void(boost::system::error_code, T)> {};

template <typename T>
struct result_signature<void(std::exception_ptr, T)> {
  using type = void(boost::outcome_v2::result<T, std::exception_ptr>);
};

template <typename Signature>
using result_signature_t = typename result_signature<Signature>::type;

}  // namespace detail

namespace boost::asio {

template <typename CompletionToken, typename Signature>
class async_result<as_result_t<CompletionToken>, Signature> {
public:
  using result_signature = ::detail::result_signature_t<Signature>;

  using return_type = typename async_result<CompletionToken, result_signature>::return_type;

  template <typename Initiation, typename... Args>
  static return_type initiate(Initiation&& initiation, as_result_t<CompletionToken>&& token, Args&&... args) {
    return async_initiate<CompletionToken, result_signature>(
      [init = std::forward<Initiation>(initiation)](auto&& handler, auto&&... callArgs) mutable {
        std::move(init)(
          ::detail::outcome_result_handler<std::decay_t<decltype(handler)>>{ std::forward<decltype(handler)>(handler) },
          std::forward<decltype(callArgs)>(callArgs)...);
      },
      token.token_, std::forward<Args>(args)...);
  }
};

template <typename Handler, typename Executor>
struct associated_executor<::detail::outcome_result_handler<Handler>, Executor> {
  typedef typename associated_executor<Handler, Executor>::type type;

  static type get(const ::detail::outcome_result_handler<Handler>& h, const Executor& ex = Executor()) noexcept {
    return associated_executor<Handler, Executor>::get(h.handler_, ex);
  }
};

template <typename Handler, typename Allocator>
struct associated_allocator<::detail::outcome_result_handler<Handler>, Allocator> {
  typedef typename associated_allocator<Handler, Allocator>::type type;

  static type get(const ::detail::outcome_result_handler<Handler>& h, const Allocator& a = Allocator()) noexcept {
    return associated_allocator<Handler, Allocator>::get(h.handler_, a);
  }
};

}  // namespace boost::asio
