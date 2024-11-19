/*
 * Click nbfs://nbhost/SystemFileSystem/Templates/Licenses/license-default.txt to change this license
 * Click nbfs://nbhost/SystemFileSystem/Templates/cFiles/main.c to edit this template
 */

/* 
 * File:   deliver.c
 * Author: longlang
 *
 * Created on January 23, 2023, 9:53 p.m.
 */

#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h> 
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>

/*client side code in file transfer lab 
 * 
 */

#define MAX_PACKET_SIZE     1000

struct packet{
    unsigned int total_frag;
    unsigned int frag_no;
    unsigned int size;
    char* filename;
    char filedata[1000];
};

int packet_to_buffer(char**, struct packet*, int);
void fill_buffer(int*, char*, char*, int);
int integer_char_length(int);

int main(int argc, char** argv) {
    
    if(argc != 3){
        printf("Incorrect input!");
        exit(-1);
    }
    
    char* server_address = argv[1];
    char* server_port = argv[2];
    
    
    struct addrinfo hints, *res; //hint is information about myself
    
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;
    
    getaddrinfo(server_address, server_port, &hints, &res); //get ip of this machine
    int sock_fd = socket(AF_INET, SOCK_DGRAM, 0); //udp socket
    
    //check file
    char command[100];
    char file_name[100];
    printf("Enter ftp <file_name>: ");
    scanf("%s", command);
    scanf("%s", file_name);
    
    if(strcmp(command, "ftp")){
        printf("Didn't enter ftp\n");
        exit(-1);
    }
   
    if(fopen(file_name, "r")){
        printf("File exists\n");
    }else{
        printf("File doesn't exist\n");
        exit(-1);
    }
    
    //if we get here, means file exits, client sends ftp to server
    //send to server
    printf("Sending 'ftp' to server\n");
    
    struct timeval starttime, endtime;
    gettimeofday(&starttime, NULL);
    int bytes_sent = sendto(sock_fd, "ftp", 3, 0, res->ai_addr, res->ai_addrlen);
    
    //recieve message from server 
    char rec_buffer[4096];
    struct sockaddr_storage* recieve_msg;
    socklen_t addr_len = sizeof (recieve_msg);
    
    if(recvfrom(sock_fd, &rec_buffer, 4096, 0, (struct sockaddr*)&recieve_msg, &addr_len) < 0){ //blocking call waits for server side to respond
        perror("recvfrom");
        printf("Error: cannot recieve from server\n");
    }
    gettimeofday(&endtime, NULL);
    int s_RTT = endtime.tv_sec - starttime.tv_sec;
    int u_RTT = endtime.tv_usec - starttime.tv_usec;
    //printf("RTT took %f seconds.\n", total_t);  //**************************measure rtt of sending ftp and receiving yes from client
    
    if(strcmp(rec_buffer, "yes") == 0){ //if string contains yes
        printf("From server: %s\n", rec_buffer);
        printf("A file transfer can start:\n");
    }else if(strcmp(rec_buffer, "no") == 0){
        printf("From server: %s\n", rec_buffer);
        printf("exit\n");
        exit(-1);
    }
    
    //************************************************************
    //Proceed to file transfer: 
    //**************************************************************
    
    //read binary files, store in a char array 
    FILE* fp = fopen(file_name, "rb"); 
    fseek(fp, 0, SEEK_END);
    
    int file_length = ftell(fp);
    
    fseek(fp, 0, SEEK_SET); //set to initial position to read into buffer.
    char* file_buffer = (char*)malloc(file_length + 1); //char is 1 byte
    
    fread(file_buffer, file_length, 1, fp); //reads 1 element of size file_length
    fclose(fp);
    
    printf("Copied file content to a buffer. \n");
    printf("File buffer: %s\n", file_buffer);
    
    //determine number of packets: 
    int num_full_packets = file_length / MAX_PACKET_SIZE;
    int remaining_size = file_length - (num_full_packets * MAX_PACKET_SIZE);
    int total_num_packets;
    if(remaining_size == 0){
        total_num_packets = num_full_packets;
    }else{
        total_num_packets = num_full_packets + 1;
    }
    printf("Full packets: %d\n", num_full_packets);
    printf("Remaining size: %d\n", remaining_size);
    printf("Determined total number of packets: %d\n", total_num_packets);
    
    //fill packets with data: 
    //generate packet format: 
    //"<total_frag>:<frag_no>:<file_size>:<file_name>": "3:2:10:foobar.txt:lo World\n"
    
    char** packet_list = (char**)malloc(sizeof(char*) * total_num_packets); //dynamic memory allocation
    int file_name_length = strlen(file_name);
    
    for(int i = 0; i < total_num_packets; i++){
        
        struct packet subpacket;
        int file_data_length;
        if(i + 1 == num_full_packets && remaining_size != 0){
            file_data_length = remaining_size;
        }else if(i + 1 == total_num_packets && remaining_size != 0){
            file_data_length = remaining_size;
        }else
            file_data_length = MAX_PACKET_SIZE;
        
        subpacket.size = file_data_length;
        subpacket.frag_no = i + 1; //starting from 1
        subpacket.total_frag = total_num_packets;
        subpacket.filename = (char*)malloc(file_name_length + 1);
        strcpy(subpacket.filename, file_name);
        //memcpy(subpacket.filedata, file_buffer + i*file_data_length, file_data_length);   
        
        for(int j = 0; j < file_data_length; j++){
            subpacket.filedata[j] = file_buffer[i*file_data_length + j];
        }
        
        int packet_length = packet_to_buffer(packet_list, &subpacket, i); //convert to standard format 
        //printf("Sending packet number:%d to server\n", i+1);
        //printf("Packet content: %s\n", packet_list[i]);
        
        //clock_t start_RTT, end_RTT;
        //start_RTT = clock();
        
        int bytes_sent = sendto(sock_fd, packet_list[i], packet_length, 0, res->ai_addr, res->ai_addrlen);
        if(bytes_sent < 0){
            printf("Error: cannot send to server\n");
        }
        
        //************************************************************************************
        // Recieve acknowledgment from server
        //************************************************************************************
        char ack_buffer[4];
        int sendtime = 1;
        struct timeval timeout;
        timeout.tv_sec = 0; // 6 x initial RTT value
        timeout.tv_usec = 6*u_RTT;
        int rv = setsockopt (sock_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);
        if(rv < 0){
            printf("rv: %d", rv);
            printf("setsockopt failed\n");
        }
        
        //printf("here?\n");
        while(sendtime <= 10){
            //printf("Here %d\n", sendtime);
            if (recvfrom(sock_fd, &ack_buffer, 4, 0, (struct sockaddr*)&recieve_msg, &addr_len) > 0){
                break;
            } else{
                /*
                end_RTT = clock();
                double RTT = (double)(end_RTT-start_RTT) / CLOCKS_PER_SEC; 
                timeout.tv_sec = 6*RTT;
                timeout.tv_usec = 0;
                if (setsockopt (sock_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout) <0 )
                    printf("setsockopt failed\n");
            
                start_RTT = clock();
                 */
                int bytes_sent = sendto(sock_fd, packet_list[i], packet_length, 0, res->ai_addr, res->ai_addrlen);
                if(bytes_sent < 0){
                    printf("Error, cannot sent to server\n");
                }
                printf("********************************\n");
                printf("Retransmitting packet: %d\n", i+1);
                printf("********************************\n");
                sendtime++;
            }
        }
        
        if(sendtime > 10){
            printf("\n");
            printf("******************\n");
            printf("Connection lost\n");
            printf("******************\n");
            exit(-1);
        }
        if(strcmp(ack_buffer, "ACK!")){ //if string contains yes
            //printf("From server: %s\n", ack_buffer);
            //printf("Received ACK from server\n");
            //printf("Packet %d successfully transferred !\n\n", i+1);
        }else{
            printf("From server: %s\n", ack_buffer);
            printf("Did not receive ACK!\n");
            exit(-1);
        }
    }
    return (EXIT_SUCCESS);
}

int packet_to_buffer(char** packet_list, struct packet* subpacket, int packet_num){
    
    int total_frag_length = integer_char_length(subpacket->total_frag);
    int frag_no_length = integer_char_length(subpacket->frag_no);
    int size_length = integer_char_length(subpacket->size);
    int file_name_length = strlen(subpacket->filename);
    
    int packet_length = total_frag_length + frag_no_length + size_length + file_name_length + subpacket->size + 4;
    packet_list[packet_num] = (char*)malloc(packet_length);
    
    //converting integers to strings
    char total_frag_char[total_frag_length];
    char frag_no_char[frag_no_length];
    char size_char[size_length];
    sprintf(total_frag_char, "%d", subpacket->total_frag);
    sprintf(frag_no_char, "%d", subpacket->frag_no);
    sprintf(size_char, "%d", subpacket->size);
    
    //copy into packet list
    //total fragment number
    int index_position = 0;
    
    //total frag number
    fill_buffer(&index_position, packet_list[packet_num], total_frag_char, total_frag_length);
    
    //sequence number
    fill_buffer(&index_position, packet_list[packet_num], frag_no_char, frag_no_length);
    
    //file size 
    fill_buffer(&index_position, packet_list[packet_num], size_char, size_length);
    
    //file name 
    fill_buffer(&index_position, packet_list[packet_num], subpacket->filename, file_name_length);
    
    //file data
    fill_buffer(&index_position, packet_list[packet_num], subpacket->filedata, subpacket->size);
    
    return packet_length;
}

void fill_buffer(int* index, char* packet_buffer, char* input_buffer, int length){
    //memcpy(packet_buffer + *index, input_buffer, length);
    for(int i = 0; i < length; i++){
        packet_buffer[*index + i] = input_buffer[i];  //memcpy?
    }
    *index += length;
    packet_buffer[*index] = ':';
    *index += 1;
}

int integer_char_length(int num){
    if(num == 0)
        return 1;
    else
        return floor(log10(abs(num))) + 1;
}