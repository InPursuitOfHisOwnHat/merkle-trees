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
#include "./cakelog/cakelog.h"


char* read_dictionary_file(const char* dict_file) {

    cakelog("===== read_dictionary() =====");
    cakelog("dictionary file: %s", dict_file);

    /* Get a file descriptor for the dictionary file */

    const int dictionary_fd = open(dict_file, O_RDONLY);
    if (dictionary_fd == -1) {
        perror("open()");
        cakelog("failed to open dictionary file");
        exit(EXIT_FAILURE);
    }
    
    cakelog("dictionary file opened with fd %d", dictionary_fd);

    /*  
     *  The stat struct is a system struct that can be loaded with various
     *  statistics about a file using the fstat() system call. 
     * 
     *  We're using it here so we can get the file size in bytes.
     *
     */
    
    struct stat dict_stats;

    if (fstat(dictionary_fd, &dict_stats) == -1) {
        cakelog("failed to get statistics for dictionary file");
        perror("fstat()");
        exit(EXIT_FAILURE);
    }

    const long file_size = dict_stats.st_size;

    cakelog("retrieved file_size of %ld bytes", file_size);

    /*  
     *  Now we have the file size we can allocate a block of memory
     *  big enough for us to load the entire file in plus one extra
     *  byte for the '\0' (NULL terminator).
     *
     */

    const long buffer_size = file_size + 1;
    char* buffer = malloc(buffer_size);

    cakelog("initialised buffer size of %ld bytes (extra one for 0 (NULL terminator))", buffer_size);

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
        cakelog("unable to load file data into buffer");
        perror("read()");
        exit(EXIT_FAILURE);
    }

    cakelog("loaded %ld bytes into buffer", bytes_read);

    /* Good housekeeping! */

    close(dictionary_fd);

    /* Add the NULL terminator to the buffer */

    buffer[file_size] = '\0';

    cakelog("added 0 (NULL terminator) to buffer position %ld, returning buffer of %ld bytes", file_size);

    return buffer;
    
}

long get_word_count(const char* data) {

    /* 
     *  We know the data is made of words, one on each line, so all we really need
     *  to do is count the number of newlines ('\n') to get the number of words and
     *  remember to add one for the last word which will terminate with '\0' 
     *  (end of the buffer), not '\n').
     */

    cakelog("===== get_word_count() =====");

    long word_count = 0;
    for (int i=0; data[i] != '\0'; i++) {
         if (data[i] == '\n') {
             word_count++;
         }
     }

     word_count++;

     cakelog("returning word count of %ld", word_count);
     
     return word_count;
}

char * hexdigest(const unsigned char* hash) {

    cakelog("===== hexdigest() =====");

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

    cakelog("returning %s", hexdigest);

    return hexdigest;
    
}

unsigned char * sha256(const char * data) {

    cakelog("===== sha256() =====");

    unsigned int data_len = strlen(data);
    unsigned char * hash_digest;

    EVP_MD_CTX *mdctx;

    cakelog("initialising new mdctx");
    mdctx = EVP_MD_CTX_new();

    EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL);
    
    cakelog("updating mdctx digest with data [%s]", data);
    EVP_DigestUpdate(mdctx, data, data_len);
    
    cakelog("initialising new hash_digest buffer");
    hash_digest = (unsigned char *)OPENSSL_malloc(EVP_MD_size(EVP_sha256()));
    
    EVP_DigestFinal_ex(mdctx, hash_digest, &data_len);
    cakelog("succesfully copied new digest to hash_digest buffer");
    
    EVP_MD_CTX_free(mdctx);
    cakelog("successfully freed mdctx digest");

    cakelog("returning");
    return hash_digest;

}

struct Node {
    struct Node * left;
    struct Node * right;
    char * sha256_digest;
    char * data;
}; 

typedef struct Node Node;

Node * new_node(Node * left, Node * right, char * sha256_digest) {

    cakelog("===== new_node() =====");
    cakelog("left: %p, right: %p, hash: [%s]", left, right, sha256_digest);

    Node * node = malloc(sizeof(Node));
    node->left = left;
    node->right = right;
    node->sha256_digest = sha256_digest;

    cakelog("returning new node at address %p", node);
    
    return node;
}


Node** build_leaves(char* buffer) {

    cakelog("===== build_leaves() =====");
  
    long word_count = get_word_count(buffer);
    Node **leaves = malloc(sizeof(Node*)*word_count);

    cakelog("allocated block of %ld bytes for leaves (number of leaves * sizeof(pointer) + NULL terminator", word_count * sizeof(unsigned char*) + 1);

    long index = 0;
    long hash_count = 0;
    Node *n;

    cakelog("beginning loop through buffer using strtok");

    char * word = strtok(buffer, "\n\0");
    
    while( word != NULL) {
    
        cakelog("next word is [%s]", word);
        
        n = new_node(NULL, NULL, hexdigest(sha256(word)));
        
        leaves[index] = n;
        hash_count++;
        index++;
        word = strtok(NULL, "\n\0");
    }

    int leaf_count = index;

    cakelog("returning %ld leaves", leaf_count);

    return leaves;
}

Node * build_merkle_tree(Node ** nodes, long len) {

    cakelog("===== build_merkle_tree() =====");

    cakelog("passed in node ** with address of %p and len of %ld", nodes, len);

    if (len == 1) {

        cakelog("len is 1 so we have root. Returning nodes[0] at address %p", nodes[0]);

        return nodes[0];
    }
   
    long node_layer_size = len/2;

    Node ** node_layer = malloc(sizeof(Node *)*node_layer_size);

    cakelog("allocated space for %ld node pointers in node_layer at address %p", node_layer_size, node_layer);
    printf("allocated space for %ld node pointers in node_layer at address %p\n", node_layer_size, node_layer);
    
    int node_layer_index = 0;
    long left_index = 0;
    long right_index = 0;
    
    Node* n;
    char* digest;

    cakelog("entering main loop");

    while (left_index < len) {

        cakelog("top of loop");

        right_index = left_index + 1;

        cakelog("left_index = %ld, right_index = %ld", left_index, right_index);
         
        if (right_index < len) {

            cakelog("both left node and right node available");
                       
            cakelog("left node addr: %p, left node hash: [%s], right node addr: %p, right node hash: [%s]", nodes[left_index], nodes[left_index]->sha256_digest,  nodes[right_index], nodes[right_index]->sha256_digest);
            
            digest = malloc(sizeof(char) * 129);

            cakelog("allocated 129 bytes for digest");

            strcpy(digest, nodes[left_index]->sha256_digest);
            strcat(digest, nodes[right_index]->sha256_digest);

            cakelog("concatenated digest is: %s", digest);

            n = new_node(nodes[left_index], 
                         nodes[right_index], 
                         hexdigest(sha256(digest)));

        }
        else {
        
            cakelog("only have left node available");
            
            cakelog("left node addr: %p, left node digest: [%s]", nodes[left_index], nodes[left_index]->sha256_digest);

            digest = malloc(sizeof(char) * 129);
                       
            strcpy(digest, nodes[left_index]->sha256_digest);
            strcat(digest, nodes[left_index]->sha256_digest);

            cakelog("new node concatenated digest is: %s", digest);

            n = new_node(nodes[left_index], 
                         nodes[left_index], 
                         hexdigest(sha256(digest)));
        }

        node_layer[node_layer_index] = n;
        
        cakelog("added node at address %p to node_layer with an index of %ld", n, node_layer_index);
        
        node_layer_index++;
        left_index = right_index + 1;
    }
    
    cakelog("recursive call with nodelayer at %p and layer_index at %ld", 
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
            cakelog_initialise(argv[0],false);
        }
        
        /* Debug with flush */
        
        else if (strcmp(argv[2], "-df") == 0) {
            cakelog_initialise(argv[0],true);
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

    cakelog_stop();

}