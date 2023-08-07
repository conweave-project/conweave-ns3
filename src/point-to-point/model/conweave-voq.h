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

#ifndef __CONWEAVE_VOQ_H__
#define __CONWEAVE_VOQ_H__

#include <map>
#include <queue>
#include <unordered_map>
#include <vector>

#include "ns3/address.h"
#include "ns3/callback.h"
#include "ns3/custom-header.h"
#include "ns3/event-id.h"
#include "ns3/log.h"
#include "ns3/net-device.h"
#include "ns3/object.h"
#include "ns3/packet.h"
#include "ns3/ptr.h"
#include "ns3/settings.h"
#include "ns3/simulator.h"

namespace ns3 {

/**
 * @brief Virtual Output Queue, implemented in FIFO
 * One additional feature - Timer for flushing and destroying by itself.
 */
class ConWeaveVOQ {
    friend class ConWeaveRouting;

   public:
    ConWeaveVOQ();
    ~ConWeaveVOQ();

    // functions
    void Set(uint64_t flowkey, uint32_t dip, Time timeToFlush, Time extraVOQFlushTime);  // setup
    void Enqueue(Ptr<Packet> pkt);           // enqueue pkt FIFO
    void FlushAllImmediately();              // flush all immediately (for scheduling)
    void EnforceFlushAll();                  // enforce to flush the queue by timeout (makes OoO)
    void RescheduleFlush(Time timeToFlush);  // reschedule timeout to flush
    bool CheckEmpty();                       // check empty
    uint32_t getQueueSize();                 // get queue size
    uint32_t getDIP() { return m_dip; };

    // logging
    static std::vector<int> m_flushEstErrorhistory;

   private:
    uint64_t m_flowkey;               // flowkey (voqMap's key)
    uint32_t m_dip;                   // destination ip (for monitoring)
    std::queue<Ptr<Packet> > m_FIFO;  // per-flow FIFO queue
    EventId m_checkFlushEvent;  // check flush schedule is on-going (will be false once the queue
                                // starts flushing)
    Time m_extraVOQFlushTime; // extra flush time (for network uncertainty) -- for debugging

    // callback
    Callback<void, uint64_t> m_deleteCallback;  // bound to SlbRouting::DeleteVoQ
    Callback<void, uint64_t, uint32_t>
        m_CallbackByVOQFlush;  // bound to SlbRouting::CallbackByVOQFlush
    Callback<void, Ptr<Packet>, CustomHeader&>
        m_switchSendToDevCallback;  // bound to SlbRouting::DoSwitchSendToDev

};

}  // namespace ns3

#endif