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
#ifndef SELECTIVE_PACKET_QUEUE_H
#define SELECTIVE_PACKET_QUEUE_H
#include <ns3/abort.h>
#include <ns3/log.h>

#include <list>
#include <ostream>

#include "ns3/object.h"
#include "ns3/sequence-number.h"

/**
 * \file
 * \ingroup systempath
 * ns3::ConfigReader declarations.
 */

namespace ns3 {

class SelectivePacketQueue : public Object {
   public:
    typedef std::list<std::pair<SequenceNumber32, SequenceNumber32>> SackList;
    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId(void);
    virtual TypeId GetInstanceTypeId(void) const;

    friend std::ostream& operator<<(std::ostream&, const SelectivePacketQueue&);

    enum PopMethod { FROM_FRONT,
                     FROM_REAR };

    void push(SequenceNumber32 seq, uint32_t sz);
    std::pair<SequenceNumber32, uint32_t> pop(uint32_t sz);
    std::pair<SequenceNumber32, uint32_t> pop(uint32_t sz, PopMethod method);
    std::pair<SequenceNumber32, uint32_t> peek(uint32_t sz);
    std::pair<SequenceNumber32, uint32_t> peek(uint32_t sz, PopMethod method);
    const bool isEmpty();
    const size_t size();
    const bool isDirty();

    void discard(SequenceNumber32 start, uint32_t len);  // discard start ~ start+len (Exclusive)

    void discardUpTo(SequenceNumber32 cumAck);  // remove all segment smaller than cumAck (exclusive)
    void updateSack(SackList list);             // remove specific SACK segment
    int32_t socketId{-1};                       // only for debugging purpose

   private:
    bool m_dirty{false};
    const PopMethod DEFAULT_POP_METHOD = FROM_FRONT;
    std::list<std::pair<SequenceNumber32, uint32_t>> m_data;

};  // class SelectivePacketQueue

}  // namespace ns3

#endif /* SELECTIVE_PACKET_QUEUE_H */
