// This file is part of MLDB. Copyright 2015 Datacratic. All rights reserved.

/* tcp_acceptor_test+http.cc
   Wolfgang Sourdeau, September 2015
   Copyright (c) 2015 Datacratic.  All rights reserved.

   Unit test for TcpAcceptor and HttpSocketHandler
*/

#define BOOST_TEST_MAIN
#define BOOST_TEST_DYN_LINK

#include <iostream>
#include <string>
#include <boost/test/unit_test.hpp>
#include <boost/asio.hpp>
#include "base/exc_assert.h"

#include "mldb/http/asio_thread_pool.h"
#include "mldb/http/event_loop.h"
#include "mldb/http/event_loop_impl.h"
#include "mldb/http/http_rest_proxy.h"
#include "mldb/http/http_socket_handler.h"
#include "mldb/http/port_range_service.h"
#include "mldb/http/tcp_acceptor.h"


using namespace std;
using namespace boost;
using namespace Datacratic;

struct MyHandler : public HttpLegacySocketHandler {
    MyHandler(TcpSocket && socket);

    virtual void handleHttpPayload(const HttpHeader & header,
                                   const std::string & payload);
};

MyHandler::
MyHandler(TcpSocket && socket)
    : HttpLegacySocketHandler(std::move(socket))
{
}

void
MyHandler::
handleHttpPayload(const HttpHeader & header,
                  const std::string & payload)
{
    HttpResponse response(200, "text/plain", "pong");

    if (header.resource == "/wait") {
        cerr << "service sleeping\n";
        ::sleep(2);
        cerr << "service done sleeping\n";
    }

    // static string responseStr("HTTP/1.1 200 OK\r\n"
    //                           "Content-Type: text/plain\r\n"
    //                           "Content-Length: 4\r\n"
    //                           "\r\n"
    //                           "pong");
    putResponseOnWire(response);
    
    // send(responseStr);
}


/* Test simple random request */
BOOST_AUTO_TEST_CASE( tcp_acceptor_http_test )
{
    AsioThreadPool pool;
    pool.ensureThreads(1);

    auto onNewConnection = [&] (TcpSocket && socket) {
        return std::make_shared<MyHandler>(std::move(socket));
    };

    TcpAcceptor acceptor(pool.nextLoop(), onNewConnection);
    acceptor.ensureThreads(1);
    acceptor.listen(0, "localhost");

    cerr << ("service accepting connections on port "
             + to_string(acceptor.effectiveTCPv4Port())
             + "\n");
    BOOST_REQUIRE_GT(acceptor.effectiveTCPv4Port(), 0);
    BOOST_REQUIRE_EQUAL(acceptor.effectiveTCPv6Port(), -1);
    HttpRestProxy proxy("http://localhost:"
                        + to_string(acceptor.effectiveTCPv4Port()));
    auto resp = proxy.get("/v1/ping");
    BOOST_REQUIRE_EQUAL(resp.code(), 200);

    cerr << "shutting down acceptor\n";
}


/* Test request and close the socket before the response */
BOOST_AUTO_TEST_CASE( tcp_acceptor_http_disconnect_test )
{
    AsioThreadPool pool;
    pool.ensureThreads(1);

    auto onNewConnection = [&] (TcpSocket && socket) {
        return std::make_shared<MyHandler>(std::move(socket));
    };

    auto & loop = pool.nextLoop();
    TcpAcceptor acceptor(loop, onNewConnection);
    acceptor.ensureThreads(1);
    acceptor.listen(0, "localhost");

    auto address = asio::ip::address::from_string("127.0.0.1");
    asio::ip::tcp::endpoint serverEndpoint(address,
                                           acceptor.effectiveTCPv4Port());

    {
        auto socket = asio::ip::tcp::socket(loop.impl().ioService());
        socket.connect(serverEndpoint);
        string request = ("GET /wait HTTP/1.1\r\n"
                          "Host: *\r\n"
                          "\r\n");
        cerr << "sending\n";
        socket.send(boost::asio::buffer(request.c_str(), request.size()));
        cerr << "sent\n";
    }
    ::sleep(1);
}
