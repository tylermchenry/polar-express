# Simple topological sort for dependencies. Not particularly
# efficient, but does not require elements of the dependency
# sequence to be hashable.

def toposort_rec(seq, reverse_topo_seq):
    for e in reversed(seq):
        if isinstance(e, list) and not isinstance(e, basestring):
            toposort_rec(e, reverse_topo_seq)
        elif e not in reverse_topo_seq:
            reverse_topo_seq.append(e)

def mkdeps(seq):
    reverse_topo_seq = []
    toposort_rec(seq, reverse_topo_seq)
    return reverse_topo_seq[::-1]
