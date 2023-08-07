/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2023 NUS
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
 *
 * Authors: Chahwan Song <songch@comp.nus.edu.sg>
 */

#include "ns3/conweave-voq.h"

#include "ns3/assert.h"
#include "ns3/conweave-routing.h"
#include "ns3/ipv4-header.h"
#include "ns3/log.h"
#include "ns3/settings.h"
#include "ns3/simulator.h"

NS_LOG_COMPONENT_DEFINE("ConWeaveVOQ");

namespace ns3 {

ConWeaveVOQ::ConWeaveVOQ() {}
ConWeaveVOQ::~ConWeaveVOQ() {}

std::vector<int> ConWeaveVOQ::m_flushEstErrorhistory; // instantiate static variable

void ConWeaveVOQ::Set(uint64_t flowkey, uint32_t dip, Time timeToFlush, Time extraVOQFlushTime) {
    m_flowkey = flowkey;
    m_dip = dip;
    m_extraVOQFlushTime = extraVOQFlushTime;
    RescheduleFlush(timeToFlush);
}

void ConWeaveVOQ::Enqueue(Ptr<Packet> pkt) { m_FIFO.push(pkt); }

void ConWeaveVOQ::FlushAllImmediately() {
    m_CallbackByVOQFlush(
        m_flowkey,
        (uint32_t)m_FIFO.size()); /** IMPORTANT: set RxEntry._reordering = false at flushing */
    while (!m_FIFO.empty()) {     // for all VOQ pkts
        Ptr<Packet> pkt = m_FIFO.front();  // get packet
        CustomHeader ch(CustomHeader::L2_Header | CustomHeader::L3_Header |
                        CustomHeader::L4_Header);
        pkt->PeekHeader(ch);
        m_switchSendToDevCallback(pkt, ch);  // SlbRouting::DoSwitchSendToDev
        m_FIFO.pop();                        // remove this element
    }
    m_deleteCallback(m_flowkey);  // delete this from SlbRouting::m_voqMap
}

void ConWeaveVOQ::EnforceFlushAll() {
    SLB_LOG(
        "--> *** Finish this epoch by Timeout Enforcement - ConWeaveVOQ Size:" << m_FIFO.size());
    ConWeaveRouting::m_nFlushVOQTotal += 1;  // statistics
    m_checkFlushEvent.Cancel();               // cancel the next schedule
    FlushAllImmediately();                    // flush VOQ immediately
}

/**
 * @brief Reschedule flushing timeout
 * @param timeToFlush relative time to flush from NOW
 */
void ConWeaveVOQ::RescheduleFlush(Time timeToFlush) {
    if (m_checkFlushEvent.IsRunning()) {  // if already exists, reschedule it

        uint64_t prevEst = m_checkFlushEvent.GetTs();
        if (timeToFlush.GetNanoSeconds() == 1) {
            // std::cout << (int(prevEst - Simulator::Now().GetNanoSeconds()) -
            //               m_extraVOQFlushTime.GetNanoSeconds())
            //           << std::endl;
            m_flushEstErrorhistory.push_back(int(prevEst - Simulator::Now().GetNanoSeconds()) -
                                             m_extraVOQFlushTime.GetNanoSeconds());
        }

        m_checkFlushEvent.Cancel();
    }
    m_checkFlushEvent = Simulator::Schedule(timeToFlush, &ConWeaveVOQ::EnforceFlushAll, this);
}

bool ConWeaveVOQ::CheckEmpty() { return m_FIFO.empty(); }

uint32_t ConWeaveVOQ::getQueueSize() { return (uint32_t)m_FIFO.size(); }

}  // namespace ns3