/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef PERIODIC_SERVER_HELPER_H
#define PERIODIC_SERVER_HELPER_H

#include "ns3/application-helper.h"

namespace ns3
{

/**
 * @ingroup distributed
 * @brief Helper to create and configure PeriodicServer applications.
 *
 * This helper simplifies the creation and configuration of PeriodicServer
 * applications for periodic frame processing on backend accelerators.
 */
class PeriodicServerHelper : public ApplicationHelper
{
  public:
    /**
     * @brief Construct helper with default PeriodicServer type.
     */
    PeriodicServerHelper();

    /**
     * @brief Construct helper with specified port.
     * @param port Port number to listen on.
     */
    explicit PeriodicServerHelper(uint16_t port);

    /**
     * @brief Set the port to listen on.
     * @param port Port number.
     */
    void SetPort(uint16_t port);
};

} // namespace ns3

#endif // PERIODIC_SERVER_HELPER_H
