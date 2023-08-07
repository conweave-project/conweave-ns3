/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 *  Copyright (c) 2007,2008,2009 INRIA, UDCAST
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
 * Author: Amine Ismail <amine.ismail@sophia.inria.fr>
 *                      <amine.ismail@udcast.com>
 */

#include "udp-server.h"

#include <climits>
#include <unordered_map>

#include "ns3/boolean.h"
#include "ns3/flow-id-num-tag.h"
#include "ns3/flow-stat-tag.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/ipv4-address.h"
#include "ns3/log.h"
#include "ns3/nstime.h"
#include "ns3/packet.h"
#include "ns3/seq-ts-header.h"
#include "ns3/settings.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/socket.h"
#include "ns3/uinteger.h"
#include "packet-loss-counter.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("UdpServer");
NS_OBJECT_ENSURE_REGISTERED(UdpServer);

static bool debug_first_terminate = true;

TypeId
UdpServer::GetTypeId(void) {
    static TypeId tid = TypeId("ns3::UdpServer")
                            .SetParent<Application>()
                            .AddConstructor<UdpServer>()
                            .AddAttribute("Port",
                                          "Port on which we listen for incoming packets.",
                                          UintegerValue(100),
                                          MakeUintegerAccessor(&UdpServer::m_port),
                                          MakeUintegerChecker<uint16_t>())
                            .AddAttribute("PacketWindowSize",  // deprecated
                                          "The size of the window used to compute the packet loss. This value should be a multiple of 8.",
                                          UintegerValue(256),  // up to 256kbits
                                          MakeUintegerAccessor(&UdpServer::GetPacketWindowSize,
                                                               &UdpServer::SetPacketWindowSize),
                                          MakeUintegerChecker<uint16_t>(8, 256))
                            .AddTraceSource("Rx", "A packet has been received",
                                            MakeTraceSourceAccessor(&UdpServer::m_rxTrace))
                            .AddTraceSource("RxWithAddresses", "A packet has been received",
                                            MakeTraceSourceAccessor(&UdpServer::m_rxTraceWithAddresses))
                            .AddAttribute("FlowSize",
                                          "Expected length of incoming flow",
                                          UintegerValue(0),
                                          MakeUintegerAccessor(&UdpServer::expected_flow_size),
                                          MakeUintegerChecker<uint32_t>())
                            .AddAttribute("MSS",
                                          "Maximum Segment Size",
                                          UintegerValue(1000),
                                          MakeUintegerAccessor(&UdpServer::m_mss),
                                          MakeUintegerChecker<uint32_t>())
                            .AddAttribute("irn",
                                          "IRN enabled?",
                                          BooleanValue(false),
                                          MakeBooleanAccessor(&UdpServer::m_irn_server),
                                          MakeBooleanChecker())
                            .AddAttribute("StatRxLen",
                                          "Total length of the flow (for statistical purpose only)",
                                          UintegerValue(0),
                                          MakeUintegerAccessor(&UdpServer::m_stat_flow_len),
                                          MakeUintegerChecker<uint32_t>())
                            .AddAttribute("StatHostSrc",
                                          "Source Host ID (for statistical purpose only)",
                                          UintegerValue(0),
                                          MakeUintegerAccessor(&UdpServer::m_stat_host_src),
                                          MakeUintegerChecker<uint32_t>())
                            .AddAttribute("StatHostDst",
                                          "Destination Host ID (for statistical purpose only)",
                                          UintegerValue(0),
                                          MakeUintegerAccessor(&UdpServer::m_stat_host_dst),
                                          MakeUintegerChecker<uint32_t>())
                            .AddAttribute("StatFlowID",
                                          "Flow ID used for statistical purpose only. different from real Flow ID",
                                          UintegerValue(0),
                                          MakeUintegerAccessor(&UdpServer::m_stat_flow_id),
                                          MakeUintegerChecker<uint32_t>());
    return tid;
}

UdpServer::UdpServer()
    : flow_end_time(0) {
    NS_LOG_FUNCTION(this);
}

UdpServer::~UdpServer() {
    NS_LOG_FUNCTION(this);
}

uint16_t
UdpServer::GetPacketWindowSize() const {
    return UINT16_MAX;
}

void UdpServer::SetPacketWindowSize(uint16_t size) {
    // Do nothing. UdpServer can monitor any range of packet.
}

uint32_t
UdpServer::GetLost(void) const {
    return expected_flow_size - m_app_recv_buffer.receivedBytes();
}

uint32_t
UdpServer::GetReceived(void) const {
    return m_app_recv_buffer.receivedBytes();
}

void UdpServer::DoDispose(void) {
    extern std::unordered_map<unsigned, Time> acc_pause_time;
    extern std::unordered_map<unsigned, unsigned> acc_timeout_count;
    NS_LOG_FUNCTION(this);
    bool is_completed = m_app_recv_buffer.isComplete(expected_flow_size);
    if (!is_completed)
        std::cerr << "[ERROR] Flow " << m_stat_flow_id << " Incomplete : Expected " << expected_flow_size << ", " << m_app_recv_buffer << std::endl;

/** DEBUGGING START **/
#if (DEBUG_UDP_SERVER == 1)
    if (debug_first_terminate) {
        fprintf(stdout, "Flow#\tsrc\tdst\tstart\tend\tduration\tsize\tcompleted\tactual#\tpaused\tdelayed%%\tT/O\n");
        debug_first_terminate = false;
    }
    double time_paused = 0;
    if (acc_pause_time.find(incoming_flow_id) != acc_pause_time.end())
        time_paused = acc_pause_time.find(incoming_flow_id)->second.GetNanoSeconds() / 1000000000.;
    unsigned timeout_count = 0;
    if (acc_timeout_count.find(incoming_flow_id) != acc_timeout_count.end())
        timeout_count = acc_timeout_count[incoming_flow_id];

    fprintf(stdout, "%d\t%d\t%d\t%.9lf\t%.9lf\t%.9lf\t%u\t%s\t%u\t%.9lf\t%.3lf%%\t%u\n", m_stat_flow_id, m_stat_host_src, m_stat_host_dst, firstUsed.GetSeconds(), lastUsed.GetSeconds(),
            lastUsed.GetSeconds() - firstUsed.GetSeconds(), m_stat_flow_len, (is_completed ? "COMPLETE" : "INCOMP"), incoming_flow_id, time_paused,
            lastUsed.GetSeconds() != firstUsed.GetSeconds() ? (100 * time_paused / (lastUsed.GetSeconds() - firstUsed.GetSeconds())) : 0, timeout_count);
    fflush(stdout);
/** DEBUGGING END **/
#endif
    Application::DoDispose();
}

bool UdpServer::IsFlowComplete() const {
    return m_app_recv_buffer.isComplete(expected_flow_size);
}
void UdpServer::StartApplication(void) {
    NS_LOG_FUNCTION(this);

    if (m_socket == 0) {
        TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
        m_socket = Socket::CreateSocket(GetNode(), tid);
        InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(),
                                                    m_port);
        if (m_socket->Bind(local) == -1) {
            NS_FATAL_ERROR("Failed to bind socket");
        }
    }

    m_socket->SetRecvCallback(MakeCallback(&UdpServer::HandleRead, this));

    if (m_socket6 == 0) {
        TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
        m_socket6 = Socket::CreateSocket(GetNode(), tid);
        Inet6SocketAddress local = Inet6SocketAddress(Ipv6Address::GetAny(),
                                                      m_port);
        if (m_socket6->Bind(local) == -1) {
            NS_FATAL_ERROR("Failed to bind socket");
        }
    }

    m_socket6->SetRecvCallback(MakeCallback(&UdpServer::HandleRead, this));
}

void UdpServer::StopApplication() {
    NS_LOG_FUNCTION(this);

    if (m_socket != 0) {
        m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket> >());
    }
}

void UdpServer::HandleRead(Ptr<Socket> socket) {
    NS_LOG_FUNCTION(this << socket);
    Ptr<Packet> packet;
    Address from;
    Address localAddress;
    while ((packet = socket->RecvFrom(from))) {
        socket->GetSockName(localAddress);
        m_rxTrace(packet);
        m_rxTraceWithAddresses(packet, from, localAddress);
        if (packet->GetSize() > 0) {
            SeqTsHeader seqTs;
            packet->RemoveHeader(seqTs);
            uint32_t currentSequenceNumber = seqTs.GetSeq();
            m_app_recv_buffer.recv(currentSequenceNumber, packet->GetSize());

            FlowStatTag fst;
            FlowIDNUMTag fit;
            if (packet->PeekPacketTag(fst) && packet->PeekPacketTag(fit)) {
                if (firstUsed.GetSeconds() == 0 && fst.GetType() == FlowStatTag::FLOW_START_AND_END) {
                    // Should print Flow start in client side for exec time of flow start
                    firstUsed = Seconds(fst.getInitiatedTime());
                    NS_LOG_INFO("FLOW END Time: " << Simulator::Now() << " FLOWID " << fit.GetId());

                    flow_end_time = Simulator::Now();
                    lastUsed = Simulator::Now();
                } else if (firstUsed.GetSeconds() == 0 && fst.GetType() == FlowStatTag::FLOW_START) {
                    // Should print Flow start in client side for exec time of flow start
                    firstUsed = Seconds(fst.getInitiatedTime());
                } else if (firstUsed.GetSeconds() != 0 && flow_end_time == 0 && fst.GetType() == FlowStatTag::FLOW_END && m_app_recv_buffer.isComplete(expected_flow_size)) {
                    NS_LOG_INFO("FLOW END Time: " << Simulator::Now() << " FLOWID " << fit.GetId());
                    flow_end_time = Simulator::Now();
                    lastUsed = Simulator::Now();
                } else if (firstUsed.GetSeconds() != 0 && flow_end_time == 0 && m_app_recv_buffer.isComplete(expected_flow_size) && m_irn_server) {
                    NS_LOG_INFO("FLOW END Time: " << Simulator::Now() << " FLOWID " << fit.GetId());
                    flow_end_time = Simulator::Now();
                    lastUsed = Simulator::Now();
                } else if (flow_end_time == 0 && fst.GetType() == FlowStatTag::FLOW_END) {
                    // std::cout << "We have " << Packet_checker.GetLost() << " lost... fid=" << fit.GetId() << std::endl;
                }
                NS_LOG_INFO(" FLOW COMMING " << fit.GetId() << " Seqnum: " << currentSequenceNumber);
                incoming_flow_id = fit.GetId();
            }
            if (InetSocketAddress::IsMatchingType(from)) {
                NS_LOG_INFO("TraceDelay: RX " << packet->GetSize() << " bytes from " << InetSocketAddress::ConvertFrom(from).GetIpv4() << " Sequence Number: " << currentSequenceNumber << " Uid: " << packet->GetUid() << " TXtime: " << seqTs.GetTs() << " RXtime: " << Simulator::Now() << " Delay: " << Simulator::Now() - seqTs.GetTs());
            } else if (Inet6SocketAddress::IsMatchingType(from)) {
                NS_LOG_INFO("TraceDelay: RX " << packet->GetSize() << " bytes from " << Inet6SocketAddress::ConvertFrom(from).GetIpv6() << " Sequence Number: " << currentSequenceNumber << " Uid: " << packet->GetUid() << " TXtime: " << seqTs.GetTs() << " RXtime: " << Simulator::Now() << " Delay: " << Simulator::Now() - seqTs.GetTs());
            }
        }
    }
}

void UdpServer::SetRemote(Ipv4Address ip, uint16_t port) {
    m_peerAddress = Address(ip);
    m_peerPort = port;
}

}  // Namespace ns3
