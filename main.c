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

	node_t *n = triad_init(ip);

	/* CLI thread */
	char *line = NULL;
	while (line = readline("triad> ")) {
		char command[32], arg1[32];
		sscanf(line, "%s %s\n", command, arg1);

		/* quit */
		if (!strcmp(command, "quit")) {
			if (n->status == ST_CONNECTED)
				triad_leave(n);
			triad_deinit(n);
			break;
		}

		/* status */
		else if (!strcmp(command, "status")) {
			rpc_get_status(strtoid(arg1));
		}

		/* join */
		else if (!strcmp(command, "join")) {
			if (n->status == ST_DISCONNECTED)
				triad_join(n, arg1);
			else
				printf("node is already connected to a ring!\n");
		}

		/* leave */
		else if (!strcmp(command, "leave")) {
			triad_leave(n);
		}

		/* lookup */
		else if (!strcmp(command, "lookup")) {
			unsigned int id;
			sscanf(arg1, "%u", &id);
			printf("%u => %15s\n", id, triad_lookup(n, id));
		}

		/* print */
		else if (!strcmp(command, "print")) {
			print_node(n);
		}
		else
			printf("unknown command: %s\n", command);

		command[0] = '\0';
		arg1[0] = '\0';
	}

	free(n);

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
