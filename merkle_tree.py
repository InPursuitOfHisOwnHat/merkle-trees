import hashlib

class Node:

    def __init__(self,
                 left_node=None,
                 right_node=None,
                 hashed_data=None):
                 
        self.left = left_node
        self.right = right_node
        self.hashed_data = hashed_data


def build_tree(nodes):
    
    node_layer = []
    left_index = 0
    nodes_len = len(nodes)

    while(left_index < nodes_len):
    
        right_index = left_index + 1
    
        if right_index < len(nodes):
        
            left_data_hash = nodes[left_index].hashed_data.hexdigest().encode('utf-8')
            right_data_hash = nodes[right_index].hashed_data.hexdigest().encode('utf-8')

            n = Node(nodes[left_index],
                     nodes[right_index], 
                     hashlib.sha256(left_data_hash + right_data_hash))
                     
        else:
        
            left_data_hash = nodes[left_index].hashed_data.hexdigest().encode('utf-8')

            n = Node(nodes[left_index],
                     nodes[left_index], 
                     hashlib.sha256(left_data_hash + left_data_hash))

        node_layer.append(n)
        left_index = right_index + 1
    
    if len(node_layer) == 1:
    
        # we already have the root if there's only one node
        # left, so just return it.
    
        return node_layer[0]
    
    else:
    
        # recursive call with this layer of nodes so we
        # can create the next one above it
        
        return build_tree(node_layer)


if __name__ == '__main__':

    data =['In', 'Pursuit', 'Of', 'His', 'Own', 'Hat']
    leaves = []
    
    # first we want to build the list of leaves (bottom)
    # layer which we do with a simple for loop
    
    for item in data:
        n = Node(left_node=None, 
                 right_node=None, 
                 hashed_data=hashlib.sha256(item.encode('utf-8')))
                 
        leaves.append(n)
    
    # now we can build our tree and get the root
    root = build_tree(leaves)

    print("Root Has Digest: {0}".format(root.hashed_data.hexdigest()))
