#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <fcntl.h>
#include <linux/if.h>
#include <linux/if_tun.h>

// CONFIGURATION
#define PORT 5555
#define BUFFER_SIZE 2048

// Helper: Create TUN interface
int tun_alloc(char *dev) {
    struct ifreq ifr;
    int fd, err;

    if ((fd = open("/dev/net/tun", O_RDWR)) < 0) {
        perror("Opening /dev/net/tun");
        return fd;
    }

    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI; 

    if (*dev) {
        strncpy(ifr.ifr_name, dev, IFNAMSIZ);
    }

    if ((err = ioctl(fd, TUNSETIFF, (void *) &ifr)) < 0) {
        perror("ioctl(TUNSETIFF)");
        close(fd);
        return err;
    }
    strcpy(dev, ifr.ifr_name);
    return fd;
}

int main() {
    int tun_fd, sock_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];
    char tun_name[IFNAMSIZ];
    fd_set readset;

    strcpy(tun_name, "tun1"); // Server uses tun1

    // 1. Create TUN Interface
    tun_fd = tun_alloc(tun_name);
    if (tun_fd < 0) {
        fprintf(stderr, "Error creating TUN interface\n");
        exit(1);
    }
    printf("[*] Interface %s created successfully.\n", tun_name);

    // 2. Configure IP (Server: 10.99.99.1 <-> Peer: 10.99.99.2)
    system("ip link set tun1 up");
    system("ip addr add 10.99.99.1 peer 10.99.99.2 dev tun1");
    // Important: Disable rp_filter so the kernel accepts the packets
    system("sysctl -w net.ipv4.conf.tun1.rp_filter=0");
    system("sysctl -w net.ipv4.conf.all.rp_filter=0");
    printf("[*] Network configured: 10.99.99.1 --> 10.99.99.2\n");

    // 3. Create UDP Socket
    if ((sock_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; // Listen on 0.0.0.0 (All interfaces)
    server_addr.sin_port = htons(PORT);

    if (bind(sock_fd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(1);
    }

    printf("[*] VPN Server Started. Listening on 0.0.0.0:%d\n", PORT);

    // 4. Main Loop
    while (1) {
        FD_ZERO(&readset);
        FD_SET(tun_fd, &readset);
        FD_SET(sock_fd, &readset);

        int max_fd = (tun_fd > sock_fd) ? tun_fd : sock_fd;

        if (select(max_fd + 1, &readset, NULL, NULL, NULL) < 0) {
            perror("select()");
            break;
        }

        // A. Packet from UDP (The Client) -> Write to TUN
        if (FD_ISSET(sock_fd, &readset)) {
            int nread = recvfrom(sock_fd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &client_len);
            if (nread < 0) {
                perror("Recvfrom failed");
                break;
            }
            // Decrypt here
            write(tun_fd, buffer, nread);
        }

        // B. Packet from Kernel (TUN) -> Send to Client (UDP)
        if (FD_ISSET(tun_fd, &readset)) {
            int nread = read(tun_fd, buffer, BUFFER_SIZE);
            if (nread < 0) {
                perror("Reading from TUN");
                break;
            }
            // Encrypt here
            // Note: We send back to whoever sent us the last packet (client_addr)
            sendto(sock_fd, buffer, nread, 0, (struct sockaddr *)&client_addr, client_len);
        }
    }

    close(tun_fd);
    close(sock_fd);
    return 0;
}