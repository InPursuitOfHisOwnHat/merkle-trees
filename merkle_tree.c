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

// The cakelog library (https://github.com/chris-j-akers/cakelog) is used to
// write out a timestamped debug log.

#include "./cakelog/cakelog.h"

// A tree is made up of Nodes, and a Node can be implemented as a basic struct.
// Recursive `left` and `right` references to itself for the branches (or `NULL`
// if a leaf) and a `char*` for the data which, in this case, is a 64-character
// hash digest stored as a hexidecimal string.

struct Node {
    struct Node * left;
    struct Node * right;
    char * sha256_digest;
}; 

typedef struct Node Node;


// A basic C-style constructor reduces clutter when creating Nodes as the tree
// is being built.

Node* new_node(Node * left, Node * right, char * sha256_digest) {

    cakelog("===== new_node() =====");
    cakelog("left: %p, right: %p, hash: [%s]", left, right, sha256_digest);

    Node * node = malloc(sizeof(Node));
    node->left = left;
    node->right = right;
    node->sha256_digest = sha256_digest;

    cakelog("returning new node at address %p", node);
    
    return node;
}


// Load the file of data which will used to build the tree into a memory buffer
// and retrieve a pointer to it. In this case the data is expected to be a list
// of words separated by '\n' (newline)

// Linux system calls are used to open and read the contents of the file:

// open(): https://man7.org/linux/man-pages/man2/open.2.html 
// fstat(): https://man7.org/linux/man-pages/man2/fstat.2.html
// read(): https://man7.org/linux/man-pages/man2/read.2.html

// A pointer to the memory buffer where the data is stored is then returned.

char* read_data_file(const char *dict_file) {

    cakelog("===== read_dictionary_file() =====");

    const int dictionary_fd = open(dict_file, O_RDONLY);
    if (dictionary_fd == -1) {
        perror("open()");
        cakelog("failed to open file: '%s'", dict_file);
        exit(EXIT_FAILURE);
    }

    cakelog("opened file %s", dict_file);
    
    // fstat() is used to get the size of the file in bytes. This value can then
    // be passed to malloc() to request a block of memory large enought to store
    // the whole file and then it can be used in the call to read() to load
    // the entire file at once

    struct stat dict_stats;
    
    if (fstat(dictionary_fd, &dict_stats) == -1) {
        cakelog("failed to get statistics for dictionary file");
        perror("fstat()");
        exit(EXIT_FAILURE);
    }

    const long file_size = dict_stats.st_size;

    cakelog("file_size is %ld bytes", file_size);

    // Increment the buffer size by 1 byte to make room for the NULL terminator
    // which will be added once all the data has been loaded

    const long buffer_size = file_size + 1;
    char* buffer = malloc(buffer_size);
     
    // Now read the whole file into memory with one call to read().

    ssize_t bytes_read;
    if((bytes_read = read(dictionary_fd, buffer, file_size)) != file_size) {
        cakelog("unable to load file");
        perror("read()");
        exit(EXIT_FAILURE);
    }

    cakelog("loaded %ld bytes into buffer", bytes_read);

    close(dictionary_fd);

    // Remember to slot on the NULL terminator ('\0')

    buffer[file_size] = '\0';

    return buffer;
    
}


// A simple function to count all the words in the data buffer. It simply scans
// along the buffer one character at a time and increments a counter each time
// it finds a newline (`\n`) character.

long get_word_count(const char* data) {

    cakelog("===== get_word_count() =====");

    long word_count = 0;
    for (int i=0; data[i] != '\0'; i++) {
         if (data[i] == '\n') {
             word_count++;
         }
     }

    // Need to increment the counter one last time because the last word in the
    // buffer is followed by '\0' (NULL terminator), not '\n' so it wouldn't
    // have been counted.

     word_count++;

     cakelog("returning word count of %ld", word_count);
     
     return word_count;
}


// The OpenSSL hash function returns hashes in the form of an unsigned char*
// which is a set of 32 bytes that make up the hash-digest. In order to print
// this out to a more familiar looking string of hexadecimal symbols it must be
// converted into a standard char* of ASCII characters that can be passed to
// 'printf()' or similar.

char* hexdigest(const unsigned char *hash) {

    cakelog("===== hexdigest() =====");

    // Remember, a byte presented as hexadecimal is two characters so the new
    // char* needs to be twice as large as the current unsigned char* so all the
    // hexadecimal digits can fit. Plus one is added for the NULL terminator
    // ('\0')

    // 'SHA256_DIGEST_LENGTH' is defined in the OpenSSL 'sha.h' header. At the
    // time of writing it is 32 which makes the final string representation 64
    // characters long

    char *hexdigest = malloc((SHA256_DIGEST_LENGTH*2)+1);

    // A bit of pointer arithmetic is used to select the next empty space in the
    // buffer and then the next two digits are added using 'sprintf()'.

    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
		sprintf(hexdigest + (i * 2), "%02x", hash[i]);
    }

    // ...and the final NULL terminator

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