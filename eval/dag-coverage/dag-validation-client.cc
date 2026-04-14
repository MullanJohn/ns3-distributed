/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "dag-validation-client.h"

#include "ns3/log.h"
#include "ns3/simple-task.h"
#include "ns3/simulator.h"
#include "ns3/tcp-connection-manager.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("DagValidationClient");
NS_OBJECT_ENSURE_REGISTERED(DagValidationClient);

TypeId
DagValidationClient::GetTypeId()
{
    static TypeId tid = TypeId("ns3::DagValidationClient")
                            .SetParent<Application>()
                            .SetGroupName("Distributed")
                            .AddConstructor<DagValidationClient>();
    return tid;
}

DagValidationClient::DagValidationClient()
    : m_connMgr(nullptr),
      m_dagId(1),
      m_submitTime(Seconds(0)),
      m_completeTime(Seconds(0)),
      m_rxBuffer(Create<Packet>()),
      m_admitted(false),
      m_completed(false)
{
}

DagValidationClient::~DagValidationClient() = default;

void
DagValidationClient::DoDispose()
{
    if (m_connMgr)
    {
        m_connMgr->Close();
        m_connMgr = nullptr;
    }
    m_rxBuffer = nullptr;
    m_dag = nullptr;
    Application::DoDispose();
}

void
DagValidationClient::StartApplication()
{
    NS_ABORT_MSG_IF(m_peer.IsInvalid(), "remote address not set");
    NS_ABORT_MSG_IF(!m_dag, "DAG not set");
    NS_ABORT_MSG_IF(m_dag->GetTaskCount() == 0, "DAG is empty");

    m_connMgr = CreateObject<TcpConnectionManager>();
    m_connMgr->SetNode(GetNode());
    m_connMgr->SetReceiveCallback(MakeCallback(&DagValidationClient::HandleReceive, this));

    Ptr<TcpConnectionManager> tcp = DynamicCast<TcpConnectionManager>(m_connMgr);
    tcp->SetConnectionCallback(MakeCallback(&DagValidationClient::HandleConnected, this));
    tcp->SetConnectionFailedCallback(
        MakeCallback(&DagValidationClient::HandleConnectionFailed, this));

    m_connMgr->Connect(m_peer);
}

void
DagValidationClient::StopApplication()
{
    if (m_connMgr)
    {
        m_connMgr->Close();
    }
}

void
DagValidationClient::HandleConnected(const Address& /* serverAddr */)
{
    SendAdmissionRequest();
}

void
DagValidationClient::HandleConnectionFailed(const Address& serverAddr)
{
    NS_LOG_ERROR("failed to connect to " << serverAddr);
}

void
DagValidationClient::SendAdmissionRequest()
{
    Ptr<Packet> metadata = m_dag->SerializeMetadata();

    OrchestratorHeader hdr;
    hdr.SetMessageType(OrchestratorHeader::ADMISSION_REQUEST);
    hdr.SetDagId(m_dagId);
    hdr.SetPayloadSize(metadata->GetSize());

    Ptr<Packet> packet = Create<Packet>();
    packet->AddAtEnd(metadata);
    packet->AddHeader(hdr);

    m_submitTime = Simulator::Now();
    m_connMgr->Send(packet);
}

void
DagValidationClient::SendFullData()
{
    Ptr<Packet> dagData = m_dag->SerializeFullData();

    OrchestratorHeader hdr;
    hdr.SetMessageType(OrchestratorHeader::DATA_UPLOAD);
    hdr.SetDagId(m_dagId);
    hdr.SetPayloadSize(dagData->GetSize());

    Ptr<Packet> packet = Create<Packet>();
    packet->AddAtEnd(dagData);
    packet->AddHeader(hdr);

    m_connMgr->Send(packet);
}

void
DagValidationClient::HandleReceive(Ptr<Packet> packet, const Address& /* from */)
{
    if (packet->GetSize() == 0)
    {
        return;
    }
    m_rxBuffer->AddAtEnd(packet);
    ProcessBuffer();
}

void
DagValidationClient::ProcessBuffer()
{
    while (m_rxBuffer->GetSize() > 0)
    {
        uint32_t sizeBefore = m_rxBuffer->GetSize();
        uint8_t firstByte;
        m_rxBuffer->CopyData(&firstByte, 1);

        if (firstByte >= OrchestratorHeader::ADMISSION_REQUEST)
        {
            if (m_rxBuffer->GetSize() < OrchestratorHeader::SERIALIZED_SIZE)
            {
                break;
            }
            OrchestratorHeader hdr;
            m_rxBuffer->PeekHeader(hdr);
            uint64_t totalSize = OrchestratorHeader::SERIALIZED_SIZE + hdr.GetPayloadSize();
            if (m_rxBuffer->GetSize() < totalSize)
            {
                break;
            }
            m_rxBuffer->RemoveAtStart(totalSize);
            HandleAdmissionResponse(hdr);
        }
        else
        {
            HandleTaskResponse();
        }

        if (m_rxBuffer->GetSize() == sizeBefore)
        {
            break;
        }
    }
}

void
DagValidationClient::HandleAdmissionResponse(const OrchestratorHeader& hdr)
{
    if (hdr.IsAdmitted())
    {
        m_admitted = true;
        SendFullData();
    }
    else
    {
        NS_LOG_ERROR("DAG " << hdr.GetDagId() << " rejected");
    }
}

void
DagValidationClient::HandleTaskResponse()
{
    uint64_t consumedBytes = 0;
    Ptr<Task> task = SimpleTask::Deserialize(m_rxBuffer, consumedBytes);
    if (consumedBytes == 0)
    {
        return;
    }
    m_rxBuffer->RemoveAtStart(consumedBytes);

    m_completeTime = Simulator::Now();
    m_completed = true;
}

} // namespace ns3
