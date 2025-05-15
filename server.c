#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

#define PORT 1337
#define MAX_CLIENTS 100
#define NICKNAME_LEN 50
#define BUFFER_SIZE 1024

typedef struct {
	int socket;
	char nickname[50];
} Client;

Client clients[MAX_CLIENTS];
int client_count = 0;
int server_socket;
pthread_mutex_t client_lock = PTHREAD_MUTEX_INITIALIZER;

const char* get_nickname(int socket) {
	for(int i = 0; i < client_count; i++) {
		if(clients[i].socket == socket) {
			return clients[i].nickname;
		}
	}
	return "Unknown";
}

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

void remove_client(int socket) {
	pthread_mutex_lock(&client_lock);
	for(int i = 0; i < client_count; i++) {
		if(clients[i].socket == socket) {
			clients[i] = clients[--client_count];
			break;
		}
	}
	pthread_mutex_unlock(&client_lock);
}

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

void *handle_client(void *arg) {
	int client_socket =  *((int *) arg);
	free(arg);

	char nickname[NICKNAME_LEN];
	if(read(client_socket, nickname, sizeof(nickname)) <= 0) {
		close(client_socket);
		pthread_exit(NULL);
	}

	nickname[strcspn(nickname, "\n")] = 0;
	add_client(client_socket, nickname);

	char buffer[BUFFER_SIZE];
	while(1) {
		ssize_t bytes_received = read(client_socket, buffer, sizeof(buffer));
		if (bytes_received <= 0) break;
		printf("[SERVER] Received message from %s: %s\n", get_nickname(client_socket), buffer);
		broadcast_message(buffer, client_socket);
	}

	printf("[SERVER] %s disconnected.\n", get_nickname(client_socket));
	remove_client(client_socket);
	close(client_socket);
	pthread_exit(NULL);
}

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

	int opt = 1;
	setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	memset(&serv_name, 0, sizeof(serv_name));
	serv_name.sin_family = AF_INET;
	serv_name.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_name.sin_port = htons(PORT);

	if(bind(server_socket, (struct sockaddr *)&serv_name, sizeof(serv_name)) < 0) {
		perror("Error binding socket");
		close(server_socket);
		exit(1);
	}

	listen(server_socket, 10);
	printf("[SERVER] Listening on port %d...\n", PORT);

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

		pthread_t tid;
		pthread_create(&tid, NULL, handle_client, client_sock);
		pthread_detach(tid);
	}

	close(server_socket);
	return 0;
}


