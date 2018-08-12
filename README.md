# TCP-Server-Client

The client is implemented in a source file called client.c and the server in server.c.

Services of the server: connecting, listing directories and downloading files.

The client and the server share some functionality and structures: 
debug mode functionality- printing to stderr/stdout any message they received.
client_state stracture - represets details of the connection like the address of the server, the connection status.

handeling errors:
The client check the return value of all socket APIs for errors. in case of error error - prints the error and exit with code -1.
The server also check the return value. on error - print the error , free any resources allocated for the client, and return to listening for new connections.

The addition files: common, line_parser are helping files for :
line_parser - parsing the text that sending from and to the client and the server (separates words and sentences by spaces and newline chars).
commom - include helper functions for listing the directories in the working directory.
