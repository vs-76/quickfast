#!/usr/bin/env python3
"""Generate the capture-file corpus used by testPCapReader.cpp.

Each capture holds one UDP datagram carrying PAYLOAD over Ethernet II / IPv4,
except where the file name says otherwise. Run from this directory:

    python3 make_captures.py

The outputs are committed, so this only needs re-running when a case changes.
"""

import struct
import sys

PAYLOAD = b"QUICKFAST-PCAP-CORPUS"

LINKTYPE_ETHERNET = 1

MAGIC_MICROSECOND = 0xA1B2C3D4
MAGIC_NANOSECOND = 0xA1B23C4D
PCAPNG_BLOCK_SHB = 0x0A0D0D0A
PCAPNG_BYTE_ORDER_MAGIC = 0x1A2B3C4D


def udp_over_ethernet(payload, sport=12345, dport=13014, udp_length=None, ihl=5):
    """Build an Ethernet II / IPv4 / UDP frame.

    udp_length and ihl are overridable so the corpus can carry the wire values
    a hostile capture would use: the reader derives buffer bounds from both.
    """
    if udp_length is None:
        udp_length = 8 + len(payload)
    udp = struct.pack("!HHHH", sport, dport, udp_length, 0) + payload

    total_length = 20 + len(udp)
    ip = struct.pack(
        "!BBHHHBBH4s4s",
        0x40 | (ihl & 0xF),  # version 4, header length in 4 byte words
        0,              # type of service
        total_length,
        0,              # identification
        0,              # flags and fragment offset
        64,             # time to live
        17,             # protocol: UDP
        0,              # header checksum, unverified by the reader
        bytes([10, 0, 0, 1]),
        bytes([224, 1, 2, 133]),
    )

    ethernet = (
        bytes([0x01, 0x00, 0x5E, 0x01, 0x02, 0x85])
        + bytes([0x02, 0x00, 0x00, 0x00, 0x00, 0x01])
        + struct.pack("!H", 0x0800)
    )

    return ethernet + ip + udp


def classic_pcap(frame, endian="<", magic=MAGIC_MICROSECOND, caplen=None):
    """Classic pcap: 24 byte file header, then 16 byte record headers."""
    header = struct.pack(
        endian + "IHHiIII",
        magic,
        2, 4,           # version
        0,              # thiszone
        0,              # sigfigs
        65535,          # snaplen
        LINKTYPE_ETHERNET,
    )
    stored = frame if caplen is None else frame[:caplen]
    record = struct.pack(
        endian + "IIII",
        1_700_000_000,  # seconds
        123_456,        # microseconds or nanoseconds, per the magic
        len(stored),
        len(frame),
    )
    return header + record + stored


def pcapng(frame):
    """pcapng: Section Header Block, Interface Description Block, Enhanced Packet Block."""

    def block(block_type, body):
        # total length counts type, both length fields, and the padded body
        total = 12 + len(body)
        return struct.pack("<II", block_type, total) + body + struct.pack("<I", total)

    shb = block(
        PCAPNG_BLOCK_SHB,
        struct.pack("<IHHq", PCAPNG_BYTE_ORDER_MAGIC, 1, 0, -1),
    )
    idb = block(0x00000001, struct.pack("<HHI", LINKTYPE_ETHERNET, 0, 65535))

    padded = frame + b"\x00" * (-len(frame) % 4)
    timestamp = 1_700_000_000 * 1_000_000 + 123_456
    epb = block(
        0x00000006,
        struct.pack(
            "<IIIII",
            0,                          # interface id
            timestamp >> 32,
            timestamp & 0xFFFFFFFF,
            len(frame),                 # captured length
            len(frame),                 # original length
        )
        + padded,
    )
    return shb + idb + epb


def main():
    frame = udp_over_ethernet(PAYLOAD)
    truncated_frame = udp_over_ethernet(PAYLOAD)

    captures = {
        # The four formats a user can plausibly arrive with.
        "classic-le.pcap": classic_pcap(frame, endian="<"),
        "classic-be.pcap": classic_pcap(frame, endian=">"),
        "nanosecond.pcap": classic_pcap(frame, magic=MAGIC_NANOSECOND),
        "modern.pcapng": pcapng(frame),
        # caplen shorter than the 14 byte Ethernet header: the dissection layer
        # subtracts that header size from an unsigned length.
        "short-link-header.pcap": classic_pcap(truncated_frame, caplen=6),
        # UDP length field of zero: the reader returns udplen - 8 as the cargo
        # size, so an unsigned size_t wraps to nearly 2^64.
        "udp-length-zero.pcap": classic_pcap(udp_over_ethernet(PAYLOAD, udp_length=0)),
        # UDP length field larger than the bytes actually captured.
        "udp-length-overlong.pcap": classic_pcap(udp_over_ethernet(PAYLOAD, udp_length=60000)),
        # IPv4 header length field of 15 words (60 bytes) on a frame that only
        # carries a 20 byte header, so it eats past the UDP header.
        "ip-header-overlong.pcap": classic_pcap(udp_over_ethernet(PAYLOAD, ihl=15)),
        # IPv4 header length field below the 20 byte minimum.
        "ip-header-undersized.pcap": classic_pcap(udp_over_ethernet(PAYLOAD, ihl=0)),
        # A record header claiming far more captured bytes than the file holds.
        "caplen-beyond-file.pcap": classic_pcap(frame)[:24]
        + struct.pack("<IIII", 1_700_000_000, 0, 0xFFFFFF00, 0xFFFFFF00)
        + frame,
        # A record header that claims more data than the file holds.
        "truncated-record.pcap": classic_pcap(frame)[: 24 + 16 + 10],
        # No records at all, just a valid file header.
        "empty.pcap": classic_pcap(frame)[:24],
        # Three packets, so a consumer can be tested over more than one.
        "three-packets.pcap": classic_pcap(frame)
        + classic_pcap(frame)[24:]
        + classic_pcap(frame)[24:],
        # Nothing resembling a capture.
        "not-a-capture.bin": b"this is not a capture file, not even close\n",
    }

    for name, data in captures.items():
        with open(name, "wb") as out:
            out.write(data)
        print(f"{name}: {len(data)} bytes", file=sys.stderr)


if __name__ == "__main__":
    main()
