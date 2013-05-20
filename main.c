#include <stdio.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <pthread.h>
#include "inet.h"
#include "triad.h"

int main(int argc, char **argv)
{
	char *ip = inet_lookup(argv[1]);
	printf("IP is: %s\n", ip);

	/* set up node */
	node_t n;
	n.id = strtoid(ip);
	n.successor = n.id;
	n.predecessor = n.id;
	int f;
	for (f = 0; f < KEYSPACE; f++) {
		n.finger_table[f].start = (n.id + (1 << f));
		n.finger_table[f].end = (n.id + (1 << (f + 1)));
		n.finger_table[f].successor = n.id;
	}
	n.status = ST_DISCONNECTED;

	/* RPC thread */
	pthread_t rpc_thread;
	pthread_create(&rpc_thread, NULL, rpc_handler, &n);

	/* CLI thread */
	char *line = NULL;
	while (line = readline("triad> ")) {
		char command[32], arg1[32];
		sscanf(line, "%s %s\n", command, arg1);
		/* quit */
		if (!strcmp(command, "quit")) {
			/* RPC */
			inet_host_t local, remote;
			inet_open(&local, IN_PROT_UDP, IN_ADDR_ANY, COM_PORT);
			inet_setup(&remote, IN_PROT_UDP, ip, RPC_PORT);
			msg_t m;
			m.type = MSG_QUIT;
			inet_send(&local, &remote, &m, sizeof(msg_t));
			msg_t ack;
			inet_receive(&remote, &local, &ack, sizeof(msg_t), -1);
			if (ack.type == MSG_QUIT_ACK)
				printf("received (MSG_QUIT_ACK)\n"), fflush(stdout);
			inet_close(&local);
			break;
		}
		/* status */
		else if (!strcmp(command, "status")) {
			rpc_get_status(strtoid(arg1));
		}
		/* join */
		else if (!strcmp(command, "join")) {
			printf("attempting to join ring at %s...\n", arg1);
			unsigned int id = strtoid(arg1);
			if (rpc_get_status(id) == ST_CONNECTED) {
				init_finger_table(&n, id);
				update_others_join(&n);
				rpc_set_status(n.id, ST_CONNECTED);
				printf("joined an existing ring!\n");
			}
			else {
				int f;
				for (f = 0; f < KEYSPACE; f++) {
					n.finger_table[f].start = (n.id + (1 << f));
					n.finger_table[f].end = (n.id + (1 << (f + 1)));
					n.finger_table[f].successor = n.id;
				}
				n.predecessor = n.id;
				n.successor = n.id;
				rpc_set_status(n.id, ST_CONNECTED);
				printf("started a new ring!\n");
			}
		}
		/* leave */
		else if (!strcmp(command, "leave")) {
			deinit_finger_table(&n);
			update_others_leave(&n);
			int f;
			for (f = 0; f < KEYSPACE; f++) {
				n.finger_table[f].start = (n.id + (1 << f));
				n.finger_table[f].end = (n.id + (1 << (f + 1)));
				n.finger_table[f].successor = n.id;
				n.successor = n.id;
				n.predecessor = n.id;
			}
		}
		/* lookup */
		else if (!strcmp(command, "lookup")) {
			unsigned int id;
			sscanf(arg1, "%u", &id);
			unsigned int node = find_successor(&n, id);
			printf("%u => %10u / %15s\n", id, node, idtostr(node));
		}
		/* print */
		else if (!strcmp(command, "print")) {
			print_node(&n);
		}
		else
			printf("unknown command: %s\n", command);

		command[0] = '\0';
		arg1[0] = '\0';
	}

	printf("waiting for child thread...\n"), fflush(stdout);
	pthread_join(rpc_thread, NULL);

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
