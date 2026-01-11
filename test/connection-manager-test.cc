/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/packet.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/simulator.h"
#include "ns3/string.h"
#include "ns3/tcp-connection-manager.h"
#include "ns3/test.h"
#include "ns3/udp-connection-manager.h"
#include "ns3/uinteger.h"

namespace ns3
{
namespace
{

/**
 * @ingroup distributed-tests
 * @brief Test TcpConnectionManager basic client-server communication
 */
class TcpConnectionManagerBasicTestCase : public TestCase
{
  public:
    TcpConnectionManagerBasicTestCase()
        : TestCase("Test TcpConnectionManager basic client-server communication"),
          m_serverReceived(0),
          m_clientReceived(0),
          m_port(9000)
    {
    }

  private:
    void DoRun() override
    {
        // Create 2 nodes: server and client
        NodeContainer nodes;
        nodes.Create(2);
        Ptr<Node> serverNode = nodes.Get(0);
        Ptr<Node> clientNode = nodes.Get(1);

        // Create point-to-point link
        PointToPointHelper p2p;
        p2p.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
        p2p.SetChannelAttribute("Delay", StringValue("1ms"));
        NetDeviceContainer devices = p2p.Install(nodes);

        // Install internet stack
        InternetStackHelper internet;
        internet.Install(nodes);

        // Assign IP addresses
        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

        m_serverAddr = InetSocketAddress(interfaces.GetAddress(0), m_port);

        // Create server connection manager
        m_serverConn = CreateObject<TcpConnectionManager>();
        m_serverConn->SetNode(serverNode);
        m_serverConn->SetReceiveCallback(
            MakeCallback(&TcpConnectionManagerBasicTestCase::ServerReceive, this));
        m_serverConn->SetConnectionCallback(
            MakeCallback(&TcpConnectionManagerBasicTestCase::ServerNewConnection, this));

        // Create client connection manager
        m_clientConn = CreateObject<TcpConnectionManager>();
        m_clientConn->SetNode(clientNode);
        m_clientConn->SetReceiveCallback(
            MakeCallback(&TcpConnectionManagerBasicTestCase::ClientReceive, this));

        // Schedule operations during simulation
        Simulator::Schedule(Seconds(0.0),
                            &TcpConnectionManagerBasicTestCase::ServerBind,
                            this);
        Simulator::Schedule(Seconds(0.1),
                            &TcpConnectionManagerBasicTestCase::ClientConnect,
                            this);
        Simulator::Schedule(Seconds(0.3),
                            &TcpConnectionManagerBasicTestCase::ClientSend,
                            this);

        // Run simulation
        Simulator::Stop(Seconds(1.0));
        Simulator::Run();

        // Cleanup
        m_serverConn->Close();
        m_clientConn->Close();

        Simulator::Destroy();

        // Verify communication occurred
        NS_TEST_ASSERT_MSG_EQ(m_serverReceived, 1, "Server should have received 1 packet");
        NS_TEST_ASSERT_MSG_EQ(m_clientReceived, 1, "Client should have received 1 response");
    }

    void ServerBind()
    {
        m_serverConn->Bind(m_port);
    }

    void ClientConnect()
    {
        m_clientConn->Connect(m_serverAddr);
    }

    void ServerReceive(Ptr<Packet> packet, const Address& from)
    {
        m_serverReceived++;
        // Send response back
        Ptr<Packet> response = Create<Packet>(50);
        m_serverConn->Send(response, from);
    }

    void ServerNewConnection(const Address& peer)
    {
        // Connection accepted
    }

    void ClientReceive(Ptr<Packet> packet, const Address& from)
    {
        m_clientReceived++;
    }

    void ClientSend()
    {
        Ptr<Packet> packet = Create<Packet>(100);
        m_clientConn->Send(packet);
    }

    uint32_t m_serverReceived;
    uint32_t m_clientReceived;
    uint16_t m_port;
    Address m_serverAddr;
    Ptr<TcpConnectionManager> m_serverConn;
    Ptr<TcpConnectionManager> m_clientConn;
};

/**
 * @ingroup distributed-tests
 * @brief Test TcpConnectionManager connection pooling
 */
class TcpConnectionManagerPoolingTestCase : public TestCase
{
  public:
    TcpConnectionManagerPoolingTestCase()
        : TestCase("Test TcpConnectionManager connection pooling"),
          m_port(9000)
    {
    }

  private:
    void DoRun() override
    {
        // Create 2 nodes
        NodeContainer nodes;
        nodes.Create(2);
        Ptr<Node> serverNode = nodes.Get(0);
        Ptr<Node> clientNode = nodes.Get(1);

        // Create point-to-point link
        PointToPointHelper p2p;
        p2p.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
        p2p.SetChannelAttribute("Delay", StringValue("1ms"));
        NetDeviceContainer devices = p2p.Install(nodes);

        // Install internet stack
        InternetStackHelper internet;
        internet.Install(nodes);

        // Assign IP addresses
        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

        m_serverAddr = InetSocketAddress(interfaces.GetAddress(0), m_port);

        // Create server
        m_serverConn = CreateObject<TcpConnectionManager>();
        m_serverConn->SetNode(serverNode);
        m_serverConn->SetReceiveCallback(
            MakeCallback(&TcpConnectionManagerPoolingTestCase::ServerReceive, this));

        // Create client with pool size 3
        m_clientConn = CreateObject<TcpConnectionManager>();
        m_clientConn->SetAttribute("PoolSize", UintegerValue(3));
        m_clientConn->SetNode(clientNode);

        // Schedule operations
        Simulator::Schedule(Seconds(0.0),
                            &TcpConnectionManagerPoolingTestCase::ServerBind,
                            this);
        Simulator::Schedule(Seconds(0.1),
                            &TcpConnectionManagerPoolingTestCase::ClientConnect,
                            this);
        Simulator::Schedule(Seconds(0.5),
                            &TcpConnectionManagerPoolingTestCase::CheckPool,
                            this);

        Simulator::Stop(Seconds(1.0));
        Simulator::Run();

        m_serverConn->Close();
        m_clientConn->Close();

        Simulator::Destroy();
    }

    void ServerBind()
    {
        m_serverConn->Bind(m_port);
    }

    void ClientConnect()
    {
        m_clientConn->Connect(m_serverAddr);
    }

    void ServerReceive(Ptr<Packet> packet, const Address& from)
    {
    }

    void CheckPool()
    {
        NS_TEST_ASSERT_MSG_EQ(m_clientConn->GetConnectionCount(),
                              3,
                              "Should have 3 connections in pool");
        NS_TEST_ASSERT_MSG_EQ(m_clientConn->GetIdleConnectionCount(),
                              3,
                              "All 3 should be idle initially");

        // Acquire a connection
        auto connId = m_clientConn->AcquireConnection();
        NS_TEST_ASSERT_MSG_NE(connId,
                              TcpConnectionManager::INVALID_CONNECTION,
                              "Should get valid connection");
        NS_TEST_ASSERT_MSG_EQ(m_clientConn->GetIdleConnectionCount(),
                              2,
                              "2 should be idle after acquire");
        NS_TEST_ASSERT_MSG_EQ(m_clientConn->GetActiveConnectionCount(),
                              1,
                              "1 should be active");

        // Release
        m_clientConn->ReleaseConnection(connId);
        NS_TEST_ASSERT_MSG_EQ(m_clientConn->GetIdleConnectionCount(),
                              3,
                              "All 3 should be idle after release");
    }

    uint16_t m_port;
    Address m_serverAddr;
    Ptr<TcpConnectionManager> m_serverConn;
    Ptr<TcpConnectionManager> m_clientConn;
};

/**
 * @ingroup distributed-tests
 * @brief Test UdpConnectionManager basic communication
 */
class UdpConnectionManagerBasicTestCase : public TestCase
{
  public:
    UdpConnectionManagerBasicTestCase()
        : TestCase("Test UdpConnectionManager basic communication"),
          m_serverReceived(0),
          m_clientReceived(0),
          m_port(9000)
    {
    }

  private:
    void DoRun() override
    {
        // Create 2 nodes
        NodeContainer nodes;
        nodes.Create(2);
        Ptr<Node> serverNode = nodes.Get(0);
        Ptr<Node> clientNode = nodes.Get(1);

        // Create point-to-point link
        PointToPointHelper p2p;
        p2p.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
        p2p.SetChannelAttribute("Delay", StringValue("1ms"));
        NetDeviceContainer devices = p2p.Install(nodes);

        // Install internet stack
        InternetStackHelper internet;
        internet.Install(nodes);

        // Assign IP addresses
        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

        m_serverAddr = InetSocketAddress(interfaces.GetAddress(0), m_port);

        // Create server
        m_serverConn = CreateObject<UdpConnectionManager>();
        m_serverConn->SetNode(serverNode);
        m_serverConn->SetReceiveCallback(
            MakeCallback(&UdpConnectionManagerBasicTestCase::ServerReceive, this));

        // Create client
        m_clientConn = CreateObject<UdpConnectionManager>();
        m_clientConn->SetNode(clientNode);
        m_clientConn->SetReceiveCallback(
            MakeCallback(&UdpConnectionManagerBasicTestCase::ClientReceive, this));

        // Schedule operations
        Simulator::Schedule(Seconds(0.0),
                            &UdpConnectionManagerBasicTestCase::ServerBind,
                            this);
        Simulator::Schedule(Seconds(0.1),
                            &UdpConnectionManagerBasicTestCase::ClientConnect,
                            this);
        Simulator::Schedule(Seconds(0.2),
                            &UdpConnectionManagerBasicTestCase::ClientSend,
                            this);

        Simulator::Stop(Seconds(1.0));
        Simulator::Run();

        m_serverConn->Close();
        m_clientConn->Close();

        Simulator::Destroy();

        NS_TEST_ASSERT_MSG_EQ(m_serverReceived, 1, "Server should have received 1 datagram");
        NS_TEST_ASSERT_MSG_EQ(m_clientReceived, 1, "Client should have received 1 response");
    }

    void ServerBind()
    {
        m_serverConn->Bind(m_port);
    }

    void ClientConnect()
    {
        m_clientConn->Connect(m_serverAddr);
    }

    void ServerReceive(Ptr<Packet> packet, const Address& from)
    {
        m_serverReceived++;
        // Send response
        Ptr<Packet> response = Create<Packet>(50);
        m_serverConn->Send(response, from);
    }

    void ClientReceive(Ptr<Packet> packet, const Address& from)
    {
        m_clientReceived++;
    }

    void ClientSend()
    {
        Ptr<Packet> packet = Create<Packet>(100);
        m_clientConn->Send(packet);
    }

    uint32_t m_serverReceived;
    uint32_t m_clientReceived;
    uint16_t m_port;
    Address m_serverAddr;
    Ptr<UdpConnectionManager> m_serverConn;
    Ptr<UdpConnectionManager> m_clientConn;
};

/**
 * @ingroup distributed-tests
 * @brief Test ConnectionManager properties
 */
class ConnectionManagerPropertiesTestCase : public TestCase
{
  public:
    ConnectionManagerPropertiesTestCase()
        : TestCase("Test ConnectionManager properties")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<TcpConnectionManager> tcp = CreateObject<TcpConnectionManager>();
        Ptr<UdpConnectionManager> udp = CreateObject<UdpConnectionManager>();

        NS_TEST_ASSERT_MSG_EQ(tcp->GetName(), "TCP", "TCP name should be 'TCP'");
        NS_TEST_ASSERT_MSG_EQ(udp->GetName(), "UDP", "UDP name should be 'UDP'");

        NS_TEST_ASSERT_MSG_EQ(tcp->IsReliable(), true, "TCP should be reliable");
        NS_TEST_ASSERT_MSG_EQ(udp->IsReliable(), false, "UDP should not be reliable");

        Simulator::Destroy();
    }
};

/**
 * @ingroup distributed-tests
 * @brief Test TcpConnectionManager with IPv6 addresses
 */
class TcpConnectionManagerIpv6TestCase : public TestCase
{
  public:
    TcpConnectionManagerIpv6TestCase()
        : TestCase("Test TcpConnectionManager with IPv6"),
          m_serverReceived(0),
          m_clientReceived(0),
          m_port(9000)
    {
    }

  private:
    void DoRun() override
    {
        // Create 2 nodes: server and client
        NodeContainer nodes;
        nodes.Create(2);
        Ptr<Node> serverNode = nodes.Get(0);
        Ptr<Node> clientNode = nodes.Get(1);

        // Create point-to-point link
        PointToPointHelper p2p;
        p2p.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
        p2p.SetChannelAttribute("Delay", StringValue("1ms"));
        NetDeviceContainer devices = p2p.Install(nodes);

        // Install internet stack (includes IPv6)
        InternetStackHelper internet;
        internet.Install(nodes);

        // Assign IPv6 addresses
        Ipv6AddressHelper ipv6;
        ipv6.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
        Ipv6InterfaceContainer interfaces = ipv6.Assign(devices);

        // Get the global address (index 1, as index 0 is link-local)
        Ipv6Address serverIpv6 = interfaces.GetAddress(0, 1);
        m_serverAddr = Inet6SocketAddress(serverIpv6, m_port);
        m_serverBindAddr = Inet6SocketAddress(Ipv6Address::GetAny(), m_port);

        // Create server connection manager
        m_serverConn = CreateObject<TcpConnectionManager>();
        m_serverConn->SetNode(serverNode);
        m_serverConn->SetReceiveCallback(
            MakeCallback(&TcpConnectionManagerIpv6TestCase::ServerReceive, this));

        // Create client connection manager
        m_clientConn = CreateObject<TcpConnectionManager>();
        m_clientConn->SetNode(clientNode);
        m_clientConn->SetReceiveCallback(
            MakeCallback(&TcpConnectionManagerIpv6TestCase::ClientReceive, this));

        // Schedule operations during simulation
        Simulator::Schedule(Seconds(0.0), &TcpConnectionManagerIpv6TestCase::ServerBind, this);
        Simulator::Schedule(Seconds(0.1), &TcpConnectionManagerIpv6TestCase::ClientConnect, this);
        Simulator::Schedule(Seconds(0.3), &TcpConnectionManagerIpv6TestCase::ClientSend, this);

        // Run simulation
        Simulator::Stop(Seconds(1.0));
        Simulator::Run();

        // Cleanup
        m_serverConn->Close();
        m_clientConn->Close();

        Simulator::Destroy();

        // Verify communication occurred
        NS_TEST_ASSERT_MSG_EQ(m_serverReceived, 1, "Server should have received 1 packet");
        NS_TEST_ASSERT_MSG_EQ(m_clientReceived, 1, "Client should have received 1 response");
    }

    void ServerBind()
    {
        m_serverConn->Bind(m_serverBindAddr);
    }

    void ClientConnect()
    {
        m_clientConn->Connect(m_serverAddr);
    }

    void ServerReceive(Ptr<Packet> packet, const Address& from)
    {
        m_serverReceived++;
        // Send response back
        Ptr<Packet> response = Create<Packet>(50);
        m_serverConn->Send(response, from);
    }

    void ClientReceive(Ptr<Packet> packet, const Address& from)
    {
        m_clientReceived++;
    }

    void ClientSend()
    {
        Ptr<Packet> packet = Create<Packet>(100);
        m_clientConn->Send(packet);
    }

    uint32_t m_serverReceived;
    uint32_t m_clientReceived;
    uint16_t m_port;
    Address m_serverAddr;
    Address m_serverBindAddr;
    Ptr<TcpConnectionManager> m_serverConn;
    Ptr<TcpConnectionManager> m_clientConn;
};

} // anonymous namespace

TestCase*
CreateTcpConnectionManagerBasicTestCase()
{
    return new TcpConnectionManagerBasicTestCase;
}

TestCase*
CreateTcpConnectionManagerPoolingTestCase()
{
    return new TcpConnectionManagerPoolingTestCase;
}

TestCase*
CreateUdpConnectionManagerBasicTestCase()
{
    return new UdpConnectionManagerBasicTestCase;
}

TestCase*
CreateConnectionManagerPropertiesTestCase()
{
    return new ConnectionManagerPropertiesTestCase;
}

TestCase*
CreateTcpConnectionManagerIpv6TestCase()
{
    return new TcpConnectionManagerIpv6TestCase;
}

} // namespace ns3
