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

const uint32_t CONGA_NULL = UINT32_MAX;

struct FeedbackInfo {
    uint32_t _ce;
    Time _updateTime;
};

struct OutpathInfo {
    uint32_t _ce;
    Time _updateTime;
};
/*----------------------------*/

class CongaTag : public Tag {
   public:
    CongaTag();
    ~CongaTag();
    static TypeId GetTypeId(void);
    void SetPathId(uint32_t pathId);
    uint32_t GetPathId(void) const;
    void SetCe(uint32_t ce);
    uint32_t GetCe(void) const;
    void SetFbPathId(uint32_t fbPathId);
    uint32_t GetFbPathId(void) const;
    void SetFbMetric(uint32_t fbMetric);
    uint32_t GetFbMetric(void) const;
    void SetHopCount(uint32_t hopCount);
    uint32_t GetHopCount(void) const;
    virtual TypeId GetInstanceTypeId(void) const;
    virtual uint32_t GetSerializedSize(void) const;
    virtual void Serialize(TagBuffer i) const;
    virtual void Deserialize(TagBuffer i);
    virtual void Print(std::ostream& os) const;

   private:
    uint32_t m_pathId;    // forward
    uint32_t m_ce;        // forward
    uint32_t m_hopCount;  // hopCount to get outPort
    uint32_t m_fbPathId;  // feedback
    uint32_t m_fbMetric;  // feedback
};

/**
 * @brief Conga object is created for each ToR Switch
 */
class CongaRouting : public Object {
    friend class SwitchMmu;
    friend class SwitchNode;

   public:
    CongaRouting();

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
    void RouteInput(Ptr<Packet> p, CustomHeader ch);
    uint32_t UpdateLocalDre(Ptr<Packet> p, CustomHeader ch, uint32_t outPort);
    uint32_t QuantizingX(uint32_t outPort, uint32_t X);  // X is bytes here and we quantizing it to 0 - 2^Q
    uint32_t GetBestPath(uint32_t dstTorId, uint32_t nSample);
    virtual void DoDispose();

    /* SET functions */
    void SetConstants(Time dreTime, Time agingTime, Time flowletTimeout, uint32_t quantizeBit, double alpha);
    void SetSwitchInfo(bool isToR, uint32_t switch_id);
    void SetLinkCapacity(uint32_t outPort, uint64_t bitRate);

    // periodic events
    EventId m_dreEvent;
    EventId m_agingEvent;
    void DreEvent();
    void AgingEvent();

    // topological info (should be initialized in the beginning)
    std::map<uint32_t, std::set<uint32_t> > m_congaRoutingTable;                 // routing table (ToRId -> pathId) (stable)
    std::map<uint32_t, std::map<uint32_t, FeedbackInfo> > m_congaFromLeafTable;  // ToRId -> <pathId -> FeedbackInfo> (aged)
    std::map<uint32_t, std::map<uint32_t, OutpathInfo> > m_congaToLeafTable;     // ToRId -> <pathId -> OutpathInfo> (aged)
    std::map<uint32_t, uint64_t> m_outPort2BitRateMap;                           // outPort -> link bitrate (bps) (stable)

    /*-----CALLBACK------*/
    void DoSwitchSend(Ptr<Packet> p, CustomHeader& ch, uint32_t outDev,
                      uint32_t qIndex);  // TxToR and Agg/CoreSw
    void DoSwitchSendToDev(Ptr<Packet> p, CustomHeader& ch);  // only at RxToR
    typedef Callback<void, Ptr<Packet>, CustomHeader&, uint32_t, uint32_t> SwitchSendCallback;
    typedef Callback<void, Ptr<Packet>, CustomHeader&> SwitchSendToDevCallback;
    void SetSwitchSendCallback(SwitchSendCallback switchSendCallback);  // set callback
    void SetSwitchSendToDevCallback(
        SwitchSendToDevCallback switchSendToDevCallback);  // set callback
    /*-----------*/
    
   private:
    // callback
    SwitchSendCallback m_switchSendCallback;  // bound to SwitchNode::SwitchSend (for Request/UDP)
    SwitchSendToDevCallback
        m_switchSendToDevCallback;  // bound to SwitchNode::SendToDevContinue (for Probe, Reply)

    // topology parameters
    bool m_isToR;          // is ToR (leaf)
    uint32_t m_switch_id;  // switch's nodeID

    // conga constants
    Time m_dreTime;          // dre alogrithm (e.g., 200us)
    Time m_agingTime;        // dre algorithm (e.g., 10ms)
    Time m_flowletTimeout;   // flowlet timeout (e.g., 100us)
    uint32_t m_quantizeBit;  // quantizing (2**X) param (e.g., X=3)
    double m_alpha;          // dre algorithm (e.g., 0.2)

    // local
    std::map<uint32_t, uint32_t> m_DreMap;        // outPort -> DRE (at SrcToR)
    std::map<uint64_t, Flowlet*> m_flowletTable;  // QpKey -> Flowlet (at SrcToR)
};

}  // namespace ns3
