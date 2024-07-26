/**
 * Sets a socket file descriptor to be nonblocking
 * 
 * @param sockfd the socket file descriptor 
 * 
 */
void set_nonblocking(int sockfd);


/**
 * Creates a socket and binds it to the server's port
 * 
 * @return the socket's file descriptor
 */
int create_server_socket();

/**
 * Handles a client's request to the database
 * 
 * @param client_fd the file descriptor of the client's socket
 */
void handle_client_request(int client_fd);
