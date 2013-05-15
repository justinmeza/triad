#include <stdio.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "inet.h"

#define RPC_PORT 12345
#define COM_PORT 12346

typedef enum msg_type {
	MSG_QUIT = 1,
	MSG_QUIT_ACK,
	MSG_GET_STATUS,
	MSG_GET_STATUS_ACK,
} msg_type_t;

typedef struct msg {
	msg_type_t type;
	int data;
} msg_t;

typedef struct node {
	int status;
	int id;
} node_t;

int strtoid(const char *str)
{
	int o1, o2, o3, o4;
	sscanf(str, "%u.%u.%u.%u", &o1, &o2, &o3, &o4);
	return (o1 << 24) + (o2 << 16) + (o3 << 8) + o4;
}

char *idtostr(int id)
{
	unsigned char o[4];
	o[0] = id & 0xff;
	o[1] = (id >> 8) & 0xff;
	o[2] = (id >> 16) & 0xff;
	o[3] = (id >> 24) & 0xff;
	char *ret = malloc(sizeof(char) * ((3 * 4) + 4 + 1));  // 4 octets + 4 dots + 1 null char
	sprintf(ret, "%u.%u.%u.%u\0", o[3], o[2], o[1], o[0]);
	return ret;
}

void rpc_get_status(node_t *node, msg_t *msg)
{
	msg->type = MSG_GET_STATUS_ACK;
	msg->data = node->status;
}

int main(int argc, char **argv)
{
	/* set up node */
	node_t n;
	n.status = 0;

	char *ip = inet_lookup("localhost");
	printf("IP is: %s\n", ip);

	/* RPC thread */
	int id = fork();
	if (id == 0) {
		inet_host_t local, remote;
		while (1) {
			inet_open(&local, IN_PROT_TCP, IN_ADDR_ANY, RPC_PORT);
			printf("waiting for node to connect...\n"), fflush(stdout);
			inet_accept(&remote, &local);
			printf("accepted connection\n"), fflush(stdout);
			msg_t m;
			if (inet_receive(&remote, &local, &m, sizeof(msg_t), -1) == sizeof(msg_t)) {
				printf("received message (%d)\n", m.type), fflush(stdout);
				msg_t ack;
				switch (m.type) {
					case MSG_QUIT:
						{
							ack.type = MSG_QUIT_ACK;
							printf("quitting...\n"), fflush(stdout);
							inet_send(&local, &remote, &ack, sizeof(msg_t));
							inet_close(&local);
							inet_close(&remote);
							return 0;
						}
					case MSG_GET_STATUS:
						{
							rpc_get_status(&n, &ack);
							inet_send(&local, &remote, &ack, sizeof(msg_t));
							break;
						}
				}
			}
			inet_close(&local);
			inet_close(&remote);
		}
		return 0;
	}

	/* CLI thread */
	char *line = NULL;
	while (line = readline("triad> ")) {
		inet_host_t local, remote;
		/* quit */
		if (!strcmp(line, "quit")) {
			inet_open(&local, IN_PROT_TCP, IN_ADDR_ANY, COM_PORT);
			inet_setup(&remote, IN_PROT_TCP, ip, RPC_PORT);
			inet_connect(&local, &remote);
			msg_t m;
			m.type = MSG_QUIT;
			inet_send(&local, &remote, &m, sizeof(msg_t));
			msg_t ack;
			inet_receive(&remote, &local, &ack, sizeof(msg_t), -1);
			if (ack.type == MSG_QUIT_ACK)
				printf("received (RPC_QUIT_ACK)\n"), fflush(stdout);
			inet_close(&local);
			break;
		}
		/* status */
		else if (!strcmp(line, "status")) {
			inet_open(&local, IN_PROT_TCP, IN_ADDR_ANY, COM_PORT);
			inet_setup(&remote, IN_PROT_TCP, ip, RPC_PORT);
			inet_connect(&local, &remote);
			msg_t m;
			m.type = MSG_GET_STATUS;
			inet_send(&local, &remote, &m, sizeof(msg_t));
			msg_t ack;
			inet_receive(&remote, &local, &ack, sizeof(msg_t), -1);
			if (ack.type == MSG_GET_STATUS_ACK) {
				printf("received (RPC_GET_STATUS_ACK)\n"), fflush(stdout);
				printf("STATUS = %d\n", ack.data), fflush(stdout);
			}
			inet_close(&local);
		}
	}

	printf("waiting for child thread...\n"), fflush(stdout);
	wait();

	/*
	printf("id: %u = str: %s\n", 0, idtostr(0));
	printf("id: %u = str: %s\n", 12345678, idtostr(12345678));
	printf("id: %u = str: %s\n", 2130706433, idtostr(2130706433));
	printf("id: %u = str: %s\n", 4294967295u, idtostr(4294967295u));
	printf("str: %s = id: %u\n", "0.0.0.0", strtoid("0.0.0.0"));
	printf("str: %s = id: %u\n", "127.0.0.1", strtoid("127.0.0.1"));
	printf("str: %s = id: %u\n", "255.255.255.255", strtoid("255.255.255.255"));
	*/

	printf("done!\n"), fflush(stdout);

	return 0;
}
