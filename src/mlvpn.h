#ifndef _MLVPN_H
#define _MLVPN_H

#define VER_MAJ 1
#define VER_MIN 1

#include <stdint.h>
#include <arpa/inet.h>
#include <linux/if_tun.h>
#include <linux/if.h>
#include <stdio.h>

#include "pkt.h"
#include "buffer.h"

/* undef this to disable control system completly.
   will gain in security and very small performance,
   but lack of statistics & remote control obviously! */
#define MLVPN_CONTROL
/* undef this to disable frame introspection
 * This can improve performance, but will
 * disable the "high priority queue"
 */

#define MLVPN_ETH_IP4 0x0800
#define MLVPN_ETH_IP6 0x86DD
#define MLVPN_ETH_ARP 0x0806

#define MLVPN_MAXHNAMSTR 1024
#define MLVPN_MAXPORTSTR 5
#define MLVPN_MAGIC 0xED

#define MLVPN_MAX_COMMAND_ARGS 32

/* 4 Kbytes re-assembly buffer */
#define BUFSIZE 1024 * 4
/* Number of packets in the queue. Each pkt is ~ 1520 */
/* 1520 * 128 ~= 24 KBytes of data maximum per channel VMSize */
#define PKTBUFSIZE 128
/* Maximum channels */
#define MAXTUNNELS 128

/* tuntap interface name size */
#define MLVPN_IFNAMSIZ IFNAMSIZ

struct tuntap_s
{
    int fd;
    int mtu;
    char devname[IFNAMSIZ];
    int type; /* MLVPN_TUNTAPMODE_* */
};


struct mlvpn_ether
{
    uint8_t src[6];
    uint8_t dst[6];
    uint16_t proto;
};

struct mlvpn_ipv4
{
    uint8_t version_and_length;
    uint8_t tos;
    uint16_t length;
    uint16_t id;
    uint16_t frag;
    uint8_t ttl;
    uint8_t proto;
    uint16_t checksum;
    uint32_t src;
    uint32_t dst;
};

struct mlvpn_buffer
{
    size_t len;
    char data[BUFSIZE];
};

struct mlvpn_options
{
    /* use ps_status or not ? */
    int change_process_title;
    /* process name if set */
    char process_name[1024];
    /* where is the config file */
    /* TODO: PATHMAX */
    char config[1024];
    int config_fd;
    /* verbose mode */
    int verbose;
    /* background */
    int background;
    /* pidfile */
    char pidfile[1024];
    /* User change if running as root */
    char unpriv_user[128];
    int root_allowed;
};

typedef struct mlvpn_tunnel_s
{
    char *name;           /* tunnel name */
    char *bindaddr;       /* packets source */
    char *bindport;       /* packets port source (or NULL) */
    char *destaddr;       /* remote server ip (can be hostname) */
    char *destport;       /* remote server port */
    int fd;               /* socket file descriptor */
    int server_fd;        /* server socket (used to accept) */
    int server_mode;      /* server or client */
    int disconnects;      /* is it stable ? */
    int conn_attempts;    /* connection attempts */
    time_t next_attempt;  /* next connection attempt */
    double weight;        /* For weight round robin */
    uint64_t sentpackets; /* 64bit packets sent counter */
    uint64_t recvpackets; /* 64bit packets recv counter */
    uint64_t sentbytes;   /* 64bit bytes sent counter */
    uint64_t recvbytes;   /* 64bit bytes recv counter */
    pktbuffer_t *sbuf;    /* send buffer */
    pktbuffer_t *hpsbuf;  /* high priority buffer */
    struct mlvpn_buffer rbuf;    /* receive buffer */
    struct mlvpn_tunnel_s *next; /* chained list to next element */
    int encap_prot;       /* ENCAP_PROTO_UDP or ENCAP_PROTO_TCP */
    struct addrinfo *addrinfo;
    int status;           /* CHAP status */
    time_t last_packet_time; /* Used to timeout the link */
    time_t timeout;
    time_t next_keepalive; /* when to send the "next" keepalive packet */
} mlvpn_tunnel_t;

enum {
    ENCAP_PROTO_UDP,
    ENCAP_PROTO_TCP
};

enum {
    MLVPN_CHAP_DISCONNECTED,
    MLVPN_CHAP_AUTHSENT,
    MLVPN_CHAP_AUTHOK
};

enum {
    MLVPN_TUNTAPMODE_TUN,
    MLVPN_TUNTAPMODE_TAP
};

int mlvpn_config(int config_file_fd, int first_time);
void init_buffers();

uint64_t mlvpn_millis();
int mlvpn_sock_set_nonblocking(int fd);

/* Should be elsewhere ! */
void print_ether(struct mlvpn_ether *ether);
void print_ip4(struct mlvpn_ipv4 *ip4);
struct mlvpn_ether *
decap_ethernet_frame(struct mlvpn_ether *ether, const void *buffer);
struct mlvpn_ipv4 *
decap_ip4_frame(struct mlvpn_ipv4 *ip4, const void *buffer);
void print_frame(const char *frame);

int mlvpn_tuntap_read();
int mlvpn_tuntap_write();
int mlvpn_taptun_alloc();

void mlvpn_rtun_status_up(mlvpn_tunnel_t *t);
void mlvpn_rtun_status_down(mlvpn_tunnel_t *t);
void mlvpn_rtun_tick(mlvpn_tunnel_t *t);
void mlvpn_rtun_tick_connect();
void mlvpn_rtun_keepalive(time_t now, mlvpn_tunnel_t *t);
void mlvpn_rtun_check_timeout();
void mlvpn_rtun_recalc_weight();
int mlvpn_rtun_bind(mlvpn_tunnel_t *t);
int mlvpn_rtun_connect(mlvpn_tunnel_t *t);
int mlvpn_rtun_tick_rbuf(mlvpn_tunnel_t *tun);
int mlvpn_rtun_read(mlvpn_tunnel_t *tun);
int mlvpn_rtun_write(mlvpn_tunnel_t *tun);
int mlvpn_rtun_write_pkt(mlvpn_tunnel_t *tun, pktbuffer_t *pktbuf);
int mlvpn_rtun_timer_write(mlvpn_tunnel_t *t);
mlvpn_tunnel_t *mlvpn_rtun_last();
mlvpn_tunnel_t *mlvpn_rtun_choose();
mlvpn_tunnel_t *
mlvpn_rtun_new(const char *name,
               const char *bindaddr, const char *bindport,
               const char *destaddr, const char *destport,
               int server_mode);

int mlvpn_server_accept();

/* privsep */
int priv_init(char *argv[], char *username);
void send_fd(int sock, int fd);
int receive_fd(int sock);
int priv_open_config(char *);
int priv_open_tun(int tuntapmode, char *devname);
FILE *priv_open_log(char *lognam);
int
priv_getaddrinfo(char *host, char *serv, struct addrinfo **addrinfo,
    struct addrinfo *hints);
void priv_set_running_state(void);
int priv_init_script(char *);
int priv_run_script(int argc, char **argv);

/* wrr */
int mlvpn_rtun_wrr_init(mlvpn_tunnel_t *start);
mlvpn_tunnel_t *mlvpn_rtun_wrr_choose();

#endif
