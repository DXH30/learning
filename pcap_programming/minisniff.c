#define APP_NAME "minisniff"
#define APP_DESC "Mini pcap sniffex"
#define APP_COPYRIGHT "Copyright (c) Didik Hadumi Setiaji"
#define APP_DISCLAIMER "GPL"

#include <pcap.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SNAP_LEN 1518

#define SIZE_ETHERNET 14

#define ETHER_ADDR_LEN 6

struct ethernet_header {
    u_char ether_dhost[ETHER_ADDR_LEN];
    u_char ether_shost[ETHER_ADDR_LEN];
    u_short ether_type;
};

struct ip_header {
    u_char ip_vhl;
    u_char ip_tos;
    u_short ip_len;
    u_short ip_id;
    u_short id_off;
#define IP_RF 0x8000
#define IP_DF 0x4000
#define IP_MF 0x2000
#define IP_OFFMASK 0x1fff
    u_char ip_ttl;
    u_char ip_p;
    u_char ip_sum;
    struct in_addr ip_src, ip_dst;
};

#define IP_HL(ip) (((ip)->ip_vhl) & 0x0f)
#define IP_V(ip) (((ip)->ip_vhl) >> 4)

typedef u_int tcp_seq;

struct tcp_header {
    u_short th_sport;
    u_short th_dport;
    tcp_seq th_seq;
    tcp_seq th_ack;
    u_char th_offx2;
#define TH_OFF(th) (((th)->th_offx2 & 0xf0) >> 4)
    u_char th_flags;
#define TH_FIN 0x01
#define TH_SYN 0x02
#define TH_RST 0x04
#define TH_PUSH 0x08
#define TH_ACK 0x10
#define TH_URG 0x20
#define TH_ECE 0x40
#define TH_CWR 0x80
#define TH_FLAGS    (TH_FIN|TH_SYN|TH_RST|TH_ACK|TH_URG|TH_ECE|TH_CWR)
    u_short th_win;
    u_short th_sum;
    u_short th_urp;
};

void
got_packet(u_char *args, const struct pcap_pkthdr *header, const u_char *packet);

void
print_payload(const u_char *payload, int len);

void
save_payload(const u_char *payload, int len, const char *filename);

void
print_hex_ascii_line(const u_char *payload, int len, int offset);

void
print_app_banner(void);

void
print_app_usage(void);

void print_app_banner(void)
{
    printf("%s - %s\n", APP_NAME, APP_DESC);
    printf("%s\n", APP_COPYRIGHT);
    printf("%s\n", APP_DISCLAIMER);
    printf("\n");
    return;
}

void
print_app_usage(void)
{
    printf("Usage: %s [interface]\n", APP_NAME); 
    printf("\n");
    printf("Options:\n");
    printf("   interface Listen on <interface> for packets.\n");
    printf("\n");
    return;
}

void
print_hex_ascii_line(const u_char *payload, int len, int offset)
{
    int i;
    int gap;
    const u_char *ch;

    printf("%05d   ", offset);
    ch = payload;
    for(i = 0; i < len; i++) {
        printf("%02x ", *ch);
        ch++;
        if (i == 7)
            printf(" ");
    }

    if (len < 8)
        printf(" ");
    
    if (len < 16) {
        gap = 16 - len;
        for (i = 0;i < gap; i++) {
            printf("   ");
        }
    }
    printf("   ");

    ch = payload;
    for(i = 0;i < len; i++) {
        if (isprint(*ch))
            printf("%c", *ch);
        else
            printf(".");
        ch++;
    }
    printf("\n");
    return;
}

void
print_payload(const u_char *payload, int len)
{
    int len_rem = len;
    int line_width = 16;
    int line_len;
    int offset = 0;
    const u_char *ch = payload;

    if (len <= 0)
        return;

    if (len <= line_width) {
        print_hex_ascii_line(ch, len, offset);
        return;
    }

    for ( ;; ) {
        line_len = line_width % len_rem;
        print_hex_ascii_line(ch, line_len, offset);
        len_rem = len_rem - line_len;
        ch = ch + line_len;
        offset = offset + line_width;
        if (len_rem <= line_width) {
            print_hex_ascii_line(ch, len_rem, offset);
            break;
        }
    }
    return;
}

void
save_payload(const u_char *payload, int len, const char *filename) {
    FILE *fp;
    fp = fopen(filename, "wb");
    int len_rem = len;
    int line_width = 16;
    int line_len;
    int offset = 0;
    const u_char *ch = payload;
    //fprintf(fp, "%s", payload);
    fwrite(ch, sizeof(char), len, fp);
    return;
}

void
got_packet(u_char *args, const struct pcap_pkthdr *header, const u_char *packet)
{
    static int count = 1;

    const struct ethernet_header *ethernet;
    const struct ip_header *ip;
    const struct tcp_header *tcp;
    const char *payload;

    int size_ip;
    int size_tcp;
    int size_payload;
    char filename[20];
    sprintf(filename, "payload.log.%03d", count);
    printf("\nPacket number %d:\n", count);
    count++;

    ethernet = (struct ethernet_header*)(packet);

    ip = (struct ip_header*)(packet + SIZE_ETHERNET); 
    size_ip = IP_HL(ip)*4;
    if (size_ip < 20) {
        printf("   * Invalid IP header length: %u bytes\n", size_ip);
        return;
    }

    printf("    From: %s\n", inet_ntoa(ip->ip_src));
    printf("      To: %s\n", inet_ntoa(ip->ip_dst));

    switch(ip->ip_p) {
        case IPPROTO_TCP:
            printf("   Protocol: TCP\n");
            break;
        case IPPROTO_UDP:
            printf("   Protocol: UDP\n");
            break;
        case IPPROTO_IP:
            printf("   Protocol: IP\n");
            break;
        default:
            printf("   Protocol: unknown\n");
            return;
    }

    tcp = (struct tcp_header*)(packet + SIZE_ETHERNET + size_ip);
    size_tcp = TH_OFF(tcp)*4;
    if (size_tcp < 20) {
        printf("   * Invalid TCP header length: %u bytes\n", size_tcp);
        return;
    }
    
    printf("   Src port: %d\n", ntohs(tcp->th_sport));
    printf("   Dst port: %d\n", ntohs(tcp->th_dport));

    payload = (u_char *)(packet + SIZE_ETHERNET + size_ip + size_tcp);

    size_payload = ntohs(ip->ip_len) - (size_ip + size_tcp);

    if (size_payload > 0) {
        printf("   Payload (%d bytes):\n", size_payload);
        print_payload(payload, size_payload);
    }
    save_payload(payload, size_payload, filename); 
    return;
}

int main(int argc, char **argv)
{
    char *dev = NULL;
    char *filename = NULL;
    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_t *handle;

    char filter_exp[] = "ip";
    struct bpf_program fp;
    bpf_u_int32 mask;
    bpf_u_int32 net;
    int num_packets = 999;

    print_app_banner();

    if (argc >= 2) {
        dev = argv[1];
        filename = argv[2];
    } else {
        /* default interface eno1 */
        dev = "eno1";
        if (dev == NULL) {
            fprintf(stderr, "Couldn't find default device: %s\n", errbuf);
            exit(EXIT_FAILURE);
        }
    }

    if (pcap_lookupnet(dev, &net, &mask, errbuf) == -1) {
        fprintf(stderr, "Couldn't get netmask for device %s: %s\n", dev, errbuf);
        net = 0;
        mask = 0;
    }

    printf("Device: %s\n", dev);
    printf("Number of packets: %d\n", num_packets);
    printf("Filter expression: %s\n", filter_exp);

    handle = pcap_open_live(dev, SNAP_LEN, 1, 1000, errbuf);
    if (handle == NULL) {
        fprintf(stderr, "Couldn't open device %s\n", dev, errbuf);
        exit(EXIT_FAILURE);
    }

    if (pcap_datalink(handle) != DLT_EN10MB) {
        fprintf(stderr, "%s is not an Ethernet\n", dev);
        exit(EXIT_FAILURE);
    }

    if (pcap_compile(handle, &fp, filter_exp, 0, net) == -1) {
        fprintf(stderr, "Couldn't parse filter %s: %s\n", filter_exp, pcap_geterr(handle));
        exit(EXIT_FAILURE);
    }

    if (pcap_setfilter(handle, &fp) == -1) {
        fprintf(stderr, "Couldn't install filter %s: %s\n", filter_exp, pcap_geterr(handle));
        exit(EXIT_FAILURE);
    }

    pcap_loop(handle, num_packets, got_packet, NULL);

    pcap_freecode(&fp);
    pcap_close(handle);

    printf("\nCapture complete.\n");
    return 0;
}
