#include "./network/websocket_server.h"

#include <functional>
#include <memory>

#include "./core/serialize/serialize_uuid.h"
#include "./core/serialize/serialize_value.h"
#include "./core/serializer.h"
#include "./network/socket_message.h"
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
        _server.set_open_handler([&](websocketpp::connection_hdl handler) { onOpen(handler); });
        _server.set_close_handler([&](websocketpp::connection_hdl handler) { onClose(handler); });
        _server.set_message_handler([&](websocketpp::connection_hdl handler, message_ptr_t msg) { onMessage(handler, msg); });

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
void WebsocketServer::queueTreeUpdates(const list<Tree::Seed>& treeUpdates)
{
    lock_guard<mutex> lock(_updatesMutex);
    for (auto& connection : _connections)
    {
        auto updatesIt = _updatesPerConnection.find(connection);
        assert(updatesIt != _updatesPerConnection.end());
        for (const auto& update : treeUpdates)
            updatesIt->second.push_back(update);
    }
}

/*************/
void WebsocketServer::onMessage(websocketpp::connection_hdl handler, message_ptr_t msg)
{
    auto data = reinterpret_cast<const uint8_t*>(msg->get_payload().data());
    auto serializedMessage = vector<uint8_t>(data, data + msg->get_payload().size());
    auto message = Serial::deserialize<Socket::Message>(serializedMessage);
    auto messageId = Socket::getMessageId(message);

    switch (Socket::getMessageType(message))
    {
    default:
    {
        assert(false);
        return;
    }
    case Socket::MessageType::ASK_TREE:
    {
        std::cout << "SEND TREE\n";
        auto tree = _root->getTree();
        auto seeds = tree->getSeedsForPath("/");
        vector<uint8_t> serializedSeeds;
        Serial::serialize(seeds, serializedSeeds);

        Socket::Message outMessage;
        Socket::MessageId outMessageId;
        std::tie(outMessageId, outMessage) = Socket::createMessage(Socket::MessageType::SEND_TREE, serializedSeeds, messageId);
        vector<uint8_t> buffer;
        Serial::serialize(outMessage, buffer);
        _server.send(handler, buffer.data(), buffer.size(), websocketpp::frame::opcode::binary);

        break;
    }
    case Socket::MessageType::ASK_UPDATES:
    {
        std::cout << "SEND UPDATES\n";
        lock_guard<mutex> lock(_updatesMutex);
        auto seeds = list<Tree::Seed>();
        auto updatesIt = _updatesPerConnection.find(handler);
        assert(updatesIt != _updatesPerConnection.end());
        std::swap(seeds, updatesIt->second);

        vector<uint8_t> serializedSeeds;
        Serial::serialize(seeds, serializedSeeds);

        Socket::Message outMessage;
        Socket::MessageId outMessageId;
        std::tie(outMessageId, outMessage) = Socket::createMessage(Socket::MessageType::SEND_UPDATES, serializedSeeds, messageId);
        vector<uint8_t> buffer;
        Serial::serialize(outMessage, buffer);
        _server.send(handler, buffer.data(), buffer.size(), websocketpp::frame::opcode::binary);

        break;
    }
    case Socket::MessageType::SEND_UPDATES:
    {
        auto payload = Socket::getMessagePayload(message);
        if (payload.size() == 0)
            break;

        auto seeds = Serial::deserialize<list<Tree::Seed>>(payload);
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
    _updatesPerConnection.emplace(make_pair(handler, list<Tree::Seed>()));
}

/*************/
void WebsocketServer::onClose(websocketpp::connection_hdl handler)
{
    auto connection = _server.get_con_from_hdl(handler);
    Log::get() << Log::MESSAGE << "WebsocketServer::" << __FUNCTION__ << " - Connection closed from " << connection->get_host() << Log::endl;
    _connections.remove_if([&](websocketpp::connection_hdl hdl) { return handler.lock() == hdl.lock(); });
    _updatesPerConnection.erase(handler);
}

} // namespace Splash
