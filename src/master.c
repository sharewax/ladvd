/*
 $Id$
*/

#include "common.h"
#include "util.h"
#include "protos.h"
#include "master.h"
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>

#ifdef USE_CAPABILITIES
#include <sys/prctl.h>
#include <sys/capability.h>
#endif

#ifdef HAVE_NETPACKET_PACKET_H
#include <netpacket/packet.h>
#endif /* HAVE_NETPACKET_PACKET_H */
#ifdef HAVE_LINUX_FILTER_H
#include <linux/filter.h>
#endif /* HAVE_LINUX_FILTER_H */
#ifdef HAVE_NET_BPF_H
#include <net/bpf.h>
#endif /* HAVE_NET_BPF_H */

#if HAVE_LINUX_SOCKIOS_H
#include <linux/sockios.h>
#endif /* HAVE_LINUX_SOCKIOS_H */

#if HAVE_LINUX_ETHTOOL_H
#include <linux/ethtool.h>
#endif /* HAVE_LINUX_ETHTOOL_H */


#ifdef HAVE_NET_BPF_H
struct bpf_insn master_filter[] = {
#elif HAVE_LINUX_FILTER_H
struct sock_filter master_filter[] = {
#endif
    // lldp
    // ether 01:80:c2:00:00:0e
    BPF_STMT(BPF_LD+BPF_W+BPF_ABS, 0),
    BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0x0180C200, 0, 5),
    BPF_STMT(BPF_LD+BPF_H+BPF_ABS, 4),
    BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0x0000000E, 0, 3),
    // ether proto 
    BPF_STMT(BPF_LD+BPF_H+BPF_ABS, ETHER_ADDR_LEN * 2),
    BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, ETHERTYPE_LLDP, 0, 1),
    // accept
    BPF_STMT(BPF_RET+BPF_K, (u_int)-1),

    // llc dsap & ssap
    BPF_STMT(BPF_LD+BPF_H+BPF_ABS, ETHER_HDR_LEN),
    BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0xAAAA, 1, 0),
    // reject
    BPF_STMT(BPF_RET+BPF_K, 0),

    // cdp
    // ether dst 01:00:0c:cc:cc:cc
    BPF_STMT(BPF_LD+BPF_W+BPF_ABS, 0),
    BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0x01000CCC, 0, 7),
    BPF_STMT(BPF_LD+BPF_H+BPF_ABS, 4),
    BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0x0000CCCC, 0, 5),
    // llc control + org
    BPF_STMT(BPF_LD+BPF_W+BPF_ABS, ETH_LLC_CONTROL),
    BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0x0300000C, 0, 3),
    // llc protoid
    BPF_STMT(BPF_LD+BPF_H+BPF_ABS, ETH_LLC_PROTOID),
    BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, LLC_PID_CDP, 0, 1),
    // accept
    BPF_STMT(BPF_RET+BPF_K, (u_int)-1),

    // edp
    // ether dst 00:0e:2b:cc:cc:cc
    BPF_STMT(BPF_LD+BPF_W+BPF_ABS, 0),
    BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0x000E2B00, 0, 7),
    BPF_STMT(BPF_LD+BPF_H+BPF_ABS, 4),
    BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0x00000000, 0, 5),
    // llc control + org
    BPF_STMT(BPF_LD+BPF_W+BPF_ABS, ETH_LLC_CONTROL),
    BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0x03000E2B, 0, 3),
    // llc protoid
    BPF_STMT(BPF_LD+BPF_H+BPF_ABS, ETH_LLC_PROTOID),
    BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, LLC_PID_EDP, 0, 1),
    // accept
    BPF_STMT(BPF_RET+BPF_K, (u_int)-1),

    // fdp
    // ether dst 01:e0:52:cc:cc:cc
    BPF_STMT(BPF_LD+BPF_W+BPF_ABS, 0),
    BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0x01E052CC, 0, 7),
    BPF_STMT(BPF_LD+BPF_H+BPF_ABS, 4),
    BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0x0000CCCC, 0, 5),
    // llc control + org
    BPF_STMT(BPF_LD+BPF_W+BPF_ABS, ETH_LLC_CONTROL),
    BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0x0300E052, 0, 3),
    // llc protoid
    BPF_STMT(BPF_LD+BPF_H+BPF_ABS, ETH_LLC_PROTOID),
    BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, LLC_PID_FDP, 0, 1),
    // accept
    BPF_STMT(BPF_RET+BPF_K, (u_int)-1),

    // ndp
    // ether dst 01:00:81:00:01:00
    BPF_STMT(BPF_LD+BPF_W+BPF_ABS, 0),
    BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0x01008100, 0, 7),
    BPF_STMT(BPF_LD+BPF_H+BPF_ABS, 4),
    BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0x00000100, 0, 5),
    // llc control + org
    BPF_STMT(BPF_LD+BPF_W+BPF_ABS, ETH_LLC_CONTROL),
    BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0x03000081, 0, 3),
    // llc protoid
    BPF_STMT(BPF_LD+BPF_H+BPF_ABS, ETH_LLC_PROTOID),
    BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, LLC_PID_NDP_HELLO, 0, 1),
    // accept
    BPF_STMT(BPF_RET+BPF_K, (u_int)-1),

    // reject
    BPF_STMT(BPF_RET+BPF_K, 0)
};

extern struct proto protos[];
extern uint8_t loglevel;
extern uint32_t options;

void master_init(struct nhead *netifs, uint16_t netifc, int ac,
		 int cmdfd, int msgfd) {

    // raw socket
    int rawfd;

    // interfaces
    struct netif *netif = NULL, *subif = NULL;
    struct master_rfd *rfds = NULL;
    unsigned int i;

    // pcap
    pcap_hdr_t pcap_hdr;

#ifdef USE_CAPABILITIES
    // capabilities
    cap_t caps;
#endif /* USE_CAPABILITIES */

    // events
    struct event ev_cmd;
    struct event ev_sigchld, ev_sigint, ev_sigterm,  ev_sighup;


    // proctitle
    setproctitle("master [priv]");

    // open a raw socket
    rawfd = master_rsocket(NULL, O_WRONLY);

    if (rawfd < 0)
	my_fatal("opening raw socket failed");

    // open listen sockets
    if (options & OPT_RECV) {

	// init
	rfds = my_calloc(netifc, sizeof(struct master_rfd));
	i = 0;

	netif = NULL;
	while ((netif = netif_iter(netif, netifs, ac)) != NULL) {
	    my_log(INFO, "starting receive loop with interface %s",
			 netif->name);

	    subif = NULL;
	    while ((subif = subif_iter(subif, netif)) != NULL) {
		my_log(INFO, "listening on %s", subif->name);

		rfds[i].index = subif->index;
		strlcpy(rfds[i].name, subif->name, IFNAMSIZ);
		memcpy(rfds[i].hwaddr, subif->hwaddr, ETHER_ADDR_LEN);
		rfds[i].fd = master_rsocket(&rfds[i], O_RDONLY);
		rfds[i].cfd = msgfd;

		if (rfds[i].fd < 0)
		    my_fatal("opening raw socket failed");

		master_rconf(&rfds[i], protos);

		i++;
	    }
	}

	netifc = i;
    }

    // debug
    if (loglevel >= DEBUG) {

	// zero
	memset(&pcap_hdr, 0, sizeof(pcap_hdr));

	// create pcap global header
	pcap_hdr.magic_number = PCAP_MAGIC;
	pcap_hdr.version_major = 2;
	pcap_hdr.version_minor = 4;
	pcap_hdr.snaplen = ETHER_MAX_LEN;
	pcap_hdr.network = 1;

	// send pcap global header
	write(rawfd, &pcap_hdr, sizeof(pcap_hdr));
    } else {

	my_chroot(PACKAGE_CHROOT_DIR);

#ifdef USE_CAPABILITIES
	// keep CAP_NET_ADMIN
	caps = cap_from_text("cap_net_admin=ep");

	if (caps == NULL)
	    my_fatal("unable to create capabilities: %s", strerror(errno));

	if (cap_set_proc(caps) == -1)
	    my_fatal("unable to set capabilities: %s", strerror(errno));

	(void) cap_free(caps);
#endif /* USE_CAPABILITIES */
    }


    // initalize the event library
    event_init();

    // listen for requests from the child
    event_set(&ev_cmd, cmdfd, EV_READ|EV_PERSIST, (void *)master_cmd, &rawfd);
    event_add(&ev_cmd, NULL);

    // handle signals
    signal_set(&ev_sigchld, SIGCHLD, master_signal, NULL);
    signal_set(&ev_sigint, SIGINT, master_signal, NULL);
    signal_set(&ev_sigterm, SIGTERM, master_signal, NULL);
    signal_set(&ev_sighup, SIGHUP, master_signal, NULL);
    signal_add(&ev_sigchld, NULL);
    signal_add(&ev_sigint, NULL);
    signal_add(&ev_sigterm, NULL);
    signal_add(&ev_sighup, NULL);


    // listen for received packets
    if (options & OPT_RECV) {
	for (i = 0; i < netifc; i++) {
	    event_set(&rfds[i].event, rfds[i].fd, EV_READ|EV_PERSIST,
		(void *)master_recv, &rfds[i]);
	    event_add(&rfds[i].event, NULL);
	}
    }

    // wait for events
    event_dispatch();

    // not reached
    exit(EXIT_FAILURE);
}


void master_signal(int sig, short event, void *p) {
    switch (sig) {
	case SIGCHLD:
	    my_fatal("child has exited");
	    break;
	case SIGINT:
	case SIGTERM:
	    my_fatal("quitting");
	    break;
	default:
	    my_fatal("unexpected signal");
    }
}


void master_cmd(int cmdfd, short event, int *rawfd) {
    struct master_msg mreq;
    size_t len;


    // receive request
    len = recv(cmdfd, &mreq, MASTER_MSG_SIZE, MSG_DONTWAIT);

    if (len == 0)
	return;

    // check request size
    if (len != MASTER_MSG_SIZE)
	my_fatal("invalid request received");

    // validate request
    if (master_rcheck(&mreq) != EXIT_SUCCESS)
	my_fatal("invalid request supplied");

    // transmit packet
    if (mreq.cmd == MASTER_SEND) {
	mreq.len = master_rsend(*rawfd, &mreq);
	mreq.completed = 1;
	write(cmdfd, &mreq, MASTER_MSG_SIZE);
#if HAVE_LINUX_ETHTOOL_H
    // fetch ethtool details
    } else if (mreq.cmd == MASTER_ETHTOOL) {
	mreq.len = master_ethtool(*rawfd, &mreq);
	mreq.completed = 1;
	write(cmdfd, &mreq, MASTER_MSG_SIZE);
#endif /* HAVE_LINUX_ETHTOOL_H */
    // invalid request
    } else {
	my_fatal("invalid request received");
    }
}


void master_recv(int fd, short event, struct master_rfd *rfd) {
    // packet
    struct master_msg mrecv;
    struct ether_hdr *ether;
    static unsigned int rcount = 0;
    unsigned int p;


    mrecv.index = rfd->index;
    mrecv.len = recv(rfd->fd, mrecv.msg, ETHER_MAX_LEN, MSG_DONTWAIT);

    // skip small packets
    if (mrecv.len < ETHER_MIN_LEN)
	return;

    // skip locally generated packets
    ether = (struct ether_hdr *)mrecv.msg;
    if (memcmp(rfd->hwaddr, ether->src, ETHER_ADDR_LEN) == 0)
	return;

    // detect the protocol
    for (p = 0; protos[p].name != NULL; p++) {
	if (memcmp(protos[p].dst_addr, ether->dst, ETHER_ADDR_LEN) != 0)
	    continue;

	mrecv.proto = p;
	break;
    }

    write(rfd->cfd, mrecv.msg, mrecv.len);
    rcount++;
}


int master_rcheck(struct master_msg *mreq) {
    struct ether_hdr ether;
    struct ether_llc llc;

    // validate ifindex
    if (if_indextoname(mreq->index, mreq->name) == NULL) {
	my_log(CRIT, "invalid ifindex supplied");
	return(EXIT_FAILURE);
    }

    if (mreq->len > ETHER_MAX_LEN) {
	my_log(CRIT, "invalid message length supplied");
	return(EXIT_FAILURE);
    }

    if (mreq->cmd == MASTER_SEND) {
	memcpy(&ether, mreq->msg, sizeof(ether));
	memcpy(&llc, mreq->msg + sizeof(ether), sizeof(llc));

	// lldp
	static uint8_t lldp_dst[] = LLDP_MULTICAST_ADDR;

	if ((memcmp(ether.dst, lldp_dst, ETHER_ADDR_LEN) == 0) &&
	    (ether.type == htons(ETHERTYPE_LLDP))) {
	    return(EXIT_SUCCESS);
	}

	// cdp
	const uint8_t cdp_dst[] = CDP_MULTICAST_ADDR;
	const uint8_t cdp_org[] = LLC_ORG_CISCO;

	if ((memcmp(ether.dst, cdp_dst, ETHER_ADDR_LEN) == 0) &&
	    (memcmp(llc.org, cdp_org, sizeof(llc.org)) == 0) &&
	    (llc.protoid == htons(LLC_PID_CDP))) {
	    return(EXIT_SUCCESS);
	}

	// edp
	const uint8_t edp_dst[] = EDP_MULTICAST_ADDR;
	const uint8_t edp_org[] = LLC_ORG_EXTREME;

	if ((memcmp(ether.dst, edp_dst, ETHER_ADDR_LEN) == 0) &&
	    (memcmp(llc.org, edp_org, sizeof(llc.org)) == 0) &&
	    (llc.protoid == htons(LLC_PID_EDP))) {
	    return(EXIT_SUCCESS);
	}

	// fdp
	const uint8_t fdp_dst[] = FDP_MULTICAST_ADDR;
	const uint8_t fdp_org[] = LLC_ORG_FOUNDRY;

	if ((memcmp(ether.dst, fdp_dst, ETHER_ADDR_LEN) == 0) &&
	    (memcmp(llc.org, fdp_org, sizeof(llc.org)) == 0) &&
	    (llc.protoid == htons(LLC_PID_FDP))) {
	    return(EXIT_SUCCESS);
	}

	// ndp
	const uint8_t ndp_dst[] = NDP_MULTICAST_ADDR;
	const uint8_t ndp_org[] = LLC_ORG_NORTEL;

	if ((memcmp(ether.dst, ndp_dst, ETHER_ADDR_LEN) == 0) &&
	    (memcmp(llc.org, ndp_org, sizeof(llc.org)) == 0) &&
	    (llc.protoid == htons(LLC_PID_NDP_HELLO))) {
	    return(EXIT_SUCCESS);
	}
    }

#if HAVE_LINUX_ETHTOOL_H
    if (mreq->cmd == MASTER_ETHTOOL) {
	if (mreq->len == sizeof(struct ethtool_cmd)) 
	    return(EXIT_SUCCESS);
    }
#endif /* HAVE_LINUX_ETHTOOL_H */

    return(EXIT_FAILURE);
}


int master_rsocket(struct master_rfd *rfd, int mode) {

    int rsocket = -1;

    // return stdout on debug
    if ((loglevel >= DEBUG) && (rfd == NULL))
	return(1);

#ifdef HAVE_NETPACKET_PACKET_H
    rsocket = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));

    // socket open failed
    if (rsocket < 0)
	return(rsocket);

    // return unbound socket if requested
    if (rfd == NULL)
	return(rsocket);

    // bind the socket to rfd
    struct sockaddr_ll sa;
    memset(&sa, 0, sizeof (sa));

    sa.sll_family = AF_PACKET;
    sa.sll_ifindex = rfd->index;
    sa.sll_protocol = htons(ETH_P_ALL);

    if (bind(rsocket, (struct sockaddr *)&sa, sizeof (sa)) != 0)
	my_fatal("failed to bind socket to %s", rfd->name);

#elif HAVE_NET_BPF_H
    int n = 0;
    char dev[50];

    do {
	snprintf(dev, sizeof(dev), "/dev/bpf%d", n++);
	rsocket = open(dev, mode);
    } while (rsocket < 0 && errno == EBUSY);

    // no free bpf available
    if (rsocket < 0)
	return(rsocket);

    // return unbound socket if requested
    if (rfd == NULL)
	return(rsocket);

    // bind the socket to rfd
    struct ifreq ifr;

    // prepare ifr struct
    memset(&ifr, 0, sizeof(ifr));
    strlcpy(ifr.ifr_name, rfd->name, IFNAMSIZ);

    if (ioctl(rsocket, BIOCSETIF, (caddr_t)&ifr) < 0)
	my_fatal("failed to bind socket to %s", rfd->name);
#endif

    return(rsocket);
}


void master_rconf(struct master_rfd *rfd, struct proto *protos) {

    struct ifreq ifr;
    int p;

#ifdef HAVE_LINUX_FILTER_H
    // install socket filter
    struct sock_fprog fprog;

    memset(&fprog, 0, sizeof(fprog));
    fprog.filter = master_filter; 
    fprog.len = sizeof(master_filter) / sizeof(struct sock_filter);

    if (setsockopt(rfd->fd, SOL_SOCKET, SO_ATTACH_FILTER,
		   &fprog, sizeof(fprog)) < 0)
	my_fatal("unable to configure socket filter for %s", rfd->name);
#elif HAVE_NET_BPF_H

    // install bpf filter
    struct bpf_program fprog;

    memset(&fprog, 0, sizeof(fprog));
    fprog.bf_insns = master_filter; 
    fprog.bf_len = sizeof(master_filter) / sizeof(struct bpf_program);

    if (ioctl(rfd->fd, BIOCSETF, (caddr_t)&fprog) < 0)
	my_fatal("unable to configure bpf filter for %s", rfd->name);
#endif

    // configure multicast recv
    memset(&ifr, 0, sizeof(ifr));
    strlcpy(ifr.ifr_name, rfd->name, IFNAMSIZ);

    for (p = 0; protos[p].name != NULL; p++) {
	
	// only enabled protos
	if (protos[p].enabled == 0)
	    continue;

#ifdef AF_PACKET
	memcpy(ifr.ifr_hwaddr.sa_data, protos[p].dst_addr, ETHER_ADDR_LEN);
#endif
#ifdef AF_LINK
	ifr.ifr_addr.sa_family = AF_UNSPEC;
	memcpy(ifr.ifr_addr.sa_data, protos[p].dst_addr, ETHER_ADDR_LEN);
#endif

	if (ioctl(rfd->fd, SIOCADDMULTI, &ifr) < 0)
	    my_fatal("unable to add %s multicast to %s",
		     protos[p].name, rfd->name);
    }
}

size_t master_rsend(int s, struct master_msg *mreq) {

    size_t count = 0;

    pcaprec_hdr_t pcap_rec_hdr;
    struct timeval tv;

    // debug
    if (loglevel >= DEBUG) {

	// write a pcap record header
	if (gettimeofday(&tv, NULL) == 0) {
	    pcap_rec_hdr.ts_sec = tv.tv_sec;
	    pcap_rec_hdr.ts_usec = tv.tv_usec;
	    pcap_rec_hdr.incl_len = mreq->len;
	    pcap_rec_hdr.orig_len = mreq->len;

	    (void) write(s, &pcap_rec_hdr, sizeof(pcap_rec_hdr));
	}

	return(write(s, mreq->msg, mreq->len));
    }

#ifdef HAVE_NETPACKET_PACKET_H
    struct sockaddr_ll sa;
    memset(&sa, 0, sizeof (sa));

    sa.sll_family = AF_PACKET;
    sa.sll_ifindex = mreq->index;
    sa.sll_protocol = htons(ETH_P_ALL);

    count = sendto(s, mreq->msg, mreq->len, 0,
		   (struct sockaddr *)&sa, sizeof (sa));
#elif HAVE_NET_BPF_H
    struct ifreq ifr;

    // prepare ifr struct
    memset(&ifr, 0, sizeof(ifr));
    strlcpy(ifr.ifr_name, mreq->name, IFNAMSIZ);

    if (ioctl(s, BIOCSETIF, (caddr_t)&ifr) < 0)
	my_fatal("ioctl failed: %s", strerror(errno));
    count = write(s, mreq->msg, mreq->len);
#endif

    if (count != mreq->len)
	my_log(WARN, "only %d bytes written: %s", count, strerror(errno));

    return(count);
}


#if HAVE_LINUX_ETHTOOL_H
size_t master_ethtool(int s, struct master_msg *mreq) {

    struct ifreq ifr;
    struct ethtool_cmd ecmd;

    // prepare ifr struct
    memset(&ifr, 0, sizeof(ifr));
    strlcpy(ifr.ifr_name, mreq->name, IFNAMSIZ);

    // prepare ecmd struct
    memset(&ecmd, 0, sizeof(ecmd));
    ecmd.cmd = ETHTOOL_GSET;
    ifr.ifr_data = (caddr_t)&ecmd;

    if (ioctl(s, SIOCETHTOOL, &ifr) >= 0) {
	memcpy(mreq->msg, &ecmd, sizeof(ecmd));
	return(sizeof(ecmd));
    } else {
	return(0);
    }
}
#endif /* HAVE_LINUX_ETHTOOL_H */
