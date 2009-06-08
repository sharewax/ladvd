
#ifndef _protos_h
#define _protos_h

#include "proto/lldp.h"
#include "proto/cdp.h"
#include "proto/edp.h"
#include "proto/fdp.h"
#include "proto/ndp.h"

#define PROTO_LLDP  0
#define PROTO_CDP   1
#define PROTO_EDP   2
#define PROTO_FDP   3
#define PROTO_NDP   4
#define PROTO_MAX   5


size_t lldp_packet(void *, struct netif *, struct sysinfo *);
size_t cdp_packet(void *, struct netif *, struct sysinfo *);
size_t edp_packet(void *, struct netif *, struct sysinfo *);
size_t fdp_packet(void *, struct netif *, struct sysinfo *);
size_t ndp_packet(void *, struct netif *, struct sysinfo *);

char * lldp_check(void *, size_t);
char * cdp_check(void *, size_t);
char * edp_check(void *, size_t);
char * fdp_check(void *, size_t);
char * ndp_check(void *, size_t);

size_t lldp_peer(struct master_msg *);
size_t cdp_peer(struct master_msg *);
size_t edp_peer(struct master_msg *);
size_t fdp_peer(struct master_msg *);
size_t ndp_peer(struct master_msg *);

char * lldp_decode(void *, size_t);
char * cdp_decode(void *, size_t);
char * edp_decode(void *, size_t);
char * fdp_decode(void *, size_t);
char * ndp_decode(void *, size_t);

#endif /* _protos_h */