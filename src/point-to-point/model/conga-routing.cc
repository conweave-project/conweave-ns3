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

#include "ns3/conga-routing.h"

#include "assert.h"
#include "ns3/assert.h"
#include "ns3/event-id.h"
#include "ns3/ipv4-header.h"
#include "ns3/log.h"
#include "ns3/nstime.h"
#include "ns3/object.h"
#include "ns3/packet.h"
#include "ns3/settings.h"
#include "ns3/simulator.h"

NS_LOG_COMPONENT_DEFINE("CongaRouting");

namespace ns3 {

/*---- Conga-Tag -----*/

CongaTag::CongaTag() {}
CongaTag::~CongaTag() {}
TypeId CongaTag::GetTypeId(void) {
    static TypeId tid = TypeId("ns3::CongaTag").SetParent<Tag>().AddConstructor<CongaTag>();
    return tid;
}
void CongaTag::SetPathId(uint32_t pathId) { m_pathId = pathId; }
uint32_t CongaTag::GetPathId(void) const { return m_pathId; }
void CongaTag::SetCe(uint32_t ce) { m_ce = ce; }
uint32_t CongaTag::GetCe(void) const { return m_ce; }
void CongaTag::SetFbPathId(uint32_t fbPathId) { m_fbPathId = fbPathId; }
uint32_t CongaTag::GetFbPathId(void) const { return m_fbPathId; }
void CongaTag::SetFbMetric(uint32_t fbMetric) { m_fbMetric = fbMetric; }
uint32_t CongaTag::GetFbMetric(void) const { return m_fbMetric; }

void CongaTag::SetHopCount(uint32_t hopCount) { m_hopCount = hopCount; }
uint32_t CongaTag::GetHopCount(void) const { return m_hopCount; }
TypeId CongaTag::GetInstanceTypeId(void) const { return GetTypeId(); }
uint32_t CongaTag::GetSerializedSize(void) const {
    return sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint32_t) +
           sizeof(uint32_t);
}
void CongaTag::Serialize(TagBuffer i) const {
    i.WriteU32(m_pathId);
    i.WriteU32(m_ce);
    i.WriteU32(m_hopCount);
    i.WriteU32(m_fbPathId);
    i.WriteU32(m_fbMetric);
}
void CongaTag::Deserialize(TagBuffer i) {
    m_pathId = i.ReadU32();
    m_ce = i.ReadU32();
    m_hopCount = i.ReadU32();
    m_fbPathId = i.ReadU32();
    m_fbMetric = i.ReadU32();
}
void CongaTag::Print(std::ostream& os) const {
    os << "m_pathId=" << m_pathId;
    os << ", m_ce=" << m_ce;
    os << ", m_hopCount=" << m_hopCount;
    os << ". m_fbPathId=" << m_fbPathId;
    os << ", m_fbMetric=" << m_fbMetric;
}

/*----- Conga-Route ------*/
uint32_t CongaRouting::nFlowletTimeout = 0;
CongaRouting::CongaRouting() {
    m_isToR = false;
    m_switch_id = (uint32_t)-1;

    // set constants
    m_dreTime = Time(MicroSeconds(200));
    m_agingTime = Time(MilliSeconds(10));
    m_flowletTimeout = Time(MicroSeconds(100));
    m_quantizeBit = 3;
    m_alpha = 0.2;
}

// it defines flowlet's 64bit key (order does not matter)
uint64_t CongaRouting::GetQpKey(uint32_t dip, uint16_t sport, uint16_t dport, uint16_t pg) {
    return ((uint64_t)dip << 32) | ((uint64_t)sport << 16) | (uint64_t)pg | (uint64_t)dport;
}

TypeId CongaRouting::GetTypeId(void) {
    static TypeId tid =
        TypeId("ns3::CongaRouting").SetParent<Object>().AddConstructor<CongaRouting>();

    return tid;
}

/** CALLBACK: callback functions  */
void CongaRouting::DoSwitchSend(Ptr<Packet> p, CustomHeader& ch, uint32_t outDev, uint32_t qIndex) {
    m_switchSendCallback(p, ch, outDev, qIndex);
}
void CongaRouting::DoSwitchSendToDev(Ptr<Packet> p, CustomHeader& ch) {
    m_switchSendToDevCallback(p, ch);
}

void CongaRouting::SetSwitchSendCallback(SwitchSendCallback switchSendCallback) {
    m_switchSendCallback = switchSendCallback;
}

void CongaRouting::SetSwitchSendToDevCallback(SwitchSendToDevCallback switchSendToDevCallback) {
    m_switchSendToDevCallback = switchSendToDevCallback;
}

void CongaRouting::SetSwitchInfo(bool isToR, uint32_t switch_id) {
    m_isToR = isToR;
    m_switch_id = switch_id;
}

void CongaRouting::SetLinkCapacity(uint32_t outPort, uint64_t bitRate) {
    auto it = m_outPort2BitRateMap.find(outPort);
    if (it != m_outPort2BitRateMap.end()) {
        // already exists, then check matching
        NS_ASSERT_MSG(it->second == bitRate,
                      "bitrate already exists, but inconsistent with new input");
    } else {
        m_outPort2BitRateMap[outPort] = bitRate;
    }
}

/* CongaRouting's main function */
void CongaRouting::RouteInput(Ptr<Packet> p, CustomHeader ch) {
    // Packet arrival time
    Time now = Simulator::Now();

    /**
     * NOTE: only DATA UDP is allowed to go through Conga because control packets are prioritized in
     * network and pass with different utility conditions!!
     **/
    if (ch.l3Prot != 0x11) {
        DoSwitchSendToDev(p, ch);
        return;
    }
    assert(ch.l3Prot == 0x11 && "Only supports UDP data packets");

    // Turn on DRE event scheduler if it is not running
    if (!m_dreEvent.IsRunning()) {
        NS_LOG_FUNCTION("Conga routing restarts dre event scheduling, Switch:" << m_switch_id
                                                                               << now);
        m_dreEvent = Simulator::Schedule(m_dreTime, &CongaRouting::DreEvent, this);
    }

    // Turn on aging event scheduler if it is not running
    if (!m_agingEvent.IsRunning()) {
        NS_LOG_FUNCTION("Conga routing restarts aging event scheduling:" << m_switch_id << now);
        m_agingEvent = Simulator::Schedule(m_agingTime, &CongaRouting::AgingEvent, this);
    }

    // get srcToRId, dstToRId
    assert(Settings::hostIp2SwitchId.find(ch.sip) !=
           Settings::hostIp2SwitchId.end());  // Misconfig of Settings::hostIp2SwitchId - sip"
    assert(Settings::hostIp2SwitchId.find(ch.dip) !=
           Settings::hostIp2SwitchId.end());  // Misconfig of Settings::hostIp2SwitchId - dip"
    uint32_t srcToRId = Settings::hostIp2SwitchId[ch.sip];
    uint32_t dstToRId = Settings::hostIp2SwitchId[ch.dip];

    /** FILTER: Quickly filter intra-pod traffic */
    if (srcToRId == dstToRId) {  // do normal routing (only one path)
        DoSwitchSendToDev(p, ch);
        return;
    }

    // it should be not in the same pod
    assert(srcToRId != dstToRId && "Should not be in the same pod");

    // get QpKey to find flowlet
    uint64_t qpkey = GetQpKey(ch.dip, ch.udp.sport, ch.udp.dport, ch.udp.pg);

    // get CongaTag from packet
    CongaTag congaTag;
    bool found = p->PeekPacketTag(congaTag);

    if (m_isToR) {     // ToR switch
        if (!found) {  // sender-side
            /*---- add piggyback info to CongaTag ----*/
            auto fbItr = m_congaFromLeafTable.find(dstToRId);
            NS_ASSERT_MSG(fbItr != m_congaFromLeafTable.end(),
                          "dstToRId cannot be found in FromLeafTable");
            auto innerFbItr = (fbItr->second).begin();
            if (!(fbItr->second).empty()) {
                std::advance(innerFbItr,
                             rand() % (fbItr->second).size());  // uniformly-random feedback
                // set values to new CongaTag
                congaTag.SetHopCount(0);                       // hopCount
                congaTag.SetFbPathId(innerFbItr->first);       // path
                congaTag.SetFbMetric(innerFbItr->second._ce);  // ce
            } else {
                // empty (nothing to feedback) then set a dummy
                congaTag.SetHopCount(0);           // hopCount
                congaTag.SetFbPathId(CONGA_NULL);  // path
                congaTag.SetFbMetric(CONGA_NULL);  // ce
            }

            /*---- choosing outPort ----*/
            struct Flowlet* flowlet = NULL;
            auto flowletItr = m_flowletTable.find(qpkey);
            uint32_t selectedPath;

            // 1) when flowlet already exists
            if (flowletItr != m_flowletTable.end()) {
                flowlet = flowletItr->second;
                assert(flowlet != NULL &&
                       "Impossible in normal cases - flowlet is not correctly registered");

                if (now - flowlet->_activeTime <= m_flowletTimeout) {  // no timeout
                    // update flowlet info
                    flowlet->_activeTime = now;
                    flowlet->_nPackets++;

                    // update/measure CE of this outPort and add CongaTag
                    selectedPath = flowlet->_PathId;
                    uint32_t outPort =
                        GetOutPortFromPath(selectedPath, 0);      // sender switch is 0th hop
                    uint32_t X = UpdateLocalDre(p, ch, outPort);  // update
                    uint32_t localCe = QuantizingX(outPort, X);   // quantize
                    congaTag.SetCe(localCe);
                    congaTag.SetPathId(selectedPath);

                    p->AddPacketTag(congaTag);
                    NS_LOG_FUNCTION("SenderToR" << m_switch_id << "Flowlet exists"
                                                << "Path/CE/outPort" << selectedPath
                                                << congaTag.GetCe() << outPort << "FbPath/Metric"
                                                << congaTag.GetFbPathId() << congaTag.GetFbMetric()
                                                << now);
                    DoSwitchSend(p, ch, GetOutPortFromPath(selectedPath, congaTag.GetHopCount()),
                                 ch.udp.pg);
                    // return GetOutPortFromPath(selectedPath, congaTag.GetHopCount());
                    return;
                }

                /*---- Flowlet Timeout ----*/
                // NS_LOG_FUNCTION("Flowlet expires, calculate the new port");
                selectedPath = GetBestPath(dstToRId, 4);
                CongaRouting::nFlowletTimeout++;

                // update flowlet info
                flowlet->_activatedTime = now;
                flowlet->_activeTime = now;
                flowlet->_nPackets++;
                flowlet->_PathId = selectedPath;

                // update/add CongaTag
                uint32_t outPort = GetOutPortFromPath(selectedPath, 0);
                uint32_t X = UpdateLocalDre(p, ch, outPort);  // update
                uint32_t localCe = QuantizingX(outPort, X);   // quantize
                congaTag.SetCe(localCe);
                congaTag.SetPathId(selectedPath);
                congaTag.SetHopCount(0);

                p->AddPacketTag(congaTag);
                NS_LOG_FUNCTION("SenderToR" << m_switch_id << "Flowlet exists & Timeout"
                                            << "Path/CE/outPort" << selectedPath << congaTag.GetCe()
                                            << outPort << "FbPath/Metric" << congaTag.GetFbPathId()
                                            << congaTag.GetFbMetric() << now);
                DoSwitchSend(p, ch, outPort, ch.udp.pg);
                // return outPort;
                return;
            }
            // 2) flowlet does not exist, e.g., first packet of flow
            selectedPath = GetBestPath(dstToRId, 4);
            struct Flowlet* newFlowlet = new Flowlet;
            newFlowlet->_activeTime = now;
            newFlowlet->_activatedTime = now;
            newFlowlet->_nPackets = 1;
            newFlowlet->_PathId = selectedPath;
            m_flowletTable[qpkey] = newFlowlet;

            // update/add CongaTag
            uint32_t outPort = GetOutPortFromPath(selectedPath, 0);
            uint32_t X = UpdateLocalDre(p, ch, outPort);  // update
            uint32_t localCe = QuantizingX(outPort, X);   // quantize
            congaTag.SetCe(localCe);
            congaTag.SetPathId(selectedPath);

            p->AddPacketTag(congaTag);
            NS_LOG_FUNCTION("SenderToR" << m_switch_id << "Flowlet does not exist"
                                        << "Path/CE/outPort" << selectedPath << congaTag.GetCe()
                                        << outPort << "FbPath/Metric" << congaTag.GetFbPathId()
                                        << congaTag.GetFbMetric() << now);
            DoSwitchSend(p, ch, GetOutPortFromPath(selectedPath, congaTag.GetHopCount()),
                         ch.udp.pg);
            // return GetOutPortFromPath(selectedPath, congaTag.GetHopCount());
            return;
        }
        /*---- receiver-side ----*/
        // update CongaToLeaf table
        auto toLeafItr = m_congaToLeafTable.find(srcToRId);
        assert(toLeafItr != m_congaToLeafTable.end() && "Cannot find srcToRId from ToLeafTable");
        auto innerToLeafItr = (toLeafItr->second).find(congaTag.GetFbPathId());
        if (congaTag.GetFbPathId() != CONGA_NULL &&
            congaTag.GetFbMetric() != CONGA_NULL) {             // if valid feedback
            if (innerToLeafItr == (toLeafItr->second).end()) {  // no feedback so far, then create
                OutpathInfo outpathInfo;
                outpathInfo._ce = congaTag.GetFbMetric();
                outpathInfo._updateTime = now;
                (toLeafItr->second)[congaTag.GetFbPathId()] = outpathInfo;
            } else {  // update statistics
                (innerToLeafItr->second)._ce = congaTag.GetFbMetric();
                (innerToLeafItr->second)._updateTime = now;
            }
        }

        // update CongaFromLeaf table
        auto fromLeafItr = m_congaFromLeafTable.find(srcToRId);
        assert(fromLeafItr != m_congaFromLeafTable.end() &&
               "Cannot find srcToRId from FromLeafTable");
        auto innerfromLeafItr = (fromLeafItr->second).find(congaTag.GetPathId());
        if (innerfromLeafItr == (fromLeafItr->second).end()) {  // no data sent so far, then create
            FeedbackInfo feedbackInfo;
            feedbackInfo._ce = congaTag.GetCe();
            feedbackInfo._updateTime = now;
            (fromLeafItr->second)[congaTag.GetPathId()] = feedbackInfo;
        } else {  // update feedback
            (innerfromLeafItr->second)._ce = congaTag.GetCe();
            (innerfromLeafItr->second)._updateTime = now;
        }

        // remove congaTag from header
        p->RemovePacketTag(congaTag);
        NS_LOG_FUNCTION("ReceiverToR" << m_switch_id << "Path/CE" << congaTag.GetPathId()
                                      << congaTag.GetCe() << "FbPath/Metric"
                                      << congaTag.GetFbPathId() << congaTag.GetFbMetric() << now);
        DoSwitchSendToDev(p, ch);
        // return CONGA_NULL;  // does not matter (outPort number is only 1)
        return;

    } else {  // agg/core switch
        // extract CongaTag
        assert(found && "If not ToR (leaf), CongaTag should be found");
        // get/update hopCount
        uint32_t hopCount = congaTag.GetHopCount() + 1;
        congaTag.SetHopCount(hopCount);

        // get outPort
        uint32_t outPort = GetOutPortFromPath(congaTag.GetPathId(), hopCount);
        uint32_t X = UpdateLocalDre(p, ch, outPort);                 // update
        uint32_t localCe = QuantizingX(outPort, X);                  // quantize
        uint32_t congestedCe = std::max(localCe, congaTag.GetCe());  // get more congested link's CE
        congaTag.SetCe(congestedCe);                                 // update CE

        // Re-serialize congaTag
        CongaTag temp_tag;
        p->RemovePacketTag(temp_tag);
        p->AddPacketTag(congaTag);
        NS_LOG_FUNCTION("Agg/CoreSw" << m_switch_id << "Path/CE/outPort" << congaTag.GetPathId()
                                     << congaTag.GetCe() << outPort << "FbPath/Metric"
                                     << congaTag.GetFbPathId() << congaTag.GetFbMetric() << now);
        DoSwitchSend(p, ch, outPort, ch.udp.pg);
        // return outPort;
        return;
    }
    assert(false && "This should not be occured");
}

// minimize the maximum link utilization
uint32_t CongaRouting::GetBestPath(uint32_t dstToRId, uint32_t nSample) {
    auto pathItr = m_congaRoutingTable.find(dstToRId);
    assert(pathItr != m_congaRoutingTable.end() && "Cannot find dstToRId from ToLeafTable");
    std::set<uint32_t>::iterator innerPathItr = pathItr->second.begin();
    if (pathItr->second.size() >= nSample) {  // exception handling
        std::advance(innerPathItr, rand() % (pathItr->second.size() - nSample + 1));
    } else {
        nSample = pathItr->second.size();
        // std::cout << "WARNING - Conga's number of path sampling is higher than available paths.
        // Enforced to reduce nSample:" << nSample << std::endl;
    }

    // path info for remote congestion, <pathId -> pathInfo>
    auto pathInfoMap = m_congaToLeafTable[dstToRId];

    // get min-max path
    std::vector<uint32_t> candidatePaths;
    uint32_t minCongestion = CONGA_NULL;
    for (uint32_t i = 0; i < nSample; i++) {
        // get info of path
        uint32_t pathId = *innerPathItr;
        auto innerPathInfo = pathInfoMap.find(pathId);

        // no info means good
        uint32_t localCongestion = 0;
        uint32_t remoteCongestion = 0;

        auto outPort = GetOutPortFromPath(pathId, 0);  // outPort from pathId (TxToR)

        // local congestion -> get Port Util and quantize it
        auto innerDre = m_DreMap.find(outPort);
        if (innerDre != m_DreMap.end()) {
            localCongestion = QuantizingX(outPort, innerDre->second);
        }

        // remote congestion
        if (innerPathInfo != pathInfoMap.end()) {
            remoteCongestion = innerPathInfo->second._ce;
        }

        // get maximum of congestion (local, remote)
        uint32_t CurrCongestion = std::max(localCongestion, remoteCongestion);

        // filter the best path
        if (minCongestion > CurrCongestion) {
            minCongestion = CurrCongestion;
            candidatePaths.clear();
            candidatePaths.push_back(pathId);  // best
        } else if (minCongestion == CurrCongestion) {
            candidatePaths.push_back(pathId);  // equally good
        }
        std::advance(innerPathItr, 1);
    }
    assert(candidatePaths.size() > 0 && "candidatePaths has no entry");
    return candidatePaths[rand() % candidatePaths.size()];  // randomly choose the best path
}

uint32_t CongaRouting::UpdateLocalDre(Ptr<Packet> p, CustomHeader ch, uint32_t outPort) {
    uint32_t X = m_DreMap[outPort];
    uint32_t newX = X + p->GetSize();
    // NS_LOG_FUNCTION("Old X" << X << "New X" << newX << "outPort" << outPort << "Switch" <<
    // m_switch_id << Simulator::Now());
    m_DreMap[outPort] = newX;
    return newX;
}

uint32_t CongaRouting::GetOutPortFromPath(const uint32_t& path, const uint32_t& hopCount) {
    return ((uint8_t*)&path)[hopCount];
}

void CongaRouting::SetOutPortToPath(uint32_t& path, const uint32_t& hopCount,
                                    const uint32_t& outPort) {
    ((uint8_t*)&path)[hopCount] = outPort;
}

uint32_t CongaRouting::QuantizingX(uint32_t outPort, uint32_t X) {
    auto it = m_outPort2BitRateMap.find(outPort);
    assert(it != m_outPort2BitRateMap.end() && "Cannot find bitrate of interface");
    uint64_t bitRate = it->second;
    double ratio = static_cast<double>(X * 8) / (bitRate * m_dreTime.GetSeconds() / m_alpha);
    uint32_t quantX = static_cast<uint32_t>(ratio * std::pow(2, m_quantizeBit));
    if (quantX > 3) {
        NS_LOG_FUNCTION("X" << X << "Ratio" << ratio << "Bits" << quantX << Simulator::Now());
    }
    return quantX;
}

void CongaRouting::SetConstants(Time dreTime, Time agingTime, Time flowletTimeout,
                                uint32_t quantizeBit, double alpha) {
    m_dreTime = dreTime;
    m_agingTime = agingTime;
    m_flowletTimeout = flowletTimeout;
    m_quantizeBit = quantizeBit;
    m_alpha = alpha;
}

void CongaRouting::DoDispose() {
    for (auto i : m_flowletTable) {
        delete (i.second);
    }
    m_dreEvent.Cancel();
    m_agingEvent.Cancel();
}

void CongaRouting::DreEvent() {
    std::map<uint32_t, uint32_t>::iterator itr = m_DreMap.begin();
    for (; itr != m_DreMap.end(); ++itr) {
        uint32_t newX = itr->second * (1 - m_alpha);
        itr->second = newX;
    }
    NS_LOG_FUNCTION(Simulator::Now());
    m_dreEvent = Simulator::Schedule(m_dreTime, &CongaRouting::DreEvent, this);
}

void CongaRouting::AgingEvent() {
    auto now = Simulator::Now();
    auto itr = m_congaToLeafTable.begin();  // always non-empty
    for (; itr != m_congaToLeafTable.end(); ++itr) {
        auto innerItr = (itr->second).begin();
        for (; innerItr != (itr->second).end(); ++innerItr) {
            if (now - (innerItr->second)._updateTime > m_agingTime) {
                (innerItr->second)._ce = 0;
            }
        }
    }

    auto itr2 = m_congaFromLeafTable.begin();
    while (itr2 != m_congaFromLeafTable.end()) {
        auto innerItr2 = (itr2->second).begin();
        while (innerItr2 != (itr2->second).end()) {
            if (now - (innerItr2->second)._updateTime > m_agingTime) {
                innerItr2 = (itr2->second).erase(innerItr2);
            } else {
                ++innerItr2;
            }
        }
        ++itr2;
    }

    auto itr3 = m_flowletTable.begin();
    while (itr3 != m_flowletTable.end()) {
        if (now - ((itr3->second)->_activeTime) > m_agingTime) {
            // delete(itr3->second); // delete pointer
            itr3 = m_flowletTable.erase(itr3);
        } else {
            ++itr3;
        }
    }
    NS_LOG_FUNCTION(Simulator::Now());
    m_agingEvent = Simulator::Schedule(m_agingTime, &CongaRouting::AgingEvent, this);
}

}  // namespace ns3