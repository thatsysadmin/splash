#include "./network/socket_message.h"

using namespace std;

namespace Splash
{
namespace Socket
{

/*************/
tuple<MessageId, Message> createMessage(const MessageType& type, const vector<uint8_t>& payload, const std::optional<MessageId>& incomingId)
{
    auto messageId = incomingId.value_or(_messageIndex.fetch_add(1));
    auto message = make_tuple(messageId, type, payload);
    return std::make_tuple(messageId, message);
}

/*************/
MessageId getMessageId(const Message& message)
{
    return get<0>(message);
}

/*************/
MessageType getMessageType(const Message& message)
{
    return get<1>(message);
}

/*************/
vector<uint8_t> getMessagePayload(const Message& message)
{
    return get<2>(message);
}

} // namespace Socket
} // namespace Splash
