/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
* Author: Youngmok Jung <tom418@kaist.ac.kr>
*/

#include "flow-id-num-tag.h"

namespace ns3 {
	NS_OBJECT_ENSURE_REGISTERED(FlowIDNUMTag);


	FlowIDNUMTag::FlowIDNUMTag() :
		Tag(),
		flow_stat(0)
	{
	}


	TypeId
		FlowIDNUMTag::GetTypeId(void)
	{
		static TypeId tid = TypeId("ns3::FlowIDNUMTag")
			.SetParent<Tag>()
			.AddConstructor<FlowIDNUMTag>()
			;
		return tid;
	}
	TypeId
		FlowIDNUMTag::GetInstanceTypeId(void) const
	{
		return GetTypeId();
	}

	uint32_t
		FlowIDNUMTag::GetSerializedSize(void) const
	{
		return sizeof(flow_stat) + sizeof(flow_size);
	}

	void
		FlowIDNUMTag::Serialize(TagBuffer i) const
	{
		i.WriteU16(flow_stat);
		i.WriteU32(flow_size);
	}

	void
		FlowIDNUMTag::Deserialize(TagBuffer i)
	{
		flow_stat = i.ReadU16();
		flow_size = i.ReadU32();
	}

	void FlowIDNUMTag::SetId(int32_t ttl)
	{
		
		flow_stat = ttl;
	}

	int32_t FlowIDNUMTag::GetId()
	{
		return flow_stat;
	}

	uint16_t FlowIDNUMTag::Getflowid()
	{
		static uint32_t nextFlowId = 0;
		flow_stat = nextFlowId++;
		return flow_stat;
	}

	uint32_t FlowIDNUMTag::GetFlowSize()
	{
		return flow_size;
	}

	void  FlowIDNUMTag::SetFlowSize(uint32_t fs)
	{
		flow_size = fs;
	}
	void FlowIDNUMTag::Print(std::ostream & os) const
	{
		os << flow_stat;
	}

} // namespace ns3

