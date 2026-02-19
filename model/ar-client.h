/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef AR_CLIENT_H
#define AR_CLIENT_H

#include "connection-manager.h"
#include "dag-task.h"
#include "orchestrator-header.h"
#include "task.h"

#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/random-variable-stream.h"
#include "ns3/traced-callback.h"

#include <map>

namespace ns3
{

class Packet;

/**
 * @ingroup distributed
 * @brief AR client that generates image frames for remote processing.
 *
 * ArClient models an augmented reality headset that captures image frames
 * at a fixed frame rate and offloads them to an edge server for processing.
 *
 * Example usage:
 * @code
 * Ptr<ArClient> client = CreateObject<ArClient>();
 * client->SetAttribute("Remote", AddressValue(orchAddress));
 * client->SetAttribute("FrameRate", DoubleValue(30.0));
 *
 * Ptr<NormalRandomVariable> frameSize = CreateObject<NormalRandomVariable>();
 * frameSize->SetAttribute("Mean", DoubleValue(60000));
 * client->SetAttribute("FrameSize", PointerValue(frameSize));
 *
 * Ptr<ConstantRandomVariable> compute = CreateObject<ConstantRandomVariable>();
 * compute->SetAttribute("Constant", DoubleValue(5e9));
 * client->SetAttribute("ComputeDemand", PointerValue(compute));
 *
 * node->AddApplication(client);
 * @endcode
 */
class ArClient : public Application
{
  public:
    /**
     * @brief Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    ArClient();
    ~ArClient() override;

    /**
     * @brief Set the remote orchestrator address.
     * @param addr Orchestrator address (InetSocketAddress or Inet6SocketAddress).
     */
    void SetRemote(const Address& addr);

    /**
     * @brief Get the remote orchestrator address.
     * @return The orchestrator address.
     */
    Address GetRemote() const;

    /**
     * @brief Get the number of frames sent.
     * @return Number of frames submitted for admission.
     */
    uint64_t GetFramesSent() const;

    /**
     * @brief Get the number of frames dropped.
     * @return Number of frames dropped because the previous frame was still pending.
     */
    uint64_t GetFramesDropped() const;

    /**
     * @brief Get the number of responses received.
     * @return Number of processed frame results received.
     */
    uint64_t GetResponsesReceived() const;

    /**
     * @brief Get the total bytes transmitted.
     * @return Total bytes sent.
     */
    uint64_t GetTotalTx() const;

    /**
     * @brief Get the total bytes received.
     * @return Total bytes received.
     */
    uint64_t GetTotalRx() const;

    /**
     * @brief Assign a fixed random variable stream number to the random variables used by this
     * application.
     * @param stream First stream index to use.
     * @return The number of stream indices assigned.
     */
    int64_t AssignStreams(int64_t stream) override;

    /**
     * @brief TracedCallback signature for frame sent events.
     * @param task The task representing the frame.
     */
    typedef void (*FrameSentTracedCallback)(Ptr<const Task> task);

    /**
     * @brief TracedCallback signature for frame processed events.
     * @param task The completed task.
     * @param latency End-to-end latency from submission to response.
     */
    typedef void (*FrameProcessedTracedCallback)(Ptr<const Task> task, Time latency);

    /**
     * @brief TracedCallback signature for frame rejected events.
     * @param task The rejected task.
     */
    typedef void (*FrameRejectedTracedCallback)(Ptr<const Task> task);

    /**
     * @brief TracedCallback signature for frame dropped events.
     * @param frameNumber The sequence number of the dropped frame.
     */
    typedef void (*FrameDroppedTracedCallback)(uint64_t frameNumber);

  protected:
    void DoDispose() override;

  private:
    void StartApplication() override;
    void StopApplication() override;

    void HandleConnected(const Address& serverAddr);
    void HandleConnectionFailed(const Address& serverAddr);
    void HandleReceive(Ptr<Packet> packet, const Address& from);

    /**
     * @brief Generate and submit the next frame.
     */
    void GenerateFrame();

    /**
     * @brief Schedule the next frame generation at fixed interval.
     */
    void ScheduleNextFrame();

    void ProcessBuffer();
    void HandleAdmissionResponse(const OrchestratorHeader& orchHeader);
    void HandleTaskResponse();
    void SendFullData(uint64_t dagId);

    // Transport
    Ptr<ConnectionManager> m_connMgr; //!< Connection manager for transport
    Address m_peer;                   //!< Remote orchestrator address

    // Configuration
    double m_frameRate;                        //!< Frames per second
    Ptr<RandomVariableStream> m_frameSize;     //!< Input image size in bytes
    Ptr<RandomVariableStream> m_computeDemand; //!< FLOPS per frame
    Ptr<RandomVariableStream> m_outputSize;    //!< Result size in bytes

    // State
    static uint32_t s_nextClientId; //!< Counter for assigning unique client IDs
    uint32_t m_clientId;            //!< Unique ID for this client instance
    EventId m_sendEvent;            //!< Next frame event
    uint64_t m_framesSent;          //!< Frames successfully submitted for admission
    uint64_t m_frameCount;          //!< Total frame generation events (sent + dropped)
    uint64_t m_framesDropped;       //!< Frames dropped due to pending workload
    uint64_t m_nextDagId;           //!< Next DAG ID
    uint64_t m_totalTx;             //!< Total bytes transmitted
    uint64_t m_totalRx;             //!< Total bytes received

    // Pending workload state
    struct PendingWorkload
    {
        Ptr<DagTask> dag; //!< The DAG wrapping the frame task
        Time submitTime;  //!< When the admission request was sent
    };

    std::map<uint64_t, PendingWorkload> m_pendingWorkloads; //!< dagId -> pending state

    // Response handling
    Ptr<Packet> m_rxBuffer;       //!< Receive buffer for stream reassembly
    uint64_t m_responsesReceived; //!< Number of responses received

    // Trace sources
    TracedCallback<Ptr<const Task>> m_frameSentTrace;            //!< Frame sent
    TracedCallback<Ptr<const Task>, Time> m_frameProcessedTrace; //!< Frame processed
    TracedCallback<Ptr<const Task>> m_frameRejectedTrace;        //!< Frame rejected
    TracedCallback<uint64_t> m_frameDroppedTrace;                //!< Frame dropped
};

} // namespace ns3

#endif // AR_CLIENT_H
