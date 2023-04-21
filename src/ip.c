#include "buf.h"
#include "config.h"
#include "net.h"
#include "ip.h"
#include "ethernet.h"
#include "arp.h"
#include "icmp.h"
#include "utils.h"
#include <bits/stdint-uintn.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

/**
 * @brief 处理一个收到的数据包
 * 
 * @param buf 要处理的数据包
 * @param src_mac 源mac地址
 */
void ip_in(buf_t *buf, uint8_t *src_mac)
{
    // TO-DO
    if (buf->len < sizeof(ip_hdr_t)) {
        return;
    }
    ip_hdr_t *hdr = (ip_hdr_t *)(buf->data);
    if (hdr->version != IP_VERSION_4) {
        return;
    }
    uint16_t total_len = swap16(hdr->total_len16);
    if (total_len > buf->len) {
        // TODO: not sure
        return;
    }
    // uint16_t checksum = swap16(hdr->hdr_checksum16);
    uint16_t checksum = hdr->hdr_checksum16;
    hdr->hdr_checksum16 = 0;
    if (checksum16((uint16_t*)hdr, sizeof(ip_hdr_t)) != checksum) {
        return;
    }
    hdr->hdr_checksum16 = checksum;
    uint8_t local_ip[NET_IP_LEN] = NET_IF_IP;
    if (memcmp(local_ip, hdr->dst_ip, NET_IP_LEN) != 0) {
        return;
    }
    if (buf->len > total_len) {
        buf_remove_padding(buf, buf->len - total_len);
    }
    uint8_t protocol = hdr->protocol;
    buf_remove_header(buf, sizeof(ip_hdr_t));
    
    if (net_in(buf, protocol, hdr->src_ip) != 0) {
        icmp_unreachable(buf, hdr->src_ip, ICMP_CODE_PROTOCOL_UNREACH);
    }

}

/**
 * @brief 处理一个要发送的ip分片
 * 
 * @param buf 要发送的分片
 * @param ip 目标ip地址
 * @param protocol 上层协议
 * @param id 数据包id
 * @param offset 分片offset，必须被8整除
 * @param mf 分片mf标志，是否有下一个分片
 */
void ip_fragment_out(buf_t *buf, uint8_t *ip, net_protocol_t protocol, int id, uint16_t offset, int mf)
{
    // TO-DO
    buf_add_header(buf, sizeof(ip_hdr_t));
    ip_hdr_t *hdr = (ip_hdr_t *)(buf->data);
    hdr->hdr_len = 5;
    hdr->version = IP_VERSION_4;
    hdr->tos = 0;
    hdr->total_len16 = swap16(buf->len);
    // hdr->id16 = id;
    hdr->id16 = swap16(id);
    // TODO not sure
    hdr->flags_fragment16 = swap16(mf);
    hdr->ttl = 64;
    hdr->protocol = protocol;
    hdr->hdr_checksum16 = 0;
    uint8_t src_ip[NET_IP_LEN] = NET_IF_IP;
    memcpy(hdr->src_ip, src_ip, NET_IP_LEN);
    memcpy(hdr->dst_ip, ip, NET_IP_LEN);
    uint16_t checksum = checksum16((uint16_t *)hdr, sizeof(ip_hdr_t));
    // hdr->hdr_checksum16 = swap16(checksum);
    hdr->hdr_checksum16 = checksum;
    arp_out(buf, ip);
}

/**
 * @brief 处理一个要发送的ip数据包
 * 
 * @param buf 要处理的包
 * @param ip 目标ip地址
 * @param protocol 上层协议
 */
void ip_out(buf_t *buf, uint8_t *ip, net_protocol_t protocol)
{
    // TO-DO
    static uint16_t GLOBAL_ID = 0;
    uint16_t id = GLOBAL_ID++;

    size_t max_len = ETHERNET_MAX_TRANSPORT_UNIT - sizeof(ip_hdr_t);

    if (buf->len > max_len) {
        // sharded
        size_t offset = 0;
        while (buf->len > 0) {
            buf_t ip_buf;
            size_t len = buf->len > max_len ? max_len : buf->len; 
            buf_init(&ip_buf, len);
            buf_copy(&ip_buf, buf, len);
            buf_remove_header(buf, len);
            if (buf->len > 0) {
                ip_fragment_out(&ip_buf, ip, protocol, id, offset, 1);
            } else {
                ip_fragment_out(&ip_buf, ip, protocol, id, offset, 0);
            }
            offset += len;
        }

    } else {
        // no need to shard
        ip_fragment_out(buf, ip, protocol, id, 0, 0);
    }
}

/**
 * @brief 初始化ip协议
 * 
 */
void ip_init()
{
    net_add_protocol(NET_PROTOCOL_IP, ip_in);
}