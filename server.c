#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include "project_headers.h"

#define MAX_MESSAGE_SIZE 1024

void listFiles(int sock){
    struct dirent *entry;
    DIR *dp;
    int count = 0;

    dp = opendir(".");
    if (dp == NULL) {
        perror("opendir");
        return;
    }

        while ((entry = readdir(dp))) {
        if (entry->d_name[0] != '.') { 
            count++;
        }
    }
    closedir(dp);
    count++;
    Frame *frames = malloc(count * sizeof(Frame));
    Frame testFrame = {
        .type = FILE_COUNT_TYPE,
        .size = count,
        .seq_num = 0,
    };
    frames[0]= testFrame;

    dp = opendir(".");
    if (dp == NULL) {
        perror("opendir");
        return;
    }
    printf("Files in the current directory:\n");
    int frameCount = 0;
    while ((entry = readdir(dp))) {
        if (entry->d_name[0] != '.') { 
            Frame tmpFrame = {
                .type = FILE_LIST_TYPE,
                .seq_num = frameCount
            };
            strncpy(tmpFrame.payload, entry->d_name, MAX_FRAME_SIZE - 1);
            tmpFrame.size = strlen(tmpFrame.payload);
            frames[frameCount]=tmpFrame;
            printf("Frame %d: %s (Size: %d)\n", frames[frameCount].seq_num, frames[frameCount].payload, frames[frameCount].size);
            frameCount++;
        }
    }

    closedir(dp);
    frames[count - 1].type = COMMAND_END_TYPE;
    frames[count - 1].size = 0;
    frames[count - 1].seq_num = count - 1;

    if (DataLinkSend(sock, frames, count) < 0) {
        perror("Failed to send Directory List response");
        free(frames);
        return;
    }
    printf("Sent Directory List! Files: %d\n", frameCount);
    free(frames);
    return;
}

void handleFileDelete(int sock, Frame *frame){
    char *filename = malloc(100);
    if(frame->type == FILE_DEL_TYPE){
        strncpy(filename, frame->payload, frame->size);
        printf("Received fileName: %s\n", filename);
    }
    if (remove(filename) != 0) {
        perror("Error deleting file");
        return;
        
    }
    char message[100];
    snprintf(message, sizeof(message),"File '%s' deleted successfully.\n", filename);
    Frame deleteResponse = { .type = FILE_DEL_TYPE, .size = sizeof(message), .seq_num = frame->seq_num };
    Frame endFrame = {.type = COMMAND_END_TYPE, .size = 0, .seq_num = 1};
    strncpy(deleteResponse.payload, message, MAX_FRAME_SIZE);
    Frame *sendFrames = malloc(2 * sizeof(Frame));
    sendFrames[0] = deleteResponse;
    sendFrames[1] = endFrame;

    if (DataLinkSend(sock, sendFrames, 2) < 0) {
        perror("Failed to send DELETE response");
    }

    free(filename);
    free(sendFrames);

}

void receiveFile(int sock, Frame *incFrame, int count) {
    char *filename = malloc(100);
    if(incFrame->type == FILE_NAME_PUT_TYPE){
        strncpy(filename, incFrame->payload, incFrame->size);
        printf("Received fileName: %s, count: %d\n", filename, count);
    }
    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("Error creating file");
        free(filename);
        return;
    }

    count = DataLinkRecv(sock, incFrame);
    if ( count> 0 ) {
        printf("Receiving file: %s\n", filename);
    } else {
        perror("Error receiving file\n");
        free(filename);
        return;
    }
    for(int i = 0; i < count; i++){
        Frame frame = incFrame[i];
        if(frame.type == FILE_PUT_TYPE){
            fwrite(frame.payload, 1, frame.size, file);
        }
    }

    fclose(file);
    free(filename);
}


void handleGetFile(int sockfd, Frame *frame) {
    char *filename = malloc(100);
    if (frame->type != FILE_GET_TYPE) {
        fprintf(stderr, "Unexpected frame type: %d. Expected FILE_GET_TYPE.\n", frame->type);
        free(filename);
        return;
    }
    strncpy(filename, frame->payload, frame->size);
    printf("Filename: %s\n", filename);
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("Error opening file");
        free(filename);
        return;
    }

    Frame name_frame = {0};
    name_frame.type = FILE_NAME_PUT_TYPE;
    strncpy(name_frame.payload, filename, MAX_FRAME_SIZE);
    name_frame.size = strlen(filename) + 1;
    name_frame.seq_num = 0;

    if (DataLinkSend(sockfd, &name_frame, 1) < 0) {
        perror("Error sending filename");
        fclose(file);
        free(filename);
        return;
    }
    printf("Sent filename: %s\n", filename);

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    int total_frames = (file_size + MAX_FRAME_SIZE - 1) / MAX_FRAME_SIZE;
    total_frames += 1;

    Frame *frames = malloc(total_frames * sizeof(Frame));
    if (!frames) {
        perror("Error allocating memory for frames");
        fclose(file);
        free(filename);
        return;
    }

    char buffer[MAX_FRAME_SIZE];
    int seq_num = 1; 
    long bytes_sent = 0;

    printf("Sending file: %s, Size: %ld bytes\n", filename, file_size);

    for (int i = 0; i < (total_frames - 1); i++) {
        size_t bytes_read = fread(buffer, 1, MAX_FRAME_SIZE, file);
        if (bytes_read == 0) break;

        frames[i].type = FILE_PUT_TYPE;
        frames[i].size = bytes_read;
        frames[i].seq_num = seq_num;
        seq_num++;
        memcpy(frames[i].payload, buffer, bytes_read);

        printf("Prepared frame: seq_num=%d, size=%d, totalframes: %d\n", frames[i].seq_num, frames[i].size, total_frames);
        bytes_sent += bytes_read;
    }
    frames[total_frames - 1].type = COMMAND_END_TYPE;
    frames[total_frames - 1].size = 0;
    frames[total_frames - 1].seq_num = seq_num;

    
     if (DataLinkSend(sockfd, frames, total_frames) < 0) {
        perror("Error sending frames");
        free(filename);
    } 
 
    printf("File transfer completed. Sent %ld bytes in %d frames.\n", bytes_sent, total_frames);

    free(frames);
    free(filename);
    fclose(file);
    printf("File transfer completed. Sent %ld bytes.\n", bytes_sent);
}

void echo(int sock, Frame *frame)
{

    printf("Received Echo: %s\n", frame->payload);

    Frame echoResponse = { .type = ECHO_TYPE, .size = frame->size, .seq_num = frame->seq_num };
    Frame endFrame = {.type = COMMAND_END_TYPE, .size = 0, .seq_num = frame->seq_num + 1};
    Frame *sendFrames = malloc(2 * sizeof(Frame));
    strncpy(echoResponse.payload, frame->payload, MAX_FRAME_SIZE);
    sendFrames[0] = echoResponse;
    sendFrames[1] = endFrame;
    printf("Sent echo response: %s\n", sendFrames[0].payload);

    if (DataLinkSend(sock, sendFrames, 2) < 0) {
        perror("Failed to send ECHO response");
    }
    
    printf("Sent echo response: %s\n", sendFrames[0].payload);
    free(sendFrames);
}

int main()
{
    int server;
    int sockfd = setup_server(&server);

    printf("Client connected.\n");

    int one = 1;
    while (one) {
        Frame *frame = malloc(512 * sizeof(Frame));
        int count = DataLinkRecv(sockfd, frame);
        if (count > 0) {
            switch (frame->type) {
                case ECHO_TYPE:
                    echo(sockfd, frame);
                    break;
                case FILE_GET_TYPE:
                    handleGetFile(sockfd, frame);
                    break;
                case FILE_NAME_PUT_TYPE:
                    receiveFile(sockfd, frame, count);
                    break;
                case FILE_PUT_TYPE:
                    receiveFile(sockfd, frame, count);
                    break;
                case FILE_LIST_TYPE:
                    listFiles(sockfd);
                    break;
                case FILE_DEL_TYPE:
                    handleFileDelete(sockfd, frame);
                    break;
                case COMMAND_END_TYPE:
                    printf("Command end\n");
                    break;
                case KILL_TYPE:
                    one = 0;
                    break;
                default:
                    printf("Server Unknown frame type: %d\n", frame->type);
            }
        }
        memset(frame, 0, 512 * sizeof(Frame));
        free(frame);
    }
    printf("\nGoodbye! :(\n");
    close(sockfd);
    close(server);
    print_statistics();
    return 0;
}
