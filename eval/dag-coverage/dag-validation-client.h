/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef DAG_VALIDATION_CLIENT_H
#define DAG_VALIDATION_CLIENT_H

#include "ns3/application.h"
#include "ns3/connection-manager.h"
#include "ns3/dag-task.h"
#include "ns3/nstime.h"
#include "ns3/orchestrator-header.h"
#include "ns3/packet.h"
#include "ns3/ptr.h"

namespace ns3
{

/**
 * @brief One-shot DAG client used by the DAG-structure validation example.
 *
 * Connects to an Orchestrator, submits a single pre-built  and
 * records the submit/complete timestamps on sink-task response.
 */
class DagValidationClient : public Application
{
  public:
    static TypeId GetTypeId();
    DagValidationClient();
    ~DagValidationClient() override;

    void SetRemote(const Address& addr) { m_peer = addr; }
    void SetDag(Ptr<DagTask> dag) { m_dag = dag; }

    bool IsComplete() const { return m_completed; }
    Time GetSubmitTime() const { return m_submitTime; }
    Time GetCompleteTime() const { return m_completeTime; }
    Time GetWorkloadLatency() const { return m_completeTime - m_submitTime; }

  protected:
    void DoDispose() override;

  private:
    void StartApplication() override;
    void StopApplication() override;

    void HandleConnected(const Address& serverAddr);
    void HandleConnectionFailed(const Address& serverAddr);
    void HandleReceive(Ptr<Packet> packet, const Address& from);

    void SendAdmissionRequest();
    void SendFullData();
    void ProcessBuffer();
    void HandleAdmissionResponse(const OrchestratorHeader& orchHeader);
    void HandleTaskResponse();

    Ptr<ConnectionManager> m_connMgr;
    Address m_peer;
    Ptr<DagTask> m_dag;
    uint64_t m_dagId;
    Time m_submitTime;
    Time m_completeTime;
    Ptr<Packet> m_rxBuffer;
    bool m_admitted;
    bool m_completed;
};

} // namespace ns3

#endif // DAG_VALIDATION_CLIENT_H
