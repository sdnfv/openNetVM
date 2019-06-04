/*********************************************************************
 *                     openNetVM
 *              https://sdnfv.github.io
 *
 *   BSD LICENSE
 *
 *   Copyright(c)
 *            2015-2019 George Washington University
 *            2015-2019 University of California Riverside
 *            2010-2019 Intel Corporation. All rights reserved.
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
 *     * The name of the author may not be used to endorse or promote
 *       products derived from this software without specific prior
 *       written permission.
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
 *
 ********************************************************************/

/******************************************************************************

                                  onvm_init.c

                  File containing initialization functions.


******************************************************************************/

#include "onvm_mgr/onvm_init.h"

/********************************Global variables*****************************/

struct onvm_nf *nfs = NULL;
struct port_info *ports = NULL;
struct core_status *cores = NULL;
struct onvm_configuration *onvm_config = NULL;
struct nf_wakeup_info *nf_wakeup_infos = NULL;

struct rte_mempool *pktmbuf_pool;
struct rte_mempool *nf_init_cfg_pool;
struct rte_mempool *nf_msg_pool;
struct rte_ring *incoming_msg_queue;
uint16_t **services;
uint16_t *nf_per_service_count;
struct onvm_service_chain *default_chain;
struct onvm_service_chain **default_sc_p;

/*************************Internal Functions Prototypes***********************/

static void
set_default_config(struct onvm_configuration *config);

static int
init_mbuf_pools(void);

static int
init_nf_init_cfg_pool(void);

static int
init_nf_msg_pool(void);

static int
init_port(uint8_t port_num);

static int
init_shm_rings(void);

static void
init_shared_sem(void);

static int
init_info_queue(void);

static void
check_all_ports_link_status(uint8_t port_num, uint32_t port_mask);

/*****************Internal Configuration Structs and Constants*****************/

/*
 * RX and TX Prefetch, Host, and Write-back threshold values should be
 * carefully set for optimal performance. Consult the network
 * controller's datasheet and supporting DPDK documentation for guidance
 * on how these parameters should be set.
 */
#define RX_PTHRESH 8 /* Default values of RX prefetch threshold reg. */
#define RX_HTHRESH 8 /* Default values of RX host threshold reg. */
#define RX_WTHRESH 4 /* Default values of RX write-back threshold reg. */

/*
 * These default values are optimized for use with the Intel(R) 82599 10 GbE
 * Controller and the DPDK ixgbe PMD. Consider using other values for other
 * network controllers and/or network drivers.
 */
#define TX_PTHRESH 36 /* Default values of TX prefetch threshold reg. */
#define TX_HTHRESH 0  /* Default values of TX host threshold reg. */
#define TX_WTHRESH 0  /* Default values of TX write-back threshold reg. */

static const struct rte_eth_conf port_conf = {
    .rxmode = {
            .mq_mode = ETH_MQ_RX_RSS,
            .max_rx_pkt_len = ETHER_MAX_LEN,
            .split_hdr_size = 0,
            .offloads = DEV_RX_OFFLOAD_CHECKSUM,
        },
    .rx_adv_conf = {
            .rss_conf = {
                    .rss_key = rss_symmetric_key, .rss_hf = ETH_RSS_IP | ETH_RSS_UDP | ETH_RSS_TCP | ETH_RSS_L2_PAYLOAD,
                },
        },
    .txmode = {.mq_mode = ETH_MQ_TX_NONE,
               .offloads = (DEV_TX_OFFLOAD_IPV4_CKSUM | DEV_TX_OFFLOAD_UDP_CKSUM | DEV_TX_OFFLOAD_TCP_CKSUM)},
};

/*********************************Interfaces**********************************/

int
init(int argc, char *argv[]) {
        int retval;
        const struct rte_memzone *mz_nf;
        const struct rte_memzone *mz_port;
        const struct rte_memzone *mz_cores;
        const struct rte_memzone *mz_scp;
        const struct rte_memzone *mz_services;
        const struct rte_memzone *mz_nf_per_service;
        const struct rte_memzone *mz_onvm_config;
        uint8_t i, total_ports, port_id;

        /* init EAL, parsing EAL args */
        retval = rte_eal_init(argc, argv);
        if (retval < 0)
                return -1;
        argc -= retval;
        argv += retval;

#ifdef RTE_LIBRTE_PDUMP
        rte_pdump_init(NULL);
#endif

        /* get total number of ports */
        total_ports = rte_eth_dev_count_avail();

        /* set up array for NF tx data */
        mz_nf = rte_memzone_reserve(MZ_NF_INFO, sizeof(*nfs) * MAX_NFS, rte_socket_id(), NO_FLAGS);
        if (mz_nf == NULL)
                rte_exit(EXIT_FAILURE, "Cannot reserve memory zone for nf information\n");
        memset(mz_nf->addr, 0, sizeof(*nfs) * MAX_NFS);
        nfs = mz_nf->addr;

        /* set up ports info */
        mz_port = rte_memzone_reserve(MZ_PORT_INFO, sizeof(*ports), rte_socket_id(), NO_FLAGS);
        if (mz_port == NULL)
                rte_exit(EXIT_FAILURE, "Cannot reserve memory zone for port information\n");
        ports = mz_port->addr;

        /* set up core status */
        mz_cores = rte_memzone_reserve(MZ_CORES_STATUS, sizeof(*cores) * onvm_threading_get_num_cores(),
                                       rte_socket_id(), NO_FLAGS);
        if (mz_cores == NULL)
                rte_exit(EXIT_FAILURE, "Cannot reserve memory zone for core information\n");
        memset(mz_cores->addr, 0, sizeof(*cores) * 64);
        cores = mz_cores->addr;

        /* set up array for NF tx data */
        mz_services =
            rte_memzone_reserve(MZ_SERVICES_INFO, sizeof(uint16_t *) * num_services, rte_socket_id(), NO_FLAGS);
        if (mz_services == NULL)
                rte_exit(EXIT_FAILURE, "Cannot reserve memory zone for services information\n");
        services = mz_services->addr;
        for (i = 0; i < num_services; i++) {
                services[i] = rte_calloc("one service NFs", MAX_NFS_PER_SERVICE, sizeof(uint16_t), 0);
        }
        mz_nf_per_service =
            rte_memzone_reserve(MZ_NF_PER_SERVICE_INFO, sizeof(uint16_t) * num_services, rte_socket_id(), NO_FLAGS);
        if (mz_nf_per_service == NULL) {
                rte_exit(EXIT_FAILURE, "Cannot reserve memory zone for NF per service information.\n");
        }
        nf_per_service_count = mz_nf_per_service->addr;

        /* set up custom flags */
        mz_onvm_config = rte_memzone_reserve(MZ_ONVM_CONFIG, sizeof(uint16_t), rte_socket_id(), NO_FLAGS);
        if (mz_onvm_config == NULL) {
                rte_exit(EXIT_FAILURE, "Cannot reserve memory zone for ONVM custom flags.\n");
        }
        onvm_config = mz_onvm_config->addr;
        set_default_config(onvm_config);

        /* parse additional, application arguments */
        retval = parse_app_args(total_ports, argc, argv);
        if (retval != 0)
                return -1;

        /* initialise mbuf pools */
        retval = init_mbuf_pools();
        if (retval != 0)
                rte_exit(EXIT_FAILURE, "Cannot create needed mbuf pools\n");

        /* initialise nf info pool */
        retval = init_nf_init_cfg_pool();
        if (retval != 0) {
                rte_exit(EXIT_FAILURE, "Cannot create nf info mbuf pool: %s\n", rte_strerror(rte_errno));
        }

        /* initialise pool for NF messages */
        retval = init_nf_msg_pool();
        if (retval != 0) {
                rte_exit(EXIT_FAILURE, "Cannot create nf message pool: %s\n", rte_strerror(rte_errno));
        }

        /* now initialise the ports we will use */
        for (i = 0; i < ports->num_ports; i++) {
                port_id = ports->id[i];
                rte_eth_macaddr_get(port_id, &ports->mac[port_id]);
                retval = init_port(port_id);
                if (retval != 0)
                        rte_exit(EXIT_FAILURE, "Cannot initialise port %u\n", port_id);
                char event_msg_buf[22];
                snprintf(event_msg_buf, sizeof(event_msg_buf), "Port %d initialized", port_id);
                onvm_stats_gen_event_info(event_msg_buf, ONVM_EVENT_PORT_INFO, NULL);
        }

        check_all_ports_link_status(ports->num_ports, (~0x0));

        /* initialise the NF queues/rings for inter-eu comms */
        init_shm_rings();

        /* initialise a queue for newly created NFs */
        init_info_queue();

        /* initialise the shared memory for shared core mode */
        init_shared_sem();

        /*initialize a default service chain*/
        default_chain = onvm_sc_create();
        retval = onvm_sc_append_entry(default_chain, ONVM_NF_ACTION_TONF, 1);
        if (retval == ENOSPC) {
                printf("Chain length can not be larger than the maximum chain length\n");
                exit(1);
        }
        printf("Default service chain: send to sdn NF\n");

        /* set up service chain pointer shared to NFs*/
        mz_scp = rte_memzone_reserve(MZ_SCP_INFO, sizeof(struct onvm_service_chain *), rte_socket_id(), NO_FLAGS);
        if (mz_scp == NULL)
                rte_exit(EXIT_FAILURE, "Cannot reserve memory zone for service chain pointer\n");
        memset(mz_scp->addr, 0, sizeof(struct onvm_service_chain *));
        default_sc_p = mz_scp->addr;
        *default_sc_p = default_chain;
        onvm_sc_print(default_chain);

        onvm_flow_dir_init();

        return 0;
}

/*****************************Internal functions******************************/

/**
 * Initialise the default onvm config structure
 */
static void
set_default_config(struct onvm_configuration *config) {
        config->flags.ONVM_NF_SHARE_CORES = ONVM_NF_SHARE_CORES_DEFAULT;
}

/**
 * Initialise the mbuf pool for packet reception for the NIC, and any other
 * buffer pools needed by the app - currently none.
 */
static int
init_mbuf_pools(void) {
        /* don't pass single-producer/single-consumer flags to mbuf create as it
         * seems faster to use a cache instead */
        printf("Creating mbuf pool '%s' [%u mbufs] ...\n", PKTMBUF_POOL_NAME, NUM_MBUFS);
        pktmbuf_pool = rte_mempool_create(PKTMBUF_POOL_NAME, NUM_MBUFS, MBUF_SIZE, MBUF_CACHE_SIZE,
                                          sizeof(struct rte_pktmbuf_pool_private), rte_pktmbuf_pool_init, NULL,
                                          rte_pktmbuf_init, NULL, rte_socket_id(), NO_FLAGS);

        return (pktmbuf_pool == NULL); /* 0  on success */
}

/**
 * Set up a mempool to store nf_msg structs
 */
static int
init_nf_msg_pool(void) {
        /* don't pass single-producer/single-consumer flags to mbuf
         * create as it seems faster to use a cache instead */
        printf("Creating mbuf pool '%s' ...\n", _NF_MSG_POOL_NAME);
        nf_msg_pool = rte_mempool_create(_NF_MSG_POOL_NAME, MAX_NFS * NF_MSG_QUEUE_SIZE, NF_INFO_SIZE,
                                         NF_MSG_CACHE_SIZE, 0, NULL, NULL, NULL, NULL, rte_socket_id(), NO_FLAGS);

        return (nf_msg_pool == NULL); /* 0 on success */
}

/**
 * Set up a mempool to store nf_init_cfg structs
 */
static int
init_nf_init_cfg_pool(void) {
        /* don't pass single-producer/single-consumer flags to mbuf
         * create as it seems faster to use a cache instead */
        printf("Creating mbuf pool '%s' ...\n", _NF_MEMPOOL_NAME);
        nf_init_cfg_pool = rte_mempool_create(_NF_MEMPOOL_NAME, MAX_NFS, NF_INFO_SIZE, 0, 0, NULL, NULL, NULL, NULL,
                                          rte_socket_id(), NO_FLAGS);

        return (nf_init_cfg_pool == NULL); /* 0 on success */
}

/**
 * Initialise an individual port:
 * - configure number of rx and tx rings
 * - set up each rx ring, to pull from the main mbuf pool
 * - set up each tx ring
 * - start the port and report its status to stdout
 */
static int
init_port(uint8_t port_num) {
        const uint16_t rx_rings = ONVM_NUM_RX_THREADS;
        uint16_t rx_ring_size = RTE_MP_RX_DESC_DEFAULT;
        /* Set the number of tx_rings equal to the tx threads. This mimics the onvm_mgr tx thread calculation. */
        const uint16_t tx_rings = rte_lcore_count() - rx_rings - ONVM_NUM_MGR_AUX_THREADS;
        uint16_t tx_ring_size = RTE_MP_TX_DESC_DEFAULT;

        struct rte_eth_rxconf rxq_conf;
        struct rte_eth_txconf txq_conf;
        struct rte_eth_dev_info dev_info;
        struct rte_eth_conf local_port_conf = port_conf;

        uint16_t q;
        int retval;

        printf("Port %u init ... \n", (unsigned)port_num);
        printf("Port %u socket id %u ... \n", (unsigned)port_num, (unsigned)rte_eth_dev_socket_id(port_num));
        printf("Port %u Rx rings %u ... \n", (unsigned)port_num, (unsigned)rx_rings);
        printf("Port %u Tx rings %u ... \n", (unsigned)port_num, (unsigned)tx_rings);
        fflush(stdout);

        /* Standard DPDK port initialisation - config port, then set up
         * rx and tx rings */
        rte_eth_dev_info_get(port_num, &dev_info);
        if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE)
                local_port_conf.txmode.offloads |= DEV_TX_OFFLOAD_MBUF_FAST_FREE;
        local_port_conf.rx_adv_conf.rss_conf.rss_hf &= dev_info.flow_type_rss_offloads;
        if (local_port_conf.rx_adv_conf.rss_conf.rss_hf != port_conf.rx_adv_conf.rss_conf.rss_hf) {
                printf(
                    "Port %u modified RSS hash function based on hardware support,"
                    "requested:%#" PRIx64 " configured:%#" PRIx64 "\n",
                    port_num, port_conf.rx_adv_conf.rss_conf.rss_hf, local_port_conf.rx_adv_conf.rss_conf.rss_hf);
        }

        if ((retval = rte_eth_dev_configure(port_num, rx_rings, tx_rings, &local_port_conf)) != 0)
                return retval;

        /* Adjust rx,tx ring sizes if not allowed by ethernet device
         * TODO if this is ajusted store the new values for future reference */
        retval = rte_eth_dev_adjust_nb_rx_tx_desc(port_num, &rx_ring_size, &tx_ring_size);
        if (retval < 0) {
                rte_panic("Cannot adjust number of descriptors for port %u (%d)\n", port_num, retval);
        }

        rxq_conf = dev_info.default_rxconf;
        rxq_conf.offloads = local_port_conf.rxmode.offloads;
        for (q = 0; q < rx_rings; q++) {
                retval = rte_eth_rx_queue_setup(port_num, q, rx_ring_size, rte_eth_dev_socket_id(port_num), &rxq_conf,
                                                pktmbuf_pool);
                if (retval < 0)
                        return retval;
        }

        txq_conf = dev_info.default_txconf;
        txq_conf.offloads = port_conf.txmode.offloads;
        for (q = 0; q < tx_rings; q++) {
                retval = rte_eth_tx_queue_setup(port_num, q, tx_ring_size, rte_eth_dev_socket_id(port_num), &txq_conf);
                if (retval < 0)
                        return retval;
        }

        rte_eth_promiscuous_enable(port_num);

        retval = rte_eth_dev_start(port_num);
        if (retval < 0)
                return retval;

        printf("done: \n");

        return 0;
}

/**
 * Initialize shared core structs (mutex/semaphore)
 */
static void
init_shared_sem(void) {
        uint16_t i;
        key_t key;
        int shmid;
        char *shm;
        sem_t *mutex;
        const char * sem_name;

        nf_wakeup_infos = rte_calloc("MGR_SHM_INFOS", sizeof(struct nf_wakeup_info), MAX_NFS, 0);

        if (!ONVM_NF_SHARE_CORES)
                return;

        for (i = 0; i < MAX_NFS; i++) {
                sem_name = get_sem_name(i);
                nf_wakeup_infos[i].sem_name = sem_name;

                mutex = sem_open(sem_name, O_CREAT, 06666, 0);
                if (mutex == SEM_FAILED) {
                        fprintf(stderr, "can not create semaphore for NF %d\n", i);
                        sem_unlink(sem_name);
                        exit(1);
                }
                nf_wakeup_infos[i].mutex = mutex;

                key = get_rx_shmkey(i);
                if ((shmid = shmget(key, SHMSZ, IPC_CREAT | 0666)) < 0) {
                        fprintf(stderr, "can not create the shared memory segment for NF %d\n", i);
                        exit(1);
                }

                if ((shm = shmat(shmid, NULL, 0)) == (char *) -1) {
                        fprintf(stderr, "can not attach the shared segment to the server space for NF %d\n", i);
                        exit(1);
                }

                nf_wakeup_infos[i].shm_server = (rte_atomic16_t *)shm;
        }
}

/**
 * Set up the DPDK rings which will be used to pass packets, via
 * pointers, between the multi-process server and NF processes.
 * Each NF needs one RX queue.
 */
static int
init_shm_rings(void) {
        unsigned i;
        unsigned socket_id;
        const char *rq_name;
        const char *tq_name;
        const char *msg_q_name;
        const unsigned ringsize = NF_QUEUE_RINGSIZE;
        const unsigned msgringsize = NF_MSG_QUEUE_SIZE;

        // use calloc since we allocate for all possible NFs
        // ensure that all fields are init to 0 to avoid reading garbage
        // TODO plopreiato, move to creation when a NF starts
        for (i = 0; i < MAX_NFS; i++) {
                /* Create an RX queue for each NF */
                socket_id = rte_socket_id();
                rq_name = get_rx_queue_name(i);
                tq_name = get_tx_queue_name(i);
                msg_q_name = get_msg_queue_name(i);
                nfs[i].instance_id = i;
                nfs[i].rx_q =
                    rte_ring_create(rq_name, ringsize, socket_id, RING_F_SC_DEQ); /* multi prod, single cons */
                nfs[i].tx_q =
                    rte_ring_create(tq_name, ringsize, socket_id, RING_F_SC_DEQ); /* multi prod, single cons */
                nfs[i].msg_q =
                    rte_ring_create(msg_q_name, msgringsize, socket_id, RING_F_SC_DEQ); /* multi prod, single cons */

                if (nfs[i].rx_q == NULL)
                        rte_exit(EXIT_FAILURE, "Cannot create rx ring queue for NF %u\n", i);

                if (nfs[i].tx_q == NULL)
                        rte_exit(EXIT_FAILURE, "Cannot create tx ring queue for NF %u\n", i);

                if (nfs[i].msg_q == NULL)
                        rte_exit(EXIT_FAILURE, "Cannot create msg queue for NF %u\n", i);
        }
        return 0;
}

/**
 * Allocate a rte_ring for newly created NFs
 */
static int
init_info_queue(void) {
        incoming_msg_queue = rte_ring_create(_MGR_MSG_QUEUE_NAME, MAX_NFS, rte_socket_id(),
                                             RING_F_SC_DEQ);  // MP enqueue (default), SC dequeue

        if (incoming_msg_queue == NULL)
                rte_exit(EXIT_FAILURE, "Cannot create incoming msg queue\n");

        return 0;
}

/* Check the link status of all ports in up to 9s, and print them finally */
static void
check_all_ports_link_status(uint8_t port_num, uint32_t port_mask) {
#define CHECK_INTERVAL 100 /* 100ms */
#define MAX_CHECK_TIME 90  /* 9s (90 * 100ms) in total */
        uint8_t portid, count, all_ports_up, print_flag = 0;
        struct rte_eth_link link;

        printf("\nChecking link status");
        fflush(stdout);
        for (count = 0; count <= MAX_CHECK_TIME; count++) {
                all_ports_up = 1;
                for (portid = 0; portid < port_num; portid++) {
                        if ((port_mask & (1 << ports->id[portid])) == 0)
                                continue;
                        memset(&link, 0, sizeof(link));
                        rte_eth_link_get_nowait(ports->id[portid], &link);
                        /* print link status if flag set */
                        if (print_flag == 1) {
                                if (link.link_status)
                                        printf(
                                            "Port %d Link Up - speed %u "
                                            "Mbps - %s\n",
                                            ports->id[portid], (unsigned)link.link_speed,
                                            (link.link_duplex == ETH_LINK_FULL_DUPLEX) ? ("full-duplex")
                                                                                       : ("half-duplex\n"));
                                else
                                        printf("Port %d Link Down\n", (uint8_t)ports->id[portid]);
                                continue;
                        }
                        /* clear all_ports_up flag if any link down */
                        if (link.link_status == 0) {
                                all_ports_up = 0;
                                break;
                        }
                }
                /* after finally printing all link status, get out */
                if (print_flag == 1)
                        break;

                if (all_ports_up == 0) {
                        printf(".");
                        fflush(stdout);
                        rte_delay_ms(CHECK_INTERVAL);
                }

                /* set the print_flag if all ports up or timeout */
                if (all_ports_up == 1 || count == (MAX_CHECK_TIME - 1)) {
                        print_flag = 1;
                        printf("done\n");
                }
        }
}
