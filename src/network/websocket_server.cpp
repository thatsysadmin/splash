#include "./network/websocket_server.h"

#include <functional>

#include "./utils/log.h"

using namespace std;

namespace Splash
{

/*************/
WebsocketServer::~WebsocketServer()
{
    _running = false;
    _server.stop();
}

/*************/
bool WebsocketServer::start()
{
    try
    {
        _server.set_access_channels(websocketpp::log::alevel::all);
        _server.set_access_channels(websocketpp::log::alevel::frame_payload);

        _server.init_asio();
        _server.set_message_handler(std::bind(&WebsocketServer::onMessage, this, std::placeholders::_1, std::placeholders::_2));

        _server.listen(_port);
        _server.start_accept();
        _serverFuture = async(std::launch::async, [&]() {
            _running = true;
            _server.run();
        });
    }
    catch (websocketpp::exception const& e)
    {
        std::cout << e.what() << std::endl;
        return false;
    }
    catch (...)
    {
        std::cout << "Caught unknown exception" << std::endl;
        return false;
    }

    return true;
}

/*************/
void WebsocketServer::onMessage(websocketpp::connection_hdl handler, message_ptr_t msg)
{
    Log::get() << Log::MESSAGE << "WebsocketServer::" << __FUNCTION__ << " - Received message : \"" << msg->get_payload() << "\"" << Log::endl;
    std::string answer("Message received!");
    _server.send(handler, answer.c_str(), answer.length() + 1, websocketpp::frame::opcode::binary);
}

} // namespace Splash
