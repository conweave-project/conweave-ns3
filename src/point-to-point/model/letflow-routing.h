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


#pragma once

#include <arpa/inet.h>

#include <map>
#include <queue>
#include <unordered_map>
#include <vector>

#include "ns3/address.h"
#include "ns3/callback.h"
#include "ns3/event-id.h"
#include "ns3/net-device.h"
#include "ns3/object.h"
#include "ns3/packet.h"
#include "ns3/ptr.h"
#include "ns3/settings.h"
#include "ns3/simulator.h"
#include "ns3/tag.h"

namespace ns3 {

const uint32_t LETFLOW_NULL = UINT32_MAX;

class LetflowTag : public Tag {
   public:
    LetflowTag();
    ~LetflowTag();
    static TypeId GetTypeId(void);
    void SetPathId(uint32_t pathId);
    uint32_t GetPathId(void) const;
    void SetHopCount(uint32_t hopCount);
    uint32_t GetHopCount(void) const;
    virtual TypeId GetInstanceTypeId(void) const;
    virtual uint32_t GetSerializedSize(void) const;
    virtual void Serialize(TagBuffer i) const;
    virtual void Deserialize(TagBuffer i);
    virtual void Print(std::ostream& os) const;

   private:
    uint32_t m_pathId;    // forward
    uint32_t m_hopCount;  // hopCount to get outPort
};

/**
 * @brief Conga object is created for each ToR Switch
 */
class LetflowRouting : public Object {
    friend class SwitchMmu;
    friend class SwitchNode;

   public:
    LetflowRouting();

    /** path <-> outPort **/
    // uint8_t pathp[4] = {uint8_t[port0], uint8_t[port1], uint8_t[port2], uint8_t[port3]}
    // uint32_t path = *((uint32_t*) pathp);
    // (uint8_t*)&path[0] -> port0

    /* static */
    static TypeId GetTypeId(void);
    static uint64_t GetQpKey(uint32_t dip, uint16_t sport, uint16_t dport, uint16_t pg);              // same as in rdma_hw.cc
    static uint32_t GetOutPortFromPath(const uint32_t& path, const uint32_t& hopCount);               // decode outPort from path, given a hop's order
    static void SetOutPortToPath(uint32_t& path, const uint32_t& hopCount, const uint32_t& outPort);  // encode outPort to path
    static uint32_t nFlowletTimeout;                                                                  // number of flowlet's timeout

    /* main function */
    uint32_t RouteInput(Ptr<Packet> p, CustomHeader ch);
    uint32_t GetRandomPath(uint32_t dstTorId);
    virtual void DoDispose();

    /* SET functions */
    void SetConstants(Time agingTime, Time flowletTimeout);
    void SetSwitchInfo(bool isToR, uint32_t switch_id);

    // periodic events for flowlet timeout
    EventId m_agingEvent;
    void AgingEvent();

    // topological info (should be initialized in the beginning)
    std::map<uint32_t, std::set<uint32_t> > m_letflowRoutingTable;  // routing table (ToRId -> pathId) (stable)

    /*-----------*/

   private:
    // topology parameters
    bool m_isToR;          // is ToR (leaf)
    uint32_t m_switch_id;  // switch's nodeID

    // conga constants
    Time m_agingTime;       // expiry of flowlet entry
    Time m_flowletTimeout;  // flowlet timeout (e.g., 100us)

    // local
    std::map<uint64_t, Flowlet*> m_flowletTable;  // QpKey -> Flowlet (at SrcToR)
};

}  // namespace ns3