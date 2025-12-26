# High-Performance Containerized VPN

A custom **Layer 3 VPN tunnel** built from scratch using **C**, **Raw Sockets**, and **Linux TUN/TAP interfaces**.

This project demonstrates how to build a VPN protocol without using existing libraries (like OpenVPN or WireGuard). It features a **Host-Container architecture** where the Client is isolated inside a Docker container to simulate a real-world remote connection and prevent routing loops.

---

## 🏗 Architecture

The system splits the VPN connection into two distinct network namespaces:

1.  **The Server (Host OS):**
    * Creates virtual interface `tun1`.
    * Listens on UDP Port `5555`.
    * Decapsulates packets and routes them to the Kernel.
    * **Virtual IP:** `10.99.99.1`

2.  **The Client (Docker Container):**
    * Isolated environment simulating a remote user.
    * Creates virtual interface `tun0`.
    * Captures OS traffic, encapsulates it in UDP, and tunnels it to the host.
    * **Virtual IP:** `10.99.99.2`

---

## 📂 Project Structure

```text
customvpn/
├── server.c           # The VPN Server (C code, runs on Host)
└── vpn-client/        # Client Subfolder
    ├── client.c       # The VPN Client (C code, runs in Docker)
    └── Dockerfile     # Configuration to build the isolated client environment

🚀 Prerequisites

    Linux OS (Kali, Ubuntu, or Debian)

    Docker (sudo apt install docker.io)

    GCC Compiler (sudo apt install gcc)

🛠 Installation & Usage
Step 1: Start the Server (Host)

The server runs on your main machine (the Host). It acts as the gateway.

    Navigate to the project root:
    Bash

cd ~/projects/customvpn

Compile the Server:
Bash

gcc server.c -o vpnserver

Run the Server (Requires Root for TUN creation):
Bash

    sudo ./vpnserver

    You will see: [*] VPN Server Started. Listening on 0.0.0.0:5555

Step 2: Build & Run the Client (Docker)

The client runs inside a Docker container to ensure it has its own routing table.

    Open a new terminal and navigate to the client folder:
    Bash

cd ~/projects/customvpn/vpn-client

Build the Docker Image:
Bash

sudo docker build -t c-vpn-client .

Start the Container (Background Mode):
Bash

sudo docker run -d \
  --name my-vpn \
  --cap-add=NET_ADMIN \
  --device=/dev/net/tun \
  c-vpn-client

Execute the Client Program:
Bash

    sudo docker exec -it my-vpn ./vpnclient

    You will see: [*] VPN Client Started. Connecting to 172.17.0.1:5555

✅ Verification

To prove the tunnel is working, we will ping the Host machine through the tunnel from inside the container.

Open a third terminal and run:
Bash

sudo docker exec -it my-vpn ping 10.99.99.1 -c 4

Expected Output:
Plaintext

64 bytes from 10.99.99.1: icmp_seq=1 ttl=64 time=0.45 ms
64 bytes from 10.99.99.1: icmp_seq=2 ttl=64 time=0.38 ms
0% packet loss

Advanced Debugging

You can watch the raw encrypted packets traveling over the Docker bridge:
Bash

# Watch the "Secret Tunnel" traffic
sudo tcpdump -i docker0 udp port 5555 -n

🧠 Technical Details

    TUN Interface: Used to capture Layer 3 (IP) packets. The code reads from /dev/net/tun.

    Encapsulation: The raw IP packet (e.g., ICMP Ping) is read from the kernel and wrapped inside a standard UDP packet payload.

    Performance: Written in C for maximum throughput and minimal latency.

    Routing: The project manually configures point-to-point routing using ip addr add ... peer ... commands invoked via system calls.

🔮 Future Improvements

    Encryption: Implement XOR or AES encryption to secure the UDP payload.

    Masquerading: Enable NAT on the Server to allow the Client to browse the real Internet.

    Multi-Client Support: Update the Server to handle multiple concurrent connections.

⚠️ Disclaimer

This project is for educational purposes only. It sends data in cleartext (unencrypted) by default. Do not use this for sensitive data without adding an encryption layer.
