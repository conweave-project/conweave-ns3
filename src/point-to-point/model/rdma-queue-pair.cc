#include "rdma-queue-pair.h"

#include <ns3/hash.h>
#include <ns3/ipv4-header.h>
#include <ns3/log.h>
#include <ns3/seq-ts-header.h>
#include <ns3/simulator.h>
#include <ns3/udp-header.h>
#include <ns3/uinteger.h>

#include "ns3/ppp-header.h"
#include "ns3/settings.h"
#include "rdma-hw.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("RdmaQueuePair");

/**************************
 * RdmaQueuePair
 *************************/
TypeId RdmaQueuePair::GetTypeId(void) {
    static TypeId tid = TypeId("ns3::RdmaQueuePair").SetParent<Object>();
    return tid;
}

RdmaQueuePair::RdmaQueuePair(uint16_t pg, Ipv4Address _sip, Ipv4Address _dip, uint16_t _sport,
                             uint16_t _dport) {
    startTime = Simulator::Now();
    sip = _sip;
    dip = _dip;
    sport = _sport;
    dport = _dport;
    m_size = 0;
    snd_nxt = snd_una = 0;
    m_pg = pg;
    m_ipid = 0;
    m_win = 0;
    m_baseRtt = 0;
    m_max_rate = 0;
    m_var_win = false;
    m_rate = 0;
    m_nextAvail = Time(0);
    mlx.m_alpha = 1;
    mlx.m_alpha_cnp_arrived = false;
    mlx.m_first_cnp = true;
    mlx.m_decrease_cnp_arrived = false;
    mlx.m_rpTimeStage = 0;
    hp.m_lastUpdateSeq = 0;
    for (uint32_t i = 0; i < sizeof(hp.keep) / sizeof(hp.keep[0]); i++) hp.keep[i] = 0;
    hp.m_incStage = 0;
    hp.m_lastGap = 0;
    hp.u = 1;
    for (uint32_t i = 0; i < IntHeader::maxHop; i++) {
        hp.hopState[i].u = 1;
        hp.hopState[i].incStage = 0;
    }

    tmly.m_lastUpdateSeq = 0;
    tmly.m_incStage = 0;
    tmly.lastRtt = 0;
    tmly.rttDiff = 0;

    dctcp.m_lastUpdateSeq = 0;
    dctcp.m_caState = 0;
    dctcp.m_highSeq = 0;
    dctcp.m_alpha = 1;
    dctcp.m_ecnCnt = 0;
    dctcp.m_batchSizeOfAlpha = 0;

    irn.m_enabled = false;
    irn.m_highest_ack = 0;
    irn.m_max_seq = 0;
    irn.m_recovery = false;

    m_timeout = MilliSeconds(4);
}

void RdmaQueuePair::SetSize(uint64_t size) { m_size = size; }

void RdmaQueuePair::SetWin(uint32_t win) { m_win = win; }

void RdmaQueuePair::SetBaseRtt(uint64_t baseRtt) { m_baseRtt = baseRtt; }

void RdmaQueuePair::SetVarWin(bool v) { m_var_win = v; }

void RdmaQueuePair::SetFlowId(int32_t v) {
    m_flow_id = v;
    irn.m_sack.socketId = v;
}

void RdmaQueuePair::SetTimeout(Time v) { m_timeout = v; }

uint64_t RdmaQueuePair::GetBytesLeft() {
    if (irn.m_enabled) {
        uint32_t sack_seq, sack_sz;
        if (irn.m_sack.peekFrontBlock(&sack_seq, &sack_sz)) {
            if (snd_nxt == sack_seq) {
                snd_nxt += sack_sz;
                irn.m_sack.discardUpTo(snd_nxt);
            }
        }
    }

    return m_size >= snd_nxt ? m_size - snd_nxt : 0;
}

uint32_t RdmaQueuePair::GetHash(void) {
    union {
        struct {
            uint32_t sip, dip;
            uint16_t sport, dport;
        };
        char c[12];
    } buf;
    buf.sip = sip.Get();
    buf.dip = dip.Get();
    buf.sport = sport;
    buf.dport = dport;
    return Hash32(buf.c, 12);
}

void RdmaQueuePair::Acknowledge(uint64_t ack) {
    if (ack > snd_una) {
        snd_una = ack;
    }
}

uint64_t RdmaQueuePair::GetOnTheFly() {
    NS_ASSERT(snd_nxt >= snd_una);
    return snd_nxt - snd_una;
}

bool RdmaQueuePair::IsWinBound() {
    uint64_t w = GetWin();
    return w != 0 && GetOnTheFly() >= w;
}

uint64_t RdmaQueuePair::GetWin() {
    if (m_win == 0) return 0;
    uint64_t w;
    if (m_var_win) {
        w = m_win * m_rate.GetBitRate() / m_max_rate.GetBitRate();
        if (w == 0) w = 1;  // must > 0
    } else {
        w = m_win;
    }
    return w;
}

uint64_t RdmaQueuePair::HpGetCurWin() {
    if (m_win == 0) return 0;
    uint64_t w;
    if (m_var_win) {
        w = m_win * hp.m_curRate.GetBitRate() / m_max_rate.GetBitRate();
        if (w == 0) w = 1;  // must > 0
    } else {
        w = m_win;
    }
    return w;
}

bool RdmaQueuePair::IsFinished() {
    if (irn.m_enabled) {
        uint32_t sack_seq, sack_sz;
        if (irn.m_sack.peekFrontBlock(&sack_seq, &sack_sz)) {
            if (snd_nxt == sack_seq) {
                snd_nxt += sack_sz;
                irn.m_sack.discardUpTo(snd_nxt);
            }
        }
    }

    return snd_una >= m_size;
}

/*********************
 * RdmaRxQueuePair
 ********************/
TypeId RdmaRxQueuePair::GetTypeId(void) {
    static TypeId tid = TypeId("ns3::RdmaRxQueuePair").SetParent<Object>();
    return tid;
}

RdmaRxQueuePair::RdmaRxQueuePair() {
    sip = dip = sport = dport = 0;
    m_ipid = 0;
    ReceiverNextExpectedSeq = 0;
    m_nackTimer = Time(0);
    m_milestone_rx = 0;
    m_lastNACK = 0;
}

uint32_t RdmaRxQueuePair::GetHash(void) {
    union {
        struct {
            uint32_t sip, dip;
            uint16_t sport, dport;
        };
        char c[12];
    } buf;
    buf.sip = sip;
    buf.dip = dip;
    buf.sport = sport;
    buf.dport = dport;
    return Hash32(buf.c, 12);
}

/*********************
 * RdmaQueuePairGroup
 ********************/
TypeId RdmaQueuePairGroup::GetTypeId(void) {
    static TypeId tid = TypeId("ns3::RdmaQueuePairGroup").SetParent<Object>();
    return tid;
}

RdmaQueuePairGroup::RdmaQueuePairGroup(void) { memset(m_qp_finished, 0, sizeof(m_qp_finished)); }

uint32_t RdmaQueuePairGroup::GetN(void) { return m_qps.size(); }

Ptr<RdmaQueuePair> RdmaQueuePairGroup::Get(uint32_t idx) { return m_qps[idx]; }

Ptr<RdmaQueuePair> RdmaQueuePairGroup::operator[](uint32_t idx) { return m_qps[idx]; }

void RdmaQueuePairGroup::AddQp(Ptr<RdmaQueuePair> qp) { m_qps.push_back(qp); }

// void RdmaQueuePairGroup::AddRxQp(Ptr<RdmaRxQueuePair> rxQp){
// 	m_rxQps.push_back(rxQp);
// }

void RdmaQueuePairGroup::Clear(void) { m_qps.clear(); }

IrnSackManager::IrnSackManager() {}

IrnSackManager::IrnSackManager(int flow_id) { socketId = flow_id; }

std::ostream& operator<<(std::ostream& os, const IrnSackManager& im) {
    auto it = im.m_data.begin();
    for (; it != im.m_data.end(); ++it) {
        uint32_t blockBegin = it->first;             // inclusive
        uint32_t blockEnd = it->first + it->second;  // exclusive
        os << "[" << blockBegin << "-" << blockEnd << "] ";
    }
    return os;
}

// put blocks
void IrnSackManager::sack(uint32_t seq, uint32_t sz) {
    if (!sz) return;
    NS_LOG_LOGIC("Flow " << socketId << " : Inserting Block " << seq << "-" << (seq + sz));
    uint32_t seqEnd = seq + sz;  // exclusive

    auto it = m_data.begin();
    for (; it != m_data.end(); ++it) {
        uint32_t blockBegin = it->first;             // inclusive
        uint32_t blockEnd = it->first + it->second;  // exclusive

        if (blockBegin <= seq && seqEnd <= blockEnd) {
            // seq-seqEnd is included inside block-blockEnd
            return;
        } else if (seq < blockBegin && blockEnd < seqEnd) {
            // block-blockEnd is included inside Endseq-seqEnd
            // first segment : seq - blockBegin
            // second segment : blockEnd - seqEnd
            m_data.insert(it, std::pair<uint32_t, uint32_t>(seq, blockBegin - seq));
            NS_LOG_LOGIC("Flow " << socketId << " : Inserting Seg " << seq << "-" << blockBegin);
            seq = blockEnd;
            sz = seqEnd - blockEnd;
            seqEnd = seq + sz;
        } else if (seq < blockBegin && seqEnd <= blockBegin) {
            // seq-seqEnd is mutually exclusive to block-blockEnd, and smaller than block-blockEnd
            m_data.insert(it, std::pair<uint32_t, uint32_t>(seq, sz));
            NS_LOG_LOGIC("Flow " << socketId << " : Inserting Seg (Mutex)" << seq << "-"
                                 << (seq + sz));
            sz = 0;
            break;
        } else if (blockBegin <= seq && seq <= blockEnd && blockEnd < seqEnd) {
            // front part of seq-seqEnd is overlapped
            // new segment : blockEnd - seqEnd
            seq = blockEnd;
            sz = seqEnd - blockEnd;
        } else if (seq < blockBegin && blockBegin <= seqEnd && seqEnd <= blockEnd) {
            // rear part of seq-seqEnd is overlapped
            // new segment : seq - blockBegin
            m_data.insert(it, std::pair<uint32_t, uint32_t>(seq, blockBegin - seq));
            sz = 0;
            break;
        } else {
            NS_ASSERT(blockEnd <= seq);
        }
    }
    if (sz) {
        NS_LOG_LOGIC("Flow " << socketId << " : Inserting Seg (rem) " << seq << "-" << (seq + sz));
        m_data.insert(it, std::pair<uint32_t, uint32_t>(seq, sz));
    }
    NS_ASSERT(m_data.size() > 0);

    // Sanity check : check duplicate, empty blocks
    // merge neighboring blocks
    auto it_prev = m_data.begin();
    for (it = m_data.begin(); it != m_data.end();) {
        if (it == it_prev) {
            ++it;
            continue;
        }
        NS_ASSERT(it_prev->first + it_prev->second <= it->first);
        NS_ASSERT(it->second > 0);
        if (it_prev->first + it_prev->second == it->first) {
            // merge neighboring block
            NS_LOG_LOGIC("Flow " << socketId << " : Merging Block " << it_prev->first << "-"
                                 << (it_prev->first + it_prev->second) << " and " << it->first
                                 << "-" << (it->first + it->second));
            it_prev->second += it->second;
            it = m_data.erase(it);
        } else {
            it_prev = it;
            ++it;
        }
    }

    NS_LOG_LOGIC("Flow " << socketId << " : Blocks " << *this);
}

// put into return number of blocks removed
size_t IrnSackManager::discardUpTo(uint32_t cumAck) {
    auto it = m_data.begin();
    size_t erase_len = 0;
    for (; it != m_data.end();) {
        if (it->first + it->second <= cumAck) {
            NS_LOG_LOGIC("Flow " << socketId << " : Removing under " << cumAck
                                 << " - Removed Whole " << it->first << "-"
                                 << (it->first + it->second));
            erase_len += it->second;
            it = m_data.erase(it);
        } else if (it->first < cumAck) {  // do we need equal here? Maybe not
            NS_LOG_LOGIC("Flow " << socketId << " : Removing under " << cumAck
                                 << " - Removed Part  " << it->first << "-" << (cumAck)
                                 << " of Entire part " << it->first << "-"
                                 << (it->first + it->second));
            erase_len += cumAck - it->first;
            it->second = it->first + it->second - cumAck;
            it->first = cumAck;
            NS_ASSERT(it->second != 0);
            break;
        } else {
            break;
        }
    }
    return erase_len;
}

bool IrnSackManager::IsEmpty() { return !m_data.size(); }

bool IrnSackManager::blockExists(uint32_t seq, uint32_t size) {
    // query if block exists inside SACK table
    auto it = m_data.begin();
    for (; it != m_data.end(); ++it) {
        if (it->first <= seq && seq + size <= it->first + it->second) {
            return true;
        }
    }
    return false;
}
bool IrnSackManager::peekFrontBlock(uint32_t* pseq, uint32_t* psize) {
    NS_ASSERT(pseq);
    NS_ASSERT(psize);

    if (!m_data.size()) {
        *pseq = 0;
        *psize = 0;
        return false;
    }

    auto it = m_data.begin();
    *pseq = it->first;
    *psize = it->second;
    return true;
}

size_t IrnSackManager::getSackBufferOverhead() {
    size_t overhead = 0;
    auto it = m_data.begin();
    for (; it != m_data.end(); ++it) {
        overhead += it->second;  // Bytes
    }
    return overhead;
}

}  // namespace ns3
