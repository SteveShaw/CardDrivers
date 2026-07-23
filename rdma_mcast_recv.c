/*
 * rdma_mcast_recv.c
 *
 * Minimal RDMA multicast receiver built on libibverbs + librdmacm.
 *
 * Target setup:
 *   RDMA device : mlx5_2
 *   Interface   : mlx1.2000   (must have an IPv4 address, this is how we
 *                              steer address resolution to mlx5_2)
 *   Group       : 224.0.204.1:45001
 *
 * Build:
 *   gcc -O2 -Wall -o rdma_mcast_recv rdma_mcast_recv.c -libverbs -lrdmacm
 *
 * Run:
 *   sudo ./rdma_mcast_recv
 *   (or grant the binary CAP_NET_ADMIN + CAP_IPC_LOCK and raise memlock,
 *   see notes at the bottom of this file)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <rdma/rdma_cma.h>
#include <infiniband/verbs.h>

#define IFACE_NAME         "mlx1.2000"
#define MCAST_GRP          "224.0.204.1"
#define MCAST_PORT         45001
#define RESOLVE_TIMEOUT_MS 2000

/* Confirmed via `show_gids mlx5_2`:
 *   index=3  ndev=mlx1.2000  ver=RoCE v2  IPv4=172.25.27.154
 * rdma_resolve_addr() selects the GID by matching this source IP, so
 * pinning it here guarantees GID index 3 (RoCEv2) is used even if the
 * interface later carries additional addresses. */
#define LOCAL_IP           "172.25.27.154"

#define GRH_SIZE        40      /* HCA prepends a 40B GRH on every UD recv */
#define MAX_PAYLOAD     4096
#define RECV_BUF_LEN    (GRH_SIZE + MAX_PAYLOAD)
#define NUM_RECV_WR     64

struct mcast_ctx {
    struct rdma_event_channel *ec;
    struct rdma_cm_id         *id;
    struct ibv_pd             *pd;
    struct ibv_cq              *cq;
    struct ibv_qp                *qp;
    struct ibv_mr                 *mr;
    uint8_t                        *buf;   /* NUM_RECV_WR * RECV_BUF_LEN */
    union ibv_gid  mgid;   /* saved for detach on cleanup */
    uint16_t       mlid;
    uint32_t       mc_qpn;
    uint32_t       mc_qkey;
};

/* Confirm that LOCAL_IP is actually present on `ifname` right now (fail
 * fast with a clear error instead of silently resolving via the wrong
 * device/GID if the address was ever reassigned). */
static int verify_iface_has_ip(const char *ifname, const char *ip_str)
{
    struct ifaddrs *ifaddr, *ifa;
    struct in_addr want;
    int found = 0;

    inet_pton(AF_INET, ip_str, &want);

    if (getifaddrs(&ifaddr) < 0) {
        perror("getifaddrs");
        return -1;
    }

    for (ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
        struct sockaddr_in *sin;

        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET)
            continue;
        if (strcmp(ifa->ifa_name, ifname) != 0)
            continue;

        sin = (struct sockaddr_in *)ifa->ifa_addr;
        if (sin->sin_addr.s_addr == want.s_addr) {
            found = 1;
            break;
        }
    }
    freeifaddrs(ifaddr);
    return found ? 0 : -1;
}

static int post_recv(struct mcast_ctx *ctx, int idx)
{
    struct ibv_sge sge;
    struct ibv_recv_wr wr, *bad_wr;

    sge.addr   = (uintptr_t)(ctx->buf + (size_t)idx * RECV_BUF_LEN);
    sge.length = RECV_BUF_LEN;
    sge.lkey   = ctx->mr->lkey;

    memset(&wr, 0, sizeof(wr));
    wr.wr_id   = idx;
    wr.sg_list = &sge;
    wr.num_sge = 1;

    return ibv_post_recv(ctx->qp, &wr, &bad_wr);
}

/* PD/CQ/QP must be created only after route resolution, because that's the
 * point at which ctx->id->verbs (the ibv_context for mlx5_2) becomes
 * valid. Buffers are pre-posted before we join so nothing is dropped once
 * the group attach completes. */
static int create_qp_resources(struct mcast_ctx *ctx)
{
    struct ibv_qp_init_attr qp_attr;
    int i;

    ctx->pd = ibv_alloc_pd(ctx->id->verbs);
    if (!ctx->pd) { perror("ibv_alloc_pd"); return -1; }

    ctx->cq = ibv_create_cq(ctx->id->verbs, NUM_RECV_WR, NULL, NULL, 0);
    if (!ctx->cq) { perror("ibv_create_cq"); return -1; }

    ctx->buf = malloc((size_t)NUM_RECV_WR * RECV_BUF_LEN);
    if (!ctx->buf) { perror("malloc"); return -1; }
    memset(ctx->buf, 0, (size_t)NUM_RECV_WR * RECV_BUF_LEN);

    ctx->mr = ibv_reg_mr(ctx->pd, ctx->buf, (size_t)NUM_RECV_WR * RECV_BUF_LEN,
                          IBV_ACCESS_LOCAL_WRITE);
    if (!ctx->mr) { perror("ibv_reg_mr"); return -1; }

    memset(&qp_attr, 0, sizeof(qp_attr));
    qp_attr.qp_type          = IBV_QPT_UD;
    qp_attr.send_cq          = ctx->cq;
    qp_attr.recv_cq          = ctx->cq;
    qp_attr.cap.max_send_wr  = 1;
    qp_attr.cap.max_recv_wr  = NUM_RECV_WR;
    qp_attr.cap.max_send_sge = 1;
    qp_attr.cap.max_recv_sge = 1;

    /* rdma_create_qp() drives the UD QP through INIT -> RTR -> RTS for us. */
    if (rdma_create_qp(ctx->id, ctx->pd, &qp_attr)) {
        perror("rdma_create_qp");
        return -1;
    }
    ctx->qp = ctx->id->qp;

    for (i = 0; i < NUM_RECV_WR; i++) {
        if (post_recv(ctx, i)) {
            perror("ibv_post_recv");
            return -1;
        }
    }

    return 0;
}

static int wait_event(struct mcast_ctx *ctx, enum rdma_cm_event_type expected,
                       struct rdma_cm_event **out_ev)
{
    struct rdma_cm_event *ev;

    if (rdma_get_cm_event(ctx->ec, &ev)) {
        perror("rdma_get_cm_event");
        return -1;
    }

    if (ev->event != expected) {
        fprintf(stderr, "unexpected event %s (wanted %s), status=%d\n",
                rdma_event_str(ev->event), rdma_event_str(expected),
                ev->status);
        rdma_ack_cm_event(ev);
        return -1;
    }

    *out_ev = ev;
    return 0;
}

static void cleanup(struct mcast_ctx *ctx)
{
    if (ctx->qp)
        ibv_detach_mcast(ctx->qp, &ctx->mgid, ctx->mlid);
    if (ctx->id && ctx->id->qp)
        rdma_destroy_qp(ctx->id);
    if (ctx->mr)
        ibv_dereg_mr(ctx->mr);
    if (ctx->cq)
        ibv_destroy_cq(ctx->cq);
    if (ctx->pd)
        ibv_dealloc_pd(ctx->pd);
    free(ctx->buf);
    if (ctx->id)
        rdma_destroy_id(ctx->id);
    if (ctx->ec)
        rdma_destroy_event_channel(ctx->ec);
}

int main(void)
{
    struct mcast_ctx ctx;
    struct sockaddr_in local_addr, mcast_addr;
    struct rdma_cm_event *ev;

    memset(&ctx, 0, sizeof(ctx));

    /* 1. Use the IPv4 address confirmed (via show_gids) to map to the
     *    RoCEv2 GID entry on mlx1.2000 / mlx5_2, so address resolution
     *    below deterministically selects that GID. */
    if (verify_iface_has_ip(IFACE_NAME, LOCAL_IP)) {
        fprintf(stderr,
                "%s is not currently assigned to %s -- check with:\n"
                "  ip addr show %s\n"
                "  show_gids mlx5_2\n", LOCAL_IP, IFACE_NAME, IFACE_NAME);
        return 1;
    }
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_port   = 0;
    inet_pton(AF_INET, LOCAL_IP, &local_addr.sin_addr);

    memset(&mcast_addr, 0, sizeof(mcast_addr));
    mcast_addr.sin_family = AF_INET;
    mcast_addr.sin_port   = htons(MCAST_PORT);
    inet_pton(AF_INET, MCAST_GRP, &mcast_addr.sin_addr);

    /* 2. Standard rdma_cm bring-up. RDMA_PS_UDP is required for multicast
     *    (it maps to a UD QP). */
    ctx.ec = rdma_create_event_channel();
    if (!ctx.ec) { perror("rdma_create_event_channel"); return 1; }

    if (rdma_create_id(ctx.ec, &ctx.id, NULL, RDMA_PS_UDP)) {
        perror("rdma_create_id");
        cleanup(&ctx);
        return 1;
    }

    if (rdma_resolve_addr(ctx.id, (struct sockaddr *)&local_addr,
                           (struct sockaddr *)&mcast_addr,
                           RESOLVE_TIMEOUT_MS)) {
        perror("rdma_resolve_addr");
        cleanup(&ctx);
        return 1;
    }

    if (wait_event(&ctx, RDMA_CM_EVENT_ADDR_RESOLVED, &ev)) { cleanup(&ctx); return 1; }
    rdma_ack_cm_event(ev);

    if (rdma_resolve_route(ctx.id, RESOLVE_TIMEOUT_MS)) {
        perror("rdma_resolve_route");
        cleanup(&ctx);
        return 1;
    }

    if (wait_event(&ctx, RDMA_CM_EVENT_ROUTE_RESOLVED, &ev)) { cleanup(&ctx); return 1; }
    rdma_ack_cm_event(ev);

    printf("route resolved via device %s, port %d\n",
           ibv_get_device_name(ctx.id->verbs->device), ctx.id->port_num);

    /* 3. Create QP + pre-post receive buffers, then join the group. */
    if (create_qp_resources(&ctx)) { cleanup(&ctx); return 1; }

    if (rdma_join_multicast(ctx.id, (struct sockaddr *)&mcast_addr, NULL)) {
        perror("rdma_join_multicast");
        cleanup(&ctx);
        return 1;
    }

    if (wait_event(&ctx, RDMA_CM_EVENT_MULTICAST_JOIN, &ev)) { cleanup(&ctx); return 1; }

    ctx.mc_qpn  = ev->param.ud.qp_num;
    ctx.mc_qkey = ev->param.ud.qkey;
    ctx.mgid    = ev->param.ud.ah_attr.grh.dgid;
    ctx.mlid    = ev->param.ud.ah_attr.dlid;

    /* Attach the QP at the verbs level so the HCA actually delivers
     * packets for this MGID/MLID into our receive queue. */
    if (ibv_attach_mcast(ctx.qp, &ctx.mgid, ctx.mlid)) {
        fprintf(stderr, "ibv_attach_mcast failed: %s\n", strerror(errno));
        rdma_ack_cm_event(ev);
        cleanup(&ctx);
        return 1;
    }

    {
        char gid_str[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &ctx.mgid, gid_str, sizeof(gid_str));
        printf("joined %s:%d -> mgid=%s mlid=0x%x qpn=0x%x qkey=0x%x\n",
               MCAST_GRP, MCAST_PORT, gid_str, ctx.mlid, ctx.mc_qpn, ctx.mc_qkey);
    }

    rdma_ack_cm_event(ev);

    /* 4. Poll for incoming multicast datagrams. */
    printf("waiting for multicast data (Ctrl-C to stop)...\n");
    while (1) {
        struct ibv_wc wc;
        int n = ibv_poll_cq(ctx.cq, 1, &wc);

        if (n < 0) {
            perror("ibv_poll_cq");
            break;
        }
        if (n == 0) {
            usleep(1000);
            continue;
        }
        if (wc.status != IBV_WC_SUCCESS) {
            fprintf(stderr, "recv completion error: %s\n",
                    ibv_wc_status_str(wc.status));
            post_recv(&ctx, (int)wc.wr_id);
            continue;
        }

        uint8_t *payload   = ctx.buf + (size_t)wc.wr_id * RECV_BUF_LEN + GRH_SIZE;
        int      payload_len = (int)wc.byte_len - GRH_SIZE;
        int      i;

        printf("recv %d bytes (src_qp=0x%x slid=%u): ", payload_len,
               wc.src_qp, wc.slid);
        for (i = 0; i < payload_len && i < 32; i++)
            printf("%02x ", payload[i]);
        printf("%s\n", payload_len > 32 ? "..." : "");

        post_recv(&ctx, (int)wc.wr_id); /* recycle the buffer */
    }

    cleanup(&ctx);
    return 0;
}

/*
 * ---------------------------------------------------------------------
 * Required host / network configuration
 * ---------------------------------------------------------------------
 *
 * 1. Packages: rdma-core (libibverbs, librdmacm) with the mlx5 provider,
 *    plus matching kernel driver (in-box mlx5_core/mlx5_ib or MLNX_OFED).
 *      - verify: ibv_devices        should list mlx5_2
 *                ibv_devinfo mlx5_2 should show port state PORT_ACTIVE
 *
 * 2. The VLAN/interface that mlx5_2's port is bound to must exist, be up,
 *    and carry the address this program pins (LOCAL_IP = 172.25.27.154):
 *      ip link show mlx1.2000
 *      ip addr add 172.25.27.154/24 dev mlx1.2000   # adjust /prefix
 *      ip link set mlx1.2000 up
 *    Confirm the netdev <-> RDMA device mapping if in doubt:
 *      rdma link show                 # shows netdev for each RDMA link
 *      ls /sys/class/infiniband/mlx5_2/device/net/
 *
 * 3. A route for the multicast range must exist via that interface so the
 *    kernel/IGMP stack (which rdma_join_multicast() drives under the
 *    hood on RoCEv2) knows where to send the join:
 *      ip route add 224.0.0.0/4 dev mlx1.2000
 *
 * 4. GID confirmed via `show_gids mlx5_2`:
 *      index=3  ndev=mlx1.2000  ver=RoCE v2  IPv4=172.25.27.154
 *    Because rdma_resolve_addr() is called with src=172.25.27.154, it
 *    will deterministically match this GID entry. To also stop the CM
 *    from ever falling back to the RoCEv1 entry (index 0) on this port,
 *    pin the mode explicitly:
 *      cma_roce_mode -d mlx5_2 -p 1 -m 2   # 2 = RoCE v2
 *      cma_roce_mode -d mlx5_2 -p 1        # verify
 *    Sanity check at runtime: the program prints the joined MGID; a
 *    RoCEv2 MGID for an IPv4 group renders as an IPv4-mapped IPv6
 *    address (contains "::ffff:224.0.204.1"-style formatting).
 *
 * 5. Switch fabric: intermediate switches must forward/flood the
 *    multicast group (IGMP snooping querier present, or snooping
 *    disabled) or packets never arrive regardless of host config.
 *
 * 6. Locked-memory limits for ibv_reg_mr(): run as root, or grant the
 *    binary capabilities and raise memlock:
 *      setcap cap_net_admin,cap_net_raw,cap_ipc_lock+ep ./rdma_mcast_recv
 *      ulimit -l unlimited     # or set in /etc/security/limits.conf
 *
 * 7. Firewalling: RoCEv2 traffic uses UDP dest port 4791 at the wire
 *    level (separate from the 45001 "port" used only as the rdma_cm
 *    service identifier above); make sure nothing blocks UDP/4791 or
 *    multicast in general on mlx1.2000.
 * ---------------------------------------------------------------------
 */
