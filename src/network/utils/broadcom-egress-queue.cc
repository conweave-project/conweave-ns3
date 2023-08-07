/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2006 Georgia Tech Research Corporation, INRIA
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include "broadcom-egress-queue.h"

#include <stdio.h>

#include <iostream>
#include <unordered_map>

#include "drop-tail-queue.h"
#include "ns3/double.h"
#include "ns3/enum.h"
#include "ns3/flow-id-num-tag.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/uinteger.h"

#define MAP_KEY_EXISTS(map, key) (((map).find(key) != (map).end()))

NS_LOG_COMPONENT_DEFINE("BEgressQueue");

namespace ns3 {

std::unordered_map<unsigned, Time> acc_pause_time;  // global

NS_OBJECT_ENSURE_REGISTERED(BEgressQueue);

TypeId BEgressQueue::GetTypeId(void) {
    static TypeId tid = TypeId("ns3::BEgressQueue")
                            .SetParent<Queue>()
                            .AddConstructor<BEgressQueue>()
                            .AddAttribute("MaxBytes",
                                          "The maximum number of bytes accepted by this BEgressQueue.",
                                          DoubleValue(1000.0 * 1024 * 1024),
                                          MakeDoubleAccessor(&BEgressQueue::m_maxBytes),
                                          MakeDoubleChecker<double>())
                            .AddTraceSource("BeqEnqueue", "Enqueue a packet in the BEgressQueue. Multiple queue",
                                            MakeTraceSourceAccessor(&BEgressQueue::m_traceBeqEnqueue))
                            .AddTraceSource("BeqDequeue", "Dequeue a packet in the BEgressQueue. Multiple queue",
                                            MakeTraceSourceAccessor(&BEgressQueue::m_traceBeqDequeue));

    return tid;
}

BEgressQueue::BEgressQueue() : Queue() {
    NS_LOG_FUNCTION_NOARGS();
    m_bytesInQueueTotal = 0;
    m_rrlast = 0;
    for (uint32_t i = 0; i < fCnt; i++) {
        m_bytesInQueue[i] = 0;
        m_queues.push_back(CreateObject<DropTailQueue>());
    }
}

BEgressQueue::~BEgressQueue() {
    NS_LOG_FUNCTION_NOARGS();
}

bool BEgressQueue::DoEnqueue(Ptr<Packet> p, uint32_t qIndex) {
    NS_LOG_FUNCTION(this << p);

    if (m_bytesInQueueTotal + p->GetSize() < m_maxBytes)  // infinite queue
    {
        m_queues[qIndex]->Enqueue(p);
        m_bytesInQueueTotal += p->GetSize();
        m_bytesInQueue[qIndex] += p->GetSize();
    } else {
        std::cout << "Warning: BEgressQueue::DoEnqueue failes to enqueue and drop a packet" << std::endl;
        return false;
    }
    return true;
}

Ptr<Packet>
BEgressQueue::DoDequeueRR(bool paused[])  // this is for switch only
{
    NS_LOG_FUNCTION(this);

    if (m_bytesInQueueTotal == 0) {
        NS_LOG_LOGIC("Queue empty");
        return 0;
    }
    bool found = false;
    uint32_t qIndex;

    if (m_queues[0]->GetNPackets() > 0)  // 0 is the highest priority
    {
        found = true;
        qIndex = 0;
    } else {
        if (!found) {
            for (qIndex = 1; qIndex <= qCnt; qIndex++) {
                bool cond1 = !paused[(qIndex + m_rrlast) % qCnt];
                bool cond2 = m_queues[(qIndex + m_rrlast) % qCnt]->GetNPackets() > 0;  // round robin

                if (!cond1 && cond2) {
                    // Packet could be scheduled by RR, but could not be scheduled because of PAUSE
                    FlowIDNUMTag fit;
                    Ptr<Packet> p = ConstCast<Packet, const Packet>(m_queues[(qIndex + m_rrlast) % qCnt]->Peek());
                    if (p->PeekPacketTag(fit)) {
                        unsigned flowid = static_cast<unsigned>(fit.GetId());
                        if (!MAP_KEY_EXISTS(current_pause_time, flowid))
                            current_pause_time[flowid] = Simulator::Now();
                    }
                } else if (cond1 && cond2) {
                    found = true;
                    break;
                }
            }
            qIndex = (qIndex + m_rrlast) % qCnt;
        }
    }
    if (found) {
        Ptr<Packet> p = m_queues[qIndex]->Dequeue();

        // Check if the flow has been blocked by PFC
        if (p) {
            FlowIDNUMTag fit;
            if (p->PeekPacketTag(fit)) {
                unsigned flowid = static_cast<unsigned>(fit.GetId());
                if (MAP_KEY_EXISTS(current_pause_time, flowid)) {
                    Time tdiff = Simulator::Now() - current_pause_time[flowid];
                    if (!MAP_KEY_EXISTS(acc_pause_time, flowid))
                        acc_pause_time[flowid] = Seconds(0);
                    acc_pause_time[flowid] = acc_pause_time[flowid] + tdiff;
                    current_pause_time.erase(flowid);
                }
            }
        }

        m_traceBeqDequeue(p, qIndex);
        m_bytesInQueueTotal -= p->GetSize();
        m_bytesInQueue[qIndex] -= p->GetSize();
        if (qIndex != 0) {
            m_rrlast = qIndex;
        }
        m_qlast = qIndex;
        NS_LOG_LOGIC("Popped " << p);
        NS_LOG_LOGIC("Number bytes " << m_bytesInQueueTotal);
        return p;
    }
    NS_LOG_LOGIC("Nothing can be sent");
    return 0;
}

bool BEgressQueue::Enqueue(Ptr<Packet> p, uint32_t qIndex) {
    NS_LOG_FUNCTION(this << p);
    //
    // If DoEnqueue fails, Queue::Drop is called by the subclass
    //
    bool retval = DoEnqueue(p, qIndex);
    if (retval) {
        NS_LOG_LOGIC("m_traceEnqueue (p)");
        m_traceEnqueue(p);
        m_traceBeqEnqueue(p, qIndex);

        uint32_t size = p->GetSize();
        m_nBytes += size;
        m_nTotalReceivedBytes += size;

        m_nPackets++;
        m_nTotalReceivedPackets++;
    }
    return retval;
}

Ptr<Packet>
BEgressQueue::DequeueRR(bool paused[]) {
    NS_LOG_FUNCTION(this);
    Ptr<Packet> packet = DoDequeueRR(paused);
    if (packet != 0) {
        NS_ASSERT(m_nBytes >= packet->GetSize());
        NS_ASSERT(m_nPackets > 0);
        m_nBytes -= packet->GetSize();
        m_nPackets--;
        NS_LOG_LOGIC("m_traceDequeue (packet)");
        m_traceDequeue(packet);
    }
    return packet;
}

bool BEgressQueue::DoEnqueue(Ptr<Packet> p)  // for compatiability
{
    std::cout << "Warning: Call Broadcom queues without priority\n";
    uint32_t qIndex = 0;
    NS_LOG_FUNCTION(this << p);
    if (m_bytesInQueueTotal + p->GetSize() < m_maxBytes) {
        m_queues[qIndex]->Enqueue(p);
        m_bytesInQueueTotal += p->GetSize();
        m_bytesInQueue[qIndex] += p->GetSize();
    } else {
        return false;
    }
    return true;
}

Ptr<Packet>
BEgressQueue::DoDequeue(void) {
    NS_ASSERT_MSG(false, "BEgressQueue::DoDequeue not implemented");
    return 0;
}

Ptr<const Packet>
BEgressQueue::DoPeek(void) const  // DoPeek doesn't work for multiple queues!!
{
    std::cout << "Warning: Call Broadcom queues without priority\n";
    NS_LOG_FUNCTION(this);
    if (m_bytesInQueueTotal == 0) {
        NS_LOG_LOGIC("Queue empty");
        return 0;
    }
    NS_LOG_LOGIC("Number bytes " << m_bytesInQueue);
    return m_queues[0]->Peek();
}

uint32_t
BEgressQueue::GetNBytes(uint32_t qIndex) const {
    return m_bytesInQueue[qIndex];
}

uint32_t
BEgressQueue::GetNBytesTotal() const {
    return m_bytesInQueueTotal;
}

uint32_t
BEgressQueue::GetLastQueue() {
    return m_qlast;
}

}  // namespace ns3
