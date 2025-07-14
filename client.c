#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include "project_headers.h"

#define MAX_MESSAGE_SIZE 1024
#define MAX_FRAME_SIZE 100
#define FILENAME_SIZE 256

void sendEcho(int sockfd, const char *message)
{
    Frame echoFrame = {.type = ECHO_TYPE, .size = strlen(message), .seq_num = 0};
    Frame endFrame = {.type = COMMAND_END_TYPE, .size = 0, .seq_num = 1};
    strncpy(echoFrame.payload, message, MAX_FRAME_SIZE);
    Frame *sendFrames = malloc(2 * sizeof(Frame));
    sendFrames[0] = echoFrame;
    sendFrames[1] = endFrame;

    if (DataLinkSend(sockfd, sendFrames, 2) < 0)
    {
        perror("Failed to send ECHO request");
        free(sendFrames);
        return;
    }

    Frame *response = malloc(2 * sizeof(Frame));
    if (DataLinkRecv(sockfd, response) > 0)
    {
        printf("ECHO Response: %s\n", response[0].payload);
    }
    free(sendFrames);
    free(response);
}

void listFiles(int sock)
{
    Frame *reqFrame = malloc(2 * sizeof(Frame));
    Frame listFrame = {
        .type = FILE_LIST_TYPE,
        .size = 0,
        .seq_num = 0};
    Frame endFrame = {
        .type = COMMAND_END_TYPE,
        .size = 0,
        .seq_num = 1};
    reqFrame[0] = listFrame;
    reqFrame[0].type = FILE_LIST_TYPE;
    reqFrame[1] = endFrame;
    if (DataLinkSend(sock, reqFrame, 2) < 0)
    {
        perror("Failed to send LIST request");
        return;
    }

    Frame *response = malloc(24 * sizeof(Frame));
    int count = DataLinkRecv(sock, response);
    printf("Count: %d\n", count);
    if (count < 0)
    {
        perror("Failed to recv LIST response");
        free(reqFrame);
        free(response);
        return;
    }

    printf("Files: %d\n", count-1);
    for (int i = 0; i < count - 1; i++)
    {
        printf("\t%s\n", response[i].payload);
    }
     memset(reqFrame, 0, 2 * sizeof(Frame));
     memset(response, 0, 24 * sizeof(Frame));
    free(reqFrame);
   
    free(response);
}

void getFile(int sockfd, const char *filename)
{
    Frame *sentFrames = malloc(2 * sizeof(Frame));
    Frame file_request = {.type = FILE_GET_TYPE, .size = strlen(filename), .seq_num = 0};
    Frame endFrame = {
        .type = COMMAND_END_TYPE,
        .size = 0,
        .seq_num = 1};

    strncpy(file_request.payload, filename, MAX_FRAME_SIZE);
    sentFrames[0] = file_request;
    sentFrames[1] = endFrame;
    printf("Filename: %s %s\n", filename, file_request.payload);
    if (DataLinkSend(sockfd, sentFrames, 2) < 0)
    {
        perror("Failed to send GETFILE request");
        free(sentFrames);
    }
    free(sentFrames);

    printf("Sent GETFILE request for: %s\n", filename);
    Frame *incFrame = malloc(1 * sizeof(Frame));
    int count = DataLinkRecv(sockfd, incFrame);
    if (incFrame->type == FILE_NAME_PUT_TYPE)
    {
        printf("Received fileName: %s, count: %d\n", incFrame->payload, count);
    }
    FILE *file = fopen(filename, "wb");
    if (!file)
    {
        perror("Error creating file");
        free(incFrame);
        return;
    }

    Frame *fileData = malloc(512 * sizeof(Frame));
    int fileCount = DataLinkRecv(sockfd, fileData);
    if (fileCount > 0)
    {
        printf("Receiving file: %s\n", filename);
    }
    else
    {
        perror("Error receiving file\n");
        free(fileData);
        free(incFrame);
        return;
    }
    printf("Receiving file contents...\n");
    for (int i = 0; i < fileCount; i++)
    {
        Frame frame = fileData[i];
        if (frame.type == FILE_PUT_TYPE)
        {
            fwrite(frame.payload, 1, frame.size, file);
        }
    }

    fclose(file);
    free(incFrame);
    free(fileData);
}
void sendFile(int sockfd, const char *filename)
{
    FILE *file = fopen(filename, "rb");
    if (!file)
    {
        perror("Error opening file");
        return;
    }

    Frame name_frame = {0};
    name_frame.type = FILE_NAME_PUT_TYPE;
    strncpy(name_frame.payload, filename, MAX_FRAME_SIZE);
    name_frame.size = strlen(filename) + 1;
    name_frame.seq_num = 0;

    if (DataLinkSend(sockfd, &name_frame, 1) < 0)
    {
        perror("Error sending filename");
        fclose(file);
        return;
    }
    printf("Sent filename: %s\n", filename);

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    int total_frames = (file_size + MAX_FRAME_SIZE - 1) / MAX_FRAME_SIZE;
    total_frames += 1;

    Frame *frames = malloc(total_frames * sizeof(Frame));
    if (!frames)
    {
        perror("Error allocating memory for frames");
        fclose(file);
        return;
    }

    char buffer[MAX_FRAME_SIZE];
    int seq_num = 1;
    long bytes_sent = 0;

    printf("Sending file: %s, Size: %ld bytes\n", filename, file_size);

    for (int i = 0; i < (total_frames - 1); i++)
    {
        size_t bytes_read = fread(buffer, 1, MAX_FRAME_SIZE, file);
        if (bytes_read == 0)
            break;

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

    if (DataLinkSend(sockfd, frames, total_frames) < 0)
    {
        perror("Error sending frames");
        free(frames);
        return;
    }

    printf("File transfer completed. Sent %ld bytes in %d frames.\n", bytes_sent, total_frames);

    memset(frames, 0, total_frames * sizeof(Frame));
    free(frames);
    fclose(file);
    printf("File transfer completed. Sent %ld bytes.\n", bytes_sent);
    return;
}

void killServ(int sock)
{
    Frame killFrame = {0};
    killFrame.type = KILL_TYPE;
    killFrame.size = 0;
    killFrame.seq_num = 0;

    if (DataLinkSend(sock, &killFrame, 1) < 0)
    {
        perror("Kill send failed");
        return;
    }
}

void deleteFile(int sock, const char *filename)
{
    Frame name_frame = {0};
    name_frame.type = FILE_DEL_TYPE;
    strncpy(name_frame.payload, filename, MAX_FRAME_SIZE);
    name_frame.size = strlen(filename) + 1;
    name_frame.seq_num = 0;
    Frame endFrame = {.type = COMMAND_END_TYPE, .size=0, .seq_num = 1};
    Frame *sendFrame = malloc(2 * sizeof(Frame));
    sendFrame[0] = name_frame;
    sendFrame[1] = endFrame;



    if (DataLinkSend(sock, sendFrame, 2) < 0)
    {
        perror("Error sending filename");
        free(sendFrame);
        return;
    }

    Frame *response = malloc(2 * sizeof(Frame));
    if (DataLinkRecv(sock, response) > 0)
    {
        printf("Delete Response: %s\n", response[0].payload);
    }
    free(sendFrame);
    free(response);
}

int main(int argc, char *argv[])
{

    srand(time(NULL));
    char *server_ip = DEFAULT_SERVER_IP;
    double error_rate = DEFAULT_ERROR_RATE;

    if (argc >= 2)
    {
        server_ip = argv[1];
    }

    if (argc >= 3)
    {
        error_rate = atof(argv[2]);
        if (error_rate < 0.0 || error_rate > 1.0)
        {
            fprintf(stderr, "Error: Invalid error rate. Must be between 0.0 and 1.0.\n");
            exit(EXIT_FAILURE);
        }
    }

    int sockfd = connect_to_server(server_ip, error_rate);

    printf("Connected to server.\n");

    int currentSeq = 0;
    while (1)
    {

        char command_input[MAX_MESSAGE_SIZE];
        char command[MAX_MESSAGE_SIZE], arg[MAX_MESSAGE_SIZE];

        printf("Enter a command (e.g., 'echo hello', 'getfile file.txt', 'putfile path/to/file', 'del file.txt', 'list', 'kill'): ");
        fgets(command_input, MAX_MESSAGE_SIZE, stdin);
        command_input[strcspn(command_input, "\n")] = '\0';

        char *space_ptr = strchr(command_input, ' ');
        if (space_ptr)
        {

            size_t command_len = space_ptr - command_input;
            strncpy(command, command_input, command_len);
            command[command_len] = '\0';

            strncpy(arg, space_ptr + 1, MAX_MESSAGE_SIZE - 1);
            arg[MAX_MESSAGE_SIZE - 1] = '\0';
        }
        else
        {

            strncpy(command, command_input, MAX_MESSAGE_SIZE - 1);
            command[MAX_MESSAGE_SIZE - 1] = '\0';
            arg[0] = '\0';
        }

        if (strcmp(command, "echo") == 0)
        {
            sendEcho(sockfd, arg);
        }
        else if (strcmp(command, "getfile") == 0)
        {
            getFile(sockfd, arg);
        }
        else if (strcmp(command, "putfile") == 0)
        {
            sendFile(sockfd, arg);
        }
        else if (strcmp(command, "list") == 0)
        {
            listFiles(sockfd);
        }
        else if (strcmp(command, "del") == 0)
        {
            deleteFile(sockfd, arg);
        }
        else if (strcmp(command, "kill") == 0)
        {
            killServ(sockfd);
            printf("\nGoodbye! :(\n");
            break;
        }
        else
        {
            printf("Invalid command.\n");
        }
        currentSeq += 1;
    }

    close(sockfd);
    print_statistics();
    return 0;
}
