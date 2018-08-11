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
#include <unistd.h>

#include "line_parser.h"

#define PORT 2018
#define FILE_BUFF_SIZE 1024
#define INPUT_SIZE 2048

int gethostname(char *name, size_t len);
int exec(cmd_line *);
int connect_cmd_handler(cmd_line *);
int bye_cmd_handler(cmd_line *);
int ls_cmd_handler(cmd_line *);
int get_cmd_handler(cmd_line *);
void print_debug_message(char *);

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
int debug_flag = 0;

int main(int argc, char **argv)
{

	/*update_debug_flag*/
	int i;
	for (i = 0; i < argc; ++i){
		if(strcmp(argv[i],"-d")==0){
			debug_flag = 1;
			break;
		}	
	}

    char input[INPUT_SIZE];
    cmd_line * line_from_server;
    
    /*Initialize*/
    client.server_addr = "nill";
    client.conn_state = IDLE;
    client.client_id = NULL;
    client.sock_fd = -1;

    while(1) 
    { 
        /*print the address of server, if the client connected to anyone*/
        printf("server:%s>", client.server_addr);
        /*read a line from stdin*/
        const char* input_user = fgets(input,INPUT_SIZE , stdin);
        if(input == NULL)
            exit(1);

        if(strcmp(input,"quit\n")==0){ /*exit program in normally way if finished*/
            if(client.sock_fd!=-1)
                close(client.sock_fd);  
            exit(0);
        }
        /*parse the input*/
        line_from_server = parse_cmd_lines(input_user);
        if(line_from_server == NULL){
            printf("there is nothing to parse.");
            free_cmd_lines(line_from_server); /*release the cmd_line resources when finished.*/
            continue;
        }
        if(strcmp(line_from_server->arguments[0],"nok")==0){
                fprintf(stderr, "Server Error: %s", line_from_server->arguments[1]);
                client.conn_state = IDLE;
                free_cmd_lines(line_from_server); /*release the cmd_line resources when finished.*/
                continue;
        }
        exec(line_from_server);
        free_cmd_lines(line_from_server); /*release the cmd_line resources when finished.*/
    }
    return 0; 
}

/*that takes a command and possible arguments.
 This function executes the various commands the
  client will support (initially just "conn" and "bye"). */
int exec(cmd_line * cmd) {

    /*connect*/
     if(strcmp(cmd->arguments[0],"conn")==0){
        if(client.conn_state == IDLE)
            return connect_cmd_handler(cmd);
        return -2;
    }

    if(strcmp(cmd->arguments[0],"bye")==0){
        if(client.conn_state != CONNECTED){
            printf("client state != connected\n");
            return -2;
        }
        return bye_cmd_handler(cmd);
    }

    if(strcmp(cmd->arguments[0],"ls")==0){
        if(client.conn_state != CONNECTED){
            printf("client state != connected\n");
            return -2;
        }
        return ls_cmd_handler(cmd);
    }

     if(strcmp(cmd->arguments[0],"get")==0){
        if(client.conn_state != CONNECTED){
            printf("client state != connected\n");
            return -2;
        }
        return get_cmd_handler(cmd);
    }

    return 0;
}

int get_cmd_handler(cmd_line * cmd){

	/*send a request get <file name> to the server*/
	char buf[INPUT_SIZE];
    strcpy(buf,"get ");
    strcat(buf,cmd->arguments[1]);
    strcat(buf,"\n");
    ssize_t s = send(client.sock_fd, buf, strlen(buf)+1, 0);
    if(s == -1 || s < strlen(buf)+1){
        printf("fail send\n");
        return -1;
    }

    char input1[3];
    /*Read the message from the server into input buffer*/
    int number_of_read_chars = recv(client.sock_fd, input1, 3, 0);
    if (number_of_read_chars == -1) {
        perror("ERROR in read");
        return -1;
    }
	cmd_line * line1 = parse_cmd_lines(input1);

    print_debug_message(input1);

    if(strcmp(line1->arguments[0],"nok")==0){
    	/*printed to stdrr the error*/
    	char input_error[INPUT_SIZE];
    	int number_of_read_chars = recv(client.sock_fd, input_error, INPUT_SIZE, 0);
    	print_debug_message(input_error);
    	if (number_of_read_chars == -1) {
        	perror("ERROR in read");
        	return -1;
    	}
    	fprintf(stderr, "Server Error: %s\n",input_error);
    	/*disconnect from the server*/
    	close(client.sock_fd);
        client.client_id = NULL;
        client.conn_state = IDLE;
        client.sock_fd = -1;
        client.server_addr = "nil";
        return -1;
    }
    if(strcmp(line1->arguments[0],"ok")==0){
    	/*get the file_size*/
    	char input_file_size[INPUT_SIZE];
    	int number_of_read_chars = recv(client.sock_fd, input_file_size, INPUT_SIZE, 0);
    	if (number_of_read_chars == -1) {
        	perror("ERROR in read");
        	return -1;
    	}
        cmd_line * line3 = parse_cmd_lines(input_file_size);
        print_debug_message(input_file_size);

    	/*open the file*/
    	char filename[INPUT_SIZE];
    	strcpy(filename,cmd->arguments[1]);
   		strcat(filename,".tmp");
		FILE *file = fopen(filename, "w"); 
    	if (file == NULL){
        	perror("ERROR - can't open the file");
        	return -1;
    	} 

    	client.conn_state = DOWNLOADING;
    	/*getting the file*/
    	char input_file[FILE_BUFF_SIZE];
    	int curr_file_size = 0;
    	int file_size = atoi(line3->arguments[0]);

    	/* loop that keeps recving BUFF_SIZE chunksfor the file and writing them to the newly opened file.*/
    	do{
    		memset(input_file ,0 , FILE_BUFF_SIZE);
    		int number_of_read_chars = recv(client.sock_fd, input_file, INPUT_SIZE, 0);
    		if (number_of_read_chars == -1) {
        		perror("ERROR in read");
        		return -1;
    		}
    		print_debug_message(input_file);
    		curr_file_size += strlen(input_file);
    		fwrite(&input_file, strlen(input_file), 1, file);/*writes the value array with 1 element, of the size size to the curr pos of file */

    	} while(curr_file_size < file_size);

    	/*send done to the server and waiting for ok*/
    	char buf[INPUT_SIZE];
    	strcpy(buf,"done\0");
    	ssize_t s = send(client.sock_fd, buf, strlen(buf)+1, 0);
    	if(s == -1 || s < strlen(buf)+1){
       	 	printf("fail send\n");
        	return -1;
    	}
    	/*Read the message from the server into input buffer*/
    	char input3[3];
    	number_of_read_chars = recv(client.sock_fd, input3, 3, 0);
    	if (number_of_read_chars == -1) {
       	 	perror("ERROR in read");
        	return -1;
    	}
    	/*wait 10 sec to ok*/
		sleep(10);
    	print_debug_message(input3);
    	if(strcmp(line1->arguments[0],"ok")==0){
    		client.conn_state = CONNECTED;
    		/*changing the filename.tmp to filename*/
			int j = 0;
			char new_filename[strlen(filename)];
    		memset(new_filename ,0 , strlen(filename));
			for(j=0; j<strlen(filename);j++){
				if(filename[j]=='.')
					break;
				new_filename[j] = filename[j];
			}				
    		rename(filename, new_filename);
		}
		else if(strcmp(line1->arguments[0],"nok")==0){
			/*deletes filename*/
			int r = remove(filename);
			if(r == -1){
				perror("ERROR in remove");
        		return -1;
			}
			/*prints error massage*/
			fprintf(stderr, "Error while downloading file %s\n", filename);
		}


        return 0;
    }
    return -1;
}

void print_debug_message(char * message){
	if(debug_flag)
		fprintf(stderr, "%s|Log: %s\n", client.server_addr, message);

}

int ls_cmd_handler(cmd_line * cmd){

	char buf[INPUT_SIZE];
    strcpy(buf,"ls\n");
    ssize_t s = send(client.sock_fd, buf, strlen(buf)+1, 0);
    if(s == -1 || s < strlen(buf)+1){
        printf("fail send\n");
        return -1;
    }
    char input1[3];
    /*Read the message from the server into input buffer*/
    int number_of_read_chars = recv(client.sock_fd, input1, 3, 0);
    if (number_of_read_chars == -1) {
        perror("ERROR in read");
        return -1;
    }
	cmd_line * line2 = parse_cmd_lines(input1);
    print_debug_message(input1);

    if(strcmp(line2->arguments[0],"nok")==0){
    	/*printed to stdrr the error*/
    	char input_error[INPUT_SIZE];
    	int number_of_read_chars = recv(client.sock_fd, input_error, INPUT_SIZE, 0);
    	print_debug_message(input_error);
    	if (number_of_read_chars == -1) {
        	perror("ERROR in read");
        	return -1;
    	}
    	fprintf(stderr, "Server Error: %s\n",input_error);
    	/*disconnect from the server*/
    	close(client.sock_fd);
        client.client_id = NULL;
        client.conn_state = IDLE;
        client.sock_fd = -1;
        client.server_addr = "nil";
        return -1;
    }
    if(strcmp(line2->arguments[0],"ok")==0){

    	/*printed to stdrr the error*/
    	char input_ls[INPUT_SIZE];
    	int number_of_read_chars = recv(client.sock_fd, input_ls, INPUT_SIZE, 0);
    	print_debug_message(input_ls);
    	if (number_of_read_chars == -1) {
        	perror("ERROR in read");
        	return -1;
    	}
    	printf("the list of files: %s\n",input_ls);
        return 0;
    }
    return -1;

}


int bye_cmd_handler(cmd_line * cmd){
    
    	char buf[INPUT_SIZE];
    	strcpy(buf,"bye\n");
        ssize_t s = send(client.sock_fd, buf, strlen(buf)+1, 0);
        if(s == -1 || s < strlen(buf)+1){
            printf("fail send\n");
            return -1;
        }       
        close(client.sock_fd);
        client.client_id = NULL;
        client.conn_state = IDLE;
        client.sock_fd = -1;
        client.server_addr = "nil";
        return 0;
}


int connect_cmd_handler(cmd_line * cmd) {

 	int socket_fd_client;
    struct sockaddr_in server_addr;
  	char input [2048];
  	cmd_line * line_from_server;
    
    /*Init server address struct*/   
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);/*using htons function to use proper byte order*/
    server_addr.sin_addr.s_addr = inet_addr(cmd->arguments[1]);/*converts the Internet host address cp from the IPv4 numbers-and-dots notation into binary form (in network byte order).*/
    memset(server_addr.sin_zero, '\0', sizeof server_addr.sin_zero);/*set all bits of the padding field to 0*/
 
    /*Get a file descriptor for the client's socket.*/
    socket_fd_client = socket(PF_INET,SOCK_STREAM,0);
    if (socket_fd_client == -1)
        return -1;

  	/*Connect to server*/
    int conn = connect(socket_fd_client, (struct sockaddr *)&server_addr, sizeof server_addr);
    if(conn != -1){/*sucsees connect*/   

        char buf[INPUT_SIZE];
        strcpy(buf,"hello\n");
        ssize_t s = send(socket_fd_client, buf, strlen(buf)+1, 0);
        if(s == -1 || s < strlen(buf)+1){
            printf("fail send\n");
            return -1;
        }

        client.conn_state = CONNECTING;

        /*Read the message from the server into input buffer*/
        int number_of_read_chars = recv(socket_fd_client, input, INPUT_SIZE, 0);
        if (number_of_read_chars == -1) {
            perror("ERROR in read");
            return -1;
        }

        printf("Data received: %s\n",input);   

        /*parse the input*/
        line_from_server = parse_cmd_lines(input);
    	print_debug_message(input);
        if(line_from_server == NULL)
            printf("there is nothing to parse.\n");
        if(strcmp(line_from_server->arguments[0],"nok")==0){
            fprintf(stderr, "Server Error: %s\n",line_from_server->arguments[1]);
            client.conn_state = IDLE;
            return 0;
        }
        if(strcmp(line_from_server->arguments[0],"hello")!=0){
            perror("ERROR: the server didn't send hello");
            return -1;
        }

        client.client_id = line_from_server->arguments[1];
        client.conn_state = CONNECTED;
        client.sock_fd = socket_fd_client;

		client.server_addr = "127.0.0.1";
		
		printf("%s\n",client.server_addr);
    }

   return -1; /*fail*/

}
