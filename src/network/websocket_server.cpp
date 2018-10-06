#include "./network/websocket_server.h"

#include <functional>
#include <memory>

#include "./core/serialize/serialize_uuid.h"
#include "./core/serialize/serialize_value.h"
#include "./core/serializer.h"
#include "./network/socket_messages.h"
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
#ifdef DEBUG
        _server.set_access_channels(websocketpp::log::alevel::all);
        _server.set_access_channels(websocketpp::log::alevel::frame_payload);
#else
        _server.set_access_channels(websocketpp::log::alevel::none);
#endif

        _server.init_asio();
        _server.set_open_handler(std::bind(&WebsocketServer::onOpen, this, std::placeholders::_1));
        _server.set_close_handler(std::bind(&WebsocketServer::onClose, this, std::placeholders::_1));
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
bool WebsocketServer::sendTreeUpdates(const ResizableArray<uint8_t>& treeUpdates)
{
    for (auto& hdl : _connections)
    {
        auto query = Socket::MessageType::UPDATE_TREE;
        ResizableArray<uint8_t> buffer(reinterpret_cast<uint8_t*>(&query), reinterpret_cast<uint8_t*>(&query) + sizeof(query));
        _server.send(hdl, buffer.data(), buffer.size(), websocketpp::frame::opcode::binary);

        uint32_t length = treeUpdates.size();
        buffer = ResizableArray<uint8_t>(reinterpret_cast<uint8_t*>(&length), reinterpret_cast<uint8_t*>(&length) + sizeof(length));
        _server.send(hdl, buffer.data(), buffer.size(), websocketpp::frame::opcode::binary);

        _server.send(hdl, treeUpdates.data(), treeUpdates.size(), websocketpp::frame::opcode::binary);
    }

    return true;
}

/*************/
void WebsocketServer::onMessage(websocketpp::connection_hdl handler, message_ptr_t msg)
{
    auto message = reinterpret_cast<const Socket::Message*>(msg->get_payload().data());

    switch (message->type)
    {
    default:
    {
        assert(false);
        return;
    }
    case Socket::MessageType::GET_TREE:
    {
        Log::get() << Log::MESSAGE << "WebsocketServer::" << __FUNCTION__ << " - Received a GET_TREE message" << Log::endl;

        auto query = Socket::MessageType::SEND_TREE;
        ResizableArray<uint8_t> buffer(reinterpret_cast<uint8_t*>(&query), reinterpret_cast<uint8_t*>(&query) + sizeof(query));
        _server.send(handler, buffer.data(), buffer.size(), websocketpp::frame::opcode::binary);

        auto tree = _root->getTree();
        auto seeds = tree->getSeedsForPath("/");
        vector<uint8_t> serializedSeeds;
        Serial::serialize(seeds, serializedSeeds);

        buffer.resize(sizeof(uint32_t));
        *reinterpret_cast<uint32_t*>(buffer.data()) = serializedSeeds.size();
        _server.send(handler, buffer.data(), buffer.size(), websocketpp::frame::opcode::binary);

        if (serializedSeeds.size() != 0)
        {
            buffer = ResizableArray<uint8_t>(serializedSeeds.data(), serializedSeeds.data() + serializedSeeds.size());
            _server.send(handler, buffer.data(), buffer.size(), websocketpp::frame::opcode::binary);
        }

        break;
    }
    case Socket::MessageType::UPDATE_TREE:
    {
        Log::get() << Log::MESSAGE << "WebsocketServer::" << __FUNCTION__ << " - Received a UPDATE_TREE message" << Log::endl;

        if (message->payloadSize == 0)
            break;

        auto serializedSeeds = vector<uint8_t>(message->payload, message->payload + message->payloadSize);
        auto seeds = Serial::deserialize<list<Tree::Seed>>(serializedSeeds);
        auto tree = _root->getTree();
        tree->addSeedsToQueue(seeds);

        break;
    }
    }
}

/*************/
void WebsocketServer::onOpen(websocketpp::connection_hdl handler)
{
    auto connection = _server.get_con_from_hdl(handler);
    Log::get() << Log::MESSAGE << "WebsocketServer::" << __FUNCTION__ << " - Connection opened from " << connection->get_host() << Log::endl;
    _connections.push_back(handler);
}

/*************/
void WebsocketServer::onClose(websocketpp::connection_hdl handler)
{
    auto connection = _server.get_con_from_hdl(handler);
    Log::get() << Log::MESSAGE << "WebsocketServer::" << __FUNCTION__ << " - Connection closed from " << connection->get_host() << Log::endl;
    _connections.remove_if([&](websocketpp::connection_hdl hdl) { return handler.lock() == hdl.lock(); });
}

} // namespace Splash
