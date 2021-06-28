/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file core/udp.h Basic functions to receive and send UDP packets.
 */

#ifndef NETWORK_CORE_UDP_H
#define NETWORK_CORE_UDP_H

#include "address.h"
#include "packet.h"

/** Enum with all types of UDP packets. The order MUST not be changed **/
enum PacketUDPType {
	PACKET_UDP_CLIENT_FIND_SERVER,   ///< Queries a game server for game information
	PACKET_UDP_SERVER_RESPONSE,      ///< Reply of the game server with game information
	PACKET_UDP_CLIENT_DETAIL_INFO,   ///< Queries a game server about details of the game, such as companies
	PACKET_UDP_SERVER_DETAIL_INFO,   ///< Reply of the game server about details of the game, such as companies
	PACKET_UDP_SERVER_REGISTER,      ///< Packet to register itself to the master server
	PACKET_UDP_MASTER_ACK_REGISTER,  ///< Packet indicating registration has succeeded
	PACKET_UDP_CLIENT_GET_LIST,      ///< Request for serverlist from master server
	PACKET_UDP_MASTER_RESPONSE_LIST, ///< Response from master server with server ip's + port's
	PACKET_UDP_SERVER_UNREGISTER,    ///< Request to be removed from the server-list
	PACKET_UDP_CLIENT_GET_NEWGRFS,   ///< Requests the name for a list of GRFs (GRF_ID and MD5)
	PACKET_UDP_SERVER_NEWGRFS,       ///< Sends the list of NewGRF's requested.
	PACKET_UDP_MASTER_SESSION_KEY,   ///< Sends a fresh session key to the client
	PACKET_UDP_END,                  ///< Must ALWAYS be on the end of this list!! (period)
};

/** The types of server lists we can get */
enum ServerListType {
	SLT_IPv4 = 0,   ///< Get the IPv4 addresses
	SLT_IPv6 = 1,   ///< Get the IPv6 addresses
	SLT_AUTODETECT, ///< Autodetect the type based on the connection

	SLT_END = SLT_AUTODETECT, ///< End of 'arrays' marker
};

/** Base socket handler for all UDP sockets */
class NetworkUDPSocketHandler : public NetworkSocketHandler {
protected:
	/** The address to bind to. */
	NetworkAddressList bind;
	/** The opened sockets. */
	SocketList sockets;

	void ReceiveInvalidPacket(PacketUDPType, NetworkAddress *client_addr);

	/**
	 * Queries to the server for information about the game.
	 * @param p           The received packet.
	 * @param client_addr The origin of the packet.
	 */
	virtual void Receive_CLIENT_FIND_SERVER(Packet *p, NetworkAddress *client_addr);

	/**
	 * Return of server information to the client.
	 * Serialized NetworkGameInfo. See game_info.h for details.
	 * @param p           The received packet.
	 * @param client_addr The origin of the packet.
	 */
	virtual void Receive_SERVER_RESPONSE(Packet *p, NetworkAddress *client_addr);

	/**
	 * Query for detailed information about companies.
	 * @param p           The received packet.
	 * @param client_addr The origin of the packet.
	 */
	virtual void Receive_CLIENT_DETAIL_INFO(Packet *p, NetworkAddress *client_addr);

	/**
	 * Reply with detailed company information.
	 * uint8_t   Version of the packet.
	 * uint8_t   Number of companies.
	 * For each company:
	 *   uint8_t   ID of the company.
	 *   string  Name of the company.
	 *   uint32_t  Year the company was inaugurated.
	 *   uint64_t  Value.
	 *   uint64_t  Money.
	 *   uint64_t  Income.
	 *   uint16_t  Performance (last quarter).
	 *   bool    Company is password protected.
	 *   uint16_t  Number of trains.
	 *   uint16_t  Number of lorries.
	 *   uint16_t  Number of busses.
	 *   uint16_t  Number of planes.
	 *   uint16_t  Number of ships.
	 *   uint16_t  Number of train stations.
	 *   uint16_t  Number of lorry stations.
	 *   uint16_t  Number of bus stops.
	 *   uint16_t  Number of airports and heliports.
	 *   uint16_t  Number of harbours.
	 *   bool    Company is an AI.
	 * @param p           The received packet.
	 * @param client_addr The origin of the packet.
	 */
	virtual void Receive_SERVER_DETAIL_INFO(Packet *p, NetworkAddress *client_addr);

	/**
	 * Registers the server to the master server.
	 * string  The "welcome" message to root out other binary packets.
	 * uint8_t   Version of the protocol.
	 * uint16_t  The port to unregister.
	 * uint64_t  The session key.
	 * @param p           The received packet.
	 * @param client_addr The origin of the packet.
	 */
	virtual void Receive_SERVER_REGISTER(Packet *p, NetworkAddress *client_addr);

	/**
	 * The master server acknowledges the registration.
	 * @param p           The received packet.
	 * @param client_addr The origin of the packet.
	 */
	virtual void Receive_MASTER_ACK_REGISTER(Packet *p, NetworkAddress *client_addr);

	/**
	 * The client requests a list of servers.
	 * uint8_t   The protocol version.
	 * uint8_t   The type of server to look for: IPv4, IPv6 or based on the received packet.
	 * @param p           The received packet.
	 * @param client_addr The origin of the packet.
	 */
	virtual void Receive_CLIENT_GET_LIST(Packet *p, NetworkAddress *client_addr);

	/**
	 * The server sends a list of servers.
	 * uint8_t   The protocol version.
	 * For each server:
	 *   4 or 16 bytes of IPv4 or IPv6 address.
	 *   uint8_t   The port.
	 * @param p           The received packet.
	 * @param client_addr The origin of the packet.
	 */
	virtual void Receive_MASTER_RESPONSE_LIST(Packet *p, NetworkAddress *client_addr);

	/**
	 * A server unregisters itself at the master server.
	 * uint8_t   Version of the protocol.
	 * uint16_t  The port to unregister.
	 * @param p           The received packet.
	 * @param client_addr The origin of the packet.
	 */
	virtual void Receive_SERVER_UNREGISTER(Packet *p, NetworkAddress *client_addr);

	/**
	 * The client requests information about some NewGRFs.
	 * uint8_t   The number of NewGRFs information is requested about.
	 * For each NewGRF:
	 *   uint32_t      The GRFID.
	 *   16 * uint8_t  MD5 checksum of the GRF.
	 * @param p           The received packet.
	 * @param client_addr The origin of the packet.
	 */
	virtual void Receive_CLIENT_GET_NEWGRFS(Packet *p, NetworkAddress *client_addr);

	/**
	 * The server returns information about some NewGRFs.
	 * uint8_t   The number of NewGRFs information is requested about.
	 * For each NewGRF:
	 *   uint32_t      The GRFID.
	 *   16 * uint8_t  MD5 checksum of the GRF.
	 *   string      The name of the NewGRF.
	 * @param p           The received packet.
	 * @param client_addr The origin of the packet.
	 */
	virtual void Receive_SERVER_NEWGRFS(Packet *p, NetworkAddress *client_addr);

	/**
	 * The master server sends us a session key.
	 * uint64_t  The session key.
	 * @param p           The received packet.
	 * @param client_addr The origin of the packet.
	 */
	virtual void Receive_MASTER_SESSION_KEY(Packet *p, NetworkAddress *client_addr);

	void HandleUDPPacket(Packet *p, NetworkAddress *client_addr);
public:
	NetworkUDPSocketHandler(NetworkAddressList *bind = nullptr);

	/** On destructing of this class, the socket needs to be closed */
	virtual ~NetworkUDPSocketHandler() { this->CloseSocket(); }

	bool Listen();
	void CloseSocket();

	void SendPacket(Packet *p, NetworkAddress *recv, bool all = false, bool broadcast = false);
	void ReceivePackets();
};

#endif /* NETWORK_CORE_UDP_H */
