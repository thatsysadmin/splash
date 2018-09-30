#define ASIO_STANDALONE

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include <iostream>

using websocketpp::lib::bind;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;

typedef websocketpp::server<websocketpp::config::asio> Server;
typedef Server::message_ptr message_ptr;

void onMessage(Server* s, websocketpp::connection_hdl hdl, message_ptr msg)
{
    std::cout << "onMessage called with hdl: " << hdl.lock().get() << "and message: " << msg->get_payload() << std::endl;
}

int main()
{
    Server echoServer;

    try
    {
        echoServer.set_access_channels(websocketpp::log::alevel::all);
        echoServer.set_access_channels(websocketpp::log::alevel::frame_payload);

        echoServer.init_asio();
        echoServer.set_message_handler(bind(&onMessage, &echoServer, ::_1, ::_2));

        echoServer.listen(1111);
        echoServer.start_accept();
        echoServer.run();
    }
    catch (websocketpp::exception const& e)
    {
        std::cout << e.what() << std::endl;
    }
    catch (...)
    {
        std::cout << "Caught unknown exception" << std::endl;
    }
}
