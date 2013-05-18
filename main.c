#include <stdio.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <pthread.h>
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

typedef enum status {
	ST_DISCONNECTED = 0,
	ST_CONNECTED,
} status_t;

typedef struct node {
	status_t status;
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
	MSG_SET_STATUS,
	MSG_SET_STATUS_ACK,
	MSG_GET_SUCCESSOR,
	MSG_GET_SUCCESSOR_ACK,
	MSG_SET_SUCCESSOR,
	MSG_SET_SUCCESSOR_ACK,
	MSG_GET_PREDECESSOR,
	MSG_GET_PREDECESSOR_ACK,
	MSG_SET_PREDECESSOR,
	MSG_SET_PREDECESSOR_ACK,
	MSG_GET_CLOSEST_PRECEDING_FINGER,
	MSG_GET_CLOSEST_PRECEDING_FINGER_ACK,
	MSG_FIND_SUCCESSOR,
	MSG_FIND_SUCCESSOR_ACK,
	MSG_FIND_PREDECESSOR,
	MSG_FIND_PREDECESSOR_ACK,
	MSG_UPDATE_FINGER_TABLE,
	MSG_UPDATE_FINGER_TABLE_ACK,
} msg_type_t;

typedef struct msg {
	msg_type_t type;
	unsigned int data;
	unsigned int data2;  // TODO:  make this not a hack
} msg_t;

/* forward declarations */
unsigned int rpc_get_status(unsigned int);
unsigned int rpc_get_successor(unsigned int);
int rpc_set_successor(unsigned int, unsigned int);
unsigned int rpc_get_predecessor(unsigned int);
int rpc_set_predecessor(unsigned int, unsigned int);
unsigned int rpc_get_closest_preceding_finger(unsigned int, unsigned int);
unsigned int rpc_find_successor(unsigned int, unsigned int);
int rpc_update_finger_table(unsigned int, unsigned int, unsigned int);

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
 * circular membership tests
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
	while (!in_range_ex_in_circular(i, rpc_get_successor(i), id))
		i = rpc_get_closest_preceding_finger(i, id);
	return i;
}

unsigned int find_successor(node_t *n, unsigned int id)
{
	// if this node is the successor
	if (in_range_ex_in_circular(n->predecessor, n->id, id))
		return n->id;
	unsigned int p = find_predecessor(n, id);
	return rpc_get_successor(p);
}

void init_finger_table(node_t *n, unsigned int remote)
{
	int f;
	for (f = 0; f < KEYSPACE; f++) {
		n->finger_table[f].start = (n->id + (1 << f));
		n->finger_table[f].end = (n->id + (1 << (f + 1)));
	}
	unsigned int successor = rpc_find_successor(remote, n->finger_table[0].start);
	n->finger_table[0].successor = successor;
	n->successor = successor;
	n->predecessor = rpc_get_predecessor(successor);
	rpc_set_predecessor(n->successor, n->id);
	rpc_set_successor(n->predecessor, n->id);
	for (f = 0; f < (KEYSPACE - 1); f++) {
		if (in_range_in_ex_circular(n->id, n->finger_table[f].successor, n->finger_table[f + 1].start))
			n->finger_table[f + 1].successor = n->finger_table[f].successor;
		else
			n->finger_table[f + 1].successor = rpc_find_successor(remote, n->finger_table[f + 1].start);
	}
}

void update_finger_table(node_t *n, int f, unsigned int id)
{
	if (in_range_ex_ex_circular(n->id, n->finger_table[f].successor, id)) {
		n->finger_table[f].successor = id;
		if (f == 0)
			n->successor = id;
		unsigned int p = n->predecessor;
		if (p == n->id)
			update_finger_table(n, f, id);
		else
			rpc_update_finger_table(p, f, id);
	}
}

void update_others(node_t *n)
{
	int f;
	for (f = 0; f < KEYSPACE; f++) {
		unsigned int temp = n->id - (1 << f);
		unsigned int p = find_predecessor(n, (n->id - (1 << f)));
		printf("finger %d (checking status of %10u / %15s)\n", f, temp, idtostr(temp)), fflush(stdout);
		status_t status = ((temp == n->id) ? n->status : rpc_get_status(temp));
		if (status == ST_CONNECTED)
			p = temp;
		rpc_update_finger_table(p, f, n->id);
	}
}

void print_node(node_t *n)
{
	int f;
	printf("         id: %u (%s)\n", n->id, idtostr(n->id));
	printf("predecessor: %u (%s)\n", n->predecessor, idtostr(n->predecessor));
	printf("  successor: %u (%s)\n", n->successor, idtostr(n->successor));
	for (f = 0; f < KEYSPACE; f++) {
		printf("finger %2d range: [ %10u / %15s, %10u / %15s ) successor: %10u / %15s\n", f, n->finger_table[f].start, idtostr(n->finger_table[f].start), n->finger_table[f].end, idtostr(n->finger_table[f].end), n->finger_table[f].successor, idtostr(n->finger_table[f].successor));
	}
}


/**
 * RPC wrapper functions
 */

unsigned int rpc_get_status(unsigned int id)
{
	unsigned int ret;
	char *ip = idtostr(id);
	/* RPC */
	inet_host_t local, remote;
	inet_open(&local, IN_PROT_UDP, IN_ADDR_ANY, COM_PORT);
	inet_setup(&remote, IN_PROT_UDP, ip, RPC_PORT);
	msg_t m;
	m.type = MSG_GET_STATUS;
	inet_send(&local, &remote, &m, sizeof(msg_t));
	msg_t ack;
	inet_receive(&remote, &local, &ack, sizeof(msg_t), 1);
	if (ack.type == MSG_GET_STATUS_ACK) {
		printf("received (MSG_GET_STATUS_ACK)\n"), fflush(stdout);
		printf("STATUS = %d\n", ack.data), fflush(stdout);
		ret = ack.data;
	}
	inet_close(&local);
	free(ip);
	return ret;
}

int rpc_set_status(unsigned int id, status_t status)
{
	unsigned int ret = 0;
	char *ip = idtostr(id);
	/* RPC */
	inet_host_t local, remote;
	inet_open(&local, IN_PROT_UDP, IN_ADDR_ANY, COM_PORT);
	inet_setup(&remote, IN_PROT_UDP, ip, RPC_PORT);
	msg_t m;
	m.type = MSG_SET_STATUS;
	m.data = status;
	inet_send(&local, &remote, &m, sizeof(msg_t));
	msg_t ack;
	inet_receive(&remote, &local, &ack, sizeof(msg_t), -1);
	if (ack.type == MSG_SET_STATUS_ACK) {
		printf("received (MSG_SET_STATUS_ACK)\n"), fflush(stdout);
		ret = 1;
	}
	inet_close(&local);
	free(ip);
	return ret;
}

unsigned int rpc_get_successor(unsigned int id)
{
	unsigned int ret;
	char *ip = idtostr(id);
	/* RPC */
	inet_host_t local, remote;
	inet_open(&local, IN_PROT_UDP, IN_ADDR_ANY, COM_PORT);
	inet_setup(&remote, IN_PROT_UDP, ip, RPC_PORT);
	msg_t m;
	m.type = MSG_GET_SUCCESSOR;
	inet_send(&local, &remote, &m, sizeof(msg_t));
	msg_t ack;
	inet_receive(&remote, &local, &ack, sizeof(msg_t), -1);
	if (ack.type == MSG_GET_SUCCESSOR_ACK) {
		printf("received (MSG_GET_SUCCESSOR_ACK)\n"), fflush(stdout);
		printf("SUCCESSOR = %u\n", ack.data), fflush(stdout);
		ret = ack.data;
	}
	inet_close(&local);
	free(ip);
	return ret;
}

int rpc_set_successor(unsigned int id, unsigned int successor)
{
	unsigned int ret = 0;
	char *ip = idtostr(id);
	/* RPC */
	inet_host_t local, remote;
	inet_open(&local, IN_PROT_UDP, IN_ADDR_ANY, COM_PORT);
	inet_setup(&remote, IN_PROT_UDP, ip, RPC_PORT);
	msg_t m;
	m.type = MSG_SET_SUCCESSOR;
	m.data = successor;
	inet_send(&local, &remote, &m, sizeof(msg_t));
	msg_t ack;
	inet_receive(&remote, &local, &ack, sizeof(msg_t), -1);
	if (ack.type == MSG_SET_SUCCESSOR_ACK) {
		printf("received (RPC_SET_SUCCESSOR_ACK)\n"), fflush(stdout);
		ret = 1;
	}
	inet_close(&local);
	free(ip);
	return ret;
}

unsigned int rpc_get_predecessor(unsigned int id)
{
	unsigned int ret;
	char *ip = idtostr(id);
	/* RPC */
	inet_host_t local, remote;
	inet_open(&local, IN_PROT_UDP, IN_ADDR_ANY, COM_PORT);
	inet_setup(&remote, IN_PROT_UDP, ip, RPC_PORT);
	msg_t m;
	m.type = MSG_GET_PREDECESSOR;
	inet_send(&local, &remote, &m, sizeof(msg_t));
	msg_t ack;
	inet_receive(&remote, &local, &ack, sizeof(msg_t), -1);
	if (ack.type == MSG_GET_PREDECESSOR_ACK) {
		printf("received (MSG_GET_PREDECESSOR_ACK)\n"), fflush(stdout);
		printf("PREDECESSOR = %u\n", ack.data), fflush(stdout);
		ret = ack.data;
	}
	inet_close(&local);
	free(ip);
	return ret;
}

int rpc_set_predecessor(unsigned int id, unsigned int predecessor)
{
	unsigned int ret = 0;
	char *ip = idtostr(id);
	/* RPC */
	inet_host_t local, remote;
	inet_open(&local, IN_PROT_UDP, IN_ADDR_ANY, COM_PORT);
	inet_setup(&remote, IN_PROT_UDP, ip, RPC_PORT);
	msg_t m;
	m.type = MSG_SET_PREDECESSOR;
	m.data = predecessor;
	inet_send(&local, &remote, &m, sizeof(msg_t));
	msg_t ack;
	inet_receive(&remote, &local, &ack, sizeof(msg_t), -1);
	if (ack.type == MSG_SET_PREDECESSOR_ACK) {
		printf("received (RPC_SET_PREDECESSOR_ACK)\n"), fflush(stdout);
		ret = 1;
	}
	inet_close(&local);
	free(ip);
	return ret;
}

unsigned int rpc_get_closest_preceding_finger(unsigned int node, unsigned int id)
{
	unsigned int ret;
	char *ip = idtostr(node);
	/* RPC */
	inet_host_t local, remote;
	inet_open(&local, IN_PROT_UDP, IN_ADDR_ANY, COM_PORT);
	inet_setup(&remote, IN_PROT_UDP, ip, RPC_PORT);
	msg_t m;
	m.type = MSG_GET_CLOSEST_PRECEDING_FINGER;
	m.data = id;
	inet_send(&local, &remote, &m, sizeof(msg_t));
	msg_t ack;
	inet_receive(&remote, &local, &ack, sizeof(msg_t), -1);
	if (ack.type == MSG_GET_CLOSEST_PRECEDING_FINGER_ACK) {
		printf("received (MSG_GET_CLOSEST_PRECEDING_FINGER_ACK)\n"), fflush(stdout);
		printf("CLOSEST_PRECEDING_FINGER = %u\n", ack.data), fflush(stdout);
		ret = ack.data;
	}
	inet_close(&local);
	free(ip);
	return ret;
}

unsigned int rpc_find_predecessor(unsigned int node, unsigned int id)
{
	unsigned int ret;
	char *ip = idtostr(node);
	/* RPC */
	inet_host_t local, remote;
	inet_open(&local, IN_PROT_UDP, IN_ADDR_ANY, COM_PORT);
	inet_setup(&remote, IN_PROT_UDP, ip, RPC_PORT);
	msg_t m;
	m.type = MSG_FIND_PREDECESSOR;
	m.data = id;
	inet_send(&local, &remote, &m, sizeof(msg_t));
	msg_t ack;
	inet_receive(&remote, &local, &ack, sizeof(msg_t), -1);
	if (ack.type == MSG_FIND_PREDECESSOR_ACK) {
		printf("received (MSG_FIND_PREDECESSOR_ACK)\n"), fflush(stdout);
		printf("PREDECESSOR(%u) = %u\n", id, ack.data), fflush(stdout);
		ret = ack.data;
	}
	inet_close(&local);
	free(ip);
	return ret;
}

unsigned int rpc_find_successor(unsigned int node, unsigned int id)
{
	unsigned int ret;
	char *ip = idtostr(node);
	/* RPC */
	inet_host_t local, remote;
	inet_open(&local, IN_PROT_UDP, IN_ADDR_ANY, COM_PORT);
	inet_setup(&remote, IN_PROT_UDP, ip, RPC_PORT);
	msg_t m;
	m.type = MSG_FIND_SUCCESSOR;
	m.data = id;
	inet_send(&local, &remote, &m, sizeof(msg_t));
	msg_t ack;
	inet_receive(&remote, &local, &ack, sizeof(msg_t), -1);
	if (ack.type == MSG_FIND_SUCCESSOR_ACK) {
		printf("received (MSG_FIND_SUCCESSOR_ACK)\n"), fflush(stdout);
		printf("SUCCESSOR(%u) = %u\n", id, ack.data), fflush(stdout);
		ret = ack.data;
	}
	inet_close(&local);
	free(ip);
	return ret;
}

int rpc_update_finger_table(unsigned int p, unsigned int f, unsigned int id)
{
	unsigned int ret = 0;
	char *ip = idtostr(p);
	/* RPC */
	inet_host_t local, remote;
	inet_open(&local, IN_PROT_UDP, IN_ADDR_ANY, COM_PORT);
	inet_setup(&remote, IN_PROT_UDP, ip, RPC_PORT);
	msg_t m;
	m.type = MSG_UPDATE_FINGER_TABLE;
	m.data = f;
	m.data2 = id;
	inet_send(&local, &remote, &m, sizeof(msg_t));
	msg_t ack;
	inet_receive(&remote, &local, &ack, sizeof(msg_t), -1);
	if (ack.type == MSG_UPDATE_FINGER_TABLE_ACK) {
		printf("received (MSG_UPDATE_FINGER_TABLE_ACK)\n"), fflush(stdout);
		ret = 1;
	}
	inet_close(&local);
	free(ip);
	return ret;
}

void *rpc_handler(void *data)
{
	node_t *n = (node_t *)data;
	inet_host_t local, remote;
	while (1) {
		inet_open(&local, IN_PROT_UDP, IN_ADDR_ANY, RPC_PORT);
		printf("waiting for node to connect...\n"), fflush(stdout);
		msg_t m;
		if (inet_receive(&remote, &local, &m, sizeof(msg_t), -1) == sizeof(msg_t)) {
			printf("received message (%d)\n", m.type), fflush(stdout);
			msg_t ack;
			switch (m.type) {
				case MSG_QUIT:
					printf("received (MSG_QUIT)\n"), fflush(stdout);
					{
						ack.type = MSG_QUIT_ACK;
						printf("quitting...\n"), fflush(stdout);
						inet_send(&local, &remote, &ack, sizeof(msg_t));
						inet_close(&local);
						pthread_exit(0);
					}
				case MSG_GET_STATUS:
					printf("received (MSG_GET_STATUS)\n"), fflush(stdout);
					{
						ack.type = MSG_GET_STATUS_ACK;
						ack.data = n->status;
						inet_send(&local, &remote, &ack, sizeof(msg_t));
						break;
					}
				case MSG_SET_STATUS:
					printf("received (MSG_SET_STATUS)\n"), fflush(stdout);
					{
						n->status = m.data;
						ack.type = MSG_SET_STATUS_ACK;
						inet_send(&local, &remote, &ack, sizeof(msg_t));
						break;
					}
				case MSG_GET_SUCCESSOR:
					printf("received (MSG_GET_SUCCESSOR)\n"), fflush(stdout);
					{
						ack.type = MSG_GET_SUCCESSOR_ACK;
						ack.data = n->successor;
						inet_send(&local, &remote, &ack, sizeof(msg_t));
						break;
					}
				case MSG_SET_SUCCESSOR:
					printf("received (MSG_SET_SUCCESSOR)\n"), fflush(stdout);
					{
						n->successor = m.data;
						ack.type = MSG_SET_SUCCESSOR_ACK;
						inet_send(&local, &remote, &ack, sizeof(msg_t));
						break;
					}
				case MSG_GET_PREDECESSOR:
					printf("received (MSG_GET_PREDECESSOR)\n"), fflush(stdout);
					{
						ack.type = MSG_GET_PREDECESSOR_ACK;
						ack.data = n->predecessor;
						inet_send(&local, &remote, &ack, sizeof(msg_t));
						break;
					}
				case MSG_SET_PREDECESSOR:
					printf("received (MSG_SET_PREDECESSOR)\n"), fflush(stdout);
					{
						n->predecessor = m.data;
						ack.type = MSG_SET_PREDECESSOR_ACK;
						inet_send(&local, &remote, &ack, sizeof(msg_t));
						break;
					}
				case MSG_GET_CLOSEST_PRECEDING_FINGER:
					printf("received (MSG_GET_CLOSEST_PRECEDING_FINGER)\n"), fflush(stdout);
					{
						ack.type = MSG_GET_CLOSEST_PRECEDING_FINGER_ACK;
						ack.data = closest_preceding_finger(n, m.data);
						inet_send(&local, &remote, &ack, sizeof(msg_t));
						break;
					}
				case MSG_FIND_SUCCESSOR:
					printf("received (MSG_FIND_SUCCESSOR)\n"), fflush(stdout);
					{
						ack.type = MSG_FIND_SUCCESSOR_ACK;
						ack.data = find_successor(n, m.data);
						inet_send(&local, &remote, &ack, sizeof(msg_t));
						break;
					}
				case MSG_FIND_PREDECESSOR:
					printf("received (MSG_FIND_PREDECESSOR)\n"), fflush(stdout);
					{
						ack.type = MSG_FIND_PREDECESSOR_ACK;
						ack.data = find_predecessor(n, m.data);
						inet_send(&local, &remote, &ack, sizeof(msg_t));
						break;
					}
				case MSG_UPDATE_FINGER_TABLE:
					printf("received (MSG_UPDATE_FINGER_TABLE)\n"), fflush(stdout);
					{
						update_finger_table(n, m.data, m.data2);
						ack.type = MSG_UPDATE_FINGER_TABLE_ACK;
						inet_send(&local, &remote, &ack, sizeof(msg_t));
						break;
					}
			}
		}
		inet_close(&local);
	}
}

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
				printf("received (RPC_QUIT_ACK)\n"), fflush(stdout);
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
				update_others(&n);
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
