/*
 * $Id$
 *
 * Copyright (c) 2008, 2009, 2010
 *      Sten Spans <sten@blinkenlights.nl>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "common.h"
#include "util.h"
#include "proto/protos.h"
#include <sys/file.h>
#include <sys/un.h>

static void print_brief(struct master_msg *);
static void usage() __noreturn;
extern struct proto protos[];
time_t now;

__noreturn
void cli_main(int argc, char *argv[]) {
    int ch, i;
    uint8_t verbose = 0, proto = 0;
    uint32_t *indexes = NULL;
    struct sockaddr_un sun;
    int fd = -1;
    struct master_msg msg = {};
    ssize_t len;

    options = 0;

    while ((ch = getopt(argc, argv, "LCEFNbdhov")) != -1) {
	switch(ch) {
	    case 'L':
		proto |= (1 << PROTO_LLDP);
		break;
	    case 'C':
		proto |= (1 << PROTO_CDP);
		break;
	    case 'E':
		proto |= (1 << PROTO_EDP);
		break;
	    case 'F':
		proto |= (1 << PROTO_FDP);
		break;
	    case 'N':
		proto |= (1 << PROTO_NDP);
		break;
	    case 'b':
		options |= OPT_BATCH;
		break;
	    case 'd':
		options |= OPT_DEBUG;
		break;
	    case 'o':
		options |= OPT_ONCE;
		break;
	    case 'v':
		verbose++;
		break;
	    default:
		usage();
	}
    }

    argc -= optind;
    argv += optind;

    // default to all protocols
    if (proto == 0)
	proto = UINT8_MAX;

    if (argc) {
	indexes = my_calloc(argc, sizeof(msg.index));
	for (i = 0; i < argc; i++) {
	    indexes[i] = if_nametoindex(argv[i]);
	    if (!indexes[i])
		usage();
	}
    }

    // open socket connection
    fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    // XXX: make do with a stream and hope for the best
    if (fd == -1)
	fd = my_socket(AF_UNIX, SOCK_STREAM, 0);
    memset(&sun, 0, sizeof(sun));
    sun.sun_family = AF_UNIX;
    strlcpy(sun.sun_path, PACKAGE_SOCKET, sizeof(sun.sun_path));
    if (connect(fd, (struct sockaddr *)&sun, sizeof(sun)) == -1)
	my_fatal("failed to open " PACKAGE_SOCKET ": %s", strerror(errno));

    if ((now = time(NULL)) == (time_t)-1)
	my_fatal("failed fetch time: %s", strerror(errno));

    // debug
    if (options & OPT_DEBUG) {
	write_pcap_hdr(fileno(stdout));
    } else if (!(options & OPT_BATCH) && !verbose){
	printf("Capability Codes:\n"
	    "\tr - Repeater, B - Bridge, H - Host, R - Router, S - Switch,\n"
	    "\tW - WLAN Access Point, C - DOCSIS Device, T - Telephone, "
	    "O - Other\n\n");
	printf("Device ID           Local Intf    Proto   "
	       "Hold-time    Capability    Port ID\n");
    }

    while ((len = read(fd, &msg, MASTER_MSG_MAX)) > 0) {

	if (len < MASTER_MSG_MIN || len != MASTER_MSG_LEN(msg.len))
	    continue;

	if (msg.proto > PROTO_MAX)
	    continue;
	if ((msg.len < ETHER_MIN_LEN) || (msg.len > ETHER_MAX_LEN))
	    continue;
	
	// skip unwanted interfaces
	if (indexes) {
	    for (i = 0; i < argc; i++) {
		if (indexes[i] == msg.index)
		    break;
	    }
	    if (i == argc)
		continue;
	}

	// skip unwanted protocols
	if (!(proto & (1 << msg.proto)))
	    continue;

	// debug
	if (options & OPT_DEBUG) {
	    write_pcap_rec(fileno(stdout), &msg);
	    continue;
	}

	// decode packet
	msg.decode = UINT16_MAX;
	if (msg.len != protos[msg.proto].decode(&msg))
	    continue;
	// skip expired packets
	if (msg.ttl < (now - msg.received))
	    continue;
	
	if (!verbose)
	    print_brief(&msg);
	// XXX: TODO
	//else
	//    print_detail(&msg);

	peer_free(msg.peer);

	if (options & OPT_ONCE)
	    exit(EXIT_SUCCESS);
    }

    exit(EXIT_SUCCESS);
}

inline void swapchr(char *str, int c, int d) {
    while ((str = strchr(str, c)) != NULL) {
	*str = d;
	 str++;
    }
}
#define STR(x)	(x) ? x : ""

void print_brief(struct master_msg *msg) {
    uint16_t holdtime = msg->ttl - (now - msg->received);
    static unsigned int count = 0;
    char *hostname = msg->peer[PEER_HOSTNAME];
    char *portname = msg->peer[PEER_PORTNAME];
    char *cap = msg->peer[PEER_CAP];

    if (options & OPT_BATCH) {

	swapchr(hostname, '\'', '\"');
	swapchr(portname, '\'', '\"');

	printf("INTERFACE%u='%s'\n", count, STR(msg->name));
	printf("HOSTNAME%u='%s'\n", count, STR(hostname));
	printf("PORTNAME%u='%s'\n", count, STR(portname));
	printf("PROTOCOL%u='%s'\n", count, protos[msg->proto].name);
	printf("CAPABILITIES%u='%s'\n", count, STR(cap));
	printf("TTL%u='%" PRIu16 "'\n", count, msg->ttl);
	printf("HOLDTIME%u='%" PRIu16 "'\n", count, holdtime);
	count++;
	return;
    }

    // shorten
    if (hostname)
	hostname[strcspn(hostname, ".")] = '\0';
    if (portname)
	portname_abbr(portname);

    printf("%-19.19s %-13.13s %-7.7s %-12" PRIu16 " %-13.13s %-10.10s\n",
	STR(hostname), STR(msg->name), protos[msg->proto].name,
	holdtime, STR(cap),  STR(portname));
}

__noreturn
static void usage() {
    extern char *__progname;

    fprintf(stderr, PACKAGE_NAME " version " PACKAGE_VERSION "\n" 
	"Usage: %s [-LCEFN] [INTERFACE] [INTERFACE]\n"
	    "\t-L = Print LLDP\n"
	    "\t-C = Print CDP\n"
	    "\t-E = Print EDP\n"
	    "\t-F = Print FDP\n"
	    "\t-N = Print NDP\n"
	    "\t-b = Print scriptable output\n"
	    "\t-d = Dump pcap-compatible packets to stdout\n"
	    "\t-h = Print this message\n"
	    "\t-o = Decode only one packet\n"
	    "\t-v = Increase verbosity\n",
	    __progname);

    exit(EXIT_FAILURE);
}

