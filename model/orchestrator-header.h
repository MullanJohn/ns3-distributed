/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef ORCHESTRATOR_HEADER_H
#define ORCHESTRATOR_HEADER_H

#include "ns3/header.h"

#include <cstdint>
#include <ostream>
#include <string>

namespace ns3
{

/**
 * @ingroup distributed
 * @brief Minimal header for orchestrator admission protocol.
 *
 * OrchestratorHeader implements the admission phase of the two-phase protocol.
 * After admission is granted, normal Task serialization (TaskHeader with
 * TASK_REQUEST/TASK_RESPONSE) is used for execution.
 *
 * Protocol:
 *
 * Phase 1 - Admission:
 * - Client: ADMISSION_REQUEST + TaskHeader (metadata only, no payload)
 * - Server: ADMISSION_RESPONSE (taskId + admit/reject)
 *
 * Phase 2 - Execution (if admitted):
 * - Client: Normal Task.Serialize(false) with same taskId
 * - Server: Normal Task.Serialize(true) response
 *
 * The taskId enables concurrent admission requests - the server echoes
 * it back so the client can correlate responses.
 *
 * Wire format (18 bytes):
 * - messageType: 1 byte
 * - taskId: 8 bytes (from TaskHeader for request, echoed in response)
 * - admitted: 1 byte (for ADMISSION_RESPONSE: 0=rejected, 1=admitted)
 * - payloadSize: 8 bytes (size of following data for header-agnostic parsing)
 */
class OrchestratorHeader : public Header
{
  public:
    /**
     * @brief Protocol message types.
     *
     * Values are chosen to be distinct from TaskHeader message types (0, 1)
     * so the orchestrator can distinguish admission protocol messages from
     * task data uploads by peeking at the first byte.
     */
    enum MessageType : uint8_t
    {
        ADMISSION_REQUEST = 2,      //!< Client requests task admission (TaskHeader follows)
        ADMISSION_RESPONSE = 3,     //!< Server responds to task admission (admit/reject)
        DAG_ADMISSION_REQUEST = 4,  //!< Client requests DAG admission (serialized DAG follows)
        DAG_ADMISSION_RESPONSE = 5  //!< Server responds to DAG admission (admit/reject)
    };

    /**
     * @brief Serialized size of the header in bytes.
     */
    static constexpr uint32_t SERIALIZED_SIZE = 18;

    /**
     * @brief Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    OrchestratorHeader();
    ~OrchestratorHeader() override;

    /**
     * @brief Get the message type.
     * @return The message type.
     */
    MessageType GetMessageType() const;

    /**
     * @brief Set the message type.
     * @param type The message type.
     */
    void SetMessageType(MessageType type);

    /**
     * @brief Get the task ID.
     *
     * For ADMISSION_REQUEST: matches the taskId in the following TaskHeader.
     * For ADMISSION_RESPONSE: echoed back to correlate with the request.
     *
     * @return The task ID.
     */
    uint64_t GetTaskId() const;

    /**
     * @brief Set the task ID.
     * @param id The task ID.
     */
    void SetTaskId(uint64_t id);

    /**
     * @brief Check if task was admitted (for ADMISSION_RESPONSE).
     * @return true if admitted, false if rejected.
     */
    bool IsAdmitted() const;

    /**
     * @brief Set admission status (for ADMISSION_RESPONSE).
     * @param admitted true if task should be admitted.
     */
    void SetAdmitted(bool admitted);

    /**
     * @brief Get the payload size.
     *
     * For ADMISSION_REQUEST: Size of following TaskHeader bytes.
     * For ADMISSION_RESPONSE: 0 (no payload).
     *
     * This enables header-agnostic parsing - the orchestrator reads
     * exactly this many bytes and passes them to the deserializer.
     *
     * @return Size of data following this header in bytes.
     */
    uint64_t GetPayloadSize() const;

    /**
     * @brief Set the payload size.
     * @param size Size of data following this header in bytes.
     */
    void SetPayloadSize(uint64_t size);

    /**
     * @brief Check if this is a request message.
     * @return true if ADMISSION_REQUEST.
     */
    bool IsRequest() const;

    /**
     * @brief Check if this is a response message.
     * @return true if ADMISSION_RESPONSE.
     */
    bool IsResponse() const;

    /**
     * @brief Check if this is a DAG request message.
     * @return true if DAG_ADMISSION_REQUEST.
     */
    bool IsDagRequest() const;

    /**
     * @brief Check if this is a DAG response message.
     * @return true if DAG_ADMISSION_RESPONSE.
     */
    bool IsDagResponse() const;

    /**
     * @brief Get string representation of message type.
     * @return Message type name.
     */
    std::string GetMessageTypeName() const;

    // Header interface
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;

  private:
    MessageType m_messageType{ADMISSION_REQUEST}; //!< Message type
    uint64_t m_taskId{0};                         //!< Task ID for correlation
    bool m_admitted{false};                       //!< Admission status (for response)
    uint64_t m_payloadSize{0};                    //!< Size of following payload bytes
};

} // namespace ns3

#endif // ORCHESTRATOR_HEADER_H
