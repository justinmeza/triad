#include <stdio.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "inet.h"
#include "hash.h"

#define RING_ID 12345

struct hash *rpc_table;

void data_delete_rpc(void *DATA) { }

void rpc_get_status(void)
{
	printf("STATUS = \n");
}

typedef struct msg {
	char kind[32];
} msg_t;

int main(int argc, char **argv)
{
	inet_host_t local;
	inet_open(&local, IN_PROT_TCP, IN_ADDR_ANY, RING_ID);
	char *ip = inet_lookup("localhost");
	printf("%s\n", ip);

	rpc_table = hash_create(data_delete_rpc, 7);

	// RPC test
	hash_insert(rpc_table, "RPC_GET_STATUS", &rpc_get_status);
	void (*fn)(void) = hash_find(rpc_table, "RPC_GET_STATUS");
	fn();

	int id = fork();
	if (id == 0) {
		inet_host_t remote;
		while (1) {
			printf("waiting for node to connect...\n");
			inet_accept(&remote, &local);
			printf("accepted connection\n");
			msg_t data;
			inet_receive(&remote, &local, &data, sizeof(msg_t), -1);
			printf("received message\n");
		}
		return 0;
	}

	char *line = NULL;
	while (line = readline("triad> ")) {
		inet_host_t remote;
		if (!strcmp(line, "quit")) {
			inet_open(&local, IN_PROT_TCP, IN_ADDR_ANY, RING_ID + 1);
			inet_setup(&remote, IN_PROT_TCP, "127.0.0.1", RING_ID);
			inet_connect(&local, &remote);
			break;
		}
	}
	wait();

	hash_delete(rpc_table);

	return 0;
}
