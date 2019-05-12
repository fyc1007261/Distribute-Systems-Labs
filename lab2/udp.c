/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <inttypes.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>

#include <rte_ip.h>
#include <rte_ether.h>
#include <rte_udp.h>

#define RX_RING_SIZE 128
#define TX_RING_SIZE 512

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32


struct ether_addr eth_addr;

static const struct rte_eth_conf port_conf_default = {
	.rxmode = { .max_rx_pkt_len = ETHER_MAX_LEN }
};

/* basicfwd.c: Basic DPDK skeleton forwarding example. */

/*
 * Initializes a given port using global settings and with the RX buffers
 * coming from the mbuf_pool passed as a parameter.
 */
static inline int
port_init(uint8_t port, struct rte_mempool *mbuf_pool)
{
	struct rte_eth_conf port_conf = port_conf_default;
	const uint16_t rx_rings = 1, tx_rings = 1;
	int retval;
	uint16_t q;

	if (port >= rte_eth_dev_count())
		return -1;

	/* Configure the Ethernet device. */
	retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
	if (retval != 0)
		return retval;

	/* Allocate and set up 1 RX queue per Ethernet port. */
	for (q = 0; q < rx_rings; q++) {
		retval = rte_eth_rx_queue_setup(port, q, RX_RING_SIZE,
				rte_eth_dev_socket_id(port), NULL, mbuf_pool);
		if (retval < 0)
			return retval;
	}

	/* Allocate and set up 1 TX queue per Ethernet port. */
	for (q = 0; q < tx_rings; q++) {
		retval = rte_eth_tx_queue_setup(port, q, TX_RING_SIZE,
				rte_eth_dev_socket_id(port), NULL);
		if (retval < 0)
			return retval;
	}

	/* Start the Ethernet port. */
	retval = rte_eth_dev_start(port);
	if (retval < 0)
		return retval;

	/* Display the port MAC address. */
	struct ether_addr addr;
	rte_eth_macaddr_get(port, &addr);
	eth_addr = addr;
	printf("Port %u MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
			   " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "\n",
			(unsigned)port,
			addr.addr_bytes[0], addr.addr_bytes[1],
			addr.addr_bytes[2], addr.addr_bytes[3],
			addr.addr_bytes[4], addr.addr_bytes[5]);

	/* Enable RX in promiscuous mode for the Ethernet device. */
	rte_eth_promiscuous_enable(port);

	return 0;
}

/*
 * The lcore main. This is the main thread that does the work, reading from
 * an input port and writing to an output port.
 */
static __attribute__((noreturn)) void
lcore_main(struct rte_mempool *mbuf_pool)
{
	const uint8_t nb_ports = rte_eth_dev_count();
	uint8_t port;

	/*
	 * Check that the port is on the same NUMA node as the polling thread
	 * for best performance.
	 */
	for (port = 0; port < nb_ports; port++)
		if (rte_eth_dev_socket_id(port) > 0 &&
				rte_eth_dev_socket_id(port) !=
						(int)rte_socket_id())
			printf("WARNING, port %u is on remote NUMA node to "
					"polling thread.\n\tPerformance will "
					"not be optimal.\n", port);
		

	printf("\nCore %u forwarding packets. [Ctrl+C to quit]\n", rte_lcore_id());
    
	

	for (;;) {
			char data[11] = "helloworld\0";
			struct ether_hdr *eth_hdr = NULL;
			struct ipv4_hdr *ip_hdr = NULL;
			struct udp_hdr *u_hdr = NULL;
			char *payload = NULL;

            struct rte_mbuf *mbuf[10];
			mbuf[0] = rte_pktmbuf_alloc(mbuf_pool);
			mbuf[0]->next = NULL;
			mbuf[0]->data_len = sizeof(*eth_hdr) + sizeof(*ip_hdr) + sizeof(*u_hdr) + sizeof(data);
			mbuf[0]->pkt_len = mbuf[0]->data_len;
			
			eth_hdr = rte_pktmbuf_mtod_offset(mbuf[0], struct ether_hdr*, 0);
			ip_hdr = rte_pktmbuf_mtod_offset(mbuf[0], struct ipv4_hdr*, sizeof(struct ether_hdr));
			u_hdr = rte_pktmbuf_mtod_offset(mbuf[0], struct udp_hdr*, sizeof(struct ether_hdr)+ sizeof(struct ipv4_hdr));
			payload = rte_pktmbuf_mtod_offset(mbuf[0], char*, sizeof(struct ether_hdr)+ sizeof(struct ipv4_hdr) + sizeof(struct udp_hdr));

			memcpy(payload, data, strlen(data));

			eth_hdr->d_addr = eth_addr;
			eth_hdr->s_addr = eth_addr;
			eth_hdr->ether_type = 0x0008; // big endian for 0x0800

			u_hdr->src_port = 0xe803;
			u_hdr->dst_port = 0xe903; 
			u_hdr->dgram_len = 0x1300; //big endian for 0x0013
			u_hdr->dgram_cksum = 0xd4d9;

			ip_hdr->version_ihl = 0x45; // ipv4
			ip_hdr->type_of_service = 0;
			ip_hdr->total_length =  0x2700; // 33 in big endian
			ip_hdr->packet_id = 0;
			ip_hdr->fragment_offset = 0;
			ip_hdr->time_to_live = IP_TTL;
			ip_hdr->next_proto_id = IPPROTO_UDP;
			// ip_hdr->hdr_checksum = rte_ipv4_cksum(ip_hdr);
			ip_hdr->hdr_checksum = 0xc4ba;
			ip_hdr->src_addr = IPv4(1,0,0,127);
			ip_hdr->dst_addr = IPv4(1,0,0,127);
           
            // fprintf(stderr, "%s, %lu\n", (char)created_pkt, sizeof(created_pkt));

			/* Send burst of TX packets, to second port of pair. */
			rte_eth_tx_burst(0, 0, mbuf, 1);
            fprintf(stderr, "packet sent.\n");
			sleep(1);
	
	}
}

/*
 * The main function, which does initialization and calls the per-lcore
 * functions.
 */
int
main(int argc, char *argv[])
{
	struct rte_mempool *mbuf_pool;
	unsigned nb_ports;
	uint8_t portid;

	/* Initialize the Environment Abstraction Layer (EAL). */
	int ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");

	argc -= ret;
	argv += ret;

	/* Check that there is an even number of ports to send/receive on. */
	nb_ports = rte_eth_dev_count();
    fprintf(stderr, "%u ports found in total!\n", nb_ports);
    

	/* Creates a new mempool in memory to hold the mbufs. */
	mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS * nb_ports,
		MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

	if (mbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

	/* Initialize all ports. */
	for (portid = 0; portid < nb_ports; portid++)
		if (port_init(portid, mbuf_pool) != 0)
			rte_exit(EXIT_FAILURE, "Cannot init port %"PRIu8 "\n",
					portid);

	if (rte_lcore_count() > 1)
		printf("\nWARNING: Too many lcores enabled. Only 1 used.\n");

	/* Call lcore_main on the master core only. */
	lcore_main(mbuf_pool);

	return 0;
}


