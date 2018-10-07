#include "./network/socket_client.h"

#include <algorithm>
#include <netdb.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>

#include "./core/coretypes.h"
#include "./core/serialize/serialize_uuid.h"
#include "./core/serialize/serialize_value.h"
#include "./core/serializer.h"
#include "./utils/log.h"

using namespace std;

namespace Splash
{

/*************/
SocketClient::~SocketClient()
{
    if (_sockfd)
        close(_sockfd);
}

/*************/
bool SocketClient::connect(const string& host, int port)
{
    _sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (_sockfd < 0)
        return false; // TODO: handle error type

    struct addrinfo hints;
    hints.ai_family = AF_INET; // Let's limit to IPv4 for now

    struct addrinfo* infoptr;
    int result = getaddrinfo(host.c_str(), nullptr, &hints, &infoptr);
    if (result)
    {
        freeaddrinfo(infoptr);
        return false; // TODO: handle error type
    }

    reinterpret_cast<sockaddr_in*>(infoptr->ai_addr)->sin_port = htons(port);
    result = ::connect(_sockfd, infoptr->ai_addr, infoptr->ai_addrlen);
    if (result < 0 && errno != EINPROGRESS)
    {
        freeaddrinfo(infoptr);
        return false; // TODO: handle error type
    }

    struct sockaddr_in adr_inet;
    socklen_t len_inet = sizeof(adr_inet);
    auto z = getsockname(_sockfd, (struct sockaddr*)&adr_inet, &len_inet);

    freeaddrinfo(infoptr);
    _connected = true;
    return true;
}

/*************/
bool SocketClient::send(const vector<uint8_t>& buffer)
{
    if (!_connected)
        return false;

    auto result = ::send(_sockfd, buffer.data(), buffer.size(), 0);
    if (result == -1)
        return false;

    return true;
}

/*************/
bool SocketClient::getTreeFromServer(Tree::Root& tree)
{
    auto message = Socket::createMessage(Socket::MessageType::ASK_TREE, {});
    vector<uint8_t> buffer;
    Serial::serialize(message, buffer);
    send(buffer);

    while (true)
    {
        buffer.resize(sizeof(Socket::MessageType));
        if (!receive(buffer, -1))
            return false;

        auto messageType = *reinterpret_cast<Socket::MessageType*>(buffer.data());
        if (messageType == Socket::MessageType::SEND_TREE)
            break;
    }

    // Get the serialized tree size
    buffer.resize(sizeof(uint32_t));
    if (!receive(buffer, -1))
        return false;
    auto length = *reinterpret_cast<uint32_t*>(buffer.data());

    // Get the tree
    buffer.resize(length);
    if (!receive(buffer, -1))
        return false;

    if (length == 0)
        return true; // The tree is empty

    auto seeds = Serial::deserialize<list<Tree::Seed>>(buffer);
    tree.cutdown();
    tree.addSeedsToQueue(seeds);
    tree.processQueue();

    return true;
}

/*************/
bool SocketClient::getTreeUpdates(Tree::Root& tree)
{
    auto message = Socket::createMessage(Socket::MessageType::ASK_UPDATES, {});
    vector<uint8_t> buffer;
    Serial::serialize(message, buffer);
    send(buffer);

    while (true)
    {
        buffer.resize(sizeof(Socket::MessageType));
        if (!receive(buffer, -1))
            return false;

        auto messageType = *reinterpret_cast<Socket::MessageType*>(buffer.data());
        if (messageType == Socket::MessageType::SEND_UPDATES)
            break;
    }

    // Get the serialized tree size
    buffer.resize(sizeof(uint32_t));
    if (!receive(buffer, -1))
        return false;
    auto length = *reinterpret_cast<uint32_t*>(buffer.data());

    // Get the tree
    buffer.resize(length);
    if (!receive(buffer, -1))
        return false;

    if (length == 0)
        return true; // The tree is empty

    auto seeds = Serial::deserialize<list<Tree::Seed>>(buffer);
    tree.addSeedsToQueue(seeds);

    return true;
}

/*************/
bool SocketClient::processMessages(Tree::Root& tree)
{
    Socket::Message message;
    while (receiveMessage(message, 0))
    {
        auto messageType = Socket::getMessageType(message);
        auto payload = Socket::getMessagePayload(message);
        switch (messageType)
        {
        default:
        {
            assert(false);
            return false;
        }
        case Socket::MessageType::ASK_TREE:
        case Socket::MessageType::SEND_TREE:
        case Socket::MessageType::SEND_UPDATES:
        {
            break;
        }
        }
    }

    return true;
}

/*************/
bool SocketClient::sendUpdatesToServer(Tree::Root& tree)
{
    auto treeSeeds = tree.getUpdateSeedList();
    if (treeSeeds.empty())
        return true;
    vector<uint8_t> serializedSeeds;
    Splash::Serial::serialize(treeSeeds, serializedSeeds);

    auto message = Socket::createMessage(Socket::MessageType::SEND_UPDATES, serializedSeeds);
    vector<uint8_t> buffer;
    Serial::serialize(message, buffer);

    if (!send(buffer))
        return false;

    return true;
}

/*************/
bool SocketClient::receive(vector<uint8_t>& buffer, int timeout)
{
    if (!_connected)
        return false;

    fd_set sockets;
    FD_ZERO(&sockets);
    FD_SET(_sockfd, &sockets);
    timeval duration{0, timeout * 1000};
    int result = select(_sockfd + 1, &sockets, nullptr, nullptr, timeout == -1 ? nullptr : &duration);
    if (result <= 0)
        return false;

    auto length = recv(_sockfd, buffer.data(), buffer.size(), 0);
    if (length <= 0)
        return false;

    buffer.resize(length);
    return true;
}

/*************/
bool SocketClient::receiveMessage(Socket::Message& message, int timeout)
{
    auto buffer = vector<uint8_t>(sizeof(Socket::MessageType));
    if (!receive(buffer, timeout))
        return false;
    auto messageType = *reinterpret_cast<Socket::MessageType*>(buffer.data());

    buffer.resize(sizeof(uint32_t));
    if (!receive(buffer, 0))
        return false;
    auto payloadSize = *reinterpret_cast<uint32_t*>(buffer.data());

    if (payloadSize == 0)
    {
        message = Socket::createMessage(messageType, {});
        return true;
    }

    vector<uint8_t> payload(static_cast<size_t>(payloadSize));
    if (!receive(payload, 0))
        return false;

    message = Socket::createMessage(messageType, payload);
    return true;
}

} // namespace Splash
