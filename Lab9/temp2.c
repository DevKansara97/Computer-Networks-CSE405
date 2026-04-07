/* Reliable UDP Client
  Team: Dev (AU2340222) & Parin (AU2340243) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 5432
// #define PACKET_SIZE 4000

int main(int argc, char *argv[]) {
    
    // how to use message:
    if (argc < 3) {
        printf("Usage: %s <server_ip> <filename>\n", argv[0]);
        return 0;
    }
    
    // argv[0] : lab9_client
    // argv[1] : 127.0.0.1
    // argv[2] : output.ts

    int sock;
    int out_of_order_packets = 0;
    
    // Server address:
    struct sockaddr_in server;
    socklen_t server_address_len = sizeof(server);

    uint8_t buffer[65536];
    
    sock = socket(AF_INET, SOCK_DGRAM, 0); // IPv4, UDP, default protocol

    memset(&server, 0, sizeof(server));
    
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    
    // 190.168.... -> 1001110...
    inet_pton(AF_INET, argv[1], &server.sin_addr);

    // File_request:
    uint8_t fname_len = strlen(argv[2]);
    // Send: GET 
    buffer[0] = 0;
    buffer[1] = fname_len;
    memcpy(&buffer[2], argv[2], fname_len);

    sendto(sock, buffer, 2 + fname_len, 0, (struct sockaddr *)&server, server_address_len);

    printf("Requested file: %s\n", argv[2]);

    FILE *fp = fopen("received.ts", "wb"); // write+binary mode

    int expected_seq = 0;

    while (1) {

        int n = recvfrom(sock, buffer, sizeof(buffer), 0,
                         (struct sockaddr *)&server, &server_address_len);
        
        // FIN packet:
        if (buffer[0] == 5) {
            printf("Received FIN\n");

            buffer[0] = 6; // FIN-ACK
            sendto(sock, buffer, 1, 0, (struct sockaddr *)&server, server_address_len);

            break;
        }
  
        // File not Found Packet:
        if (buffer[0] == 4) {
            int packet_type = buffer[0];
            printf("Packet of type: %d received\n", buffer[0]);
            printf("File not found on server\n");
            break;
        }

        int idx = 0;
        uint8_t type = buffer[idx++];

        uint16_t seq = ntohs(*(uint16_t *)&buffer[idx]);
        idx += 2; // 2 bytes added

        // First Packet:
        if (type == 2) {
            uint8_t fname_len = buffer[idx++];
            idx += fname_len;
            idx += 4; // 4 bytes added
        }
        
        // Normal Packets:
        uint16_t data_len = ntohs(*(uint16_t *)&buffer[idx]);
        idx += 2; // 2 bytes added

        printf("Received packet %d\n", seq);

        if (seq == expected_seq) {
            fwrite(&buffer[idx], 1, data_len, fp);

            buffer[0] = 1;
            buffer[1] = 1;
            *(uint16_t *)&buffer[2] = htons(seq);

            sendto(sock, buffer, 4, 0,
                   (struct sockaddr *)&server, server_address_len);

            printf("Sent ACK %d\n", seq);

            expected_seq++;
        } else {
            out_of_order_packets++;
            buffer[0] = 1;
            buffer[1] = 1;
            // send ack of expected sequence - 1:
            *(uint16_t *)&buffer[2] = htons(expected_seq - 1);

            sendto(sock, buffer, 4, 0,
                   (struct sockaddr *)&server, server_address_len);
                   
            printf("Sent ACK %d\n", expected_seq-1);
        }
    }

    fclose(fp);
    close(sock);

    printf("Download complete\n");
    
    printf("\n\n");
    printf("Total packets out of order packets: %d\n", out_of_order_packets);
    
    return 0;
}
