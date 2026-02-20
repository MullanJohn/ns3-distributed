/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef AR_SERVER_HELPER_H
#define AR_SERVER_HELPER_H

#include "ns3/application-helper.h"

namespace ns3
{

/**
 * @ingroup distributed
 * @brief Helper to create and configure ArServer applications.
 *
 * This helper simplifies the creation and configuration of ArServer
 * applications for AR frame processing on backend accelerators.
 */
class ArServerHelper : public ApplicationHelper
{
  public:
    /**
     * @brief Construct helper with default ArServer type.
     */
    ArServerHelper();

    /**
     * @brief Construct helper with specified port.
     * @param port Port number to listen on.
     */
    explicit ArServerHelper(uint16_t port);

    /**
     * @brief Set the port to listen on.
     * @param port Port number.
     */
    void SetPort(uint16_t port);
};

} // namespace ns3

#endif // AR_SERVER_HELPER_H
