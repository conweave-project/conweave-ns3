#include <ns3/log.h>
#include "app-recv-buffer.h"
#include <ostream>

namespace ns3 {

    NS_LOG_COMPONENT_DEFINE("AppRecvBuffer");
    static const std::string socketId = "AppRecvBuffer";

    std::ostream &operator<<(std::ostream &os, const AppRecvBuffer &im)
    {
        os << "Received up to " << im.cumAck;
        if (im.m_data.size())
            os << " (SACK ";

        auto it = im.m_data.begin();
        for(;it != im.m_data.end();++it) {
            uint32_t blockBegin = it->first; // inclusive
            uint32_t blockEnd = it->first + it->second; // exclusive
            os << blockBegin << "-" << blockEnd << " ";
        }
        if (im.m_data.size())
            os << ")";
        return os;
    }

    uint32_t AppRecvBuffer::receivedBytes() const {
        uint32_t bytes = cumAck;
        auto it = m_data.begin();
        for(;it != m_data.end();++it) {
            bytes += it->second;
        }
        return bytes;
    }

    // put blocks
    void AppRecvBuffer::sack(uint32_t seq, uint32_t sz) {
        if(!sz) return;
        NS_LOG_LOGIC("Flow " << socketId << " : Inserting Block " << seq << "-" << (seq + sz));
        uint32_t seqEnd = seq + sz; //exclusive

        auto it = m_data.begin();
        for(;it != m_data.end();++it) {
            uint32_t blockBegin = it->first; // inclusive
            uint32_t blockEnd = it->first + it->second; // exclusive
            
            if(blockBegin <= seq && seqEnd <= blockEnd) {
                // seq-seqEnd is included inside block-blockEnd
                return;
            } else if (seq < blockBegin && blockEnd < seqEnd) {
                // block-blockEnd is included inside Endseq-seqEnd
                // first segment : seq - blockBegin
                // second segment : blockEnd - seqEnd
                m_data.insert(it, std::pair<uint32_t, uint32_t>(seq, blockBegin-seq));
                NS_LOG_LOGIC("Flow " << socketId << " : Inserting Seg " << seq << "-" << blockBegin);
                seq = blockEnd;
                sz = seqEnd - blockEnd;
                seqEnd = seq + sz;
            } else if (seq < blockBegin && seqEnd <= blockBegin) {
                // seq-seqEnd is mutually exclusive to block-blockEnd, and smaller than block-blockEnd
                m_data.insert(it, std::pair<uint32_t, uint32_t>(seq, sz));
                NS_LOG_LOGIC("Flow " << socketId << " : Inserting Seg (Mutex)" << seq << "-" << (seq+sz));
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
                m_data.insert(it, std::pair<uint32_t, uint32_t>(seq, blockBegin-seq));
                sz = 0;
                break;
            } else {
                NS_ASSERT(blockEnd <= seq);
            }
        }
        if(sz){
            NS_LOG_LOGIC("Flow " << socketId << " : Inserting Seg (rem) " << seq << "-" <<  (seq+sz));
            m_data.insert(it, std::pair<uint32_t, uint32_t>(seq, sz));
        }
        NS_ASSERT(m_data.size() > 0);

        // Sanity check : check duplicate, empty blocks
        // merge neighboring blocks
        auto it_prev = m_data.begin();
        for(it=m_data.begin(); it != m_data.end();) {
            if(it == it_prev) {
                ++it;
                continue;
            }
            NS_ASSERT(it_prev->first + it_prev->second <= it->first);
            NS_ASSERT(it->second > 0);
            if(it_prev->first + it_prev->second == it->first) {
                // merge neighboring block
                NS_LOG_LOGIC("Flow " << socketId << " : Merging Block " << it_prev->first << "-" << (it_prev->first + it_prev->second) << " and " << it->first << "-" << (it->first + it->second));
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
    size_t AppRecvBuffer::discardUpTo(uint32_t cumAck){
        auto it = m_data.begin();
        size_t erase_len = 0;
        for (; it != m_data.end();)
        {
            if(it->first + it->second <= cumAck) {
                NS_LOG_LOGIC("Flow " << socketId << " : Removing under " << cumAck << " - Removed Whole " << it->first << "-" << (it->first + it->second));
                erase_len += it->second;
                it = m_data.erase(it);
            } else if(it->first < cumAck) { // do we need equal here? Maybe not
                NS_LOG_LOGIC("Flow " << socketId << " : Removing under " << cumAck << " - Removed Part  " << it->first << "-" << (cumAck) << " of Entire part " << it->first << "-" << (it->first + it->second));
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

    void AppRecvBuffer::recv(uint32_t seq, uint32_t len) {
        if (seq + len <= cumAck)
            return;

        if (seq <= cumAck && seq + len > cumAck){ 
            cumAck = seq + len;
            if (m_data.size() && m_data.begin()->first <= cumAck) {
                cumAck += m_data.begin()->second - (cumAck - m_data.begin()->first);
            }
            discardUpTo(cumAck);
        }

        if (seq > cumAck) {
            sack(seq, len);
        }
        
    }
};