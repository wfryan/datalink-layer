#ifndef PROJECT_HEADERS_H
#define PROJECT_HEADERS_H

#define MAX_FRAME_SIZE 100  
#define PORT 8080 
#define ECHO_TYPE 1
#define FILE_PUT_TYPE 2
#define FILE_GET_TYPE 3
#define FILE_LIST_TYPE 4
#define FILE_DEL_TYPE 5
#define FILE_NAME_PUT_TYPE 6
#define ACK_TYPE 7  
#define FILE_COUNT_TYPE 9
#define KILL_TYPE -1

#define COMMAND_END_TYPE 0
#define DEFAULT_ERROR_RATE 0.15
#define DEFAULT_SERVER_IP "127.0.0.1"
#define MAX_QUEUE_SIZE 32     

typedef struct {
    int type;             
    char payload[MAX_FRAME_SIZE];  
    int size;
    int seq_num;         
} Frame;

typedef struct {
    int total_frames_sent;
    int total_frames_received;
    int total_retransmissions;
    int total_acks_sent;
    int total_acks_received;
    int total_duplicates_received;
    int total_data_bytes_sent;
    int total_data_bytes_received;
    int total_oo_dupe;
} LoggingStats;

int DataLinkSend(int sockfd, Frame *frame, int total_frames);
int DataLinkRecv(int sockfd, Frame *frame);
int physicalSend(int sock, Frame *frame);
int physicalRecv(int sock, Frame *frame);
int connect_to_server(const char *server_ip, double errRate);
int setup_server(int *serverfd);
void print_statistics();

#endif
