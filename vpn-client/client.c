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
#define SERVER_IP "172.17.0.1"  // The Docker Host IP (Kali)
#define SERVER_PORT 5555
#define BUFFER_SIZE 2048

// Helper function to create the TUN interface
int tun_alloc(char *dev) {
    struct ifreq ifr;
    int fd, err;

    if ((fd = open("/dev/net/tun", O_RDWR)) < 0) {
        perror("Opening /dev/net/tun");
        return fd;
    }

    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI; // Tun device, No Packet Info

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
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    char tun_name[IFNAMSIZ];
    fd_set readset;
    
    strcpy(tun_name, "tun0");

    // 1. Create TUN Interface
    tun_fd = tun_alloc(tun_name);
    if (tun_fd < 0) {
        fprintf(stderr, "Error creating TUN interface\n");
        exit(1);
    }
    printf("[*] Interface %s created successfully.\n", tun_name);

    // 2. Configure IP and Route (Using system calls for simplicity)
    // Client IP: 10.99.99.2, Peer: 10.99.99.1
    system("ip link set tun0 up");
    system("ip addr add 10.99.99.2 peer 10.99.99.1 dev tun0");
    printf("[*] Network configured: 10.99.99.2 --> 10.99.99.1\n");

    // 3. Create UDP Socket
    if ((sock_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        exit(1);
    }

    printf("[*] VPN Client Started. Connecting to %s:%d\n", SERVER_IP, SERVER_PORT);

    // 4. Main Loop (Reading/Writing packets)
    while (1) {
        FD_ZERO(&readset);
        FD_SET(tun_fd, &readset);
        FD_SET(sock_fd, &readset);

        int max_fd = (tun_fd > sock_fd) ? tun_fd : sock_fd;

        // Wait for data on either TUN or Socket
        if (select(max_fd + 1, &readset, NULL, NULL, NULL) < 0) {
            perror("select()");
            break;
        }

        // A. Packet from Kernel (TUN) -> Encrypt -> Send to Server
        if (FD_ISSET(tun_fd, &readset)) {
            int nread = read(tun_fd, buffer, BUFFER_SIZE);
            if (nread < 0) {
                perror("Reading from TUN");
                break;
            }
            // (Encryption would happen here)
            sendto(sock_fd, buffer, nread, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
        }

        // B. Packet from Server (UDP) -> Decrypt -> Write to Kernel
        if (FD_ISSET(sock_fd, &readset)) {
            int nread = recvfrom(sock_fd, buffer, BUFFER_SIZE, 0, NULL, NULL);
            if (nread < 0) {
                perror("Reading from Socket");
                break;
            }
            // (Decryption would happen here)
            write(tun_fd, buffer, nread);
        }
    }

    close(tun_fd);
    close(sock_fd);
    return 0;
}