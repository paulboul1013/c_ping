# c_ping

## Introduction

A custom ICMP ping implementation from scratch in C, featuring:
- Full IP and ICMP packet construction
- RFC 1071 checksum calculation
- Raw socket operations
- RTT (Round Trip Time) measurement
- Statistics tracking (packet loss, min/avg/max RTT)

ICMP type 8 (echo request) <-> ICMP type 0 (echo reply)

## Build and Run

```bash
# Build
make

# Run (requires root privileges for raw socket)
sudo ./ping <target_ip>

# Examples
sudo ./ping 8.8.8.8      # Recommended: test with public IP
sudo ./ping 1.1.1.1      # Cloudflare DNS
sudo ./ping 127.0.0.1    # Loopback (may fail in WSL2)

# Stop: Press Ctrl+C
```

### WSL2 Users

If you encounter `sendto: Operation not permitted` when pinging 127.0.0.1:
- This is a known WSL2 limitation with custom IP headers on loopback
- **Solution**: Test with a public IP address instead (e.g., `sudo ./ping 8.8.8.8`)
- See CLAUDE.md for detailed explanations and alternatives

## Features

- ✅ ICMP echo request/reply implementation
- ✅ Custom IP header construction
- ✅ Internet checksum (RFC 1071)
- ✅ RTT calculation with microsecond precision
- ✅ Packet statistics (transmitted/received/loss percentage)
- ✅ Signal handling for graceful shutdown 

## record

02 Internet Protocol -Ping from scratch
1:25:30

## reference 
https://en.wikipedia.org/wiki/Internet_Control_Message_Protocol
https://www.youtube.com/watch?v=3mv7E5kJTA4&list=PLdNUbYq5poiXDcqmOAW4I-U30i9rxUIe7&index=1
https://www.youtube.com/watch?v=SO-UF8Ggw6k&t=1064s
https://www.rfc-editor.org/rfc/rfc1071
https://dingdingqiuqiu.github.io/2024/11/29/RFC1071%E7%BF%BB%E8%AF%91%E5%8F%8A%E5%85%B6%E5%BA%94%E7%94%A8/
https://en.wikipedia.org/wiki/Internet_Protocol
https://en.wikipedia.org/wiki/IPv4
man 7 raw
man sendmsg
