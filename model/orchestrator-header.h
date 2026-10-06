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
 * @ingroup continuum
 * @brief Minimal header for the orchestrator's client-facing admission protocol.
 *
 * A WORKLOAD_REQUEST carries a client's complete DAG (metadata and payload
 * together) in one message; the orchestrator evaluates admission and, once
 * accepted, dispatches immediately rather than waiting on a separate upload.
 * ADMISSION_RESPONSE is a non-blocking acknowledgment of that decision.
 * WORKLOAD_RESPONSE returns the fully executed DAG once the workload
 * completes. See OffloadClient's AdmissionMode attribute (SessionMode vs
 * WorkloadMode) for how often WORKLOAD_REQUEST/ADMISSION_RESPONSE are
 * actually exchanged per client.
 *
 * Wire format (18 bytes):
 * - messageType: 1 byte
 * - dagId: 8 bytes (DAG identifier, echoed in response for correlation)
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
     * so the orchestrator can distinguish these from task data by peeking at
     * the first byte. Values 5 and 6 are reserved by ScalingCommandHeader and
     * DeviceMetricsHeader respectively.
     */
    enum MessageType : uint8_t
    {
        ADMISSION_RESPONSE = 3, //!< Server's non-blocking admit/reject acknowledgment
        WORKLOAD_RESPONSE = 7,  //!< Completed workload returned to client (full DAG follows)
        WORKLOAD_REQUEST = 8    //!< Client's DAG (metadata + payload together), admit-and-run
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
     * @brief Get the DAG ID.
     *
     * For WORKLOAD_REQUEST: identifies the DAG being submitted.
     * For ADMISSION_RESPONSE/WORKLOAD_RESPONSE: echoed back to correlate with the request.
     *
     * @return The DAG ID.
     */
    uint64_t GetDagId() const;

    /**
     * @brief Set the DAG ID.
     * @param id The DAG ID.
     */
    void SetDagId(uint64_t id);

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
     * For WORKLOAD_REQUEST/WORKLOAD_RESPONSE: size of the following serialized
     * DAG bytes. For ADMISSION_RESPONSE: 0 (no payload).
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
     * @brief Check if this is an admission response message.
     * @return true if ADMISSION_RESPONSE.
     */
    bool IsResponse() const;

    /**
     * @brief Check if this is a workload request message.
     * @return true if WORKLOAD_REQUEST.
     */
    bool IsWorkloadRequest() const;

    /**
     * @brief Check if this is a completed-workload response message.
     * @return true if WORKLOAD_RESPONSE.
     */
    bool IsWorkloadResponse() const;

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
    MessageType m_messageType{WORKLOAD_REQUEST}; //!< Message type
    uint64_t m_dagId{0};                         //!< DAG ID for correlation
    bool m_admitted{false};                      //!< Admission status (for response)
    uint64_t m_payloadSize{0};                   //!< Size of following payload bytes
};

} // namespace ns3

#endif // ORCHESTRATOR_HEADER_H
