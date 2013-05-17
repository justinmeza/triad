#include <stdio.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "inet.h"

#define RPC_PORT 12345
#define COM_PORT 12346
#define KEYSPACE 32

/**
 * Chord structures
 */

typedef struct finger {
	unsigned int start;
	unsigned int end;
	unsigned int successor;
} finger_t;

typedef struct node {
	int status;
	unsigned int id;
	unsigned int predecessor;
	unsigned int successor;
	finger_t finger_table[KEYSPACE];
} node_t;


/**
 * RPC stuff
 */

typedef enum msg_type {
	MSG_QUIT = 1,
	MSG_QUIT_ACK,
	MSG_GET_STATUS,
	MSG_GET_STATUS_ACK,
	MSG_GET_SUCCESSOR,
	MSG_GET_SUCCESSOR_ACK,
	MSG_GET_PREDECESSOR,
	MSG_GET_PREDECESSOR_ACK,
	MSG_GET_CLOSEST_PRECEDING_FINGER,
	MSG_GET_CLOSEST_PRECEDING_FINGER_ACK,
} msg_type_t;

typedef struct msg {
	msg_type_t type;
	unsigned int data;
} msg_t;

/* forward declarations */
unsigned int get_successor(node_t *, unsigned int);
unsigned int get_closest_preceding_finger(node_t *, unsigned int, unsigned int);

/**
 * helper functions for converting between IP addresses and IDs
 */

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


/**
 * check if a value is within a range with circular wraparound
 */

int in_range_ex_ex_circular(unsigned int a, unsigned int b, unsigned int c)
{
	if (a < b)
		return ((c > a) && (c < b));
	return ((c > a) || (c < b));
}

int in_range_in_in_circular(unsigned int a, unsigned int b, unsigned int c)
{
	if ((c == a) || (c == b))
		return 1;
	if (a < b)
		return ((c >= a) && (c <= b));
	return ((c >= a) || (c <= b));
}

int in_range_in_ex_circular(unsigned int a, unsigned int b, unsigned int c)
{
	if (c == a)
		return 1;
	if (a < b)
		return ((c >= a) && (c < b));
	return ((c >= a) || (c < b));
}

int in_range_ex_in_circular(unsigned int a, unsigned int b, unsigned int c)
{
	if (c == b)
		return 1;
	if (a < b)
		return ((c > a) && (c <= b));
	return ((c > a) || (c <= b));
}


/**
 * Chord functions and helper functions
 */

unsigned int closest_preceding_finger(node_t *n, unsigned int id)
{
	int i;
	for (i = (KEYSPACE - 1); i >= 0; i--) {
		unsigned int check = n->finger_table[i].successor;
		if (in_range_ex_ex_circular(n->id, id, check))
			return check;
	}
	return n->id;
}

unsigned int find_predecessor(node_t *n, unsigned int id)
{
	if (in_range_in_ex_circular(n->id, n->successor, id))
		return n->id;
	unsigned int i = n->id;
	while (!in_range_ex_in_circular(i, get_successor(n, i), id))
		i = get_closest_preceding_finger(n, i, id);
	return i;
}

unsigned int find_successor(node_t *n, unsigned int id)
{
	// if this node is the successor
	if (in_range_ex_in_circular(n->predecessor, n->id, id))
		return n->id;
	unsigned int p = find_predecessor(n, id);
	return get_successor(n, p);
}

void init_finger_table(node_t *n)
{
	int f;
	for (f = 0; f < KEYSPACE; f++) {
		n->finger_table[f].start = (n->id + (1 << f));
		n->finger_table[f].end = (n->id + (1 << (f + 1)));
	}
}

void print_node(node_t *n)
{
	int f;
	printf("id: %u (%s)\n", n->id, idtostr(n->id));
	for (f = 0; f < KEYSPACE; f++) {
		printf("finger %2d [ %10u / %s, %10u / %s )\n", f, n->finger_table[f].start, idtostr(n->finger_table[f].start), n->finger_table[f].end, idtostr(n->finger_table[f].end));
	}
}


/**
 * RPC wrapper functions
 */

unsigned int get_successor(node_t *n, unsigned int id)
{
	unsigned int ret;
	if (id == n->id)
		return n->successor;
	char *ip = idtostr(id);
	/* RPC */
	inet_host_t local, remote;
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
		ret = ack.data;
	}
	inet_close(&local);
	free(ip);
	return ret;
}

unsigned int get_closest_preceding_finger(node_t *n, unsigned int i, unsigned int id)
{
	unsigned int ret;
	if (i == n->id)
		return closest_preceding_finger(n, id);
	char *ip = idtostr(i);
	/* RPC */
	inet_host_t local, remote;
	inet_open(&local, IN_PROT_TCP, IN_ADDR_ANY, COM_PORT);
	inet_setup(&remote, IN_PROT_TCP, ip, RPC_PORT);
	inet_connect(&local, &remote);
	msg_t m;
	m.type = MSG_GET_CLOSEST_PRECEDING_FINGER;
	m.data = id;
	inet_send(&local, &remote, &m, sizeof(msg_t));
	msg_t ack;
	inet_receive(&remote, &local, &ack, sizeof(msg_t), -1);
	if (ack.type == MSG_GET_CLOSEST_PRECEDING_FINGER_ACK) {
		printf("received (RPC_GET_CLOSEST_PRECEDING_FINGER_ACK)\n"), fflush(stdout);
		printf("CLOSEST_PRECEDING_FINGER = %d\n", ack.data), fflush(stdout);
		ret = ack.data;
	}
	inet_close(&local);
	free(ip);
	return ret;
}

int main(int argc, char **argv)
{
	char *ip = inet_lookup("localhost");
	printf("IP is: %s\n", ip);

	/* set up node */
	node_t n;
	n.id = strtoid(ip);
	n.successor = n.id;
	n.predecessor = n.id;
	n.status = 0;

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
							ack.type = MSG_GET_STATUS_ACK;
							ack.data = n.status;
							inet_send(&local, &remote, &ack, sizeof(msg_t));
							break;
						}
					case MSG_GET_SUCCESSOR:
						{
							ack.type = MSG_GET_SUCCESSOR_ACK;
							ack.data = n.successor;
							inet_send(&local, &remote, &ack, sizeof(msg_t));
							break;
						}
					case MSG_GET_PREDECESSOR:
						{
							ack.type = MSG_GET_PREDECESSOR_ACK;
							ack.data = n.predecessor;
							inet_send(&local, &remote, &ack, sizeof(msg_t));
							break;
						}
					case MSG_GET_CLOSEST_PRECEDING_FINGER:
						{
							ack.type = MSG_GET_CLOSEST_PRECEDING_FINGER_ACK;
							ack.data = closest_preceding_finger(&n, m.data);
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
		/* quit */
		if (!strcmp(line, "quit")) {
			/* RPC */
			inet_host_t local, remote;
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
			/* RPC */
			inet_host_t local, remote;
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
		/* successor */
		else if (!strcmp(line, "successor")) {
			/* RPC */
			inet_host_t local, remote;
			inet_open(&local, IN_PROT_TCP, IN_ADDR_ANY, COM_PORT);
			inet_setup(&remote, IN_PROT_TCP, ip, RPC_PORT);
			inet_connect(&local, &remote);
			msg_t m;
			m.type = MSG_GET_SUCCESSOR;
			inet_send(&local, &remote, &m, sizeof(msg_t));
			msg_t ack;
			inet_receive(&remote, &local, &ack, sizeof(msg_t), -1);
			if (ack.type == MSG_GET_SUCCESSOR_ACK) {
				printf("received (RPC_GET_SUCCESSOR_ACK)\n"), fflush(stdout);
				printf("SUCCESSOR = %d\n", ack.data), fflush(stdout);
			}
			inet_close(&local);
		}
		/* predecessor */
		else if (!strcmp(line, "predecessor")) {
			/* RPC */
			inet_host_t local, remote;
			inet_open(&local, IN_PROT_TCP, IN_ADDR_ANY, COM_PORT);
			inet_setup(&remote, IN_PROT_TCP, ip, RPC_PORT);
			inet_connect(&local, &remote);
			msg_t m;
			m.type = MSG_GET_PREDECESSOR;
			inet_send(&local, &remote, &m, sizeof(msg_t));
			msg_t ack;
			inet_receive(&remote, &local, &ack, sizeof(msg_t), -1);
			if (ack.type == MSG_GET_PREDECESSOR_ACK) {
				printf("received (RPC_GET_PREDECESSOR_ACK)\n"), fflush(stdout);
				printf("PREDECESSOR = %d\n", ack.data), fflush(stdout);
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

	init_finger_table(&n);
	print_node(&n);

	return 0;
}
