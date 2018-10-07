#include "./network/socket_message.h"

using namespace std;

namespace Splash
{
namespace Socket
{

/*************/
Message createMessage(const MessageType& type, const vector<uint8_t>& payload)
{
    return make_tuple(type, payload);
}

/*************/
MessageType getMessageType(const Message& message)
{
    return get<0>(message);
}

/*************/
vector<uint8_t> getMessagePayload(const Message& message)
{
    return get<1>(message);
}

} // namespace Socket
} // namespace Splash
