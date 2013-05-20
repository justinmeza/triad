#ifndef __TRIAD_H__
#define __TRIAD_H__

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
	MSG_UPDATE_FINGER_TABLE_JOIN,
	MSG_UPDATE_FINGER_TABLE_JOIN_ACK,
	MSG_UPDATE_FINGER_TABLE_LEAVE,
	MSG_UPDATE_FINGER_TABLE_LEAVE_ACK,
} msg_type_t;

typedef struct msg {
	msg_type_t type;
	unsigned int data;
	unsigned int data2;  // TODO:  make this not a hack
} msg_t;


unsigned int strtoid(const char *);
char *idtostr(int);

int in_range_ex_ex_circular(unsigned int, unsigned int, unsigned int);
int in_range_in_in_circular(unsigned int, unsigned int, unsigned int);
int in_range_in_ex_circular(unsigned int, unsigned int, unsigned int);
int in_range_ex_in_circular(unsigned int, unsigned int, unsigned int);

unsigned int closest_preceding_finger(node_t *, unsigned int);
unsigned int find_predecessor(node_t *, unsigned int);
unsigned int find_successor(node_t *, unsigned int);
void init_finger_table(node_t *, unsigned int);
void deinit_finger_table(node_t *);
void update_finger_table_join(node_t *, int, unsigned int);
void update_finger_table_leave(node_t *, int, unsigned int);
void update_others_join(node_t *);
void update_others_leave(node_t *);
void print_node(node_t *);

unsigned int rpc_get_status(unsigned int);
int rpc_set_status(unsigned int, status_t);
unsigned int rpc_get_successor(unsigned int);
int rpc_set_successor(unsigned int, unsigned int);
unsigned int rpc_get_predecessor(unsigned int);
int rpc_set_predecessor(unsigned int, unsigned int);
unsigned int rpc_get_closest_preceding_finger(unsigned int, unsigned int);
unsigned int rpc_find_predecessor(unsigned int, unsigned int);
unsigned int rpc_find_successor(unsigned int, unsigned int);
int rpc_update_finger_table_join(unsigned int, unsigned int, unsigned int);
int rpc_update_finger_table_leave(unsigned int, unsigned int, unsigned int);

void *rpc_handler(void *);

#endif /* __TRIAD_H__ */
