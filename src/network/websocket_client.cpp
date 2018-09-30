#include "./network/websocket_client.h"

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
WebSocketClient::WebSocketClient() {}

/*************/
WebSocketClient::~WebSocketClient()
{
    if (_sockfd)
        close(_sockfd);
}

/*************/
bool WebSocketClient::connect(const string& host, int port)
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
    return true;
}

/*************/
bool WebSocketClient::send(const ResizableArray<uint8_t>& /*buffer*/)
{
    return true;
}

/*************/
bool WebSocketClient::receive(ResizableArray<uint8_t>& /*buffer*/)
{
    return true;
}

} // namespace Splash
