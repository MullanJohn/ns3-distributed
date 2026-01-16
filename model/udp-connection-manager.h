/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef UDP_CONNECTION_MANAGER_H
#define UDP_CONNECTION_MANAGER_H

#include "connection-manager.h"

#include "ns3/socket.h"

namespace ns3
{

/**
 * @ingroup distributed
 * @brief UDP implementation of ConnectionManager.
 *
 * UdpConnectionManager provides unreliable, unordered datagram delivery
 * using UDP sockets. It is simpler than TCP as there are no connections
 * to manage - datagrams can be sent to any address at any time.
 *
 * ## Client Mode
 *
 * Call Connect() to set a default destination address. Subsequent calls
 * to Send(packet) will send to that address. You can still use
 * Send(packet, address) to send to other addresses.
 *
 * ## Server Mode
 *
 * Call Bind() to start receiving datagrams. The ReceiveCallback provides
 * both the packet and the sender's address, allowing you to respond.
 */
class UdpConnectionManager : public ConnectionManager
{
  public:
    /**
     * @brief Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    /**
     * @brief Default constructor.
     */
    UdpConnectionManager();

    /**
     * @brief Destructor.
     */
    ~UdpConnectionManager() override;

    // ConnectionManager interface implementation
    void SetNode(Ptr<Node> node) override;
    Ptr<Node> GetNode() const override;
    void Bind(uint16_t port) override;
    void Bind(const Address& local) override;
    void Connect(const Address& remote) override;
    void Send(Ptr<Packet> packet) override;
    void Send(Ptr<Packet> packet, const Address& to) override;
    void SetReceiveCallback(ReceiveCallback callback) override;
    void Close() override;
    void Close(const Address& peer) override;
    std::string GetName() const override;
    bool IsReliable() const override;
    bool IsConnected() const override;

  protected:
    void DoDispose() override;

  private:
    void HandleRead(Ptr<Socket> socket);

    Ptr<Node> m_node;
    Ptr<Socket> m_socket;
    Address m_defaultDestination;
    bool m_hasDefaultDestination;

    ReceiveCallback m_receiveCallback;
};

} // namespace ns3

#endif // UDP_CONNECTION_MANAGER_H
