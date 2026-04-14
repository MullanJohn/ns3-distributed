/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef BACKEND_SERVER_HELPER_H
#define BACKEND_SERVER_HELPER_H

#include "ns3/application-helper.h"

namespace ns3
{

/**
 * @ingroup continuum
 * @brief Helper to create and configure BackendServer applications.
 *
 * This helper simplifies the creation and configuration of BackendServer
 * applications for processing offloaded tasks on backend accelerators.
 */
class BackendServerHelper : public ApplicationHelper
{
  public:
    /**
     * @brief Construct helper with default BackendServer type.
     */
    BackendServerHelper();

    /**
     * @brief Construct helper with specified port.
     * @param port Port number to listen on.
     */
    explicit BackendServerHelper(uint16_t port);

    /**
     * @brief Set the port to listen on.
     * @param port Port number.
     */
    void SetPort(uint16_t port);
};

} // namespace ns3

#endif // BACKEND_SERVER_HELPER_H
