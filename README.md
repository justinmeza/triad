triad
=====

An implementation of the Chord peer-to-peer lookup service in C.

API overview
------------

<a id="triad_init" href="#triad_init">#</a> <i>node_t *</i><b>triad_init</b>(<i>const char *ip</i>)

Initializes a new node structure for the given IP address that is used for
subsequent API calls.

<a id="triad_join" href="#triad_join">#</a> <i>int</i><b>triad_join</b>(<i>node_t *n</i>, <i>const char *ip</i>)

Joins the node <i>n</i> to the Chord ring that the node at <i>ip</i> is already
connected to.  If <i>ip</i> is <i>n</i>'s IP address, then a new Chord ring is
started at <i>ip</i>, with <i>n</i> as its sole member.

<a id="triad_lookup" href="#triad_lookup">#</a> <i>char *</i><b>triad_lookup</b>(<i>node_t *n</i>, <i>unsigned int id</i>)

Looks up the IP address of the node that <i>id</i> is located on, in the Chord
ring that <i>n</i> has joined to.

<a id="triad_leave" href="#triad_leave">#</a> <i>int</i><b>triad_leave</b>(<i>node_t *n</i>)

Leaves the Chord ring that <i>n</i> is a member of.  This is necessary for
correctly updating the state of the other nodes in the ring.

<a id="triad_deinit" href="#triad_deinit">#</a> <i>int</i><b>triad_deinit</b>(<i>node_t *n</i>)

Releases any allocated resources and joins any threads used by <i>n</i>.
