import hashlib

class Node:

    def __init__(self,
                 left_node=None,
                 right_node=None,
                 data=None,
                 hashed_data=None):
                 
        self.left = left_node
        self.right = right_node
        self.data = data
        self.hashed_data = hashed_data


def build_tree(nodes):
    
    if len(nodes) == 1:
        return nodes
    
    node_layer = []
    left_index = 0
    while(left_index < len(nodes)):
    
        right_index = left_index + 1
    
        if right_index < len(nodes):
        
            n = Node(nodes[left_index],
                     nodes[right_index], 
                     nodes[left_index].data + nodes[right_index].data,
                     hashlib.sha256(nodes[left_index].hashed_data.hexdigest().encode('utf-8') + nodes[right_index].hashed_data.hexdigest().encode('utf-8')))
                     
        else:
        
            n = Node(nodes[left_index],
                     None, 
                     nodes[left_index].data, 
                     hashlib.sha256(nodes[left_index].hashed_data.hexdigest().encode('utf-8') + nodes[left_index].hashed_data.hexdigest().encode('utf-8')))

        node_layer.append(n)
        left_index = right_index + 1
    
    if len(node_layer) == 1:
        return node_layer[0]
    else:
        return build_tree(node_layer)


if __name__ == '__main__':

    data =['In', 'Pursuit', 'Of', 'His', 'Own', 'Hat']
    leaves = []
    for item in data:
        n = Node(left_node=None, 
                           right_node=None, 
                           data=item, 
                           hashed_data=hashlib.sha256(item.encode('utf-8')))
        leaves.append(n)
    
    # Build our tree
    root = build_tree(leaves)

    print("Root Has Digest: {0}".format(root.hashed_data.hexdigest()))
