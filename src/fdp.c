/*
 $Id$
*/

#include "common.h"
#include "util.h"
#include "fdp.h"
#include "cdp.h"
#include "tlv.h"

size_t fdp_packet(void *packet, struct netif *netif, struct sysinfo *sysinfo) {

    struct ether_hdr ether;
    struct ether_llc llc;
    struct fdp_header fdp;

    uint8_t *tlv;
    uint8_t *pos = packet;
    size_t length = ETHER_MAX_LEN;

    void *fdp_start, *cap_str;
    uint8_t addr_count = 0;
    struct netif *master;

    const uint8_t fdp_dst[] = FDP_MULTICAST_ADDR;
    const uint8_t llc_org[] = LLC_ORG_FOUNDRY;
    const struct cdp_proto fdp_protos[] = {
	ADDR_PROTO_CLNP, ADDR_PROTO_IPV4, ADDR_PROTO_IPV6,
    };

    // fixup master netif
    if (netif->master != NULL)
	master = netif->master;
    else
	master = netif;

    // ethernet header
    memcpy(ether.dst, fdp_dst, ETHER_ADDR_LEN);
    memcpy(ether.src, netif->hwaddr, ETHER_ADDR_LEN);
    pos += sizeof(struct ether_hdr);

    // llc snap header
    llc.dsap = llc.ssap = 0xaa;
    llc.control = 0x03;
    memcpy(llc.org, llc_org, sizeof(llc.org));
    llc.protoid = htons(LLC_PID_FDP);
    memcpy(pos, &llc, sizeof(struct ether_llc));
    pos += sizeof(struct ether_llc);

    // fdp header
    fdp.version = FDP_VERSION;
    fdp.ttl = LADVD_TTL;
    fdp.checksum = 0;
    memcpy(pos, &fdp, sizeof(struct fdp_header));
    fdp_start = pos;

    // update tlv counters
    pos += sizeof(struct fdp_header);
    length -= VOIDP_DIFF(pos, packet);


    // device id
    if (!(
	START_FDP_TLV(FDP_TYPE_DEVICE_ID) &&
	PUSH_BYTES(sysinfo->hostname, strlen(sysinfo->hostname))
    ))
	return 0;
    END_FDP_TLV;


    // port id
    if (!(
	START_FDP_TLV(FDP_TYPE_PORT_ID) &&
	PUSH_BYTES(netif->name, strlen(netif->name))
    ))
	return 0;
    END_FDP_TLV;


    // capabilities
    if (sysinfo->cap_active & CAP_ROUTER)
	cap_str = "Router";
    else if (sysinfo->cap_active & CAP_SWITCH)
	cap_str = "Switch";
    else if (sysinfo->cap_active & CAP_BRIDGE)
	cap_str = "Bridge";
    else
	cap_str = "Host";

    if (!(
	START_FDP_TLV(FDP_TYPE_CAPABILITIES) &&
	PUSH_BYTES(cap_str, strlen(cap_str))
    ))
	return 0;
    END_FDP_TLV;


    // version
    if (!(
	START_FDP_TLV(FDP_TYPE_SW_VERSION) &&
	PUSH_BYTES(sysinfo->uts_str, strlen(sysinfo->uts_str))
    ))
	return 0;
    END_FDP_TLV;


    // platform
    if (!(
	START_FDP_TLV(FDP_TYPE_PLATFORM) &&
	PUSH_BYTES(sysinfo->uts.sysname, strlen(sysinfo->uts.sysname))
    ))
	return 0;
    END_FDP_TLV;


    // management addrs
    if (master->ipaddr4 != 0)
	addr_count++;
    if (!IN6_IS_ADDR_UNSPECIFIED((struct in6_addr *)master->ipaddr6)) 
	addr_count++;

    if (addr_count > 0) {
	if (!(
	    START_FDP_TLV(FDP_TYPE_ADDRESS) &&
	    PUSH_UINT32(addr_count)
	))
	    return 0;

	if (master->ipaddr4 != 0) {
	    if (!(
		PUSH_UINT8(fdp_protos[CDP_ADDR_PROTO_IPV4].protocol_type) &&
		PUSH_UINT8(fdp_protos[CDP_ADDR_PROTO_IPV4].protocol_length) &&
		PUSH_BYTES(fdp_protos[CDP_ADDR_PROTO_IPV4].protocol,
			   fdp_protos[CDP_ADDR_PROTO_IPV4].protocol_length) &&
		PUSH_UINT16(sizeof(master->ipaddr4)) &&
		PUSH_BYTES(&master->ipaddr4, sizeof(master->ipaddr4))
	    ))
		return 0;
	}

	if (!IN6_IS_ADDR_UNSPECIFIED((struct in6_addr *)master->ipaddr6)) {
	    if (!(
		PUSH_UINT8(fdp_protos[CDP_ADDR_PROTO_IPV6].protocol_type) &&
		PUSH_UINT8(fdp_protos[CDP_ADDR_PROTO_IPV6].protocol_length) &&
		PUSH_BYTES(fdp_protos[CDP_ADDR_PROTO_IPV6].protocol,
			   fdp_protos[CDP_ADDR_PROTO_IPV6].protocol_length) &&
		PUSH_UINT16(sizeof(master->ipaddr6)) &&
		PUSH_BYTES(master->ipaddr6, sizeof(master->ipaddr6))
	    ))
		return 0;
	}

	END_FDP_TLV;
    }


    // fdp header
    fdp.checksum = my_chksum(fdp_start, VOIDP_DIFF(pos, fdp_start), 0);
    memcpy(fdp_start, &fdp, sizeof(struct fdp_header));

    // ethernet header
    ether.length = htons(VOIDP_DIFF(pos, packet + sizeof(struct ether_hdr)));
    memcpy(packet, &ether, sizeof(struct ether_hdr));

    // packet length
    return(VOIDP_DIFF(pos, packet));
}
