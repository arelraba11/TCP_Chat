#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

#define PORT 1337
#define NICKNAME_LEN 50
#define BUFFER_SIZE 1024

void *receive_handler(void *arg) {
	int sock = *(int *) arg;
	char buffer[BUFFER_SIZE];

	while(1) {
		ssize_t n = read(sock, buffer, sizeof(buffer));
		if (n <= 0) {
			printf("[CLIENT] Disconnected from server. Exiting... \n");
			close(sock);
			exit(0);
		}
		printf("[CHAT] %s\n", buffer);
	}
	return NULL;
}

int main(void) {
	int sock;
	struct sockaddr_in server_name;
	char buffer[BUFFER_SIZE];
	char nickname[NICKNAME_LEN];

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		perror("[CLIENT] Error opening channel");
		exit(1);
	}

	memset(&server_name, 0, sizeof(server_name));
	server_name.sin_family = AF_INET;
	server_name.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // 127.0.0.1
	server_name.sin_port = htons(PORT);

	printf("Please enter your nickname: ");
	if(fgets(nickname, sizeof(nickname), stdin) == NULL) {
		printf("[CLIENT] Failed to read nickname. Exiting...\n");
		exit(1);
	}
	nickname[strcspn(nickname, "\n")] = 0;

	if (strlen(nickname) == 0) {
		printf("[CLIENT] Invaild nickname. Exiting..\n");
		exit(1);
	}

	if(connect(sock, (struct sockaddr *)&server_name, sizeof(server_name)) < 0) {
		perror("[CLIENT] Error establishing connection to server");
		exit(1);
	}

	if (write(sock,nickname, strlen(nickname) + 1) <= 0) {
		perror("[CLIENT] Failed to send nickname to server");
		close(sock);
		exit(1);
	}

	pthread_t tid;
	pthread_create(&tid, NULL, receive_handler, &sock);

	while (fgets(buffer, sizeof(buffer), stdin) != NULL) {
		if(write(sock, buffer, strlen(buffer) + 1) <= 0) {
			printf("[CLIENT] Failed to send message. Exiting...\n");
			break;
		}
	}

	close(sock);
	return 0;
}
