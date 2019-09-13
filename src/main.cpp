#include "log.hpp"
#include "result.hpp"
#include <boost/asio/async_result.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>
#include <cstdio>
#include <cstdlib>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;

template <typename Buffer, typename Request>
asio::awaitable<beast::error_code> recv(beast::tcp_stream& stream, Buffer& buffer, Request& request) noexcept {
  const auto result = co_await http::async_read(stream, buffer, request, as_result(asio::use_awaitable));
  co_return result ? beast::error_code{} : result.error();
}

template <typename Response>
asio::awaitable<beast::error_code> send(beast::tcp_stream& stream, Response response) noexcept {
  const auto result = co_await http::async_write(stream, response, as_result(asio::use_awaitable));
  co_return result ? beast::error_code{} : result.error();
}

// Return a reasonable mime type based on the extension of a file.
std::string_view mime_type(std::string_view path) {
  using beast::iequals;
  auto const ext = [&path] {
    auto const pos = path.rfind(".");
    if (pos == std::string_view::npos)
      return std::string_view{};
    return path.substr(pos);
  }();
  if (iequals(ext, ".htm"))
    return "text/html";
  if (iequals(ext, ".html"))
    return "text/html";
  if (iequals(ext, ".php"))
    return "text/html";
  if (iequals(ext, ".css"))
    return "text/css";
  if (iequals(ext, ".txt"))
    return "text/plain";
  if (iequals(ext, ".js"))
    return "application/javascript";
  if (iequals(ext, ".json"))
    return "application/json";
  if (iequals(ext, ".xml"))
    return "application/xml";
  if (iequals(ext, ".swf"))
    return "application/x-shockwave-flash";
  if (iequals(ext, ".flv"))
    return "video/x-flv";
  if (iequals(ext, ".png"))
    return "image/png";
  if (iequals(ext, ".jpe"))
    return "image/jpeg";
  if (iequals(ext, ".jpeg"))
    return "image/jpeg";
  if (iequals(ext, ".jpg"))
    return "image/jpeg";
  if (iequals(ext, ".gif"))
    return "image/gif";
  if (iequals(ext, ".bmp"))
    return "image/bmp";
  if (iequals(ext, ".ico"))
    return "image/vnd.microsoft.icon";
  if (iequals(ext, ".tiff"))
    return "image/tiff";
  if (iequals(ext, ".tif"))
    return "image/tiff";
  if (iequals(ext, ".svg"))
    return "image/svg+xml";
  if (iequals(ext, ".svgz"))
    return "image/svg+xml";
  return "application/text";
}

// Append an HTTP rel-path to a local filesystem path.
// The returned path is normalized for the platform.
std::string path_cat(std::string_view base, std::string_view path) {
  if (base.empty())
    return std::string(path);
  std::string result(base);
#ifdef BOOST_MSVC
  char constexpr path_separator = '\\';
  if (result.back() == path_separator)
    result.resize(result.size() - 1);
  result.append(path.data(), path.size());
  for (auto& c : result)
    if (c == '/')
      c = path_separator;
#else
  char constexpr path_separator = '/';
  if (result.back() == path_separator)
    result.resize(result.size() - 1);
  result.append(path.data(), path.size());
#endif
  return result;
}

template <typename Request = http::request<http::string_body>>
asio::awaitable<beast::error_code> handle(beast::tcp_stream& stream, const Request& request, std::string_view root) {
  // Returns a bad request response.
  const auto bad_request = [&request](beast::string_view why) {
    http::response<http::string_body> response{
      http::status::bad_request,
      request.version(),
    };
    response.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    response.set(http::field::content_type, "text/html");
    response.keep_alive(request.keep_alive());
    response.body() = std::string(why);
    response.prepare_payload();
    return response;
  };

  // Returns a not found response.
  const auto not_found = [&request](beast::string_view target) {
    http::response<http::string_body> response{
      http::status::not_found,
      request.version(),
    };
    response.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    response.set(http::field::content_type, "text/html");
    response.keep_alive(request.keep_alive());
    response.body() = "The resource '" + std::string(target) + "' was not found.";
    response.prepare_payload();
    return response;
  };

  // Returns a server error response.
  const auto server_error = [&request](beast::string_view what) {
    http::response<http::string_body> response{
      http::status::internal_server_error,
      request.version(),
    };
    response.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    response.set(http::field::content_type, "text/html");
    response.keep_alive(request.keep_alive());
    response.body() = "An error occurred: '" + std::string(what) + "'";
    response.prepare_payload();
    return response;
  };

  // Make sure we can handle the method.
  if (request.method() != http::verb::get && request.method() != http::verb::head) {
    co_return co_await send(stream, bad_request("Unknown HTTP-method"));
  }

  // Request path must be absolute and not contain "..".
  if (request.target().empty() || request.target()[0] != '/' || request.target().find("..") != beast::string_view::npos) {
    co_return co_await send(stream, bad_request("Illegal request-target"));
  }

  // Build the path to the requested file.
  auto path = path_cat(root, request.target());
  if (request.target().back() == '/') {
    path.append("index.html");
  }

  // Attempt to open the file.
  beast::error_code ec;
  http::file_body::value_type body;
  body.open(path.data(), beast::file_mode::scan, ec);

  // Handle the case where the file doesn't exist.
  if (ec == beast::errc::no_such_file_or_directory) {
    co_return co_await send(stream, not_found(request.target()));
  }

  // Handle an unknown error.
  if (ec) {
    co_return co_await send(stream, server_error(ec.message()));
  }

  // Cache the size since we need it after the move.
  const auto size = body.size();

  // Respond to HEAD request.
  if (request.method() == http::verb::head) {
    http::response<http::empty_body> response{
      http::status::ok,
      request.version(),
    };
    response.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    response.set(http::field::content_type, mime_type(path));
    response.content_length(size);
    response.keep_alive(request.keep_alive());
    co_return co_await send(stream, std::move(response));
  }

  // Respond to GET request.
  http::response<http::file_body> response{
    std::piecewise_construct,
    std::make_tuple(std::move(body)),
    std::make_tuple(http::status::ok, request.version()),
  };
  response.set(http::field::server, BOOST_BEAST_VERSION_STRING);
  response.set(http::field::content_type, mime_type(path));
  response.content_length(size);
  response.keep_alive(request.keep_alive());
  co_return co_await send(stream, std::move(response));
}

auto session(asio::ip::tcp::socket socket, std::string_view root) {
  return [socket = std::move(socket), root]() mutable -> asio::awaitable<void> {
    beast::tcp_stream stream(std::move(socket));
    stream.expires_after(std::chrono::seconds(30));

    while (true) {
      beast::flat_buffer buffer;
      http::request<http::string_body> request;
      if (auto ec = co_await recv(stream, buffer, request)) {
        if (ec == http::error::end_of_stream) {
          stream.socket().shutdown(asio::ip::tcp::socket::shutdown_send, ec);
          co_return;
        }
        LOGE("recv: {}", ec.message());
        co_return;
      }

      if (auto ec = co_await handle(stream, request, root)) {
        if (ec == http::error::end_of_stream) {
          stream.socket().shutdown(asio::ip::tcp::socket::shutdown_send, ec);
          co_return;
        }
        LOGE("send: {}", ec.message());
        co_return;
      }
    }
  };
}

auto server(asio::executor executor, asio::ip::tcp::endpoint endpoint, std::string_view root) {
  return [executor, endpoint = std::move(endpoint), root]() -> asio::awaitable<void> {
    try {
      asio::ip::tcp::acceptor acceptor{ asio::make_strand(executor), endpoint, true };
      while (true) {
        if (auto socket = co_await acceptor.async_accept(as_result(asio::use_awaitable))) {
          asio::co_spawn(executor, session(std::move(socket.value()), root), asio::detached);
        }
      }
    }
    catch (const std::exception& e) {
      LOGC("{}", e.what());
      co_return;
    }
  };
}

int main(int argc, char* argv[]) {
  if (argc != 4) {
    std::fputs("Usage: http-server-async <address> <port> <root>\n", stderr);
    return EXIT_FAILURE;
  }

  const auto address = asio::ip::make_address(argv[1]);
  const auto port = static_cast<unsigned short>(std::atoi(argv[2]));
  const auto root = std::string(argv[3]);

  spdlog::init_thread_pool(8192, 1);
  spdlog::stdout_color_mt<spdlog::async_factory>("server")->set_pattern(LOG_PATTERN);
  spdlog::set_default_logger(spdlog::get("server"));

  asio::io_context ioc{ 1 };
  auto executor = ioc.get_executor();
  asio::co_spawn(executor, server(executor, asio::ip::tcp::endpoint{ address, port }, root), asio::detached);
  ioc.run();

  return EXIT_SUCCESS;
}
