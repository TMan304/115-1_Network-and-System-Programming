#!/usr/bin/env python3
"""
generate_sample_pcap.py

Generates a valid standard libpcap (.pcap) file containing TCP/IP loopback packets
exchanging NSP protocol requests and responses (echo, time, sysinfo, quit).
Used to ensure demonstration, testing, and Wireshark / tcpdump analysis work
deterministically even when raw packet capture permissions (sudo) are restricted.
"""

import sys
import struct
import time

# Protocol and TCP Constants
NSP_MAGIC = 0x4E53
NSP_VERSION = 0x01

NSP_MSG_REQ_SYSINFO = 0x01
NSP_MSG_REQ_ECHO = 0x02
NSP_MSG_REQ_TIME = 0x03
NSP_MSG_REQ_QUIT = 0x06

NSP_MSG_RESP_OK = 0x80
NSP_MSG_RESP_BYE = 0x83

TCP_FLAG_SYN = 0x02
TCP_FLAG_SYN_ACK = 0x12
TCP_FLAG_ACK = 0x10
TCP_FLAG_PSH_ACK = 0x18
TCP_FLAG_FIN_ACK = 0x11

def make_eth_ip_tcp(src_ip, dst_ip, src_port, dst_port, payload, seq, ack, flags=TCP_FLAG_PSH_ACK):
    # Ethernet II: dst_mac (6), src_mac (6), ethertype=0x0800 (IPv4)
    eth = b'\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x02\x08\x00'
    
    # IPv4 header: ver/ihl=0x45, tos=0, total_len, id=1, flags/frag=0x4000, ttl=64, proto=6 (TCP), csum=0, src, dst
    total_len = 20 + 20 + len(payload)
    src_bytes = bytes(map(int, src_ip.split('.')))
    dst_bytes = bytes(map(int, dst_ip.split('.')))
    ip_hdr = struct.pack('!BBHHHBBH4s4s', 0x45, 0, total_len, 1, 0x4000, 64, 6, 0, src_bytes, dst_bytes)
    
    # TCP header: src_port, dst_port, seq, ack, data_offset=5 (20 bytes), flags, win=65535, csum=0, urp=0
    tcp_hdr = struct.pack('!HHIIBBHHH', src_port, dst_port, seq, ack, 0x50, flags, 65535, 0, 0)
    
    return eth + ip_hdr + tcp_hdr + payload

def make_nsp_frame(msg_type, payload_str):
    payload_bytes = payload_str.encode('utf-8')
    # Header: Magic 0x4E53 ('NS'), Version 1, MsgType, Length (uint32 big-endian)
    hdr = struct.pack('!HBB I', NSP_MAGIC, NSP_VERSION, msg_type, len(payload_bytes))
    return hdr + payload_bytes

def write_pcap(filepath, port=9000):
    # Global pcap header (24 bytes): magic=0xa1b2c3d4, ver=2.4, thiszone=0, sigfigs=0, snaplen=65535, network=1 (LINKTYPE_ETHERNET)
    global_hdr = struct.pack('<IHHIIII', 0xa1b2c3d4, 2, 4, 0, 0, 65535, 1)
    
    client_ip = "127.0.0.1"
    server_ip = "127.0.0.1"
    client_port = 54321
    server_port = int(port)
    
    now = time.time()
    packets = []
    
    client_seq = 1000
    server_seq = 5000
    
    # 1. TCP 3-way handshake
    # SYN (client -> server)
    p = make_eth_ip_tcp(client_ip, server_ip, client_port, server_port, b'', client_seq, 0, flags=TCP_FLAG_SYN)
    packets.append(p)
    client_seq += 1
    
    # SYN-ACK (server -> client)
    p = make_eth_ip_tcp(server_ip, client_ip, server_port, client_port, b'', server_seq, client_seq, flags=TCP_FLAG_SYN_ACK)
    packets.append(p)
    server_seq += 1
    
    # ACK (client -> server)
    p = make_eth_ip_tcp(client_ip, server_ip, client_port, server_port, b'', client_seq, server_seq, flags=TCP_FLAG_ACK)
    packets.append(p)
    
    # Session Request/Response exchanges: (req_type, req_payload, resp_type, resp_payload)
    exchanges = [
        (NSP_MSG_REQ_ECHO, "Hello NSP!", NSP_MSG_RESP_OK, "Hello NSP!"),
        (NSP_MSG_REQ_TIME, "", NSP_MSG_RESP_OK, "Epoch: 1762858000\nLocal: 2026-11-11 18:46:40 CST\nUTC:   2026-11-11 10:46:40 UTC"),
        (NSP_MSG_REQ_SYSINFO, "", NSP_MSG_RESP_OK, "Hostname: ubuntu\nOS: Linux 6.8.0-generic\nUptime: 3600 seconds\nLoad Average: 0.15, 0.20, 0.18"),
        (NSP_MSG_REQ_QUIT, "", NSP_MSG_RESP_BYE, "Goodbye! Session closed.")
    ]
    
    for req_type, req_payload, resp_type, resp_payload in exchanges:
        # Client request frame
        req_frame = make_nsp_frame(req_type, req_payload)
        p = make_eth_ip_tcp(client_ip, server_ip, client_port, server_port, req_frame, client_seq, server_seq, flags=TCP_FLAG_PSH_ACK)
        packets.append(p)
        client_seq += len(req_frame)
        
        # Server response frame
        resp_frame = make_nsp_frame(resp_type, resp_payload)
        p = make_eth_ip_tcp(server_ip, client_ip, server_port, client_port, resp_frame, server_seq, client_seq, flags=TCP_FLAG_PSH_ACK)
        packets.append(p)
        server_seq += len(resp_frame)
    
    # TCP Connection teardown (FIN-ACK)
    p = make_eth_ip_tcp(client_ip, server_ip, client_port, server_port, b'', client_seq, server_seq, flags=TCP_FLAG_FIN_ACK)
    packets.append(p)
    client_seq += 1
    
    p = make_eth_ip_tcp(server_ip, client_ip, server_port, client_port, b'', server_seq, client_seq, flags=TCP_FLAG_FIN_ACK)
    packets.append(p)
    server_seq += 1
    
    with open(filepath, 'wb') as f:
        f.write(global_hdr)
        for i, packet_bytes in enumerate(packets):
            timestamp_sec = now + (i * 0.05)
            sec = int(timestamp_sec)
            usec = int((timestamp_sec % 1) * 1e6)
            pkt_hdr = struct.pack('<IIII', sec, usec, len(packet_bytes), len(packet_bytes))
            f.write(pkt_hdr + packet_bytes)
            
    print(f"Generated sample PCAP trace with {len(packets)} packets at: {filepath}")

if __name__ == '__main__':
    out_file = sys.argv[1] if len(sys.argv) > 1 else "nsp_sample_capture.pcap"
    port_arg = int(sys.argv[2]) if len(sys.argv) > 2 else 9000
    write_pcap(out_file, port_arg)
