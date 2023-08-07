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

#include "ns3/conweave-routing.h"

#include <assert.h>
#include <stdio.h>

#include <algorithm>
#include <random>

#include "ns3/assert.h"
#include "ns3/event-id.h"
#include "ns3/flow-id-tag.h"
#include "ns3/ipv4-header.h"
#include "ns3/log.h"
#include "ns3/nstime.h"
#include "ns3/object.h"
#include "ns3/packet.h"
#include "ns3/ppp-header.h"
#include "ns3/qbb-header.h"
#include "ns3/random-variable.h"
#include "ns3/settings.h"
#include "ns3/simulator.h"
#include "ns3/udp-header.h"

namespace ns3 {

/**
 * @brief tag for DATA header
 */
ConWeaveDataTag::ConWeaveDataTag() : Tag() {}
TypeId ConWeaveDataTag::GetTypeId(void) {
    static TypeId tid =
        TypeId("ns3::ConWeaveDataTag").SetParent<Tag>().AddConstructor<ConWeaveDataTag>();
    return tid;
}
void ConWeaveDataTag::SetPathId(uint32_t pathId) { m_pathId = pathId; }
uint32_t ConWeaveDataTag::GetPathId(void) const { return m_pathId; }
void ConWeaveDataTag::SetHopCount(uint32_t hopCount) { m_hopCount = hopCount; }
uint32_t ConWeaveDataTag::GetHopCount(void) const { return m_hopCount; }
void ConWeaveDataTag::SetEpoch(uint32_t epoch) { m_epoch = epoch; }
uint32_t ConWeaveDataTag::GetEpoch(void) const { return m_epoch; }
void ConWeaveDataTag::SetPhase(uint32_t phase) { m_phase = phase; }
uint32_t ConWeaveDataTag::GetPhase(void) const { return m_phase; }
void ConWeaveDataTag::SetTimestampTx(uint64_t timestamp) { m_timestampTx = timestamp; }
uint64_t ConWeaveDataTag::GetTimestampTx(void) const { return m_timestampTx; }
void ConWeaveDataTag::SetTimestampTail(uint64_t timestamp) { m_timestampTail = timestamp; }
uint64_t ConWeaveDataTag::GetTimestampTail(void) const { return m_timestampTail; }
void ConWeaveDataTag::SetFlagData(uint32_t flag) { m_flagData = flag; }
uint32_t ConWeaveDataTag::GetFlagData(void) const { return m_flagData; }

TypeId ConWeaveDataTag::GetInstanceTypeId(void) const { return GetTypeId(); }
uint32_t ConWeaveDataTag::GetSerializedSize(void) const {
    return sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint32_t) +
           sizeof(uint64_t) + sizeof(uint64_t) + sizeof(uint32_t);
}
void ConWeaveDataTag::Serialize(TagBuffer i) const {
    i.WriteU32(m_pathId);
    i.WriteU32(m_hopCount);
    i.WriteU32(m_epoch);
    i.WriteU32(m_phase);
    i.WriteU64(m_timestampTx);
    i.WriteU64(m_timestampTail);
    i.WriteU32(m_flagData);
}
void ConWeaveDataTag::Deserialize(TagBuffer i) {
    m_pathId = i.ReadU32();
    m_hopCount = i.ReadU32();
    m_epoch = i.ReadU32();
    m_phase = i.ReadU32();
    m_timestampTx = i.ReadU64();
    m_timestampTail = i.ReadU64();
    m_flagData = i.ReadU32();
}
void ConWeaveDataTag::Print(std::ostream &os) const {
    os << "m_pathId=" << m_pathId;
    os << ", m_hopCount=" << m_hopCount;
    os << ", m_epoch=" << m_epoch;
    os << ", m_phase=" << m_phase;
    os << ". m_timestampTx=" << m_timestampTx;
    os << ", m_timestampTail=" << m_timestampTail;
    os << ", m_flagData=" << m_flagData;
}

/**
 * @brief tag for reply/notify packet
 */
ConWeaveReplyTag::ConWeaveReplyTag() : Tag() {}
TypeId ConWeaveReplyTag::GetTypeId(void) {
    static TypeId tid =
        TypeId("ns3::ConWeaveReplyTag").SetParent<Tag>().AddConstructor<ConWeaveReplyTag>();
    return tid;
}
void ConWeaveReplyTag::SetFlagReply(uint32_t flagReply) { m_flagReply = flagReply; }
uint32_t ConWeaveReplyTag::GetFlagReply(void) const { return m_flagReply; }
void ConWeaveReplyTag::SetEpoch(uint32_t epoch) { m_epoch = epoch; }
uint32_t ConWeaveReplyTag::GetEpoch(void) const { return m_epoch; }
void ConWeaveReplyTag::SetPhase(uint32_t phase) { m_phase = phase; }
uint32_t ConWeaveReplyTag::GetPhase(void) const { return m_phase; }
TypeId ConWeaveReplyTag::GetInstanceTypeId(void) const { return GetTypeId(); }
uint32_t ConWeaveReplyTag::GetSerializedSize(void) const {
    return sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint32_t);
}
void ConWeaveReplyTag::Serialize(TagBuffer i) const {
    i.WriteU32(m_flagReply);
    i.WriteU32(m_epoch);
    i.WriteU32(m_phase);
}
void ConWeaveReplyTag::Deserialize(TagBuffer i) {
    m_flagReply = i.ReadU32();
    m_epoch = i.ReadU32();
    m_phase = i.ReadU32();
}
void ConWeaveReplyTag::Print(std::ostream &os) const {
    os << "m_flagReply=" << m_flagReply;
    os << "m_epoch=" << m_epoch;
    os << "m_phase=" << m_phase;
}

/**
 * @brief tag for notify packet
 */
ConWeaveNotifyTag::ConWeaveNotifyTag() : Tag() {}
TypeId ConWeaveNotifyTag::GetTypeId(void) {
    static TypeId tid =
        TypeId("ns3::ConWeaveNotifyTag").SetParent<Tag>().AddConstructor<ConWeaveNotifyTag>();
    return tid;
}
void ConWeaveNotifyTag::SetPathId(uint32_t pathId) { m_pathId = pathId; }
uint32_t ConWeaveNotifyTag::GetPathId(void) const { return m_pathId; }
TypeId ConWeaveNotifyTag::GetInstanceTypeId(void) const { return GetTypeId(); }
uint32_t ConWeaveNotifyTag::GetSerializedSize(void) const { return sizeof(uint32_t); }
void ConWeaveNotifyTag::Serialize(TagBuffer i) const { i.WriteU32(m_pathId); }
void ConWeaveNotifyTag::Deserialize(TagBuffer i) { m_pathId = i.ReadU32(); }
void ConWeaveNotifyTag::Print(std::ostream &os) const { os << "m_pathId=" << m_pathId; }

/*---------------- ConWeaveRouting ---------------*/
// debugging to check timing
uint64_t ConWeaveRouting::debug_time = 0;

// static members for topology information and statistics
uint64_t ConWeaveRouting::m_nReplyInitSent = 0;
uint64_t ConWeaveRouting::m_nReplyTailSent = 0;
uint64_t ConWeaveRouting::m_nTimelyInitReplied = 0;
uint64_t ConWeaveRouting::m_nTimelyTailReplied = 0;
uint64_t ConWeaveRouting::m_nNotifySent = 0;
uint64_t ConWeaveRouting::m_nReRoute = 0;
uint64_t ConWeaveRouting::m_nOutOfOrderPkts = 0;
uint64_t ConWeaveRouting::m_nFlushVOQTotal = 0;
uint64_t ConWeaveRouting::m_nFlushVOQByTail = 0;
std::vector<uint32_t> ConWeaveRouting::m_historyVOQSize;

// functions
ConWeaveRouting::ConWeaveRouting() {
    m_isToR = false;
    m_switch_id = (uint32_t)-1;

    // set constants
    m_extraReplyDeadline = MicroSeconds(4);       // 1 hop of 50KB / 100Gbps = 4us
    m_extraVOQFlushTime = MicroSeconds(8);        // for uncertainty
    m_txExpiryTime = MicroSeconds(300);           // flowlet timegap
    m_defaultVOQWaitingTime = MicroSeconds(200);  // 200us
    m_pathPauseTime = MicroSeconds(8);            // 100KB queue, 100Gbps -> 8us
    m_pathAwareRerouting = true;                  // enable path-aware rerouting
    m_agingTime = MilliSeconds(2);                // 2ms
    m_conweavePathTable.resize(65536);            // initialize table size
}

ConWeaveRouting::~ConWeaveRouting() {}
void ConWeaveRouting::DoDispose() { m_agingEvent.Cancel(); }
TypeId ConWeaveRouting::GetTypeId(void) {
    static TypeId tid =
        TypeId("ns3::ConWeaveRouting").SetParent<Object>().AddConstructor<ConWeaveRouting>();
    return tid;
}

uint32_t ConWeaveRouting::GetOutPortFromPath(const uint32_t &path, const uint32_t &hopCount) {
    return ((uint8_t *)&path)[hopCount];
}

void ConWeaveRouting::SetOutPortToPath(uint32_t &path, const uint32_t &hopCount,
                                       const uint32_t &outPort) {
    ((uint8_t *)&path)[hopCount] = outPort;
}

uint64_t ConWeaveRouting::GetFlowKey(uint32_t ip1, uint32_t ip2, uint16_t port1, uint16_t port2) {
    /** IP_ADDRESS: 11.X.X.1 */
    assert(((ip1 & 0xff000000) >> 24) == 11);
    assert(((ip2 & 0xff000000) >> 24) == 11);
    assert((ip1 & 0x000000ff) == 1);
    assert((ip2 & 0x000000ff) == 1);

    uint64_t ret = 0;
    ret += uint64_t((ip1 & 0x00ffff00) >> 8) + uint64_t((uint32_t)port1 << 16);
    ret = ret << 32;
    ret += uint64_t((ip2 & 0x00ffff00) >> 8) + uint64_t((uint32_t)port2 << 16);
    return ret;
}

uint32_t ConWeaveRouting::DoHash(const uint8_t *key, size_t len, uint32_t seed) {
    uint32_t h = seed;
    if (len > 3) {
        const uint32_t *key_x4 = (const uint32_t *)key;
        size_t i = len >> 2;
        do {
            uint32_t k = *key_x4++;
            k *= 0xcc9e2d51;
            k = (k << 15) | (k >> 17);
            k *= 0x1b873593;
            h ^= k;
            h = (h << 13) | (h >> 19);
            h += (h << 2) + 0xe6546b64;
        } while (--i);
        key = (const uint8_t *)key_x4;
    }
    if (len & 3) {
        size_t i = len & 3;
        uint32_t k = 0;
        key = &key[i - 1];
        do {
            k <<= 8;
            k |= *key--;
        } while (--i);
        k *= 0xcc9e2d51;
        k = (k << 15) | (k >> 17);
        k *= 0x1b873593;
        h ^= k;
    }
    h ^= len;
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h;
}

void ConWeaveRouting::SendReply(Ptr<Packet> p, CustomHeader &ch, uint32_t flagReply,
                                uint32_t pkt_epoch) {
    qbbHeader seqh;
    seqh.SetSeq(0);
    seqh.SetPG(ch.udp.pg);
    seqh.SetSport(ch.udp.dport);
    seqh.SetDport(ch.udp.sport);
    seqh.SetIntHeader(ch.udp.ih);

    Ptr<Packet> replyP = Create<Packet>(
        std::max(64 - 14 - 20 - (int)seqh.GetSerializedSize(), 0));  // at least 64 Bytes
    replyP->AddHeader(seqh);                                         // qbbHeader

    // ACK-like packet, no L4 header
    Ipv4Header ipv4h;
    ipv4h.SetSource(Ipv4Address(ch.dip));
    ipv4h.SetDestination(Ipv4Address(ch.sip));
    ipv4h.SetProtocol(0xFD);  // (N)ACK - (IRN)
    ipv4h.SetTtl(64);
    ipv4h.SetPayloadSize(replyP->GetSize());
    ipv4h.SetIdentification(UniformVariable(0, 65536).GetValue());
    replyP->AddHeader(ipv4h);  // ipv4Header

    PppHeader ppp;
    ppp.SetProtocol(0x0021);  // EtherToPpp(0x800), see point-to-point-net-device.cc
    replyP->AddHeader(ppp);   // pppHeader

    // attach slbControlTag
    ConWeaveReplyTag conweaveReplyTag;
    conweaveReplyTag.SetFlagReply(flagReply);
    conweaveReplyTag.SetEpoch(pkt_epoch);
    if (flagReply == ConWeaveReplyTag::INIT) {
        ConWeaveRouting::m_nReplyInitSent += 1;
        conweaveReplyTag.SetPhase(0);
    } else if (flagReply == ConWeaveReplyTag::TAIL) {
        ConWeaveRouting::m_nReplyTailSent += 1;
        conweaveReplyTag.SetPhase(1);
    } else {
        SLB_LOG("ERROR - Unknown ConWeaveReplyTag flag:" << flagReply);
        exit(1);
    }

    replyP->AddPacketTag(conweaveReplyTag);

    // dummy reply's inDev interface
    replyP->AddPacketTag(FlowIdTag(Settings::CONWEAVE_CTRL_DUMMY_INDEV));

    // extract customheader
    CustomHeader replyCh(CustomHeader::L2_Header | CustomHeader::L3_Header |
                         CustomHeader::L4_Header);
    replyP->PeekHeader(replyCh);

    // send reply packets
    SLB_LOG(PARSE_FIVE_TUPLE(ch) << "================================### Send REPLY"
                                 << ",ReplyFlag:" << flagReply);
    DoSwitchSendToDev(replyP, replyCh);  // will have ACK's priority
    return;
}

void ConWeaveRouting::SendNotify(Ptr<Packet> p, CustomHeader &ch, uint32_t pathId) {
    qbbHeader seqh;
    seqh.SetSeq(0);
    seqh.SetPG(ch.udp.pg);
    seqh.SetSport(ch.udp.dport);
    seqh.SetDport(ch.udp.sport);
    seqh.SetIntHeader(ch.udp.ih);

    Ptr<Packet> fbP = Create<Packet>(
        std::max(64 - 14 - 20 - (int)seqh.GetSerializedSize(), 0));  // at least 64 Bytes
    fbP->AddHeader(seqh);                                            // qbbHeader

    // ACK-like packet, no L4 header
    Ipv4Header ipv4h;
    ipv4h.SetSource(Ipv4Address(ch.dip));
    ipv4h.SetDestination(Ipv4Address(ch.sip));
    ipv4h.SetProtocol(0xFD);  // (N)ACK - (IRN)
    ipv4h.SetTtl(64);
    ipv4h.SetPayloadSize(fbP->GetSize());
    ipv4h.SetIdentification(UniformVariable(0, 65536).GetValue());
    fbP->AddHeader(ipv4h);  // ipv4Header

    PppHeader ppp;
    ppp.SetProtocol(0x0021);  // EtherToPpp(0x800), see point-to-point-net-device.cc
    fbP->AddHeader(ppp);      // pppHeader

    // attach ConWeaveNotifyTag
    ConWeaveNotifyTag conweaveNotifyTag;
    conweaveNotifyTag.SetPathId(pathId);
    fbP->AddPacketTag(conweaveNotifyTag);

    // dummy notify's inDev interface
    fbP->AddPacketTag(FlowIdTag(Settings::CONWEAVE_CTRL_DUMMY_INDEV));

    // extract customheader
    CustomHeader fbCh(CustomHeader::L2_Header | CustomHeader::L3_Header | CustomHeader::L4_Header);
    fbP->PeekHeader(fbCh);

    /** OVERHEAD: reply overhead statistics **/
    ConWeaveRouting::m_nNotifySent += 1;

    // send notify packets
    SLB_LOG(PARSE_FIVE_TUPLE(ch) << "================================### Send NOTIFY");
    DoSwitchSendToDev(fbP, fbCh);  // will have ACK's priority
    return;
}

/** MAIN: Every SLB packet is hijacked to this function at switches */
void ConWeaveRouting::RouteInput(Ptr<Packet> p, CustomHeader &ch) {
    // Packet arrival time
    Time now = Simulator::Now();

    // Turn on aging event scheduler if it is not running
    if (!m_agingEvent.IsRunning()) {
        SLB_LOG("ConWeave routing restarts aging event scheduling:" << m_switch_id << now);
        m_agingEvent = Simulator::Schedule(m_agingTime, &ConWeaveRouting::AgingEvent, this);
    }

    // get srcToRId, dstToRId
    assert(Settings::hostIp2SwitchId.find(ch.sip) !=
           Settings::hostIp2SwitchId.end());  // Misconfig of Settings::hostIp2SwitchId - sip
    assert(Settings::hostIp2SwitchId.find(ch.dip) !=
           Settings::hostIp2SwitchId.end());  // Misconfig of Settings::hostIp2SwitchId - dip
    uint32_t srcToRId = Settings::hostIp2SwitchId[ch.sip];
    uint32_t dstToRId = Settings::hostIp2SwitchId[ch.dip];

    /** FILTER: Quickly filter intra-pod traffic */
    if (srcToRId == dstToRId) {  // do normal routing (only one path)
        DoSwitchSendToDev(p, ch);
        return;
    }
    assert(srcToRId != dstToRId);  // Should not be in the same pod

    if (ch.l3Prot != 0x11 && ch.l3Prot != 0xFD) {
        SLB_LOG(PARSE_FIVE_TUPLE(ch) << "ACK/PFC or other control pkts -> do flow-ECMP. Sw("
                                     << m_switch_id << "),l3Prot:" << ch.l3Prot);
        DoSwitchSendToDev(p, ch);
        return;
    }
    assert(ch.l3Prot == 0x11 || ch.l3Prot == 0xFD);  // Only supports UDP (data) or (N)ACK packets

    // get conweaveDataTag from packet
    ConWeaveDataTag conweaveDataTag;
    bool foundConWeaveDataTag = p->PeekPacketTag(conweaveDataTag);

    // get SlbControlTag from packet
    ConWeaveReplyTag conweaveReplyTag;
    bool foundConWeaveReplyTag = p->PeekPacketTag(conweaveReplyTag);

    // get conweaveNotifyTag from packet
    ConWeaveNotifyTag conweaveNotifyTag;
    bool foundConWeaveNotifyTag = p->PeekPacketTag(conweaveNotifyTag);

    if (ch.l3Prot == 0xFD) { /** ACK or ConWeave's Control Packets */
        /** NOTE: ConWeave uses 0xFD protocol id for its control packets.
         * Quick filter purely (N)ACK packets (not ConWeave control packets)
         **/
        if (!foundConWeaveReplyTag && !foundConWeaveNotifyTag) {  // pure-(N)ACK
            if (m_switch_id == srcToRId) {
                SLB_LOG(PARSE_FIVE_TUPLE(ch)
                        << "[TxToR/*PureACK] Sw(" << m_switch_id << "),ACK detected");
            }
            if (m_switch_id == dstToRId) {
                SLB_LOG(PARSE_FIVE_TUPLE(ch)
                        << "[RxToR/*PureACK] Sw(" << m_switch_id << "),ACK detected");
            }
            DoSwitchSendToDev(p, ch);
            return;
        }

        /** NOTE: ConWeave's control packets are forwarded with default flow-ECMP */
        if (!m_isToR) {
            SLB_LOG(PARSE_FIVE_TUPLE(ch) << "ConWeave Ctrl Pkts use flow-ECMP at non-ToR switches");
            DoSwitchSendToDev(p, ch);
            return;
        }
    }

    // print every 1ms for logging
    if (Simulator::Now().GetMilliSeconds() > debug_time) {
        std::cout << "[Logging] Current time: " << Simulator::Now() << std::endl;
        debug_time = Simulator::Now().GetMilliSeconds();
    }

    /**------------------------  Start processing SLB packets  ------------------------*/
    if (m_isToR) {  // ToR switch
        /***************************
         *    Source ToR Switch    *
         ***************************/
        if (m_switch_id == srcToRId) {  // SrcToR -
            assert(ch.l3Prot == 0x11);  // Only UDP (Data) - TxToR can see only UDP (DATA) packets
            assert(!foundConWeaveDataTag &&
                   !foundConWeaveReplyTag);  // ERROR - dataTag and replyTag
                                             // should not be found at TxToR

            /** PIPELINE: emulating the p4 pipelining */
            conweaveTxMeta tx_md;  // SrcToR packet metadata
            tx_md.pkt_flowkey = GetFlowKey(ch.sip, ch.dip, ch.udp.sport, ch.udp.dport);
            auto &txEntry = m_conweaveTxTable[tx_md.pkt_flowkey];

            /** INIT: initialize flowkey */
            if (txEntry._flowkey == 0) { /* if new */
                txEntry._flowkey = tx_md.pkt_flowkey;
                tx_md.newConnection = true;
            }
            uint64_t baseRTT =
                m_rxToRId2BaseRTT[dstToRId];  // get base RTT (used for setting REPLY timer)

            /**
             * CHECK: Expiry or Stability
             */
            if (txEntry._activeTime + m_txExpiryTime < now) { /* expired */
                tx_md.flagExpired = true;
                txEntry._stabilized = false;
            } else if (txEntry._stabilized == true) { /* stabilized */
                tx_md.flagStabilized = true;
                txEntry._stabilized = false;
            }
            txEntry._activeTime = now;  // record the entry's last-accessed time

            // sanity check - new connections are first always having "expired" flag
            if (tx_md.newConnection) {
                assert(tx_md.flagExpired == true);
            }

            /** ACTIVE: if expired or stabilized, reset timer. Otherwise, check timeout */
            if (tx_md.flagExpired ||
                tx_md.flagStabilized) { /* expired or stabilized -> send INIT  */
                txEntry._replyTimer =
                    now + NanoSeconds(baseRTT) + m_extraReplyDeadline; /* set new reply deadline */
            } else if (txEntry._replyTimer < now) { /* reply-timeout -> send TAIL */
                txEntry._replyTimer = CW_MAX_TIME;
                tx_md.flagReplyTimeout = true; /* create TAIL packet */
            }

            SLB_LOG(PARSE_FIVE_TUPLE(ch)
                    << "\t[TxToR/UDP] Sw(" << m_switch_id << "),E/R/S:" << tx_md.flagExpired << "/"
                    << tx_md.flagReplyTimeout << "/" << tx_md.flagStabilized
                    << ",flowkey:" << tx_md.pkt_flowkey);
            if (tx_md.newConnection) {
                SLB_LOG(PARSE_FIVE_TUPLE(ch)
                        << "\t--> new connection with flowkey:" << tx_md.pkt_flowkey);
            }

            /**
             * ROUND: if expiry or stabilized, increase epoch by 1
             */
            if (tx_md.flagExpired || tx_md.flagStabilized) { /* expired or stabilized */
                txEntry._epoch += 1;
                tx_md.epoch = txEntry._epoch; /* increase and get */
            } else {                          /* reply-timeout, or usual */
                tx_md.epoch = txEntry._epoch; /* just get */
            }

            /**
             * PHASE: expiry, stabilized, reply-timeout, or just get
             */
            if (tx_md.flagExpired || tx_md.flagStabilized) { /* expired or stabilized  */
                txEntry._phase = 0;                          /* set phase to 0 */
                tx_md.phase = 0;                             /* pkt's phase = 0 */
            } else if (tx_md.flagReplyTimeout) {
                assert(txEntry._phase == 0);
                txEntry._phase = 1; /* set phase to 1 */
                tx_md.phase = 0;    /* pkt's phase = 0 */
            } else {                /* normal pkt */
                tx_md.phase = txEntry._phase;
            }

            /**
             * PATH: sample 2 ports and choose a good port
             */
            std::set<uint32_t> pathSet = m_ConWeaveRoutingTable[dstToRId];  // pathSet to RxToR
            uint32_t initPath =
                *(std::next(pathSet.begin(),
                            rand() % pathSet.size()));  // to initialize (empty: CW_DEFAULT_32BIT)

            if (m_pathAwareRerouting) {
                /* path-aware decision */
                uint32_t randPath1 = *(std::next(pathSet.begin(), rand() % pathSet.size()));
                uint32_t randPath2 = *(std::next(pathSet.begin(), rand() % pathSet.size()));
                const auto pathEntry1 =
                    m_conweavePathTable[DoHash((uint8_t *)&randPath1, 4, m_switch_id) %
                                        m_conweavePathTable.size()];
                const auto pathEntry2 =
                    m_conweavePathTable[DoHash((uint8_t *)&randPath2, 4, m_switch_id) %
                                        m_conweavePathTable.size()];
                bool goodPath1 = true;
                bool goodPath2 = true;

                if (pathEntry1._pathId == randPath1 &&
                    pathEntry1._invalidTime > now) {  // ECN marked
                    goodPath1 = false;
                }
                if (pathEntry2._pathId == randPath2 &&
                    pathEntry2._invalidTime > now) {  // ECN marked
                    goodPath2 = false;
                }

                if (goodPath1 == true) {
                    tx_md.foundGoodPath = true;
                    tx_md.goodPath = randPath1;
                    // SLB_LOG(PARSE_FIVE_TUPLE(ch) << "--> First trial has good path");
                } else if (goodPath2 == true) {
                    tx_md.foundGoodPath = true;
                    tx_md.goodPath = randPath2;
                    // SLB_LOG(PARSE_FIVE_TUPLE(ch) << "--> Second trial has good path");
                } else {
                    assert(tx_md.foundGoodPath == false);
                    tx_md.goodPath = randPath1;  // random path (unused)
                    // SLB_LOG(PARSE_FIVE_TUPLE(ch) << "--> Cannot find good path, so use current
                    // path");
                }
            } else {
                /* random path selection */
                tx_md.foundGoodPath = true;
                tx_md.goodPath = *(std::next(pathSet.begin(), rand() % pathSet.size()));
            }

            /** PATH: update and get current path */
            /** NOTE: if new connection, set initial random path */
            if (txEntry._pathId == CW_DEFAULT_32BIT) {
                assert(tx_md.newConnection == true);
                txEntry._pathId = tx_md.goodPath;
            }
            if (tx_md.flagExpired) { /* expired -> update path and use the new path */
                if (tx_md.foundGoodPath) {
                    ConWeaveRouting::m_nReRoute += (tx_md.newConnection == false ? 1 : 0);
                    txEntry._pathId = tx_md.goodPath;
                    SLB_LOG(PARSE_FIVE_TUPLE(ch)
                            << "\t#*#*#*#*#*#*#*#*#*#*#*#* EXPIRED -> PATH CHANGED to "
                            << txEntry._pathId << " #*#*#*#*#*#*#*#*#*#*#*#*");
                }
                tx_md.currPath = txEntry._pathId;
            } else if (tx_md.flagReplyTimeout) {  /* reply-timeout -> update path but use the
                                                     previous path (TAIL pkt) */
                tx_md.currPath = txEntry._pathId; /* TAIL uses current path. */
                if (tx_md.foundGoodPath) {        /* next pkts will use the (changed) next path */
                    ConWeaveRouting::m_nReRoute += (tx_md.newConnection == false ? 1 : 0);
                    txEntry._pathId = tx_md.goodPath;
                    SLB_LOG(PARSE_FIVE_TUPLE(ch)
                            << "\t#*#*#*#*#*#*#*#*#*#*#*#* REPLY TIMEOUT -> PATH CHANGED to "
                            << txEntry._pathId << " #*#*#*#*#*#*#*#*#*#*#*#*");
                }
            } else { /* stabilized or usual -> use current path and do not change the path */
                tx_md.currPath = txEntry._pathId;
            }

            /** TAILTIME: Memorize TAIL packet timestamp or get if phase=1 */
            if (tx_md.flagExpired) { /* expiry -> set zero */
                txEntry._tailTime = NanoSeconds(0);
            } else if (tx_md.flagReplyTimeout) { /* reply-timeout -> set now */
                txEntry._tailTime = now;
            } else if (tx_md.flagStabilized) { /* stabilized -> set zero */
                txEntry._tailTime = NanoSeconds(0);
            }
            tx_md.tailTime = txEntry._tailTime.GetNanoSeconds();

            /**
             * SUMMARY: based on tx_md, update packet header
             */
            conweaveDataTag.SetPathId(tx_md.currPath);
            conweaveDataTag.SetHopCount(0);
            conweaveDataTag.SetEpoch(tx_md.epoch);
            conweaveDataTag.SetPhase(tx_md.phase);
            conweaveDataTag.SetTimestampTx(now.GetNanoSeconds());
            conweaveDataTag.SetTimestampTail(tx_md.tailTime);
            if (tx_md.flagExpired || tx_md.flagStabilized) { /* ask reply of INIT */
                conweaveDataTag.SetFlagData(ConWeaveDataTag::INIT);
                assert(tx_md.phase == 0);
            } else if (tx_md.flagReplyTimeout) { /* ask reply of TAIL (CLEAR packet) */
                conweaveDataTag.SetFlagData(ConWeaveDataTag::TAIL);
                assert(tx_md.phase == 0);
            } else {
                conweaveDataTag.SetFlagData(ConWeaveDataTag::DATA);
            }
            p->AddPacketTag(conweaveDataTag);

            uint32_t outDev =
                GetOutPortFromPath(conweaveDataTag.GetPathId(), conweaveDataTag.GetHopCount());
            uint32_t qIndex = ch.udp.pg;
            SLB_LOG(PARSE_FIVE_TUPLE(ch)
                    << "\t--> outDev:" << outDev << ",qIndex:" << qIndex
                    << ",pktEpoch:" << tx_md.epoch << ",pktPhase:" << tx_md.phase
                    << ",tailTime:" << tx_md.tailTime << ",pktPath:" << conweaveDataTag.GetPathId()
                    << ",flag:" << conweaveDataTag.GetFlagData() << " (2:INIT,3:TAIL)");
            DoSwitchSend(p, ch, outDev, qIndex);
            return;
        }
        /***************************
         *  Destination ToR Switch *
         ***************************/
        else if (m_switch_id == dstToRId) {
            if (foundConWeaveDataTag) {  // DATA

                /** PIPELINE: emulating the p4 pipelining */
                conweaveRxMeta rx_md;
                rx_md.pkt_flowkey = GetFlowKey(ch.sip, ch.dip, ch.udp.sport, ch.udp.dport);
                auto &rxEntry = m_conweaveRxTable[rx_md.pkt_flowkey];

                /** INIT: setup flowkey */
                if (rxEntry._flowkey == 0) {
                    rxEntry._flowkey = rx_md.pkt_flowkey;
                    assert(rxEntry._epoch == 1);  // sanity check
                    rx_md.newConnection = true;
                }

                /**
                 * ACTIVE: update active time (for aging)
                 */
                rxEntry._activeTime = now;

                /**
                 * PARSING: parse packet's conweaveDataTag
                 */
                rx_md.pkt_pathId = conweaveDataTag.GetPathId();
                rx_md.pkt_epoch = conweaveDataTag.GetEpoch();
                rx_md.pkt_phase = conweaveDataTag.GetPhase();
                rx_md.pkt_timestamp_Tx = conweaveDataTag.GetTimestampTx();
                rx_md.pkt_timestamp_TAIL = conweaveDataTag.GetTimestampTail();
                rx_md.pkt_flagData = conweaveDataTag.GetFlagData();
                rx_md.pkt_ecnbits = ch.GetIpv4EcnBits(); /* ECN bits */

                /**
                 * ROUND: check epoch: 2(prev), 0(current), or 1(new)
                 */
                if (rxEntry._epoch < rx_md.pkt_epoch) { /* new epoch */
                    rxEntry._epoch = rx_md.pkt_epoch;   /* update to new epoch  */
                    rx_md.resultEpochMatch = 1;
                } else if (rxEntry._epoch > rx_md.pkt_epoch) { /* prev epoch */
                    rx_md.resultEpochMatch = 2;
                } else { /* current epoch */
                    rx_md.resultEpochMatch = 0;
                }

                /** FILTER: if previous epoch, just pass to destination */
                if (rx_md.resultEpochMatch == 2) { /* prev epoch */
                    DoSwitchSendToDev(p, ch);      /* immediately send to dst */
                    return;
                }

                /*------- Current or Next Epoch Pkts -------*/

                /**
                 * PHASE: Phase0-Timestamp, Phase, Phase0-Cache
                 */
                if (rx_md.pkt_phase == 0) { /* update phase0 timestamp */
                    rxEntry._phase0TxTime = NanoSeconds(rx_md.pkt_timestamp_Tx);
                    rxEntry._phase0RxTime = now;
                }
                rx_md.phase0TxTime = rxEntry._phase0TxTime; /* get TxTime of phase 0 */
                rx_md.phase0RxTime = rxEntry._phase0RxTime; /* get RxTime of phase 0 */

                if (rx_md.resultEpochMatch == 1) { /* new epoch */
                    // TAIL -> set phase=1. Otherwise, set phase 0.
                    rxEntry._phase = (rx_md.pkt_flagData == ConWeaveDataTag::TAIL) ? 1 : 0;
                    if (rx_md.pkt_phase > rxEntry._phase) { /* check out-of-order */
                        rx_md.flagOutOfOrder = true;
                    }
                    // phase0-cache
                    rxEntry._phase0Cache = (rx_md.pkt_phase == 0) ? true : false; /* reset/update*/
                    rx_md.flagPhase0Cache = rxEntry._phase0Cache;

                    /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -- */

                    /** DEBUGGING: check there is on-going reordering
                     *  If new epoch progresses but on-going reordering, then current parameter of
                     *  epoch expiration might be too aggressive.
                     *  Try to increase "conweave_txExpiryTime" if this message appears in many
                     *  times.
                     */
                    auto voq = m_voqMap.find(rx_md.pkt_flowkey);
                    if (rxEntry._reordering || voq != m_voqMap.end()) {
                        std::cout
                            << __FILE__ << "(" << __LINE__ << "):" << Simulator::Now() << ","
                            << PARSE_FIVE_TUPLE(ch)
                            << " New epoch packet arrives, but reordering is in progress."
                            << " Maybe TxToR made the epoch progress too aggressively."
                            << " If this is frequent, try to increase `cwh_txExpiryTime` value."
                            << std::endl;

                        if (rxEntry._reordering != (voq != m_voqMap.end())) {
                            std::cout
                                << "--> ERROR: reordering status and VOQ status are different..."
                                << std::endl;
                            assert(rxEntry._reordering == (voq != m_voqMap.end()));
                        }
                    }

                    /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -- */

                } else {                                               /* current epoch */
                    assert(rx_md.resultEpochMatch == 0);               // sanity check
                    if (rx_md.pkt_flagData == ConWeaveDataTag::TAIL) { /* TAIL */
                        if (!rxEntry._reordering) {
                            rxEntry._phase = 1; /* set phase to 1*/
                        } else {
                            /** NOTE: we set phase to 1 AFTER the current reordering VOQ is flushed
                             */
                        }
                    } else if (rxEntry._phase < rx_md.pkt_phase) { /* check out-of-order */
                        rx_md.flagOutOfOrder = true;
                    }
                    // phase0-cache
                    if (rx_md.pkt_phase == 0) {  // update info
                        rxEntry._phase0Cache = true;
                    }
                    rx_md.flagPhase0Cache = rxEntry._phase0Cache;
                }

                /** TAIL: update or read TAIL TIMESTAMP*/
                if (rx_md.pkt_flagData == ConWeaveDataTag::TAIL ||
                    rx_md.pkt_phase == 1) { /* update TAIL Time */
                    rxEntry._tailTime = NanoSeconds(rx_md.pkt_timestamp_TAIL);
                    rx_md.tailTime = rxEntry._tailTime.GetNanoSeconds();
                } else { /* read TAIL Time */
                    rx_md.tailTime = rxEntry._tailTime.GetNanoSeconds();
                }

                /** PREDICTION: PREDICTION OF TAIL_ARRIVAL_TIME
                 * 1) Tx Timegap
                 * 2) Either <now + timegap (phase 0)>, or <tx_TAIL + timegap (phase 1)>
                 **/
                if (rx_md.flagPhase0Cache) { /* phase0-timestamp is available */
                    rx_md.timegapAtTx = (rx_md.tailTime > rx_md.phase0TxTime.GetNanoSeconds())
                                            ? rx_md.tailTime - rx_md.phase0TxTime.GetNanoSeconds()
                                            : 0;

                    /** DEBUGGING: */
                    if (rx_md.pkt_phase == 1 || rx_md.pkt_flagData == ConWeaveDataTag::TAIL) {
                        if (rx_md.tailTime < rx_md.phase0TxTime.GetNanoSeconds()) {
                            SLB_LOG(PARSE_FIVE_TUPLE(ch)
                                    << "** CONWEAVE WARNING - Though this pkt has tailTime, the "
                                       "tailTime is before Phase0-TxTime");
                            SLB_LOG(PARSE_FIVE_TUPLE(ch)
                                    << "** tailTime:" << rx_md.tailTime
                                    << ",Phase0TxTime:" << rx_md.phase0TxTime.GetNanoSeconds());
                            std::cout << "** CONWEAVE WARNING - Though this pkt has tailTime, the "
                                         "tailTime is before Phase0-TxTime"
                                      << std::endl;
                            exit(1);
                        }
                    }
                } else { /* no RTT info, so use default value */
                    rx_md.timegapAtTx = m_defaultVOQWaitingTime.GetNanoSeconds();
                } /* rx_md.timegapAtTx >= 0 */

                if (rx_md.pkt_phase == 1) {          /* phase 1 */
                    if (rx_md.flagOutOfOrder) {      /* if out-of-order of phase 1 */
                        if (rx_md.flagPhase0Cache) { /* phase0 info exists */
                            rx_md.timeExpectedToFlush =
                                rx_md.phase0RxTime.GetNanoSeconds() + rx_md.timegapAtTx +
                                m_extraVOQFlushTime.GetNanoSeconds(); /* phase0 Rx + timegap */
                        } else {                                      /* no phase0 info */
                            rx_md.timeExpectedToFlush =
                                now.GetNanoSeconds() + rx_md.timegapAtTx +
                                m_extraVOQFlushTime.GetNanoSeconds(); /* now + timegap */
                        }
                    } else { /* not out-of-order */
                        rx_md.timeExpectedToFlush = 0;
                    }
                } else {                                               /* phase 0 */
                    assert(rx_md.flagPhase0Cache);                     // sanity check
                    if (rx_md.pkt_flagData == ConWeaveDataTag::TAIL) { /* TAIL -> Flush VOQ!! */
                        rx_md.timeExpectedToFlush =
                            now.GetNanoSeconds() +
                            1; /* reschedule to flush after 1ns (almost immediately)*/
                    } else {   /* otherwise */
                        rx_md.timeExpectedToFlush = now.GetNanoSeconds() + rx_md.timegapAtTx +
                                                    m_extraVOQFlushTime.GetNanoSeconds();
                    }
                }

                /**
                 * DEBUGGING: print for debugging
                 */
                SLB_LOG(PARSE_FIVE_TUPLE(ch)
                        << "[RxToR] Sw(" << m_switch_id << "),PktEpoch:" << rx_md.pkt_epoch
                        << ",RegEpoch:" << rxEntry._epoch << ",PktPhase:" << rx_md.pkt_phase
                        << ",RegPhase:" << rxEntry._phase << ",DataFlag:" << rx_md.pkt_flagData
                        << ",OoO:" << rx_md.flagOutOfOrder << ",Cch:" << rx_md.flagPhase0Cache
                        << ",Flowkey:" << rx_md.pkt_flowkey);
                SLB_LOG(PARSE_FIVE_TUPLE(ch)
                        << "--> DEBUG - #VOQ:" << m_voqMap.size());  // debugging

                /**
                 * RESCHEDULE: reschedule of VOQ flush time
                 */
                if (rx_md.pkt_phase == 0) {    /* phase 0 -> update if currently reordering */
                    if (rxEntry._reordering) { /* reordering */
                        if (rx_md.pkt_flagData == ConWeaveDataTag::TAIL) {
                            ConWeaveRouting::m_nFlushVOQByTail += 1; /* debugging */
                        }
                        /* new deadline */
                        auto voq = m_voqMap.find(rx_md.pkt_flowkey);
                        assert(voq != m_voqMap.end());  // sanity check

                        rx_md.timeExpectedToFlush =
                            (rx_md.timeExpectedToFlush > now.GetNanoSeconds())
                                ? (rx_md.timeExpectedToFlush - now.GetNanoSeconds())
                                : 0;
                        voq->second.RescheduleFlush(
                            NanoSeconds(rx_md.timeExpectedToFlush)); /* new deadline */
                        SLB_LOG(PARSE_FIVE_TUPLE(ch)
                                << "--> Phase 0 while OoO"
                                << ",VOQ size:" << voq->second.getQueueSize() + 1
                                << ",NextFlushTime:" << NanoSeconds(rx_md.timeExpectedToFlush)
                                << "(TxTimegap:" << rx_md.timegapAtTx
                                << ",Phase0 Tx:" << rx_md.phase0TxTime
                                << ",Phase0 Rx:" << rx_md.phase0RxTime << ")");
                    }
                } else {                          /* phase 1 */
                    if (rx_md.flagOutOfOrder) {   /* out-of-order */
                        rx_md.flagEnqueue = true; /* enqueue */

                        if (rxEntry._reordering) { /* reordering is on-going */
                            auto voq = m_voqMap.find(rx_md.pkt_flowkey);
                            assert(voq != m_voqMap.end());  // sanity check
                            SLB_LOG(PARSE_FIVE_TUPLE(ch)
                                    << "--> SUBSEQ OoO"
                                    << ",VOQ size:" << voq->second.getQueueSize() + 1
                                    << ",No New Deadline Update");

                        } else { /* new out-of-order */
                            rxEntry._reordering = true;
                            ConWeaveVOQ &voq = m_voqMap[rx_md.pkt_flowkey];

                            rx_md.timeExpectedToFlush =
                                (rx_md.timeExpectedToFlush > now.GetNanoSeconds())
                                    ? (rx_md.timeExpectedToFlush - now.GetNanoSeconds())
                                    : 0;
                            voq.Set(rx_md.pkt_flowkey, ch.dip,
                                    NanoSeconds(rx_md.timeExpectedToFlush),
                                    m_extraVOQFlushTime); /* new deadline */
                            voq.m_deleteCallback = MakeCallback(&ConWeaveRouting::DeleteVOQ, this);
                            voq.m_CallbackByVOQFlush =
                                MakeCallback(&ConWeaveRouting::CallbackByVOQFlush, this);
                            voq.m_switchSendToDevCallback =
                                MakeCallback(&ConWeaveRouting::DoSwitchSendToDev, this);
                            SLB_LOG(PARSE_FIVE_TUPLE(ch)
                                    << "--> FIRST OoO"
                                    << ",VOQ size:" << voq.getQueueSize() + 1
                                    << ",NextFlushTime:" << NanoSeconds(rx_md.timeExpectedToFlush)
                                    << "(TxTimegap:" << rx_md.timegapAtTx << ",P0Tx:"
                                    << rx_md.phase0TxTime << ",P0Rx:" << rx_md.phase0RxTime << ")");
                        }
                    } else { /* in-order */
                        assert(rxEntry._reordering == false);
                        assert(m_voqMap.find(rx_md.pkt_flowkey) == m_voqMap.end());
                    }
                }

                /**
                 * NOTIFY: Generate NOTIFY packet if ECN marked
                 */
                if (m_pathAwareRerouting) {
                    if (rx_md.pkt_ecnbits == 0x03) {
                        SendNotify(p, ch, rx_md.pkt_pathId);
                    }
                }

                /**
                 * REPLY: send reply if requested
                 */
                if (rx_md.pkt_flagData == ConWeaveDataTag::INIT) {
                    assert(rx_md.pkt_phase == 0);  // sanity check
                    SendReply(p, ch, ConWeaveReplyTag::INIT, rx_md.pkt_epoch);
                }
                if (rx_md.pkt_flagData == ConWeaveDataTag::TAIL) {
                    assert(rx_md.pkt_phase == 0);  // sanity check
                    SendReply(p, ch, ConWeaveReplyTag::TAIL,
                              rx_md.pkt_epoch);  // send reply
                }

                /**
                 * ENQUEUE: enqueue the packet
                 */
                if (rx_md.flagEnqueue) {
                    m_voqMap[rx_md.pkt_flowkey].Enqueue(p);
                    m_nOutOfOrderPkts++;
                    return;
                }

                /**
                 * SEND: send to end-host
                 */
                DoSwitchSendToDev(p, ch);
                return;
            }

            if (foundConWeaveReplyTag) {  // Received REPLY
                conweaveTxMeta tx_md;
                tx_md.pkt_flowkey = GetFlowKey(ch.dip, ch.sip, ch.udp.dport, ch.udp.sport);
                tx_md.reply_flag = conweaveReplyTag.GetFlagReply();
                tx_md.reply_epoch = conweaveReplyTag.GetEpoch();
                tx_md.reply_phase = conweaveReplyTag.GetPhase();
                auto &txEntry = m_conweaveTxTable[tx_md.pkt_flowkey];

                /**
                 * CHECK: Reply timeout check only when epoch&phase are matched
                 */
                if (tx_md.reply_epoch == txEntry._epoch && tx_md.reply_phase == txEntry._phase) {
                    if (tx_md.reply_flag == ConWeaveReplyTag::INIT) { /* reply of INIT */
                        if (now < txEntry._replyTimer &&
                            txEntry._replyTimer != CW_MAX_TIME) { /* if replied timely */
                            txEntry._stabilized = true;           /* stabilized */
                            txEntry._replyTimer = CW_MAX_TIME;    /* no more timeout */
                            ConWeaveRouting::m_nTimelyInitReplied += 1;
                            SLB_LOG(PARSE_REVERSE_FIVE_TUPLE(ch)
                                    << "[TxToR/GotReplied] Sw(" << m_switch_id << "),PktEpoch:"
                                    << tx_md.reply_epoch << ",PktPhase:" << tx_md.reply_phase
                                    << ",ReplyFlag:" << tx_md.reply_flag << ",ReplyDL"
                                    << txEntry._replyTimer);
                            SLB_LOG(
                                PARSE_REVERSE_FIVE_TUPLE(ch)
                                << "--------------------------------->>> INIT Replied timely!!");
                        } else { /* late reply -> ignore */
                            /* do nothing */
                        }
                    }
                    if (tx_md.reply_flag == ConWeaveReplyTag::TAIL) { /* reply of TAIL */
                        txEntry._stabilized =
                            true;  // out-of-order issue is resolved for this "flowcut"
                        txEntry._replyTimer = CW_MAX_TIME; /* no more timeout */
                        ConWeaveRouting::m_nTimelyTailReplied += 1;
                        SLB_LOG(PARSE_REVERSE_FIVE_TUPLE(ch)
                                << "[TxToR/GotReplied] Sw(" << m_switch_id << "),PktEpoch:"
                                << tx_md.reply_epoch << ",PktPhase:" << tx_md.reply_phase
                                << ",ReplyFlag:" << tx_md.reply_flag << ",ReplyDL"
                                << txEntry._replyTimer);
                        SLB_LOG(PARSE_REVERSE_FIVE_TUPLE(ch)
                                << "-------------------------------------->>> TAIL Replied!!");
                    }
                }
                return;  // drop this reply
            }

            if (m_pathAwareRerouting) {        // Received NOTIFY
                if (foundConWeaveNotifyTag) {  // Received NOTIFY (from ECN)
                    conweaveTxMeta tx_md;
                    auto congestedPathId = conweaveNotifyTag.GetPathId();
                    auto &pathEntry =
                        m_conweavePathTable[DoHash((uint8_t *)&congestedPathId, 4, m_switch_id) %
                                            m_conweavePathTable.size()];
                    SLB_LOG(PARSE_REVERSE_FIVE_TUPLE(ch)
                            << "[TxToR/GotNOTIFY] Sw(" << m_switch_id
                            << ") =-*=-*=-*=-*=-*=-*=-=-*>>> pathId:" << congestedPathId);

                    /**
                     * UPDATE: if entry is expired, overwrite not to use the congested path
                     */
                    pathEntry._pathId = congestedPathId;
                    pathEntry._invalidTime = now + m_pathPauseTime;
                    return;  // drop this NOTIFY
                }
            }

            /**
             * NOTAG: impossible
             */
            SLB_LOG(PARSE_FIVE_TUPLE(ch) << "Sw(" << m_switch_id << "),isToR:" << m_isToR);
            std::cout << __FILE__ << "(" << __LINE__ << "):" << Simulator::Now() << ","
                      << PARSE_FIVE_TUPLE(ch) << std::endl;
            assert(false && "No Tag is impossible");
        }
        // should not reach here (TOR, but neither TxToR nor RxToR)
        SLB_LOG(PARSE_FIVE_TUPLE(ch) << "Sw(" << m_switch_id << "),isToR:" << m_isToR);
        std::cout << __FILE__ << "(" << __LINE__ << "):" << Simulator::Now() << ","
                  << PARSE_FIVE_TUPLE(ch) << std::endl;
        printf(
            "[ERROR] TxToR: %u, RxToR: %u, Current SwitchId: %u, isToR: %u, CwhData: %u, REPLY:%u, "
            "NOTIFY:%u\n",
            srcToRId, dstToRId, m_switch_id, m_isToR, foundConWeaveDataTag, foundConWeaveReplyTag,
            foundConWeaveNotifyTag);
        assert(false);
    }

    /******************************
     *  Non-ToR Switch (Core/Agg) *
     ******************************/
    if (foundConWeaveDataTag) {  // UDP with PathId
        // update hopCount
        uint32_t hopCount = conweaveDataTag.GetHopCount() + 1;
        conweaveDataTag.SetHopCount(hopCount);

        // get outPort
        uint32_t outDev =
            GetOutPortFromPath(conweaveDataTag.GetPathId(), conweaveDataTag.GetHopCount());
        uint32_t qIndex = ch.udp.pg;

        // re-serialize tag
        ConWeaveDataTag temp_tag;
        p->RemovePacketTag(temp_tag);
        p->AddPacketTag(conweaveDataTag);

        // send packet
        SLB_LOG(PARSE_FIVE_TUPLE(ch) << "[NonToR/DATA] Sw(" << m_switch_id << "),"
                                     << "outDev:" << outDev << ",qIndex:" << qIndex
                                     << ",PktEpoch:" << conweaveDataTag.GetEpoch()
                                     << ",PktPhase:" << conweaveDataTag.GetPhase());
        DoSwitchSend(p, ch, outDev, qIndex);
        return;
    }

    // UDP with ECMP
    SLB_LOG("[NonToR/ECMP] Sw(" << m_switch_id << ")," << PARSE_FIVE_TUPLE(ch));
    DoSwitchSendToDev(p, ch);
    return;
}

void ConWeaveRouting::SetConstants(Time extraReplyDeadline, Time extraVOQFlushTime,
                                   Time txExpiryTime, Time defaultVOQWaitingTime,
                                   Time pathPauseTime, bool pathAwareRerouting) {
    NS_LOG_FUNCTION("Setup new parameters at sw" << m_switch_id);
    m_extraReplyDeadline = extraReplyDeadline;
    m_extraVOQFlushTime = extraVOQFlushTime;
    m_txExpiryTime = txExpiryTime;
    m_defaultVOQWaitingTime = defaultVOQWaitingTime;
    m_pathPauseTime = pathPauseTime;
    m_pathAwareRerouting = pathAwareRerouting;
    assert(m_pathAwareRerouting);  // by default, path-aware routing
}

void ConWeaveRouting::SetSwitchInfo(bool isToR, uint32_t switch_id) {
    m_isToR = isToR;
    m_switch_id = switch_id;
}

/** CALLBACK: callback functions  */
void ConWeaveRouting::DoSwitchSend(Ptr<Packet> p, CustomHeader &ch, uint32_t outDev,
                                   uint32_t qIndex) {
    m_switchSendCallback(p, ch, outDev, qIndex);
}
void ConWeaveRouting::DoSwitchSendToDev(Ptr<Packet> p, CustomHeader &ch) {
    m_switchSendToDevCallback(p, ch);
}

// used for callback in VOQ
void ConWeaveRouting::DeleteVOQ(uint64_t flowkey) { m_voqMap.erase(flowkey); }

void ConWeaveRouting::CallbackByVOQFlush(uint64_t flowkey, uint32_t voqSize) {
    SLB_LOG(
        "#################################################################### VOQ FLush, flowkey: "
        << flowkey << ",VOQ size:" << voqSize << "#################");  // debugging

    m_historyVOQSize.push_back(voqSize);  // statistics - track VOQ size
    // update RxEntry
    auto &rxEntry = m_conweaveRxTable[flowkey];  // flowcut entry
    assert(rxEntry._flowkey == flowkey);         // sanity check
    assert(rxEntry._reordering == true);         // sanity check

    rxEntry._reordering = false;
    rxEntry._phase = 1;
}

void ConWeaveRouting::SetSwitchSendCallback(SwitchSendCallback switchSendCallback) {
    m_switchSendCallback = switchSendCallback;
}

void ConWeaveRouting::SetSwitchSendToDevCallback(SwitchSendToDevCallback switchSendToDevCallback) {
    m_switchSendToDevCallback = switchSendToDevCallback;
}

uint32_t ConWeaveRouting::GetVolumeVOQ() {
    uint32_t nTotalPkts = 0;
    for (auto voq : m_voqMap) {
        nTotalPkts += voq.second.getQueueSize();
    }
    return nTotalPkts;
}

void ConWeaveRouting::AgingEvent() {
    auto now = Simulator::Now();

    auto itr1 = m_conweaveTxTable.begin();
    while (itr1 != m_conweaveTxTable.end()) {
        if (now - ((itr1->second)._activeTime) > m_agingTime) {
            itr1 = m_conweaveTxTable.erase(itr1);
        } else {
            ++itr1;
        }
    }

    auto itr2 = m_conweaveRxTable.begin();
    while (itr2 != m_conweaveRxTable.end()) {
        if (now - ((itr2->second)._activeTime) > m_agingTime) {
            itr2 = m_conweaveRxTable.erase(itr2);
        } else {
            ++itr2;
        }
    }

    m_agingEvent = Simulator::Schedule(m_agingTime, &ConWeaveRouting::AgingEvent, this);
}

}  // namespace ns3