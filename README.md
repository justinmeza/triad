triad
=====

An implementation of the Chord peer-to-peer lookup service in C.

API overview
------------

<i>node_t *</i><b>triad_init</b>(<i>const char *ip</i>)

Initializes a new node structure for the given IP address that is used for
subsequent API calls.

<i>int</i> <b>triad_join</b>(<i>node_t *n</i>, <i>const char *ip</i>)

Joins the node <i>n</i> to the Chord ring that the node at <i>ip</i> is already
connected to.  If <i>ip</i> is <i>n</i>'s IP address, then a new Chord ring is
started at <i>ip</i>, with <i>n</i> as its sole member.

<i>char *</i><b>triad_lookup</b>(<i>node_t *n</i>, <i>unsigned int id</i>)

Looks up the IP address of the node that <i>id</i> is located on, in the Chord
ring that <i>n</i> has joined to.

<i>int</i> <b>triad_leave</b>(<i>node_t *n</i>)

Leaves the Chord ring that <i>n</i> is a member of.  This is necessary for
correctly updating the state of the other nodes in the ring.

<i>int</i> <b>triad_deinit</b>(<i>node_t *n</i>)

Releases any allocated resources and joins any threads used by <i>n</i>.

Example usage
-------------

    char local[15] = "192.168.1.7";   // this node's IP address
    char remote[15] = "192.168.1.8";  // the IP address of a node that has already joined a Chord ring
    int start_new = 0;  // whether to start a new Chord ring or join an existing one
    node_t *n = triad_init(local);
    if (start_new)
        triad_join(n, local);
    else
        triad_join(n, remote);
    printf("12345678 => %s\n", triad_lookup(n, 12345678));
    triad_leave(n);
    triad_deinit(n);
