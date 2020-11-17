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
#include <stdbool.h>
#include <mcheck.h>
#include "./hatlog/hatlog.h"


char* read_dictionary_file(const char* dict_file) {

    hatlog("===== read_dictionary() =====");
    hatlog("dictionary file: %s", dict_file);

    /* Get a file descriptor for the dictionary file */

    const int dictionary_fd = open(dict_file, O_RDONLY);
    if (dictionary_fd == -1) {
        perror("open()");
        hatlog("failed to open dictionary file");
        exit(EXIT_FAILURE);
    }
    
    hatlog("dictionary file opened with fd %d", dictionary_fd);

    /*  
     *  The stat struct is a system struct that can be loaded with various
     *  statistics about a file using the fstat() system call. 
     * 
     *  We're using it here so we can get the file size in bytes.
     *
     */
    
    struct stat dict_stats;

    if (fstat(dictionary_fd, &dict_stats) == -1) {
        hatlog("failed to get statistics for dictionary file");
        perror("fstat()");
        exit(EXIT_FAILURE);
    }

    const long file_size = dict_stats.st_size;

    hatlog("retrieved file_size of %ld bytes", file_size);

    /*  
     *  Now we have the file size we can allocate a block of memory
     *  big enough for us to load the entire file in plus one extra
     *  byte for the '\0' (NULL terminator).
     *
     */

    const long buffer_size = file_size + 1;
    char* buffer = malloc(buffer_size);

    hatlog("initialised buffer size of %ld bytes (extra one for 0 (NULL terminator))", buffer_size);

    /*
     *  Now read the whole file in. I know it will fit
     *  comfortably because this is 2020 and my machine has 16GB of RAM,
     *  so just pass file_size retrieved above into 
     *  the read() system call which tells it to grab the lot.
     *
     *  We also check that the total amount read - returned by read() -
     *  matches the file size. This confirms we got it all. I don't do 
     *  any retries at this point, just re-run the program.
     *
     */
     
    ssize_t bytes_read;
    if((bytes_read = read(dictionary_fd, buffer, file_size)) != file_size) {
        hatlog("unable to load file data into buffer");
        perror("read()");
        exit(EXIT_FAILURE);
    }

    hatlog("loaded %ld bytes into buffer", bytes_read);

    /* Good housekeeping! */

    close(dictionary_fd);

    /* Add the NULL terminator to the buffer */

    buffer[file_size] = '\0';

    hatlog("added 0 (NULL terminator) to buffer position %ld, returning buffer of %ld bytes", file_size);

    return buffer;
    
}

long get_word_count(const char* data) {

    /* 
     *  We know the data is made of words, one on each line, so all we really need
     *  to do is count the number of newlines ('\n') to get the number of words and
     *  remember to add one for the last word which will terminate with '\0' 
     *  (end of the buffer), not '\n').
     */

    hatlog("===== get_word_count() =====");

    long word_count = 0;
    for (int i=0; data[i] != '\0'; i++) {
         if (data[i] == '\n') {
             word_count++;
         }
     }

     word_count++;

     hatlog("returning word count of %ld", word_count);
     
     return word_count;
}

char * hexdigest(const unsigned char* hash) {

    hatlog("===== hexdigest() =====");

    /* 
    *   The SHA256 digest is stored in a 32 byte block pointed to by an unsigned char *. 
    *   We want to each byte converted to its hex value, two alphanumeric characters per byte.
    *   This means we need a 64 character buffer available, plus an extra one
    *   for the '\0'.

    *   We move along the SHA256 digest one byte at a time, converting it to two alphanumeric
    *   characters and storing the result in the buffer *. We then have to move to
    *   the next available slot in the string which is the one after the next because,
    *   remember, each byte of the digest takes up two characters.
    */

    char * hexdigest = calloc(1,65);

    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
		sprintf(hexdigest + (i * 2), "%02x", hash[i]);

    hexdigest[64]='\0';

    hatlog("returning %s", hexdigest);

    return hexdigest;
    
}

unsigned char * sha256(const char * data) {

    /*
     *  Use the SHA256 function in the official openssl library to get a hash digest
     *  of data (via the data param).
     *
     *  The correct way to use OpenSSL is through the EVP interface which,
     *  to be honest, isn't terrifically documented. You will find some
     *  Stack Overflow answers around SSL advising to use the functions behind this
     *  interface directly, but I'm wary of that - it's worth the hassle of getting
     *  through the SSL docs and examples to use the preferred interfaces
     *
     *  Bizzarly, the EVP functions all return 1 for success and 0 for error ...
     *  but it's a free country, I s'pose.
     *
     */


    hatlog("===== sha256() =====");

    unsigned int data_len = strlen(data);
    unsigned char * hash_digest;

    EVP_MD_CTX *mdctx;

    hatlog("initialising new mdctx");
    mdctx = EVP_MD_CTX_new();

    EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL);
    
    hatlog("updating mdctx digest with data [%s]", data);
    EVP_DigestUpdate(mdctx, data, data_len);
    
    hatlog("initialising new hash_digest buffer");
    hash_digest = (unsigned char *)OPENSSL_malloc(EVP_MD_size(EVP_sha256()));
    
    EVP_DigestFinal_ex(mdctx, hash_digest, &data_len);
    hatlog("succesfully copied new digest to hash_digest buffer");
    
    EVP_MD_CTX_free(mdctx);
    hatlog("successfully freed mdctx digest");

    hatlog("returning");
    return hash_digest;

}

/*  A fairly simple tree node struct.
 * 
 *  Only thing to explain that might raise a few eyebrows is that I'm 
 *  including a representation of the data used to generate each hash.
 *  
 *  This is an exercise and I want to be able to take a few peeks 
 *  under the hood either when debugging or tracing to check all is 
 *  going as expected. 
 * 
 *  There is no real need to do this at all when building a Merkle 
 *  Tree (and it will increase your attack  surface), just the hashes
 *  is enough.
 *  
 */

struct Node {
    struct Node * left;
    struct Node * right;
    char * sha256_digest;
    char * data;
}; 

typedef struct Node Node;

/* A helper function to construct nodes */

Node * new_node(Node * left, Node * right, char * data, char * sha256_digest) {

    hatlog("===== new_node() =====");
    hatlog("left: %p, right: %p, data: [%s], hash: [%s]", left, right, data, sha256_digest);

    Node * node = malloc(sizeof(Node));
    node->left = left;
    node->right = right;
    node->data = data;
    node->sha256_digest = sha256_digest;

    hatlog("returning new node at address %p", node);
    
    return node;
}


Node ** build_leaves(char* data) {

    hatlog("===== build_leaves() =====");

    /*
     *  Just need to allocate enough memory to store pointers to all the leaf nodes,
     *  the actual nodes themselves will be allocated when they're created.
     *
     *  To find out how many leaves there are, we call get_word_count()
     *
     */
    
    long word_count = get_word_count(data);
    Node ** leaves = malloc(sizeof(Node *)*word_count);

    hatlog("allocated block of %ld bytes for leaves (number of leaves * sizeof(pointer) + NULL terminator", word_count * sizeof(unsigned char *) + 1);

    long index = 0;
    long hash_count = 0;
    Node * n;

    hatlog("beginning loop through buffer using strtok");

    /*
     *  We know each word in our data is on a separate line or, to put it another way, 
     *  each word is separated by a newline (\n) character (except for the last word
     *  which ends with the null terminator (\0)). We can, therefore, use strtok() 
     *  to tell us where, in the buffer, each word begins and then build a node 
     *  around a char * pointer to that address.
     *
     */

    char * word = strtok(data, "\n\0");
    
    while( word != NULL) {
    
        hatlog("next word is [%s]", word);
        
        n = new_node(NULL, NULL, word, hexdigest(sha256(word)));
        
        leaves[index] = n;
        hash_count++;
        index++;
        word = strtok(NULL, "\n\0");
    }

    int leaf_count = index;

    hatlog("returning %ld leaves", leaf_count);

    return leaves;
}

Node * build_merkle_tree(Node ** nodes, long len) {

    hatlog("===== build_merkle_tree() =====");

    hatlog("passed in node ** with address of %p and len of %ld", nodes, len);

    if (len == 1) {

        /* We already have the root, just send back */

        hatlog("len is 1 so we have root. Returning nodes[0] at address %p", nodes[0]);

        return nodes[0];
    }

    /*
     *  This new layer of tree nodes will be exactly half the number of the previous layer
     *  (passed in as the len parameter) because we split the layer into groups of two.
     *   
     *  We are safe in the knowledge that this number will always be divisible by
     *  two because, in the event the previous layer had an odd number of nodes, the left
     *  leaf of the final node would have been duplicated as the right node, 
     *  as per convention.
    */
   
    long node_layer_size = len/2;

    Node ** node_layer = malloc(sizeof(Node *)*node_layer_size);

    hatlog("allocated space for %ld node pointers in node_layer at address %p", node_layer_size, node_layer);
    printf("allocated space for %ld node pointers in node_layer at address %p\n", node_layer_size, node_layer);
    
    int node_layer_index = 0;
    long left_index = 0;
    long right_index = 0;
    
    Node* n;
    char* data;
    char* digest;

    hatlog("entering main loop");

    while (left_index < len) {

        hatlog("top of loop");

        right_index = left_index + 1;

        hatlog("left_index = %ld, right_index = %ld", left_index, right_index);
         
        if (right_index < len) {

            hatlog("both left node and right node available");
            
            int data_len = strlen(nodes[left_index]->data) 
                                            + strlen(nodes[right_index]->data) + 1;

            /*  hatlog has a default buffer output size of 255 chars, so for large 
             *  trees with lots of data not everything will be printed in the 
             *  log output, below.
             *
             *  You can increase the size of this buffer at compile time using 
             *  the -DHATLOG_MAX_BUFFER_SIZE flag
             */
             
            hatlog("left node addr: %p, left node data: [%s], right node addr: %p, right node data: [%s]", 
                                            nodes[left_index], 
                                            nodes[left_index]->data, 
                                            nodes[right_index], 
                                            nodes[right_index]->data);
            
            data = malloc(sizeof(char) * data_len);

            hatlog("allocated %ld bytes for new node data at %p", data_len, data);

            digest = malloc(sizeof(char) * 129);

            hatlog("allocated 129 bytes for digest");

            /* contactenate left and right node data into new node's data field */
            
            strcpy(data, nodes[left_index]->data);
            strcat(data, nodes[right_index]->data);

            hatlog("concatenated data is: %s", data);

            /* concatenate left and right node hash digest into new node's digest field */
            
            strcpy(digest, nodes[left_index]->sha256_digest);
            strcat(digest, nodes[right_index]->sha256_digest);

            hatlog("concatenated digest is: %s", digest);

            n = new_node(nodes[left_index], 
                         nodes[right_index], 
                         data, 
                         hexdigest(sha256(digest)));

        }
        else {
        
            /*
             *  only have a left, leaf left ... Try saying that after six beers.
             *   
             *  this means we duplicate the left leaf to the right leaf of the
             *  new node.
            */

            hatlog("only have left node available");
            
            int data_len = (strlen(nodes[left_index]->data) * 2) + 1;

            hatlog("left node addr: %p, left node data: [%s]", nodes[left_index], nodes[left_index]->data);

            data = malloc(sizeof(char) * data_len);

            hatlog("allocated %ld bytes for new node data at %p", data_len, data);

            digest = malloc(sizeof(char) * 129);
            
            hatlog("allocated 129 bytes for digest");

            /* concatenate left data with itself */
            
            strcpy(data, nodes[left_index]->data);
            strcat(data, nodes[left_index]->data);

            hatlog("concatenated data is: %s", data);

            /* concatenate left data hash with itself */
            
            strcpy(digest, nodes[left_index]->sha256_digest);
            strcat(digest, nodes[left_index]->sha256_digest);

            hatlog("new node concatenated digest is: %s", digest);

            n = new_node(nodes[left_index], 
                         nodes[left_index], 
                         data, 
                         hexdigest(sha256(digest)));
            
        }

        node_layer[node_layer_index] = n;
        
        hatlog("added node at address %p to node_layer with an index of %ld", n, node_layer_index);
        
        node_layer_index++;
        left_index = right_index + 1;
    }

    /*
     *  we make a recursive call, passing the newly formed layer (which will 
     *  become the previous layer) and the node_layer_index which will now
     *  contain the number of nodes added to the new layer and, therefore, it's length
     *
     */
     
    hatlog("recursive call with nodelayer at %p and layer_index at %ld", 
            node_layer, 
            node_layer_index);

    return build_merkle_tree(node_layer, node_layer_index);
}

void print_command_line( char* program_name ) {
    printf("%s [-d, debug | -df, debug & flush] [filename]\n", program_name);
}

int main(int argc, char *argv[])
{
    /*  Process args */

    if (argc == 3) {
    
        /* Debug with no flush */
        
        if (strcmp(argv[2], "-d") == 0) {
            hatlog_initialise(argv[0],false);
        }
        
        /* Debug with flush */
        
        else if (strcmp(argv[2], "-df") == 0) {
            hatlog_initialise(argv[0],true);
        }
        
        /* No valid debug option */
        
        else {
            print_command_line(argv[0]);
            exit(EXIT_FAILURE);
        }
    }
    else if (argc < 2) {
            print_command_line(argv[0]);
            exit(EXIT_FAILURE);
    }

    char * words = read_dictionary_file(argv[1]);
    long word_count = get_word_count(words);
    printf("building leaves\n");
    Node ** leaves = build_leaves(words);
    printf("build %ld leaves out of %ld words\n", word_count, word_count);
    Node * root = build_merkle_tree(leaves, word_count);
    printf("Root digest is: %s\n", root->sha256_digest);

    hatlog_stop();

}