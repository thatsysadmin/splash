/*
 * Copyright (C) 2018 Emmanuel Durand
 *
 * This file is part of Splash.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Splash is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Splash.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * @socket_messages.h
 * Header file containing the ID of various socket messages types
 */

#ifndef SPLASH_SOCKET_MESSAGES_H
#define SPLASH_SOCKET_MESSAGES_H

#include <atomic>
#include <optional>
#include <tuple>
#include <vector>
#include <stdint.h>

namespace Splash
{
namespace Socket
{

/*************/
enum MessageType : uint32_t
{
    ASK_TREE = 1,
    SEND_TREE,
    ASK_UPDATES,
    SEND_UPDATES
};

/*************/
typedef uint32_t MessageId;
typedef std::tuple<MessageId, MessageType, std::vector<uint8_t>> Message;

static std::atomic_uint _messageIndex{0};

std::tuple<MessageId, Message> createMessage(const MessageType& type, const std::vector<uint8_t>& payload, const std::optional<MessageId>& incomingId = {});
MessageId getMessageId(const Message& message);
MessageType getMessageType(const Message& message);
std::vector<uint8_t> getMessagePayload(const Message& message);

} // namespace Socket
} // namespace Splash

#endif // SPLASH_SOCKET_MESSAGES_H
