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

// Return a reasonable mime type based on the extension of a file.
std::string_view mime_type(std::string_view path) {
  using boost::beast::iequals;
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

template <typename Request = boost::beast::http::request<boost::beast::http::string_body>>
boost::asio::awaitable<boost::beast::error_code> handle(boost::beast::tcp_stream& stream, Request& request, std::string_view root) {
  // Returns a bad request response.
  const auto bad_request = [&request](boost::beast::string_view why) {
    boost::beast::http::response<boost::beast::http::string_body> response{
      boost::beast::http::status::bad_request,
      request.version(),
    };
    response.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
    response.set(boost::beast::http::field::content_type, "text/html");
    response.keep_alive(request.keep_alive());
    response.body() = std::string(why);
    response.prepare_payload();
    return response;
  };

  // Returns a not found response.
  const auto not_found = [&request](boost::beast::string_view target) {
    boost::beast::http::response<boost::beast::http::string_body> response{
      boost::beast::http::status::not_found,
      request.version(),
    };
    response.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
    response.set(boost::beast::http::field::content_type, "text/html");
    response.keep_alive(request.keep_alive());
    response.body() = "The resource '" + std::string(target) + "' was not found.";
    response.prepare_payload();
    return response;
  };

  // Returns a server error response.
  const auto server_error = [&request](boost::beast::string_view what) {
    boost::beast::http::response<boost::beast::http::string_body> response{
      boost::beast::http::status::internal_server_error,
      request.version(),
    };
    response.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
    response.set(boost::beast::http::field::content_type, "text/html");
    response.keep_alive(request.keep_alive());
    response.body() = "An error occurred: '" + std::string(what) + "'";
    response.prepare_payload();
    return response;
  };

  // Sends response to client.
  const auto send = [&](auto response) -> boost::asio::awaitable<boost::beast::error_code> {
    const auto result = co_await boost::beast::http::async_write(stream, response, as_result(boost::asio::use_awaitable));
    co_return result.error();
  };

  // Make sure we can handle the method.
  if (request.method() != boost::beast::http::verb::get && request.method() != boost::beast::http::verb::head) {
    return send(bad_request("Unknown HTTP-method"));
  }

  // Request path must be absolute and not contain "..".
  if (request.target().empty() || request.target()[0] != '/' || request.target().find("..") != boost::beast::string_view::npos) {
    return send(bad_request("Illegal request-target"));
  }

  // Build the path to the requested file.
  auto path = path_cat(root, request.target());
  if (request.target().back() == '/') {
    path.append("index.html");
  }

  // Attempt to open the file.
  boost::beast::error_code ec;
  boost::beast::http::file_body::value_type body;
  body.open(path.data(), boost::beast::file_mode::scan, ec);

  // Handle the case where the file doesn't exist.
  if (ec == boost::beast::errc::no_such_file_or_directory) {
    return send(not_found(request.target()));
  }

  // Handle an unknown error.
  if (ec) {
    return send(server_error(ec.message()));
  }

  // Cache the size since we need it after the move.
  const auto size = body.size();

  // Respond to HEAD request.
  if (request.method() == boost::beast::http::verb::head) {
    boost::beast::http::response<boost::beast::http::empty_body> response{
      boost::beast::http::status::ok,
      request.version(),
    };
    response.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
    response.set(boost::beast::http::field::content_type, mime_type(path));
    response.content_length(size);
    response.keep_alive(request.keep_alive());
    return send(std::move(response));
  }

  // Respond to GET request.
  boost::beast::http::response<boost::beast::http::file_body> response{
    std::piecewise_construct,
    std::make_tuple(std::move(body)),
    std::make_tuple(boost::beast::http::status::ok, request.version()),
  };
  response.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
  response.set(boost::beast::http::field::content_type, mime_type(path));
  response.content_length(size);
  response.keep_alive(request.keep_alive());
  return send(std::move(response));
}

auto session(boost::asio::ip::tcp::socket socket, std::string_view root) {
  return [socket = std::move(socket), root]() mutable -> boost::asio::awaitable<void> {
    boost::beast::tcp_stream stream(std::move(socket));
    stream.expires_after(std::chrono::seconds(30));

    boost::beast::flat_buffer buffer;
    boost::beast::http::request<boost::beast::http::string_body> request;
    const auto size = co_await boost::beast::http::async_read(stream, buffer, request, as_result(boost::asio::use_awaitable));
    if (!size) {
      if (size.error() == boost::beast::http::error::end_of_stream) {
        boost::beast::error_code ec;
        stream.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
        co_return;
      }
      LOGE("read: {}", size.error().message());
      co_return;
    }
    co_await handle(stream, request, root);
  };
}

auto server(boost::asio::io_context& ioc, boost::asio::ip::tcp::endpoint endpoint, std::string_view root) {
  return [&ioc, endpoint = std::move(endpoint), root]() -> boost::asio::awaitable<void> {
    boost::asio::ip::tcp::acceptor acceptor{ boost::asio::make_strand(ioc) };
    try {
      acceptor.open(endpoint.protocol());
      acceptor.set_option(boost::asio::socket_base::reuse_address(true));
      acceptor.bind(endpoint);
      acceptor.listen(boost::asio::socket_base::max_listen_connections);
    }
    catch (const std::exception& e) {
      LOGC("{}", e.what());
      co_return;
    }
    while (true) {
      if (auto socket = co_await acceptor.async_accept(as_result(boost::asio::use_awaitable))) {
        boost::asio::co_spawn(ioc, session(std::move(socket.value()), root), boost::asio::detached);
      }
    }
  };
}

int main(int argc, char* argv[]) {
  if (argc != 4) {
    std::fputs("Usage: http-server-async <address> <port> <root>\n", stderr);
    return EXIT_FAILURE;
  }
  const auto address = boost::asio::ip::make_address(argv[1]);
  const auto port = static_cast<unsigned short>(std::atoi(argv[2]));
  const auto root = std::string(argv[3]);

  spdlog::init_thread_pool(8192, 1);
  spdlog::stdout_color_mt<spdlog::async_factory>("server")->set_pattern(LOG_PATTERN);
  spdlog::set_default_logger(spdlog::get("server"));

  boost::asio::io_context ioc{ 1 };
  boost::asio::ip::tcp::endpoint endpoint{ address, port };
  boost::asio::co_spawn(ioc, server(ioc, endpoint, root), boost::asio::detached);
  ioc.run();

  return EXIT_SUCCESS;
}
