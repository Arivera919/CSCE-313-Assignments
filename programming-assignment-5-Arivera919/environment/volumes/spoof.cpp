#include "common.h"

int main()
{
    char buffer[PACKET_LEN];
    memset(buffer, 0, PACKET_LEN);

    ipheader *ip = (ipheader *)buffer;
    udpheader *udp = (udpheader *)(buffer + sizeof(ipheader));

    // add data
    char *data = (char *)udp + sizeof(udpheader);
    int data_len = strlen(CLIENT_IP);
    strncpy(data, CLIENT_IP, data_len);

    // create udp header
    // TODO
    udp->udp_sport = htons(CLIENT_PORT);
    udp->udp_dport = htons(SERVER_PORT);
    udp->udp_ulen = htons(sizeof(struct udpheader) + data_len);
    udp->udp_sum = 0;
    // create ip header
    // TODO
    ip->iph_ver = 4;
    ip->iph_ihl = 5;
    ip->iph_ttl = 20;
    ip->iph_sourceip.s_addr = inet_addr(SPOOF_IP);
    ip->iph_destip.s_addr = inet_addr(SERVER_IP);
    ip->iph_protocol = IPPROTO_UDP;
    ip->iph_len = htons(sizeof(struct ipheader) + sizeof(struct udpheader) + data_len);
    ip->iph_chksum = 0;
    // send packet
    send_raw_ip_packet(ip);
    // TODO

    return 0;
}