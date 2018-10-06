#include "./network/socket_client.h"

#include <netdb.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>

#include "./core/coretypes.h"
#include "./core/serialize/serialize_uuid.h"
#include "./core/serialize/serialize_value.h"
#include "./core/serializer.h"
#include "./network/socket_messages.h"
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
bool SocketClient::send(const ResizableArray<uint8_t>& buffer)
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
    // Ask the server for the tree
    auto query = Socket::MessageType::GET_TREE;
    ResizableArray<uint8_t> buffer(reinterpret_cast<uint8_t*>(&query), reinterpret_cast<uint8_t*>(&query) + sizeof(query));
    send(buffer);

    // Wait for the server to answer
    while (true)
    {
        if (!receive(buffer, -1))
            return false;

        query = *reinterpret_cast<Socket::MessageType*>(buffer.data());
        if (query == Socket::MessageType::SEND_TREE)
            break;
    }

    // Get the serialized tree size
    buffer.resize(sizeof(uint32_t));
    if (!receive(buffer, -1))
        return false;
    auto length = *reinterpret_cast<uint32_t*>(buffer.data());

    if (length == 0)
        return true; // The tree is empty

    // Get the tree
    buffer.resize(length);
    if (!receive(buffer, -1))
        return false;

    auto serializedSeeds = vector<uint8_t>(buffer.data(), buffer.data() + buffer.size());
    auto seeds = Serial::deserialize<list<Tree::Seed>>(serializedSeeds);
    tree.cutdown();
    tree.addSeedsToQueue(seeds);

    return true;
}

/*************/
bool SocketClient::sendUpdatesToServer(Tree::Root& tree)
{
    // Get the seeds
    auto treeSeeds = tree.getSeedList();

    if (treeSeeds.empty())
        return true;

    vector<uint8_t> serializedSeeds;
    Serial::serialize(treeSeeds, serializedSeeds);
    auto seeds = Serial::deserialize<list<Tree::Seed>>(serializedSeeds);

    size_t messageSize = sizeof(Socket::Message) + serializedSeeds.size() * sizeof(uint8_t);
    ResizableArray<uint8_t> buffer(messageSize);
    auto message = reinterpret_cast<Socket::Message*>(buffer.data());
    message->type = Socket::MessageType::UPDATE_TREE;
    message->payloadSize = serializedSeeds.size();
    memcpy(message->payload, serializedSeeds.data(), serializedSeeds.size());

    if (!send(buffer))
        return false;

    return true;
}

/*************/
bool SocketClient::receive(ResizableArray<uint8_t>& buffer, int timeout)
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

} // namespace Splash
