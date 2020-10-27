#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <string.h>
#include <sys/stat.h>
#include <openssl/sha.h>
#include <openssl/evp.h>

#define DEBUG 0
 
char* read_dictionary_file(const char* dict_file) {

    // Using sycalls for this because I'm awkward and have been reading 
    // Robert Love's book
    const int dictionary_fd = open(dict_file, O_RDONLY);
    if (dictionary_fd == -1) {
        perror("open()");
        exit(EXIT_FAILURE);
    }

    // Lets get the size of the file so we can work out how big our text buffer
    // should be. The file is nothing but text and we're on Linux, so the
    // number of bytes returned by fstat() will match the number of bytes we 
    // need to allocate.
    struct stat dict_stats;

    if (fstat(dictionary_fd, &dict_stats) == -1) {
        perror("fstat()");
        exit(EXIT_FAILURE);
    }

    // Total size in bytes can be found in the st_size member of the 'stat' 
    // struct returned.
    const int file_size = dict_stats.st_size;

    // ...But add extra space for the last '\0'
    const int buffer_size = file_size + 1;

    // ...And allocate our buffer
    char* buffer = malloc(buffer_size);

    // Use read syscall to pull in all file data at once - it's not *that*
    // much and this is an experiment, so let's make it easy.
    ssize_t bytes_read;
    if(read(dictionary_fd, buffer, file_size) != file_size) {
        perror("read()");
        exit(EXIT_FAILURE);
    }
    close(dictionary_fd);

    // ...And add terminating '\0' to the buffer
    buffer[buffer_size] = '\0';

    return buffer;
}

long get_word_count(const char* data) {
    // We know the data is made of of words, one per line so we're just going to
    // count the newlines '\n' in the array, but then add one more because the
    // last word of the file does not have one. Instead it has a '\0' that the
    // import function should have added.

    long word_count = 0;
    for (int i=0; data[i] != '\0'; i++) {
         if (data[i] == '\n') {
             word_count++;
         }
     }
     // Add one because the last \0 replaces the last \n
     word_count++;
     return word_count;
}

char * hexdigest(const unsigned char* hash) {
    // Return a 64 character string representation of the hash.

    char * hexdigest = calloc(1,65);
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        // We're converting to Hex which takes up two digits per byte
        // Sha256 produces a 32 byte value
        // so when converting to string we need to add 2
        // Sha Digest Length is 32 bytes but when you represent each byte as a hexadecimal character in a string
        // you need two digits, so that means we need to shift along an extra one when loading up the array.
		sprintf(hexdigest + (i * 2), "%02x", hash[i]);
    }
    hexdigest[64]='\0';
    return hexdigest;
    
}

unsigned char * sha256(const char * data) {

    // Use openssl library to get a SHA256 hash of `data`. This is returned
    // as binary values in an unsigned char * so we will need a conversion
    // function to actually see the 64 character hash digest itself.

    unsigned int data_len = strlen(data);
    unsigned char * hash_digest;

    EVP_MD_CTX *mdctx;
    mdctx = EVP_MD_CTX_new();
    
    // All these functions return 1 for success and 0 for error ... well, it's a free country.
    EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL);
    EVP_DigestUpdate(mdctx, data, data_len);
    hash_digest = (unsigned char *)OPENSSL_malloc(EVP_MD_size(EVP_sha256()));
    EVP_DigestFinal_ex(mdctx, hash_digest, &data_len);
    EVP_MD_CTX_free(mdctx);

    return hash_digest;

}


struct Node {
    struct Node * left;
    struct Node * right;
    char * sha256_digest;
    char * data;
};
typedef struct Node Node;

Node * new_node(Node * left, Node * right, char * data, char * sha256_digest) {

    // Helper function n to construct nodes
    printf("In new_node(): Left: %p, Right: %p, Data: %s, Hash: %s\n", left, right, data, sha256_digest);

    Node * node = malloc(sizeof(Node));
    node->left = left;
    node->right = right;
    node->data = data;
    node->sha256_digest = sha256_digest;
    printf("Returning %p\n", node);
    return node;

}

Node ** build_leaves(char* data) {

    // We want to build a list of Nodes that will be the leaves of our tree.

    long word_count = get_word_count(data);
    unsigned char ** hash_values = malloc(word_count * sizeof(unsigned char *));

    Node ** leaves = malloc(sizeof(Node *)*word_count);

    long index = 0;
    long hash_count = 0;
    char * word = strtok(data, "\n");
    while( word != NULL) {
        Node * n = new_node(NULL, NULL, word, hexdigest(sha256(word)));
        leaves[index] = n;
        hash_count++;
        if (hash_count % 1000 == 0 && DEBUG) {
            printf("%ld: %s\n", hash_count, word);
        }
        index++;
        word = strtok(NULL, "\n\0");
    }
    return leaves;
}

Node * build_merkle_tree(Node ** nodes, long len) {

    printf("Passed nodes in starting at address %p\n", nodes);
    // We already have the root
    if (len == 1) {
        return nodes[0];
    }

    Node ** node_layer = malloc(sizeof(Node*));
    node_layer[0] = malloc(sizeof(Node*));

    printf("Node Layer addr: %p\n", node_layer);
    int layer_index = 0;

    long left_index = 0;
    long right_index = 0;

    while (left_index < len) {
        right_index = left_index + 1;
        if (right_index < len) {
            int data_len = strlen(nodes[left_index]->data) + strlen(nodes[right_index]->data);
            printf("Left and right hand data\n");
            printf("\tData Length: %d\n", data_len);
            printf("\tData Left: %s\n", nodes[left_index]->data);
            printf("\tLeft Node addr: %p\n", nodes[left_index]);
            printf("\tData Right: %s\n", nodes[right_index]->data);
            printf("\tRight Node addr: %p\n", nodes[right_index]);

            char* data = malloc(data_len);
            char* digest = malloc(129);

            // Store the actual data. Clearly you wouldn't do this if you were
            // building a real Merkle Tree. The whole point is that it
            // obsfuscates the data, doesn't stick it right next to the hash like
            // it was written by an idiot.
            strcpy(data, nodes[left_index]->data);
            strcat(data, nodes[right_index]->data);

            printf("\tData: %s.\n", data);

            // We're going to store the digest as a hex string because I want to
            // do some rudimentary checking. We could probably just store
            // the unsigned char * binary version that openssl returns, though,
            // and SHA256 those instead.
            strcpy(digest, nodes[left_index]->sha256_digest);
            strcat(digest, nodes[right_index]->sha256_digest);

            Node * n = new_node(nodes[left_index], nodes[right_index], data, hexdigest(sha256(digest)));
            printf("\tGot new Node at address: %p\n", n);
            node_layer[layer_index] = n;
            printf("\tNew Node added to node_layer index: %d, address %p, data len is %d: [%s] [%s]\n", layer_index, node_layer[layer_index], data_len, node_layer[layer_index]->data, node_layer[layer_index]->sha256_digest);
            layer_index++;
        }
        else {
            // We only have a left leaf left (say that after eight pints)
            char * data = malloc(strlen(nodes[left_index]->data));
            char * digest = malloc(65);
            strcpy(data, nodes[left_index]->data);
            strcpy(digest, nodes[left_index]->sha256_digest);
            printf("Left Hand Data Only: %s\n", data);
            printf("\tData Length: %ld\n", strlen(nodes[left_index]->data));
            printf("\tData: %s\n", data);
            Node * n = new_node(nodes[left_index], NULL, data, hexdigest(sha256(digest)));
            node_layer[layer_index] = n;
            printf("\tNew Node added to node_layer index: %d, address %p [%s] [%s]\n", layer_index, node_layer[layer_index], node_layer[layer_index]->data, node_layer[layer_index]->sha256_digest);
            layer_index++;
        }
        left_index = right_index + 1;
    }

    // Recursive call
    return build_merkle_tree(node_layer, layer_index);
}

int main(int argc, char *argv[])
{

    char * words = read_dictionary_file("ukenglish-test-2.txt");
    long word_count = get_word_count(words);
    printf("Word Count: %ld\n", word_count);
    Node ** leaves = build_leaves(words);
    Node * root = build_merkle_tree(leaves, word_count);
    printf("Root digest is: %s\n", root->sha256_digest);

}