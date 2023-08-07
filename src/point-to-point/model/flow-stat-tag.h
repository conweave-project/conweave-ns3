/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Author: Youngmok Jung <tom418@kaist.ac.kr>
 */

#include "ns3/tag.h"

namespace ns3 {
/**
 * \ingroup tlt
 * \brief The packet header for an TLT packet
 */
class FlowStatTag : public Tag {
   public:
    FlowStatTag();
    static TypeId GetTypeId(void);
    virtual TypeId GetInstanceTypeId(void) const;
    virtual void Print(std::ostream &os) const;
    virtual uint32_t GetSerializedSize(void) const;
    virtual void Serialize(TagBuffer i) const;
    virtual void Deserialize(TagBuffer i);
    void SetType(uint8_t ttl);
    uint8_t GetType();

    enum FlowEnd_t {
        FLOW_END = 0x01,
        FLOW_NOTEND = 0x00,
        FLOW_START = 0x02,
        FLOW_START_AND_END = 0x03,
        FLOW_FIN = 0x04,
    };
    void setInitiatedTime(double t);
    double getInitiatedTime();

   private:
    uint8_t flow_stat;
    double initiatedTime;
};

}  // namespace ns3
