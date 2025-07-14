#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/select.h>

#include "project_headers.h"

#define MAX_WINDOW_SIZE 4
#define FRAME_SIZE 100
#define TIMEOUT 2

LoggingStats stats = {0};

void print_statistics()
{
    printf("\n--- Data Link Layer Statistics ---\n");
    printf("Total Frames Sent: %d\n", stats.total_frames_sent);
    printf("Total Frames Received: %d\n", stats.total_frames_received);
    printf("Total Retransmissions: %d\n", stats.total_retransmissions);
    printf("Total ACKs Sent: %d\n", stats.total_acks_sent);
    printf("Total ACKs Received: %d\n", stats.total_acks_received);
    printf("Total Data Bytes Sent: %d\n", stats.total_data_bytes_sent);
    printf("Total Data Bytes Received: %d\n", stats.total_data_bytes_received);
    printf("Total Duplicates or Out Of Order: %d\n", stats.total_oo_dupe);
    printf("----------------------------------\n");
    fflush(stdout);
}

int DataLinkSend(int sockfd, Frame *frames, int total_frames)
{
    int base = 0;
    int next_seq_num = 0;
    Frame window[MAX_WINDOW_SIZE];
    printf("Frame info type1: %d, type: %d Count: %d\n", frames[0].type, frames[total_frames - 1].type, total_frames);
    fflush(stdout);
    while (base < total_frames)
    {
        while (next_seq_num < base + MAX_WINDOW_SIZE && next_seq_num < total_frames)
        {
            Frame frame = {0};
            frame = frames[next_seq_num];
            frame.seq_num = next_seq_num;
            if (frame.type == FILE_PUT_TYPE || frame.type == FILE_NAME_PUT_TYPE)
            {
                printf("Frame type: %d Sequence: %d\n", frame.type, frame.seq_num);
                fflush(stdout);
            }
            if (physicalSend(sockfd, &frame) < 0)
            {
                perror("Failed to send frame");
                stats.total_retransmissions++;
                fflush(stderr);
            }
            else
            {
                printf("Sent frame %d of type %d\n", frame.seq_num, frame.type);
                fflush(stdout);
            }
            stats.total_frames_sent++;
            stats.total_data_bytes_sent += frame.size;
            window[next_seq_num % MAX_WINDOW_SIZE] = frame;
            next_seq_num++;
        }

        struct timeval timeout;
        fd_set readfds;
        timeout.tv_sec = TIMEOUT;
        timeout.tv_usec = 0;

        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);

        int ret = select(sockfd + 1, &readfds, NULL, NULL, &timeout);
        if (ret > 0)
        {
            Frame ack_frame;
            if (physicalRecv(sockfd, &ack_frame) < 0)
            {
                perror("Failed to receive ACK");
                fflush(stderr);
            }
            if (ack_frame.type == ACK_TYPE && ack_frame.seq_num >= base)
            {
                printf("Received ACK for frame %d\n", ack_frame.seq_num);
                fflush(stdout);
                base = ack_frame.seq_num + 1;
            }
            stats.total_acks_received++;
        }
        else
        {
            printf("Timeout, resending frames from base %d, next: %d\n", base, next_seq_num);
            fflush(stdout);
            for (int i = base; i < next_seq_num; i++)
            {
                if (physicalSend(sockfd, &window[i % MAX_WINDOW_SIZE]) < 0)
                {
                    perror("Resend failed");
                    fflush(stderr);
                }
                else
                {
                    printf("Resent frame %d, type%d\n", window[i % MAX_WINDOW_SIZE].seq_num, window[i % MAX_WINDOW_SIZE].type);
                    fflush(stdout);
                }
                stats.total_retransmissions++;
            }
        }
    }
    return 1;
}

int DataLinkRecv(int sockfd, Frame *frames)
{
    Frame *recv_buffer = malloc(MAX_WINDOW_SIZE * sizeof(Frame));
    int *recv_status = malloc(MAX_WINDOW_SIZE * sizeof(int));
    int expected_seq_num = 0;

    while (1)
    {
        Frame *frame = malloc(sizeof(Frame));
        if (physicalRecv(sockfd, frame) < 0)
        {
            perror("Failed to receive frame");
            fflush(stderr);
        }

        if (frame->type != ECHO_TYPE && frame->type != FILE_DEL_TYPE && frame->type != FILE_GET_TYPE && frame->type != ACK_TYPE && frame->type != FILE_PUT_TYPE && frame->type != FILE_NAME_PUT_TYPE && frame->type != FILE_LIST_TYPE && frame->type != FILE_COUNT_TYPE && frame->type != COMMAND_END_TYPE && frame->type != KILL_TYPE)
        {
            printf("Unknown frame type: %d\n", frame->type);
            fflush(stdout); 
            continue;
        }
        if (frame->seq_num >= expected_seq_num && frame->seq_num < expected_seq_num + MAX_WINDOW_SIZE)
        {
            int buffer_index = frame->seq_num % MAX_WINDOW_SIZE;
            recv_buffer[buffer_index] = *frame;
            recv_status[buffer_index] = 1;

            while (recv_status[expected_seq_num % MAX_WINDOW_SIZE])
            {
                Frame current_frame = recv_buffer[expected_seq_num % MAX_WINDOW_SIZE];
                frames[expected_seq_num] = current_frame;
                recv_status[expected_seq_num % MAX_WINDOW_SIZE] = 0;
                expected_seq_num++;

                printf("Processed frame %d of type %d\n", current_frame.seq_num, current_frame.type);
                fflush(stdout);
                stats.total_frames_received++;
                stats.total_data_bytes_received += current_frame.size;

                Frame ack_frame = {0};
                ack_frame.type = ACK_TYPE;
                ack_frame.seq_num = current_frame.seq_num;

                if (physicalSend(sockfd, &ack_frame) < 0)
                {
                    perror("Failed to send ACK");
                    fflush(stderr);
                    stats.total_retransmissions++;
                }
                else
                {
                    stats.total_acks_sent++;
                    printf("Sent ACK for frame %d, type %d\n", ack_frame.seq_num, current_frame.type);
                    fflush(stdout);
                    if ((current_frame.type == FILE_NAME_PUT_TYPE || current_frame.type == COMMAND_END_TYPE || current_frame.type == KILL_TYPE) && (current_frame.seq_num == expected_seq_num - 1))
                    {
                        printf("Condition result: %d\n", (frame->type == FILE_NAME_PUT_TYPE || frame->type == COMMAND_END_TYPE || frame->type == KILL_TYPE) && (frame->seq_num == expected_seq_num - 1));
                        fflush(stdout);
                        free(recv_buffer);
                        free(recv_status);
                        free(frame);
                        return expected_seq_num;
                    }
                }
                stats.total_acks_sent++;
            }
        }
        else
        {
            printf("Out-of-order or duplicate frame %d, retransmitting ACK. type: %d, expect %d\n", frame->seq_num, frame->type, expected_seq_num);
            fflush(stdout);
            stats.total_oo_dupe++;
            Frame ack_frame = {0};
            ack_frame.type = ACK_TYPE;
            ack_frame.seq_num = frame->seq_num;
            if (physicalSend(sockfd, &ack_frame) < 0)
            {
                perror("Failed to send ACK DUPE");
                fflush(stderr);
                stats.total_retransmissions++;
            }
            else if ((frame->type == FILE_NAME_PUT_TYPE || frame->type == COMMAND_END_TYPE || frame->type == KILL_TYPE) && (frame->seq_num == expected_seq_num - 1))
            {
                printf("Condition result: %d\n", (frame->type == FILE_NAME_PUT_TYPE || frame->type == COMMAND_END_TYPE || frame->type == KILL_TYPE) && (frame->seq_num == expected_seq_num - 1));
                free(frame);
                break;
            }
        }
    }
    free(recv_buffer);
    free(recv_status);
    return expected_seq_num;
}