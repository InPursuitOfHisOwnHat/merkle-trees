import hashlib

class Node:

    def __init__(self, left_node=None, right_node=None, data=None, hashed_data=None):
        self.left = left_node
        self.right = right_node
        self.data = data
        self.hashed_data = hashed_data

def build_tree(nodes):
    # Print out the list of nodes to have some confidence
    # we're building the tree properly
    for n in nodes:
        print('{0} [{1}]\t'.format(n.data, n.hashed_data.hexdigest()),end='')
    print()

    if len(nodes) == 1:
        return nodes
    
    node_layer = []
    left_index = 0
    while(left_index < len(nodes)):
        right_index = left_index + 1
        if right_index < len(nodes):
            node_layer.append(Node(nodes[left_index], nodes[right_index], nodes[left_index].data + nodes[right_index].data, hashlib.sha256(nodes[left_index].hashed_data.hexdigest().encode('utf-8') + nodes[right_index].hashed_data.hexdigest().encode('utf-8'))))
        else:
            node_layer.append(Node(nodes[left_index], None, nodes[left_index].data, hashlib.sha256(nodes[left_index].hashed_data.hexdigest().encode('utf-8'))))
        left_index = right_index + 1

    if len(node_layer) == 1:
        # We have the Root, print it out and return
        print('{0}[{1}'.format(node_layer[0].data, node_layer[0].hashed_data.hexdigest()),end='')
        return node_layer[0]
    else:
        return build_tree(node_layer)



data =['In', 'Pursuit', 'Of', 'His', 'Own', 'Hat']
leaves = []
for item in data:
    leaves.append(Node(left_node=None, right_node=None, data=item, hashed_data=hashlib.sha256(item.encode('utf-8'))))
root = build_tree(leaves)

