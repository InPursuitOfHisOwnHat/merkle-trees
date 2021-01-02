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
#include <math.h>
#include <getopt.h>
#include "./cakelog/cakelog.h"

char* read_data_file(const char *dict_file) {

    cakelog("===== read_dictionary_file() =====");

    const int dictionary_fd = open(dict_file, O_RDONLY);
    if (dictionary_fd == -1) {
        perror("open()");
        cakelog("failed to open file: '%s'", dict_file);
        exit(EXIT_FAILURE);
    }

    cakelog("opened file %s", dict_file);
    
    struct stat dict_stats;

    if (fstat(dictionary_fd, &dict_stats) == -1) {
        cakelog("failed to get statistics for dictionary file");
        perror("fstat()");
        exit(EXIT_FAILURE);
    }

    const long file_size = dict_stats.st_size;

    cakelog("file_size is %ld bytes", file_size);

    const long buffer_size = file_size + 1;
    char* buffer = malloc(buffer_size);
     
    ssize_t bytes_read;
    if((bytes_read = read(dictionary_fd, buffer, file_size)) != file_size) {
        cakelog("unable to load file");
        perror("read()");
        exit(EXIT_FAILURE);
    }

    cakelog("loaded %ld bytes into buffer", bytes_read);

    close(dictionary_fd);

    buffer[file_size] = '\0';

    return buffer;
    
}

long get_word_count(const char* data) {

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

char* hexdigest(const unsigned char *hash) {

    cakelog("===== hexdigest() =====");

    char *hexdigest = malloc((SHA256_DIGEST_LENGTH*2)+1);

    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
		sprintf(hexdigest + (i * 2), "%02x", hash[i]);
    }

    hexdigest[64]='\0';

    cakelog("returning %s", hexdigest);

    return hexdigest;
    
}

unsigned char* sha256(const char *data) {

    cakelog("===== sha256() =====");

    unsigned int data_len = strlen(data);
    unsigned char *hash_digest;

    EVP_MD_CTX *mdctx;

    cakelog("initialising new mdctx");

    mdctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL);
    
    cakelog("updating mdctx digest with data [%s]", data);
    EVP_DigestUpdate(mdctx, data, data_len);
    
    cakelog("initialising new hash_digest buffer");
    hash_digest = OPENSSL_malloc(EVP_MD_size(EVP_sha256()));
    
    EVP_DigestFinal_ex(mdctx, hash_digest, &data_len);
    cakelog("succesfully copied new digest to hash_digest buffer");
    
    EVP_MD_CTX_free(mdctx);
    cakelog("successfully freed mdctx digest");

    return hash_digest;

}

struct Node {
    struct Node * left;
    struct Node * right;
    char * sha256_digest;
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

    cakelog("allocated %ld bytes for %ld leaves (number of leaves * sizeof(pointer) + NULL terminator", word_count * sizeof(unsigned char*) + 1, word_count);

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

    cakelog("returning %ld leaves", index);

    return leaves;
}

Node* build_merkle_tree(Node **previous_layer, long previous_layer_len) {

    cakelog("===== build_merkle_tree() =====");

    if (previous_layer_len == 1) {

        cakelog("previous_layer_len is 1 so we have root. Returning previous_layer[0] at address %p", previous_layer[0]);

        return previous_layer[0];
    }
   
    long next_layer_len = ceil(previous_layer_len/2.0);
    Node **next_layer = malloc(sizeof(Node*)*next_layer_len);

    cakelog("allocated space for %ld node pointers in next_layer at address %p", next_layer_len, next_layer);
    printf("allocated space for %ld node pointers in next_layer at address %p\n", next_layer_len, next_layer);
    
    int next_layer_index = 0;
    long previous_layer_left_index = 0;
    long previous_layer_right_index = 0;
    
    Node *n;
    char *digest = malloc(sizeof(char) * 129);

    while (previous_layer_left_index < previous_layer_len) {

        cakelog("top of loop");

        previous_layer_right_index = previous_layer_left_index + 1;

        if (previous_layer_right_index < previous_layer_len) {

            cakelog("both left node and right node available");
                       
            cakelog("left node addr: %p, left node hash: [%s], right node addr: %p, right node hash: [%s]", previous_layer[previous_layer_left_index], previous_layer[previous_layer_left_index]->sha256_digest,  previous_layer[previous_layer_right_index], previous_layer[previous_layer_right_index]->sha256_digest);
            
            strcpy(digest, previous_layer[previous_layer_left_index]->sha256_digest);
            strcat(digest, previous_layer[previous_layer_right_index]->sha256_digest);

            cakelog("concatenated digest is: %s", digest);

            n = new_node(previous_layer[previous_layer_left_index], 
                         previous_layer[previous_layer_right_index], 
                         hexdigest(sha256(digest)));

        }

        else {
        
            cakelog("only have left node available");
            
            cakelog("left node addr: %p, left node digest: [%s]", previous_layer[previous_layer_left_index], previous_layer[previous_layer_left_index]->sha256_digest);
                     
            strcpy(digest, previous_layer[previous_layer_left_index]->sha256_digest);
            strcat(digest, previous_layer[previous_layer_left_index]->sha256_digest);

            cakelog("new node concatenated digest is: %s", digest);

            n = new_node(previous_layer[previous_layer_left_index], 
                         previous_layer[previous_layer_left_index], 
                         hexdigest(sha256(digest)));
        }

        next_layer[next_layer_index] = n;
        
        cakelog("added node at address %p to next_layer with an index of %ld", n, next_layer_index);
        
        next_layer_index++;
        previous_layer_left_index = previous_layer_right_index + 1;
    }
    
    return build_merkle_tree(next_layer, next_layer_index);
}

int main(int argc, char *argv[]) {

    int opt;

    while ((opt = getopt(argc, argv, "df")) != -1) {
        if ((unsigned char)opt == 'd') {
            /* debug without flush */
            cakelog_initialise(argv[0], false);
        }
        else if ((unsigned char)opt == 'f') {
            /* debug with flush */
            cakelog_initialise(argv[0], true);
        }
        else {
            printf("Usage: %s [-d] <datafile>\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

	if (optind >= argc) {
		printf("Missing filename (Usage: %s [-d] <datafile>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

    char *words = read_data_file(argv[optind]);
    long word_count = get_word_count(words);

    printf("building leaves\n");

    Node **leaves = build_leaves(words);
    Node *root = build_merkle_tree(leaves, word_count);
    printf("Root digest is: %s\n", root->sha256_digest);

    cakelog_stop();

}