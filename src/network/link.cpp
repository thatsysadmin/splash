#include "./network/link.h"

#include <algorithm>
#include <chrono>
#include <thread>

#include "./core/attribute.h"
#include "./core/buffer_object.h"
#include "./core/constants.h"
#include "./core/root_object.h"
#include "./core/serializer.h"
#include "./core/serialize/serialize_value.h"
#include "./network/channel_zmq.h"
#include "./utils/log.h"
#include "./utils/timer.h"

namespace Splash
{

/*************/
Link::Link(RootObject* root, const std::string& name)
    : _rootObject(root)
    , _name(name)
    , _channelOutput(std::make_unique<ChannelOutput_ZMQ>(root, name))
    , _channelInput(std::make_unique<ChannelInput_ZMQ>(
          root, name, [&](const std::vector<uint8_t>& message) { handleInputMessages(message); }, [&](SerializedObject&& buffer) { handleInputBuffers(std::move(buffer)); }))
{
}

/*************/
Link::~Link()
{
}

/*************/
void Link::connectTo(const std::string& name)
{
    _channelOutput->connectTo(name);
}

/*************/
void Link::disconnectFrom(const std::string& name)
{
    _channelOutput->disconnectFrom(name);
}

/*************/
bool Link::waitForBufferSending(std::chrono::milliseconds maximumWait)
{
    return _channelOutput->waitForBufferSending(maximumWait);
}

/*************/
bool Link::sendBuffer(SerializedObject&& buffer)
{
    return _channelOutput->sendBuffer(std::move(buffer));
}

/*************/
bool Link::sendBuffer(const std::shared_ptr<BufferObject>& object)
{
    auto buffer = object->serialize();
    return sendBuffer(std::move(buffer));
}

/*************/
bool Link::sendMessage(const std::string& name, const std::string& attribute, const Values& message)
{
    std::vector<uint8_t> serializedMessage;
    Serial::serialize(name, serializedMessage);
    Serial::serialize(attribute, serializedMessage);
    Serial::serialize(message, serializedMessage);

    auto result = _channelOutput->sendMessage(serializedMessage);

#ifdef DEBUG
    // We don't display broadcast messages, for visibility
    if (name != Constants::ALL_PEERS)
        Log::get() << Log::DEBUGGING << "Link::" << __FUNCTION__ << " - Sending message to " << name << "::" << attribute << Log::endl;
#endif

    return result;
}

/*************/
void Link::handleInputMessages(const std::vector<uint8_t>& message)
{
    size_t offset = 0;
    auto name = Serial::deserialize<std::string>(message);
    offset += Serial::getSize(name);
    auto attribute = Serial::deserialize<std::string>(message, offset);
    offset += Serial::getSize(attribute);
    auto value = Serial::deserialize<Values>(message, offset);

    if (_rootObject)
        _rootObject->set(name, attribute, value);
#ifdef DEBUG
    // We don't display broadcast messages, for visibility
    if (name != Constants::ALL_PEERS)
        Log::get() << Log::DEBUGGING << "Link::" << __FUNCTION__ << " (" << _rootObject->getName() << ")"
                   << " - Receiving message for " << name << "::" << attribute << Log::endl;
#endif
}

/*************/
void Link::handleInputBuffers(SerializedObject&& buffer)
{
    auto bufferDataIt = buffer._data.cbegin();
    const auto name = Serial::detail::deserializer<std::string>(bufferDataIt);
    if (_rootObject)
        _rootObject->setFromSerializedObject(name, std::move(buffer));
}

} // namespace Splash
