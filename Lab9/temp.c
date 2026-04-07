/* Reliable UDP Server - 
  Team: Dev (AU2340222) & Parin (AU2340243)*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>

#define PORT 5432
#define PACKET_SIZE 8000
#define WINDOW_SIZE 5

int main() {

    int sock;
    
    // Server and Client Address:
    struct sockaddr_in server, client;
    socklen_t client_address_len = sizeof(client);

    uint8_t buffer[65536]; // 2 ^ 16 (just to store packet and file information)
    uint8_t data_buf[PACKET_SIZE]; // data buffer to store data
    FILE *fp;

    // Create socket:
    sock = socket(AF_INET, SOCK_DGRAM, 0);

    memset(&server, 0, sizeof(server));

    server.sin_family = AF_INET; // IPv4
    server.sin_addr.s_addr = INADDR_ANY; // request from any ip
    server.sin_port = htons(PORT);

    bind(sock, (struct sockaddr *)&server, sizeof(server));

    printf("Server listening...\n");

    // Receive file request
    int n = recvfrom(sock, buffer, sizeof(buffer), 0,
                     (struct sockaddr *)&client, &client_address_len);

    if (buffer[0] != 0) {
        printf("Invalid request\n");
        return 0;
    }

    uint8_t fname_len = buffer[1];
    char filename[256];
    memcpy(filename, &buffer[2], fname_len);
    filename[fname_len] = '\0';

    printf("Client requested: %s\n", filename);

    fp = fopen(filename, "rb");

    if (!fp) {
        buffer[0] = 4;
        buffer[1] = fname_len;
        memcpy(&buffer[2], filename, fname_len);
        printf("Packet of type: %d received\n", buffer[0]);
        printf("Client requested file which does not exist\n");
        sendto(sock, buffer, 2 + fname_len, 0, (struct sockaddr *)&client, client_address_len);
        return 0;
    }
    
    // to calculate size of file in bytes
    fseek(fp, 0, SEEK_END); // point to end
    uint32_t file_size = ftell(fp); // current location of fp (EOF)
    rewind(fp); // reset fp to top

    struct timeval tv = {0, 500}; // 1 sec, 500 u_sec
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  
    // Structure of sliding window:
    int base = 0, next_seq = 0;
    uint8_t window[WINDOW_SIZE][PACKET_SIZE + 50];
    int window_arr[WINDOW_SIZE];

    while (1) {

        while (next_seq < base + WINDOW_SIZE) { // packet in order
            // PACKET_SIZE -> fixed, data_len -> how much to send
            int data_len = fread(data_buf, 1, PACKET_SIZE, fp);
            if (data_len <= 0) break;

            int idx = 0;

            if (next_seq == 0) {
                buffer[idx++] = 2; // type 2 packet
                *(uint16_t *)&buffer[idx] = htons(next_seq);
                idx += 2; // 2 bytes added

                buffer[idx++] = fname_len;
                memcpy(&buffer[idx], filename, fname_len);
                idx += fname_len;

                *(uint32_t *)&buffer[idx] = htonl(file_size);
                idx += 4; // 4 bytes added
            } else {
                buffer[idx++] = 3;
                *(uint16_t *)&buffer[idx] = htons(next_seq);
                idx += 2; // 2 bytes added
            }

            *(uint16_t *)&buffer[idx] = htons(data_len);
            idx += 2; // 2 bytes added

            memcpy(&buffer[idx], data_buf, data_len);
            idx += data_len; // data_len bytes added

            memcpy(window[next_seq % WINDOW_SIZE], buffer, idx);
            window_arr[next_seq % WINDOW_SIZE] = idx;

            sendto(sock, buffer, idx, 0,
                   (struct sockaddr *)&client, client_address_len);

            printf("Sent packet %d\n", next_seq);

            next_seq++;
        }

        n = recvfrom(sock, buffer, sizeof(buffer), 0,
                     (struct sockaddr *)&client, &client_address_len);

        if (n < 0) {
            printf("Timeout → Retransmitting\n");
            for (int i = base; i < next_seq; i++) {
                sendto(sock,
                       window[i % WINDOW_SIZE],
                       window_arr[i % WINDOW_SIZE],
                       0,
                       (struct sockaddr *)&client,
                       client_address_len);
            }
            continue;
        }

        if (buffer[0] == 1) {
            uint16_t ack = ntohs(*(uint16_t *)&buffer[2]);
            printf("ACK %d\n", ack);
            base = ack + 1;
        }

        if (feof(fp)) {
            if (base == next_seq) {
                printf("Last packet %d Acknowledged\n", next_seq-1);
                break;
            } else {
                continue;
            }
        }
    }
    
    buffer[0] = 5;  // type = FIN

    sendto(sock, buffer, 1, 0, (struct sockaddr *)&client, client_address_len);
    
    /* for (int i = 0; i < 5; i++) {
        sendto(sock, buffer, 1, 0, (struct sockaddr *)&client, client_address_len);
        usleep(100000); // 100ms
    } */
    
    int last_packet = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&client, &client_address_len);
    
    if (buffer[0] == 6) {
        printf("FIN acknowledged\n");
    }

    printf("Transfer complete\n");

    fclose(fp);
    close(sock);
    return 0;
}
