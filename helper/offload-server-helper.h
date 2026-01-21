/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef OFFLOAD_SERVER_HELPER_H
#define OFFLOAD_SERVER_HELPER_H

#include "ns3/address.h"
#include "ns3/application-helper.h"

namespace ns3
{

/**
 * @ingroup distributed
 * @brief Helper to create and configure OffloadServer applications.
 *
 * This helper simplifies the creation and configuration of OffloadServer
 * applications for distributed computing simulations.
 */
class OffloadServerHelper : public ApplicationHelper
{
  public:
    /**
     * @brief Construct helper with default OffloadServer type.
     */
    OffloadServerHelper();

    /**
     * @brief Construct helper with specified port.
     * @param port Port number to listen on.
     */
    explicit OffloadServerHelper(uint16_t port);

    /**
     * @brief Construct helper with specified local address.
     * @param localAddress Local address to bind to.
     */
    explicit OffloadServerHelper(const Address& localAddress);

    /**
     * @brief Set the port to listen on.
     * @param port Port number.
     */
    void SetPort(uint16_t port);

    /**
     * @brief Set the local address to bind to.
     * @param localAddress Local address (InetSocketAddress or Inet6SocketAddress).
     */
    void SetLocalAddress(const Address& localAddress);
};

} // namespace ns3

#endif // OFFLOAD_SERVER_HELPER_H
