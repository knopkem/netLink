/*
    netLink: c++ 11 networking library
    Copyright 2014 Alexander Meißner (lichtso@gamefortec.net)

    This software is provided 'as-is', without any express or implied warranty.
    In no event will the authors be held liable for any damages arising from the use of this software.
    Permission is granted to anyone to use this software for any purpose,
    including commercial applications, and to alter it and redistribute it freely,
    subject to the following restrictions:

    1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
    2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
    3. This notice may not be removed or altered from any source distribution.
*/

#include <iostream>
#include <chrono>
#include <ctime>
#include "../include/netLink.h"
#include "../include/json.hpp"
using json = nlohmann::json;

bool responseReceived = false;


int main(int argc, char** argv) {
    #ifdef WINVER
    netLink::init();
    #endif

    netLink::SocketManager socketManager;

    // Define a callback, fired when a socket receives data
    socketManager.onReceiveMsgPack = [](netLink::SocketManager* manager, std::shared_ptr<netLink::Socket> socket, std::unique_ptr<MsgPack::Element> element) {
        std::stringstream buffer;
        buffer << *element;
        auto j = json::parse(buffer);
        std::string ident = j["Identifier"].get<std::string>();
        if (ident.compare("ServerResponse") == 0) {
            std::string databaseLocation = j["DatabaseLocation"].get<std::string>();
            std::cout << "#" << socket->hostRemote << "#" << databaseLocation << std::endl;
            responseReceived = true;
        }
        else {
            //std::cout << "ignoring invalid message: " << *element << std::endl;
        }
    };

    // Alloc a new socket and insert it into the SocketManager
    std::shared_ptr<netLink::Socket> socket = socketManager.newMsgPackSocket();

    try {
        // Init socket as UDP listening to all incoming adresses (IPv4) on port 3824
        socket->initAsUdpPeer("0.0.0.0", 3824);
    } catch(netLink::Exception exc) {
        (void)exc;
        std::cout << "Address is already in use" << std::endl;
        return 1;
    }

    // Define the destination for the next sent message (depending on the choosen IP version)
    socket->hostRemote = "224.0.0.100";
    socket->portRemote = socket->portLocal;

    // Join the multicast group to receive messages from the given address
    socket->setMulticastGroup(socket->hostRemote, true);

    // Prepare a MsgPack encoded message
    netLink::MsgPackSocket& msgPackSocket = *static_cast<netLink::MsgPackSocket*>(socket.get());
    msgPackSocket << MsgPack__Factory(MapHeader(1));
    msgPackSocket << MsgPack::Factory("Identifier");
    msgPackSocket << MsgPack::Factory("ClientRequest");

    // Let the SocketManager poll from all sockets, events will be triggered here

    auto start = std::chrono::system_clock::now();
    while (!responseReceived) {
        socketManager.listen();
        auto end = std::chrono::system_clock::now();
        std::chrono::duration<double> duration_seconds = (end - start);
        if (duration_seconds.count() > 3) return 1;
    }

    return 0;
}
