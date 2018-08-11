#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/sendfile.h>

#include "line_parser.h"
#include "common.h"

#define PORT 2018
#define FILE_BUFF_SIZE 1024
#define INPUT_SIZE 2048

int fileno(FILE *stream);
int gethostname(char *name, size_t len);

int exec(cmd_line *);
int hello_cmd_handler(cmd_line *);
int bye_cmd_handler(cmd_line *);
int ls_cmd_handler(cmd_line *);
int get_cmd_handler(cmd_line *);
void print_debug_message(char * message);

typedef enum {
    IDLE,
    CONNECTING,
    CONNECTED,
    DOWNLOADING,
} c_state;
    
typedef struct {
    char* server_addr;  /* Address of the server as given in the [connect] command. "nil" if not connected to any server*/
    c_state conn_state; /* Current state of the client. Initially set to IDLE*/
    char* client_id;    /* Client identification given by the server. NULL if not connected to a server.*/
    int sock_fd;        /* The file descriptor used in send/recv*/
} client_state;

client_state client;
int client_socket;
int debug_flag = 0;

int main(int argc, char **argv)
{
    char input[INPUT_SIZE];
    cmd_line * line_from_client;
    int accept_socket;
    struct sockaddr_in server_addr;
    struct sockaddr_storage server_storage;

	/*update_debug_flag*/
	int i;
	for (i = 0; i < argc; ++i){
		if(strcmp(argv[i],"-d")==0){
			debug_flag = 1;
			break;
		}	
	}

    /*Initialize state*/
    client.server_addr = "127.0.0.1";
    client.conn_state = IDLE;
    client.client_id = NULL;
    client.sock_fd = -1;

    /*Init the server address struct*/
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);/*using htons function to use proper byte order*/
    server_addr.sin_addr.s_addr = inet_addr(client.server_addr);/*converts the Internet host address cp from the IPv4 numbers-and-dots notation into binary form (in network byte order).*/
    memset(server_addr.sin_zero, '\0', sizeof server_addr.sin_zero);/*set all bits of the padding field to 0*/

    /*Create the accepting socket - Get a file descriptor for the server's socket*/
    accept_socket = socket(PF_INET, SOCK_STREAM, 0);
    if(accept_socket == -1){
        perror("ERROR in socket");
        return -1;
    }

    /*Bind the address struct to the accepting socket -  Associate a Layer-4 port with this process*/
    int b = bind(accept_socket, (struct sockaddr *) &server_addr, sizeof(server_addr));/*assigns the address specified by addr to the socket referred to by the file descriptor sockfd.*/
    if(b == -1){
        perror("ERROR in bind");
        return -1;
    }

    /*Listen on the socket with max one connection requests*/
    int l = listen(accept_socket,1);/*marks the socket referred to by sockfd as a socket that will be used to accept incoming connection requests using accept(2), 1-for blocking.*/
    if(l == -1){
        perror("ERROR in listen");
        return -1;
    }

    /*Accept - creates a new socket for the incoming connection*/
    socklen_t addr_size = sizeof server_storage;
    client_socket = accept(accept_socket, (struct sockaddr *) &server_storage, &addr_size);/* On success, these system calls return a nonnegative integer that is a file descriptor for the accepted socket*/
    if(client_socket ==-1){
        perror("ERROR in accept");
        return -1;
    }

    /*Client-loop : received client messages and executes them.*/
    while(1) 
    { 
        /*receive a line from user*/
        ssize_t number_read_chars = recv(client_socket, input, INPUT_SIZE, 0);    
        if(number_read_chars == -1){
            perror("ERROR- fail in read");
            exit(-1);
        }

        printf("Data received: %s",input);   

        /*parse the input*/
        line_from_client = parse_cmd_lines(input);
        print_debug_message(input);
        if(line_from_client == NULL){
            printf("there is nothing to parse.");
            free_cmd_lines(line_from_client); /*release the cmd_line resources when finished.*/
            continue;
        }
        if(strcmp(line_from_client->arguments[0],"nok")==0){/*"nok <error>" response triggers the server to disconnect the client*/
            client.conn_state = IDLE;
            if(client.sock_fd!=-1)
                close(client.sock_fd);  
        }
        if(strcmp(line_from_client->arguments[0],"bye")==0){ /*exit program in normally way if finished*/
            exec(line_from_client);
            if(client.sock_fd!=-1)
                close(client.sock_fd); /*close the socket to finish listening for new connection.*/
            exit(0);
        }
        exec(line_from_client);
        free_cmd_lines(line_from_client); /*release the cmd_line resources when finished.*/
    }
    return 0; 
}

/*that takes a command and possible arguments.
 This function executes the various commands the
  client will support (initially just "conn" and "bye"). */
int exec(cmd_line * cmd) {

    /*connect*/
     if(strcmp(cmd->arguments[0],"hello")==0){
        if(client.conn_state == IDLE)
            return hello_cmd_handler(cmd);
        char buf[INPUT_SIZE];
        memset(buf, '\0', sizeof(buf));
        strcpy(buf,"nok state\n");
        strcat(buf,client.client_id);
        strcat(buf,"\n");
        ssize_t s = send(client.sock_fd, buf, strlen(buf), 0);
        if(s==-1 || s < strlen(buf))
            perror("ERROR in send");
        return -2;
    }

    else if(strcmp(cmd->arguments[0],"bye")==0){
        if(client.conn_state != CONNECTED){
            printf("client state != connected\n");
            char buf[INPUT_SIZE];
            memset(buf, '\0', sizeof(buf));
            strcpy(buf,"nok state ");
            strcat(buf,client.client_id);
            strcat(buf,"\n");
            ssize_t s = send(client.sock_fd, buf, strlen(buf), 0);
            if(s==-1 || s < strlen(buf))
                perror("ERROR in send");
            return -2;
        }
        return bye_cmd_handler(cmd);
    }

     else if(strcmp(cmd->arguments[0],"ls")==0){
        if(client.conn_state != CONNECTED){
            printf("client state != connected\n");
            char buf[INPUT_SIZE];
            memset(buf, '\0', sizeof(buf));
            strcpy(buf,"nok state ");
            strcat(buf,client.client_id);
            strcat(buf,"\n");
            ssize_t s = send(client.sock_fd, buf, strlen(buf), 0);
            if(s==-1 || s < strlen(buf))
                perror("ERROR in send");
            return -2;
        }
        return ls_cmd_handler(cmd);
    }

    else if(strcmp(cmd->arguments[0],"get")==0){
        if(client.conn_state != CONNECTED){
            printf("client state != connected\n");
            char buf[INPUT_SIZE];
            memset(buf, '\0', sizeof(buf));
            strcpy(buf,"nok state ");
            strcat(buf,client.client_id);
            strcat(buf,"\n");
            ssize_t s = send(client.sock_fd, buf, strlen(buf), 0);
            if(s==-1 || s < strlen(buf))
                perror("ERROR in send");
            return -2;
        }
        return get_cmd_handler(cmd);
    }

    else/*Any message received from the client that isn't a defined by the protocol is printed to stderr*/
        fprintf(stderr,"%s|ERROR: Unknown message %s", client.client_id, cmd->arguments[0]);

    return 0;
}

int get_cmd_handler(cmd_line * cmd){

    char* file_name = cmd->arguments[1];

    /*calculate the size of <file_name> */
    int file_size1 = file_size(file_name);
    if(file_size1 == -1){
        char buf1[INPUT_SIZE];
        strcpy(buf1,"nok file\0");
        ssize_t s = send(client.sock_fd, buf1, strlen(buf1)+1, 0);
        if(s == -1 || s < strlen(buf1)+1){
            perror("ERROR in send");
            return -1;
        }
    }

    /*respond with "ok %d" where %d is the file size*/
    char buf2[INPUT_SIZE];
    strcpy(buf2,"ok ");
    char str_file_size[INPUT_SIZE];
    sprintf(str_file_size, "%d", file_size1);
    strcat(buf2, str_file_size);
    ssize_t s = send(client.sock_fd, buf2, strlen(buf2)+1, 0);
    if(s == -1 || s < strlen(buf2)+1){
        perror("ERROR in send");
        return -1;
    }

    client.conn_state = DOWNLOADING;
    /*start sending the file*/
    FILE *file = fopen(file_name, "r"); 
    if (file == NULL){
        perror("ERROR - can't open the file");
        return -1;
    } 

    s = sendfile(client.sock_fd, fileno(file), NULL, FILE_BUFF_SIZE);
    if(s == -1){
        perror("ERROR in send");
        return -1;
    }

    /*expect a "done" message*/
    char input[INPUT_SIZE];
    ssize_t number_read_chars = recv(client_socket, input, INPUT_SIZE, 0);    
    if(number_read_chars == -1){
        perror("ERROR- fail in read");
        exit(-1);
    }

    /*If a "done" message doesn't arrives*/
    cmd_line * line = parse_cmd_lines(input);
    print_debug_message(input);
    if(line == NULL || strcmp(line->arguments[0],"done")!=0){
        char buf3[INPUT_SIZE];
        strcpy(buf3,"nok done\0");
        ssize_t s = send(client.sock_fd, buf3, strlen(buf3)+1, 0);
        if(s == -1 || s < strlen(buf3)+1){
            perror("ERROR in send");
            return -1;
        }
    }
    /*If a "done" message indeed arrives*/
    if(strcmp(line->arguments[0],"done")==0){
        client.conn_state = CONNECTED;
        char buf4[INPUT_SIZE];
        strcpy(buf4,"ok\0");
        ssize_t s = send(client.sock_fd, buf4, strlen(buf4)+1, 0);
        if(s == -1 || s < strlen(buf4)+1){
            perror("ERROR in send");
            return -1;
        }
        printf("Sent file %s\n", file_name);
        return 0;
    }

    return -1;
}

void print_debug_message(char * message){
    if(debug_flag)
        fprintf(stderr, "%s|Log: %s\n", client.server_addr, message);

}

int ls_cmd_handler(cmd_line * cmd){

    char* listing = list_dir();

    if(listing == NULL){/*list_dir fails*/
        char buf[INPUT_SIZE];
        strcpy(buf,"nok filesystem\n");
        ssize_t s = send(client.sock_fd, buf, strlen(buf)+1, 0);
        if(s == -1 || s < strlen(buf)+1){
            perror("ERROR in send");
            return -1;
        }
    }
    else{
        /*send "ok" to the client*/
        char buf1[INPUT_SIZE];
        strcpy(buf1,"ok\0");
        ssize_t s = send(client.sock_fd, buf1, strlen(buf1)+1, 0);
        if(s == -1 || s < strlen(buf1)+1){
            perror("ERROR in send");
            return -1;
        }
        /*send the list to the client*/
        char buf2[INPUT_SIZE];
        strcpy(buf2,listing);
        s = send(client.sock_fd, buf2, strlen(buf2)+1, 0);
        if(s == -1 || s < strlen(buf2)+1){
            perror("ERROR in send");
            return -1;
        }
        /*print to stdout*/
        char working_dir[INPUT_SIZE];
        char * w = getcwd(working_dir, INPUT_SIZE);
        if(w == NULL){
            perror("ERROR in get working dir");
            return -1;
        }
        printf("Listed files at %s\n", working_dir);
        return 0;
    }
    return -1;
}


int bye_cmd_handler(cmd_line * cmd){
    
    	char buf[INPUT_SIZE];
    	strcpy(buf,"bye\n");
        ssize_t s = send(client.sock_fd, buf, strlen(buf)+1, 0);
        if(s == -1 || s < strlen(buf)+1){
            perror("ERROR in send");
            return -1;
        }
        printf("Client %s disconnected\n", client.client_id);
        free_cmd_lines(cmd);
        return 0;
}


int hello_cmd_handler(cmd_line * cmd) {

    client.conn_state = CONNECTED;

    /*Send message to the client*/
    char buf[INPUT_SIZE];
    memset(buf, '\0', sizeof(buf));
    client.client_id = "client";
    client.sock_fd = client_socket;
    strcpy(buf,"hello ");
    strcat(buf,client.client_id);
    strcat(buf,"\n");
    ssize_t s = send(client_socket, buf, strlen(buf)+1, 0);
    if(s == -1 || s < strlen(buf)+1){
        perror("ERROR in send");
        return -1;
    }

    printf("Client %s connected\n", client.client_id);


    return 0;

}

