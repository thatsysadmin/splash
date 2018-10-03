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
 * @socket_client.h
 * The SocketClient class, used for communication through POSIX sockets
 * Note that when compiled with Emscripten, the backend is websocket
 */

#ifndef SPLASH_SOCKETCLIENT_H
#define SPLASH_SOCKETCLIENT_H

#include <string>
#include <sys/socket.h>
#include <netinet/in.h>

#include "./core/resizable_array.h"

namespace Splash
{

/*************/
class SocketClient
{
  public:
    /**
     * Constructor
     */
    SocketClient();

    /**
     * Destructor
     */
    ~SocketClient();

    /**
     * Connect to the given host
     * \param host Hostname or its IP address
     * \param port Port number
     * \return Return true if connection has been successful
     */
    bool connect(const std::string& host, int port);

    /**
     * Send a message through the link
     * \param buffer Buffer to send
     * \return Return true if the buffer was sent successfully
     */
    bool send(const ResizableArray<uint8_t>& buffer);

    /**
     * Read a message from the link
     * \param buffer Read buffer
     * \param timeout Timeout for reading a buffer
     * \return Return true if a buffer was read
     */
    bool receive(ResizableArray<uint8_t>& buffer, int timeout = 0);

  private:
    bool _connected{false};
    int _sockfd{0};
    int _port{0};
    struct hostent* _server{nullptr};
};

} // namespace Splash

#endif // SPLASH_SOCKETCLIENT_H
