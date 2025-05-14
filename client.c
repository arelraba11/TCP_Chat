#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

#define PORT 1337

void *receive_handler(void *arg) {
	int sock = *(int *) arg;
	char buffer[1024];

	while(1) {
		ssize_t n = read(sock, buffer, sizeof(buffer));
		if (n <= 0) {
			printf("[CLIENT] Disconnected from server. Exiting... \n");
			close(sock);
			exit(0);
		}
		printf("[CHAT] %s", buffer);
	}
	return NULL;
}

int main(void) {
	int sock;
	struct sockaddr_in server_name;
	size_t len;
	char buffer[1024];
	char nickname[50];

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		perror("Error opening channel");
		exit(1);
	}

	bzero(&server_name, sizeof(server_name));
	server_name.sin_family = AF_INET;
	server_name.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // 127.0.0.1
	server_name.sin_port = htons(PORT);

	printf("Please enter your nickname: ");
	fgets(nickname, sizeof(nickname), stdin);

	if (strlen(nickname) == 0) {
		printf("Invaild nickname. Exiting..\n");
		exit(1);
	}

	nickname[strcspn(nickname, "\n")] = 0;

	if(connect(sock, (struct sockaddr *)&server_name, sizeof(server_name)) < 0) {
		perror("Error establishing communications");
		exit(1);
	}

	write(sock, nickname, strlen(nickname) + 1);

	pthread_t tid;
	pthread_create(&tid, NULL, receive_handler, &sock);

	while (1) {
		if(fgets(buffer, sizeof(buffer), stdin) == NULL) break;
		if(write(sock, buffer, strlen(buffer) + 1) <= 0) break;
	}

	close(sock);
	return 0;
}
