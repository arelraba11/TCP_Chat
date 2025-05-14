#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <pthread.h>

#define PORT 1337
#define MAX_CLIENTS 100
int sock;
int client_sockets[MAX_CLIENTS];
int client_count = 0;
pthread_mutex_t client_lock = PTHREAD_MUTEX_INITIALIZER;

void add_client(int socket) {
	pthread_mutex_lock(&client_lock);
	if(client_count < MAX_CLIENTS) client_sockets[client_count++] = socket;
	pthread_mutex_unlock(&client_lock);
}

void remove_client(int socket) {
	pthread_mutex_lock(&client_lock);
	for(int i = 0; i < client_count; i++) {
		if(client_sockets[i] == socket) {
			client_sockets[i] = client_sockets[--client_count];
			break;
		}
	}
	pthread_mutex_unlock(&client_lock);
}

void broadcast_message(char *msg, int sender_socket) {
	pthread_mutex_lock(&client_lock);
	for(int i = 0; i < client_count; i++) {
		int client_sock = client_sockets[i];
		if (client_sock != sender_socket) write(client_sock, msg, strlen(msg) + 1);
	}
	pthread_mutex_unlock(&client_lock);
}

void *handle_client(void *arg) {
	int client_socket =  *((int *) arg);
	free(arg);
	char buffer[1024];
	add_client(client_socket);

	while(1) {
		ssize_t bytes_received = read(client_socket, buffer, sizeof(buffer));
		if (bytes_received <= 0){
			break;
		}
		printf("\n[SERVER] Received message: %s\n", buffer);
		broadcast_message(buffer, client_socket);
	}
	remove_client(client_socket);

	close(client_socket);
	pthread_exit(NULL);
}

void handle_sigint(int sig){
	printf("\n[SERVER] Caught Ctrl+C - shutting down... \n");
	close(sock);
	exit(0);
}

int main(void) {
	signal(SIGINT, handle_sigint);
	struct sockaddr_in serv_name;

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		perror("Error opening channel");
		close(sock);
		exit(1);
	}

	int opt = 1;
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	bzero(&serv_name, sizeof(serv_name));
	serv_name.sin_family = AF_INET;
	serv_name.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_name.sin_port = htons(PORT);

	if(bind(sock, (struct sockaddr *)&serv_name, sizeof(serv_name)) < 0) {
		perror("Error naming channel");
		close(sock);
		exit(1);
	}

	listen(sock,1);

	while(1) {
		struct sockaddr_in client_addr;
		socklen_t client_len = sizeof(client_addr);

		int *client_sock = malloc(sizeof(int));
		*client_sock = accept(sock, (struct sockaddr *)&client_addr, &client_len);

		if(*client_sock < 0){
			perror("Error accepting client");
			free(client_sock);
			continue;
		}
		pthread_t tid;
		pthread_create(&tid, NULL, handle_client, client_sock);
		pthread_detach(tid);
	}

	close(sock);
	return 0;
}


