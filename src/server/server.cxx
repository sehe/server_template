#include "server.hxx"
#include "src/logic/logic.hxx"
#include <algorithm>
#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>
#include <iostream>

#include <coroutine> // enable if build with gcc
#include <utility>
//#include <experimental/coroutine> //enable if build with clang

namespace asio      = boost::asio;
namespace beast     = boost::beast;
namespace websocket = beast::websocket;
namespace http      = beast::http;
using asio::ip::tcp;

Server::Server(tcp::endpoint endpoint) : _endpoint{std::move(endpoint)} {}

asio::awaitable<std::string> Server::my_read(Websocket& ws_)
{
    std::cout << "read" << std::endl;
    beast::flat_buffer buffer;
    co_await ws_.async_read(buffer, asio::use_awaitable);
    auto msg = beast::buffers_to_string(buffer.data());
    std::cout << "number of letters '" << msg.size() << "' msg: '" << msg << "'"
              << std::endl;
    co_return msg;
}

asio::awaitable<void>
Server::readFromClient(std::list<std::shared_ptr<User>>::iterator user,
                       Websocket&                                 connection)
{
    try {
        for (;;) {
            auto readResult = co_await my_read(connection);
            handleMessage(readResult, users, *user);
        }
    } catch (std::exception& e) {
        removeUser(user);
        std::cout << "read Exception: " << e.what() << std::endl;
    }
}

void Server::removeUser(std::list<std::shared_ptr<User>>::iterator user)
{
    users.erase(user);
}

asio::awaitable<void>
Server::writeToClient(std::shared_ptr<User>     user,
                      std::weak_ptr<Websocket>& connection)
{
    try {
        while (not connection.expired()) {
            // TODO this is polling because we check every 100 milli seconds.
            auto timer = asio::steady_timer(co_await asio::this_coro::executor);
            auto const waitForNewMessagesToSend =
                std::chrono::milliseconds{100};
            timer.expires_after(waitForNewMessagesToSend);
            co_await timer.async_wait(asio::use_awaitable);
            while (not user->msgQueue.empty() && not connection.expired()) {
                auto tmpMsg = std::move(user->msgQueue.front());
                std::cout << " msg: " << tmpMsg << std::endl;
                user->msgQueue.pop_front();
                co_await connection.lock()->async_write(asio::buffer(tmpMsg),
                                                        asio::use_awaitable);
            }
        }
    } catch (std::exception& e) {
        std::cout << "write Exception: " << e.what() << std::endl;
    }
}

asio::awaitable<void> Server::listener()
{
    auto          executor = co_await asio::this_coro::executor;
    tcp::acceptor acceptor(executor, _endpoint);
    for (;;) {
        try {
            auto socket = co_await acceptor.async_accept(asio::use_awaitable);
            auto connection = std::make_shared<Websocket>(std::move(socket));
            users.emplace_back(std::make_shared<User>(User{{}}));
            auto user = std::next(users.end(), -1);
            connection->set_option(websocket::stream_base::timeout::suggested(
                beast::role_type::server));
            connection->set_option(websocket::stream_base::decorator(
                [](websocket::response_type& res) {
                    res.set(http::field::server,
                            std::string(BOOST_BEAST_VERSION_STRING) +
                                " websocket-server-async");
                }));
            co_await connection->async_accept();
            co_spawn(
                executor,
                [connection, this, &user]() mutable {
                    return readFromClient(user, *connection);
                },
                asio::detached);
            co_spawn(
                executor,
                [connectionWeakPointer = std::weak_ptr<Websocket>{connection},
                 this, &user]() mutable {
                    return writeToClient(*user, connectionWeakPointer);
                },
                asio::detached);
        } catch (std::exception& e) {
            std::cout << "Server::listener () connect  Exception : " << e.what()
                      << std::endl;
        }
    }
}
