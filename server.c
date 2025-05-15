#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

#define PORT 1337		// server listening port
#define MAX_CLIENTS 100		// maximum number of concurrrent clients
#define NICKNAME_LEN 50		// max length of nickname
#define BUFFER_SIZE 1024	// size of message buffer

// Structure to store client socket and nickname
typedef struct {
	int socket;
	char nickname[NICKNAME_LEN];
} Client;

Client clients[MAX_CLIENTS];
int client_count = 0;
int server_socket;
pthread_mutex_t client_lock = PTHREAD_MUTEX_INITIALIZER;

// Get nickname for a given socket
const char* get_nickname(int socket) {
	for(int i = 0; i < client_count; i++) {
		if(clients[i].socket == socket) {
			return clients[i].nickname;
		}
	}
	return "Unknown";
}

// Add a new client to the client list
void add_client(int socket, char *nickname) {
	pthread_mutex_lock(&client_lock);
	if(client_count >= MAX_CLIENTS){
		pthread_mutex_unlock(&client_lock);
		return;
	}
	clients[client_count].socket = socket;
	strncpy(clients[client_count].nickname, nickname, NICKNAME_LEN);
	client_count++;
	pthread_mutex_unlock(&client_lock);
}

// Remove client by socket
void remove_client(int socket) {
	pthread_mutex_lock(&client_lock);
	for(int i = 0; i < client_count; i++) {
		if(clients[i].socket == socket) {
			clients[i] = clients[--client_count]; // Replace with last and decrement
			break;
		}
	}
	pthread_mutex_unlock(&client_lock);
}
// Send message to all clients except the sender
void broadcast_message(char *msg, int sender_socket) {
	pthread_mutex_lock(&client_lock);

	char final_msg[BUFFER_SIZE + NICKNAME_LEN + 10];
	const char *sender_nick = get_nickname(sender_socket);
	snprintf(final_msg, sizeof(final_msg), "%s: %s", sender_nick, msg);

	for(int i = 0; i < client_count; i++) {
		if (clients[i].socket != sender_socket) {
			write(clients[i].socket, final_msg, strlen(final_msg) + 1);
		}
	}

	pthread_mutex_unlock(&client_lock);
}

// Handle each client in a separate thread
void *handle_client(void *arg) {
	int client_socket =  *((int *) arg);
	free(arg);

	char nickname[NICKNAME_LEN];
	if(read(client_socket, nickname, sizeof(nickname)) <= 0) {
		close(client_socket);
		pthread_exit(NULL);
	}

	nickname[strcspn(nickname, "\n")] = 0; // Remove newline
	add_client(client_socket, nickname);

	char buffer[BUFFER_SIZE];
	while(1) {
		ssize_t bytes_received = read(client_socket, buffer, sizeof(buffer));
		if (bytes_received <= 0) break;
		buffer[strcspn(buffer, "\n")] = 0; // Trim newline

		// Handle "/list" command - show all connected users
		if(strncmp(buffer, "/list", 5) == 0) {
			char list_msg[BUFFER_SIZE] = "[SERVER] Connected users:\n";

			pthread_mutex_lock(&client_lock);
			for(int i = 0; i < client_count; i++) {
				strncat(list_msg, " - ",BUFFER_SIZE - strlen(list_msg) - 1);
				strncat(list_msg, clients[i].nickname, BUFFER_SIZE - strlen(list_msg) - 1);
				strncat(list_msg, "\n", BUFFER_SIZE - strlen(list_msg) - 1);
			}
			pthread_mutex_unlock(&client_lock);

			write(client_socket, list_msg, strlen(list_msg));
			continue;
		}

		// Handle "/quit" command - quit from server
		if(strncmp(buffer, "/quit", 5) == 0) {
			printf("[SERVER] %s has requested to quit.\n", get_nickname(client_socket));
			break;
		}

		// Handle "/help" command
		if(strncmp(buffer, "/help", 5) == 0) {
			const char *help_msg =
				"[SERVER] Available commands:\n"
				"/list		- show all connected users\n"
				"/quit		- disconnect from the chat\n"
				"/help		- show this help message\n";
			write(client_socket, help_msg, strlen(help_msg));
			continue;
		}
		// Handle private message
		if(buffer[0] == '@') {
			char *space_pos = strchr(buffer, ' ');
			if(space_pos == NULL) {
				const char *error = "[SERVER] Invalid private message format. Use @nickname message\n";
				write(client_socket, error, strlen(error));
				continue;
			}

			char target_nickname[NICKNAME_LEN];
			size_t name_len = space_pos - buffer - 1;
			if (name_len >= NICKNAME_LEN) name_len = NICKNAME_LEN - 1;

			strncpy(target_nickname, buffer + 1, name_len);
			target_nickname[name_len] = '\0';

			char *message_content = space_pos + 1;

			// Find recipient socket
			int target_socket = -1;
			pthread_mutex_lock(&client_lock);
			for(int i = 0; i< client_count; i++) {
				if(strcmp(clients[i].nickname, target_nickname) == 0) {
					target_socket = clients[i].socket;
					break;
				}
			}
			pthread_mutex_unlock(&client_lock);

			// if recipient not found
			if(target_socket == -1) {
				char error_msg[100];
				snprintf(error_msg, sizeof(error_msg), "[SERVER] No such user: %s\n", target_nickname);
				write(client_socket, error_msg, strlen(error_msg));
				continue;
			}

			// Send private message to both sender and recipient
			char private_msg[BUFFER_SIZE + NICKNAME_LEN + 20];
			snprintf(private_msg, sizeof(private_msg), "[PRIVATE] %s: %s\n", get_nickname(client_socket), message_content);

			write(target_socket, private_msg, strlen(private_msg));
			write(client_socket, private_msg, strlen(private_msg));

			printf("[SERVER] Received message from %s: %s\n", get_nickname(client_socket), buffer);
		} else {
			// Broadcast public message
			printf("[SERVER] Received message from %s: %s\n", get_nickname(client_socket), buffer);
			broadcast_message(buffer, client_socket);
		}
	}

	// Cleanup after disconnection
	printf("[SERVER] %s disconnected.\n", get_nickname(client_socket));
	remove_client(client_socket);
	close(client_socket);
	pthread_exit(NULL);
}

// Handle CTRL+C interrupt (graceful shutdown)
void handle_sigint(int sig){
	printf("\n[SERVER] Caught Ctrl+C - shutting down... \n");
	close(server_socket);
	exit(0);
}

int main(void) {
	signal(SIGINT, handle_sigint);

	struct sockaddr_in serv_name;
	server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (server_socket < 0) {
		perror("Error creating socket");
		exit(1);
	}

	// Allow address reuse
	int opt = 1;
	setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	// Set up server address
	memset(&serv_name, 0, sizeof(serv_name));
	serv_name.sin_family = AF_INET;
	serv_name.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_name.sin_port = htons(PORT);

	// Bind socket
	if(bind(server_socket, (struct sockaddr *)&serv_name, sizeof(serv_name)) < 0) {
		perror("Error binding socket");
		close(server_socket);
		exit(1);
	}

	// Start socket
	listen(server_socket, 10);
	printf("[SERVER] Listening on port %d...\n", PORT);

	// Accept client in loop
	while(1) {
		struct sockaddr_in client_addr;
		socklen_t client_len = sizeof(client_addr);

		int *client_sock = malloc(sizeof(int));
		if(!client_sock) continue;

		*client_sock = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
		if(*client_sock < 0){
			perror("Error accepting client");
			free(client_sock);
			continue;
		}

		// Create a thread for each client
		pthread_t tid;
		pthread_create(&tid, NULL, handle_client, client_sock);
		pthread_detach(tid);
	}

	close(server_socket);
	return 0;
}


