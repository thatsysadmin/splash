#include "./network/socket_client.h"

#include <chrono>
#include <netdb.h>
#include <string.h>
#include <sys/select.h>
#include <thread>
#include <unistd.h>

#include "./core/coretypes.h"

using namespace std;

namespace Splash
{

/*************/
SocketClient::SocketClient() {}

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
    if (result < 0)
    {
        if (errno == EINPROGRESS)
        {
            fd_set sockets;
            FD_ZERO(&sockets);
            FD_SET(_sockfd, &sockets);

            while (select(_sockfd + 1, nullptr, &sockets, nullptr, nullptr) < 0)
                this_thread::sleep_for(chrono::milliseconds(5));
        }
        else
        {
            freeaddrinfo(infoptr);
            return false; // TODO: handle error type
        }
    }

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
bool SocketClient::receive(ResizableArray<uint8_t>& buffer, int timeout)
{
    if (!_connected)
        return false;

    fd_set sockets;
    FD_ZERO(&sockets);
    FD_SET(_sockfd, &sockets);
    timeval duration{0, timeout * 1000};
    int result = select(_sockfd + 1, &sockets, nullptr, nullptr, &duration);
    if (result <= 0)
        return false;

    auto length = recv(_sockfd, buffer.data(), buffer.size(), 0);
    if (length <= 0)
        return false;

    buffer.resize(length);
    return true;
}

} // namespace Splash
