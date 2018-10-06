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
 * @websocket_server.h
 * The WebsocketServer class
 */

#ifndef SPLASH_WEBSOCKET_SERVER_H
#define SPLASH_WEBSOCKET_SERVER_H

#define ASIO_STANDALONE

#include <future>
#include <list>

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include "./core/coretypes.h"
#include "./core/root_object.h"

namespace Splash
{

/*************/
class WebsocketServer
{
  public:
    /**
     * Constructor
     */
    explicit WebsocketServer(RootObject* root, uint32_t port = 9090)
        : _root(root)
        , _port(port)
    {
    }

    /**
     * Destructor
     */
    ~WebsocketServer();

    /**
     * Returns whether the server is running
     * \return Return true if the server is running
     */
    bool isRunning() const { return _running; }

    /**
     * Send the tree serialized updates to the clients
     * \param treeUpdates Tree updates
     * \return Return true if all went well
     */
    bool sendTreeUpdates(const ResizableArray<uint8_t>& treeUpdates);

    /**
     * Starts the server
     * \return Return true if the server started correctly
     */
    bool start();

  private:
    typedef websocketpp::server<websocketpp::config::asio> server_t;
    typedef server_t::message_ptr message_ptr_t;

    std::future<void> _serverFuture;
    RootObject* _root;
    bool _running{false};
    server_t _server;
    uint32_t _port;

    std::list<websocketpp::connection_hdl> _connections;

    void onOpen(websocketpp::connection_hdl handler);
    void onClose(websocketpp::connection_hdl handler);
    void onMessage(websocketpp::connection_hdl handler, message_ptr_t msg);
};

} // namespace Splash

#endif // SPLASH_WEBSOCKET_SERVER_H
