/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2019 KAIST
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
 * Authors: Hwijoon Lim <wjuni@kaist.ac.kr>
 */

#include "selective-packet-queue.h"
#include "ns3/log.h"
using namespace std;

namespace ns3 {
    NS_LOG_COMPONENT_DEFINE("SelectivePacketQueue");

	NS_OBJECT_ENSURE_REGISTERED(SelectivePacketQueue);

	TypeId SelectivePacketQueue::GetTypeId(void)
	{
		static TypeId tid = TypeId("ns3::SelectivePacketQueue")
            .SetParent<Object> ()
            .SetGroupName ("Internet")
			.AddConstructor<SelectivePacketQueue>();
		return tid;
	}

    TypeId SelectivePacketQueue::GetInstanceTypeId (void) const
    {
    NS_LOG_FUNCTION (this);
    return GetTypeId ();
    }

    ostream& operator<<(ostream& os, const SelectivePacketQueue& p) {
        for(auto it = p.m_data.begin(); it != p.m_data.end(); ++it) {
            os << it->first << "-" << (it->first + it->second) << " ";
        }
        return os;
    }

    
    void SelectivePacketQueue::push(SequenceNumber32 seq, uint32_t sz) {
        if(!sz) return;
        NS_LOG_LOGIC("Flow " << socketId << " : Inserting Block " << seq << "-" << (seq + sz));
        SequenceNumber32 seqEnd = seq + sz; //exclusive

        auto it = m_data.begin();
        for(;it != m_data.end();++it) {
            SequenceNumber32 blockBegin = it->first; // inclusive
            SequenceNumber32 blockEnd = it->first + it->second; // exclusive
            
            if(blockBegin <= seq && seqEnd <= blockEnd) {
                // seq-seqEnd is included inside block-blockEnd
                return;
            } else if (seq < blockBegin && blockEnd < seqEnd) {
                // block-blockEnd is included inside Endseq-seqEnd
                // first segment : seq - blockBegin
                // second segment : blockEnd - seqEnd
                m_data.insert(it, pair<SequenceNumber32, uint32_t>(seq, blockBegin-seq));
                NS_LOG_LOGIC("Flow " << socketId << " : Inserting Seg " << seq << "-" << blockBegin);
                m_dirty = true;
                seq = blockEnd;
                sz = seqEnd - blockEnd;
                seqEnd = seq + sz;
            } else if (seq < blockBegin && seqEnd <= blockBegin) {
                // seq-seqEnd is mutually exclusive to block-blockEnd, and smaller than block-blockEnd
                m_data.insert(it, pair<SequenceNumber32, uint32_t>(seq, sz));
                NS_LOG_LOGIC("Flow " << socketId << " : Inserting Seg (Mutex)" << seq << "-" << (seq+sz));
                m_dirty = true;
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
                m_data.insert(it, pair<SequenceNumber32, uint32_t>(seq, blockBegin-seq));
                sz = 0;
                m_dirty = true;
                break;
            } else {
                NS_ASSERT(blockEnd <= seq);
            }
        }
        if(sz){
            NS_LOG_LOGIC("Flow " << socketId << " : Inserting Seg (rem) " << seq << "-" <<  (seq+sz));
            m_data.insert(it, pair<SequenceNumber32, uint32_t>(seq, sz));
            m_dirty = true;
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

    std::pair<SequenceNumber32, uint32_t> SelectivePacketQueue::pop(uint32_t sz) {
        return pop(sz, DEFAULT_POP_METHOD);
    }

    std::pair<SequenceNumber32, uint32_t> SelectivePacketQueue::pop(uint32_t sz, PopMethod method) {
        std::pair<SequenceNumber32, uint32_t> result = peek(sz, method);
        if(result.second == 0)
            return result;

        NS_LOG_LOGIC("Flow " << socketId << " : Popping " << result.first << "-" << (result.first + result.second) << ", because requestSz=" << sz);
                
        if(method == FROM_FRONT) {
            auto it = m_data.begin();
            NS_ASSERT(result.first == it->first);
            if(it->second == result.second) {
                m_data.erase(it);
            } else {
                it->second = it->second - result.second;
                it->first = it->first + result.second;
            }
        } else if(method == FROM_REAR) {
            auto it = m_data.rbegin();
            NS_ASSERT(result.first + result.second == it->first+it->second);
            if(it->second == result.second) {
                m_data.erase(next(it).base());
            } else {
                it->second = it->second - result.second;
            } 
        } else {
            abort();
            return pair<SequenceNumber32, uint32_t>(SequenceNumber32(0), 0);   
        }
        return result;
    }



    std::pair<SequenceNumber32, uint32_t> SelectivePacketQueue::peek(uint32_t sz) {
        return peek(sz, DEFAULT_POP_METHOD);
    }
    std::pair<SequenceNumber32, uint32_t> SelectivePacketQueue::peek(uint32_t sz, PopMethod method) {
        if(!m_data.size())
            return pair<SequenceNumber32, uint32_t>(SequenceNumber32(0), 0);   

        if(method == FROM_FRONT) {
            auto it = m_data.begin();
            return pair<SequenceNumber32, uint32_t> (it->first, min(it->second, sz));
        } else if (method == FROM_REAR) {
            auto it = m_data.rbegin();
            if(it->second > sz) {
                return pair<SequenceNumber32, uint32_t> (it->first+(it->second-sz), sz);
            } else {
                return pair<SequenceNumber32, uint32_t> (it->first, it->second);
            }
        } else {
            abort();
            return pair<SequenceNumber32, uint32_t>(SequenceNumber32(0), 0);   
        }
    }

    void SelectivePacketQueue::discardUpTo(SequenceNumber32 cumAck) {
        auto it = m_data.begin();
        for(; it != m_data.end();) {
            if(it->first + it->second <= cumAck) {
                NS_LOG_LOGIC("Flow " << socketId << " : Removing under " << cumAck << " - Removed Whole " << it->first << "-" << (it->first + it->second));
                it = m_data.erase(it);
            } else if(it->first < cumAck) { // do we need equal here? Maybe not
                NS_LOG_LOGIC("Flow " << socketId << " : Removing under " << cumAck << " - Removed Part  " << it->first << "-" << (cumAck) << " of Entire part " << it->first << "-" << (it->first + it->second));
                it->second = it->first + it->second - cumAck;
                it->first = cumAck;
                NS_ASSERT(it->second != 0);
                break;
            } else {
                break;
            }
        }
    }

    void SelectivePacketQueue::discard(SequenceNumber32 start, uint32_t len) {
        SackList list;
        list.push_back(std::pair<SequenceNumber32, SequenceNumber32>(start, start + len));

        updateSack(list);
    }

    void SelectivePacketQueue::updateSack(SackList list) {
        for (auto option_it = list.begin (); option_it != list.end (); ++option_it) {
            SequenceNumber32 seq = option_it->first;
            SequenceNumber32 seqEnd = option_it->second;
            NS_ASSERT(seq < seqEnd);
            NS_LOG_LOGIC("Flow " << socketId << " : SACK Block " << seq << " - " << seqEnd);

            // left end : inclusive, right end : exclusive
            // remove certain block
            for(auto it = m_data.begin(); it != m_data.end();) {
                SequenceNumber32 blockBegin = it->first; // inclusive
                SequenceNumber32 blockEnd = it->first + it->second; // exclusive
                NS_LOG_LOGIC("Flow " << socketId << " : SACK examining : block " << blockBegin << " - " << blockEnd);
                
                if(blockBegin <= seq && seqEnd <= blockEnd) {
                    // seq-seqEnd is included inside block-blockEnd
                    // first segment : blockBegin - seq
                    // second segment : seqEnd - blockEnd
                    if(blockBegin != seq)
                        m_data.insert(it, pair<SequenceNumber32, uint32_t>(blockBegin, seq-blockBegin));
                    it->first = seqEnd;
                    it->second = blockEnd - seqEnd;
                    NS_LOG_LOGIC("Flow " << socketId << " : SACK removing : splitting block " << blockBegin << " - " << seq << ", " << seqEnd << " - " << blockEnd);
                    if(blockEnd == seqEnd) {
                        it = m_data.erase(it);
                        continue;
                    }
                } else if (seq < blockBegin && blockEnd < seqEnd) {
                    // block-blockEnd is included inside Endseq-seqEnd
                    // remove this block
                    it = m_data.erase(it);
                    NS_LOG_LOGIC("Flow " << socketId << " : SACK removing : removing block " << blockBegin << " - " << blockEnd);
                    continue; // intentional
                    
                } else if ((blockBegin < seq && blockEnd <= seq) || (blockBegin >= seqEnd && blockEnd > seqEnd) ) {
                    // seq-seqEnd is mutually exclusive to block-blockEnd
                    // do nothing
                } else if (blockBegin <= seq && seq < blockEnd && blockEnd < seqEnd) {
                    // front part of seq-seqEnd is overlapped
                    // segment modified to blockBegin - seq
                    it->second = seq - blockBegin;
                    NS_LOG_LOGIC("Flow " << socketId << " : SACK removing : trimming(RE) block " << blockBegin << " - " << seq);
                    if(seq == blockBegin) {
                        it = m_data.erase(it);
                        continue;
                    }
                } else if (seq < blockBegin && blockBegin < seqEnd && seqEnd <= blockEnd) {
                    // rear part of seq-seqEnd is overlapped
                    // segment modified to seqEnd - blockEnd
                    it->first = seqEnd;
                    it->second = blockEnd - seqEnd;
                    NS_LOG_LOGIC("Flow " << socketId << " : SACK removing : trimming(LE) block " << seqEnd << " - " << blockEnd);
                    if(blockEnd == seqEnd) {
                        it = m_data.erase(it);
                        continue;
                    }
                } else {
                    NS_ABORT_MSG("Unhandled SACK selective-packet-queue");
                }
                ++it;
            }
        }

        // Sanity check : check duplicate, empty blocks
        // merge neighboring blocks
        auto it_prev = m_data.begin();
        for(auto it=m_data.begin(); it != m_data.end();) {
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
    }

    const size_t SelectivePacketQueue::size() {
        size_t sum = 0;
        for(auto it = m_data.begin(); it != m_data.end(); ++it) {
            sum += it->second;
        }
        return sum;
    }
    const bool SelectivePacketQueue::isEmpty() {
        for(auto it = m_data.begin(); it != m_data.end(); ++it) {
            if(it->second)
                return false;
        }
        return true;
    }

    const bool SelectivePacketQueue::isDirty() {
        return m_dirty;
    }

}