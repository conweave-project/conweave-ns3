/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
* Copyright (c) 2007,2008,2009 INRIA, UDCAST
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
* Author: Hwijoon Lim <hwijoon.lim@kaist.ac.kr>
*
*/
#ifndef APP_RECV_BUFFER_H
#define APP_RECV_BUFFER_H

#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/address.h"
#include <list>
#include <ostream>

namespace ns3 {

    class AppRecvBuffer {
        typedef std::pair<uint32_t, uint32_t> SackBlock;

    private:
        std::list<SackBlock> m_data;
        uint32_t cumAck{0};
        void sack(uint32_t seq, uint32_t size); // put blocks
        size_t discardUpTo(uint32_t seq); // return number of blocks removed
        
    public:
        inline bool isComplete(uint32_t expected_size) const {
            return cumAck >= expected_size;
        }
        uint32_t receivedBytes() const;
        void recv(uint32_t seq, uint32_t len);
        friend std::ostream &operator<<(std::ostream &os, const AppRecvBuffer &im);
    };
}

#endif