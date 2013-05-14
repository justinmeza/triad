#include <stdio.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "inet.h"
#include "hash.h"

#define RPC_PORT 12345
#define COM_PORT 12346

struct hash *rpc_table;

void data_delete_rpc(void *DATA) { }

void rpc_get_status(void)
{
	printf("STATUS = \n");
}

typedef struct msg {
	char type[32];
	char data[32];
} msg_t;

typedef struct node {
	int status;
} node_t;

int main(int argc, char **argv)
{
	/* set up node */
	node_t n;
	n.status = 0;

	char *ip = inet_lookup("localhost");
	printf("IP is: %s\n", ip);

	rpc_table = hash_create(data_delete_rpc, 7);

	hash_insert(rpc_table, "RPC_GET_STATUS", &rpc_get_status);
	void (*fn)(void) = hash_find(rpc_table, "RPC_GET_STATUS");
	fn();

	/* RPC thread */
	int id = fork();
	if (id == 0) {
		inet_host_t local, remote;
		inet_open(&local, IN_PROT_TCP, IN_ADDR_ANY, RPC_PORT);
		while (1) {
			printf("waiting for node to connect...\n"), fflush(stdout);
			inet_accept(&remote, &local);
			printf("accepted connection\n"), fflush(stdout);
			msg_t m;
			if (inet_receive(&remote, &local, &m, sizeof(msg_t), -1) == sizeof(msg_t)) {
				printf("received message (%s)\n", m.type), fflush(stdout);
				if (!strcmp(m.type, "RPC_QUIT")) {
					msg_t ack = { "RPC_QUIT_ACK" };
					printf("quitting...\n"), fflush(stdout);
					inet_send(&local, &remote, &ack, sizeof(msg_t));
					inet_close(&local);
					inet_close(&remote);
					return 0;
				}
				else if (!strcmp(m.type, "RPC_GET_STATUS")) {
					printf("sending status...\n"), fflush(stdout);
				}
			}
		}
		return 0;
	}

	/* CLI thread */
	char *line = NULL;
	while (line = readline("triad> ")) {
		inet_host_t local, remote;
		if (!strcmp(line, "quit")) {
			inet_open(&local, IN_PROT_TCP, IN_ADDR_ANY, COM_PORT);
			inet_setup(&remote, IN_PROT_TCP, ip, RPC_PORT);
			inet_connect(&local, &remote);
			msg_t m = { "RPC_QUIT" };
			inet_send(&local, &remote, &m, sizeof(msg_t));
			msg_t ack;
			inet_receive(&remote, &local, &ack, sizeof(msg_t), -1);
			if (!strcmp(ack.type, "RPC_QUIT_ACK"))
				printf("received RPC_QUIT_ACK\n"), fflush(stdout);
			inet_close(&local);
			break;
		}
	}

	printf("waiting for child thread...\n"), fflush(stdout);
	wait();

	printf("cleaning up...\n"), fflush(stdout);
	hash_delete(rpc_table);

	return 0;
}
