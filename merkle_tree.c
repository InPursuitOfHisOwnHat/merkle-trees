#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <string.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <mcheck.h>
#include <math.h>
#include <getopt.h>

// The OpenSSL library is used for the hashing functions. It needs to be
// installed separately:
//
// https://www.howtoforge.com/tutorial/how-to-install-openssl-from-source-on-linux/

#include <openssl/sha.h>
#include <openssl/evp.h>

// The cakelog library (https://github.com/chris-j-akers/cakelog) is used to
// write out a timestamped debug log. The cakelog.c file needs to be compiled
// with any program that uses it along with the -lm switch to include the Maths
// library

#include "./cakelog/cakelog.h"

// A tree is made up of Nodes and a Node can be implemented as a basic struct.
// The struct is made up of recursive 'left' and 'right' references to itself for
// the branches (or 'NULL' if a leaf) and a 'char*' for the data which, in this
// case, is a 64-character hash digest stored as a hexidecimal string.

struct Node {
    struct Node * left;
    struct Node * right;
    char * sha256_digest;
}; 

typedef struct Node Node;

// A basic C-style constructor reduces clutter when creating Nodes as the tree
// is being built.

Node* new_node(Node *left, Node *right, char *sha256_digest) {

    cakelog("===== new_node() =====");
    cakelog("left: %p, right: %p, hash: [%s]", left, right, sha256_digest);

    Node * node = malloc(sizeof(Node));
    node->left = left;
    node->right = right;
    node->sha256_digest = sha256_digest;

    cakelog("returning new node at address %p", node);
    
    return node;
}

// Load the data used to populate the tree. For this test program, the
// data is expected to be a series of words separated by an '\n' (newline)
// character in a file. The folder /test-data contains some examples, including
// a large 5MB file that contains the English dictionary.

// Linux system calls are used to open and read the contents of the file:

//      open(): https://man7.org/linux/man-pages/man2/open.2.html 
//      fstat(): https://man7.org/linux/man-pages/man2/fstat.2.html
//      read(): https://man7.org/linux/man-pages/man2/read.2.html

// A pointer to the data is stored is then returned.

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

    // Increment the buffer size by 1 to make room for the NULL terminator which
    // will be added once all the data has been loaded

    const long buffer_size = file_size + 1;
    char* buffer = malloc(buffer_size);
     
    // Now read the whole file into memory with one call to read(). Any failure
    // will result in the program aborting.

    ssize_t bytes_read;
    if((bytes_read = read(dictionary_fd, buffer, file_size)) != file_size) {
        cakelog("unable to load file");
        perror("read()");
        exit(EXIT_FAILURE);
    }

    cakelog("loaded %ld bytes into buffer", bytes_read);

    close(dictionary_fd);

    // Add on the NULL terminator ('\0')

    buffer[file_size] = '\0';

    return buffer;
    
}

// A simple function to count all the words in the data buffer. It scans the
// buffer one character at a time and increments a counter each time it finds a
// newline ('\n') character.

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

// 'sha256()' generates a hash from the 'char*' provided in the 'buffer'
// parameter and returns a new 'char*' pointing to that hash. It uses the
// OpenSSL EVP (or Digital EnVeloPe) interface which provides a high-level way
// to interact with the various hashing and cryptography functions provided by
// the OpenSSL library. In order to use these functions, the library must first
// be installed on the system and the following include directives added to the
// relevant source file (see above):
//
//      #include <openssl/sha.h>
//      #include <openssl/evp.h>
//
// When compiling, the '-lssl' and '-lcrypto' switches must also be used to
// link the OpenSSL libraries.

unsigned char* sha256(const char *data) {

    cakelog("===== sha256() =====");

    unsigned int data_len = strlen(data);

    // Declare an EnVeloPe Message Digest Context pointer 

    EVP_MD_CTX *mdctx;

    cakelog("initialising new mdctx");

    // Allocate memory for 'mdctx' using the special allocation function provided by
    // OpenSSL (not c's 'new').

    mdctx = EVP_MD_CTX_new();

    // Initialise 'mdctx' by setting it to use the OpenSSL EVP_sha256() function
    // which is the type of hash used in this Merkle Tree. In theory,
    // the 'EVP_sha256()' parameter, here, can be swapped out to use any other
    // hashing function available in the OpenSSL EVP interface (e.g. EVP_md5()
    // or EVP_sha512()) 

    EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL);

    // Now pass the raw data to be hashed along with it's length to the
    // EVP_DigestUpdate() function. Note that this function can be called
    // multiple times, each time adding more data before retrieving the final
    // hash. For instance, if the data to be hashed is being streamed from a
    // socket and read in chunks, each chunk can be passed to this function
    // until the end of the stream is reached. Here, the function is only called
    // once because all the data needed is in the 'data' parameter.

    cakelog("updating mdctx digest with data [%s]", data);
    EVP_DigestUpdate(mdctx, data, data_len);
    
    // All the data has been gathered and entered. Now to allocate space for the
    // hash itself. OpenSSL provides its own version of 'malloc()'. The
    // 'EVP_sha256()' function passed to 'OPENSSL_malloc()' simply returns a
    // SHA256 version of the 'EVP_MD' data-structure and the 'EVP_MD_size()'
    // function wrapped around it returns the size of that data structure (a
    // kind of 'sizeof' function). Obviously, then, both these functions called
    // in this way provide the size information required by 'OPENSSL_malloc()'.

    cakelog("initialising new hash_digest buffer");
    unsigned char* hash_digest = OPENSSL_malloc(EVP_MD_size(EVP_sha256()));
    
    // Now copy the generated hash into the 'hash_digest' char* declared and
    // initialised above.

    EVP_DigestFinal_ex(mdctx, hash_digest, &data_len);
    cakelog("succesfully copied new digest to hash_digest buffer");
    
    // Use the free function provided by the EVP interface to free up the
    // message digest context.

    EVP_MD_CTX_free(mdctx);
    cakelog("successfully freed mdctx digest");

    return hash_digest;

}

// The sha256() hash function returns hashes straight from OpenSSL in the form
// of an unsigned char*, a set of 32 bytes that make up the hash-digest. In
// order to print this as the more familiar looking string of hexadecimal
// symbols it must be converted to a standard char* that can be passed to
// 'printf()' or similar.

char* hexdigest(const unsigned char *hash) {

    cakelog("===== hexdigest() =====");

    // A byte presented in hexadecimal is two characters, so the length of the
    // new char* needs to be twice as large as the current unsigned char*, but
    // with an extra character to make room for the NULL terminator ('\0')

    // 'SHA256_DIGEST_LENGTH' is defined in the OpenSSL 'sha.h' header. At the
    // time of writing it is 32 which makes the final string representation 64
    // characters long (65 with the NULL terminator ('\0')

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

// To start building the tree a list of Nodes is required to act at the bottom
// layer, or leaves. This function scans the buffer of words read in during
// 'read_data_file()' and adds them (or, rather, pointers to them) to new Node
// objects. The leaves are returned as a chain of Node pointers ready to be
// turned into a tree.

Node** build_leaves(char* buffer) {

    cakelog("===== build_leaves() =====");
  
    // Because the list of words in the test data is of fixed length and, thanks
    // to the way they're separated, easy to count, enough memory to store all
    // the Node pointers in the leaves can be pre-allocated with one call to
    // malloc(). get_word_count() can be used to establish how may words there are
    // in the data which will reveal how many Nodes are needed and, therefore,
    // how much memory to allocate.

    long word_count = get_word_count(buffer);
    Node **leaves = malloc(sizeof(Node*)*word_count);

    cakelog("allocated %ld bytes for %ld leaves (number of leaves * sizeof(pointer) + NULL terminator", word_count * sizeof(unsigned char*) + 1, word_count);

    long index = 0;
    long hash_count = 0;
    Node *n;

    cakelog("beginning loop through buffer using strtok");

    // Words are pulled out of the data buffer using strtok()
    // (https://man7.org/linux/man-pages/man3/strtok.3.html) which splits a
    // block of char data into it's component words (or tokens), according to
    // the delimeters provided (in this case `\n` for each word and `\0` to get
    // the last word in the buffer).

    char * word = strtok(buffer, "\n\0");
    
    while( word != NULL) {

        cakelog("next word is [%s]", word);

        // A new node is built out of the word. To get the hash of each
        // word the sha256() function is called which wraps calls to the OpenSSL
        // library's own SHA256 hashing functions. This call is further wrapped
        // by another call to hexdigest() to get the text representation of the
        // hash digest.
        //
        // Left and right Nodes are assigned NULL because this is the bottom
        // layer of the tree - there are no branches beneath it

        n = new_node(NULL, NULL, hexdigest(sha256(word)));
        leaves[index] = n;
        hash_count++;
        index++;
        word = strtok(NULL, "\n\0");

    }

    cakelog("returning %ld leaves", index);

    return leaves;
}

// build_merkle_tree() builds our Merkle Tree recursively, layer by layer from
// the bottom up. It returns a pointer to the 'Node' at the root of the tree.
// This Node will contain the hash of the entire data-set.
//
// The 'previous_layer' parameter is a pointer-chain of 'Node' objects that are
// used to build the next layer of nodes. The first time this function is
// called, 'previous_layer' will contain the leaves, or bottom layer, of our
// tree (see build_leaves() function)

// The 'previous_layer_len' parameter contains the number of 'Node' pointers in
// 'previous_layer'. This value is used to calculate the amount of memory needed
// when allocating our new layer.

Node* build_merkle_tree(Node **previous_layer, long previous_layer_len) {

    cakelog("===== build_merkle_tree() =====");

    // If the number of Nodes in the previous layer is just 1 then
    // previous_layer is already at the root of the tree.

    if (previous_layer_len == 1) {

        cakelog("previous_layer_len is 1 so we have root. Returning previous_layer[0] at address %p", previous_layer[0]);

        return previous_layer[0];
    }
   
    // A Merkle Tree is also a Perfect Binary Tree
    // (https://www.programiz.com/dsa/perfect-binary-tree) so, in theory, new
    // layers should have half the number of nodes as their previous layer
    // meaning dividing 'previous_layer_len' by 2 should get the right number of
    // slots required for eah Node. A problem arises, though, if the previous
    // layer has an odd number of nodes. In C, integer division rounds down
    // (e.g. 5 / 2 = 1) but this time it needs to round up (if there is an odd
    // number of Nodes in the previous layer, the last, orphaned Node is
    // duplicated so that it can form both the left and right branches of the
    // node above it). 
    //
    // The 'ceil()' function from the 'Math.h' library will round up integer
    // division so it's now possible to allocate the correct amount of memory
    // required for the number of Nodes in this layer.

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

        // If the previous layer has an odd number of Nodes then the
        // final iteration of this while loop will only be able to pull out a valid
        // left Node, there won't be a right node. With Merkle Trees, this means
        // the left Node is duplicated and use for both the 'left' and 'right'
        // branches of the Node (see the 'else' branch)

        if (previous_layer_right_index < previous_layer_len) {

            cakelog("both left node and right node available");
                       
            cakelog("left node addr: %p, left node hash: [%s], right node addr: %p, right node hash: [%s]", previous_layer[previous_layer_left_index], previous_layer[previous_layer_left_index]->sha256_digest,  previous_layer[previous_layer_right_index], previous_layer[previous_layer_right_index]->sha256_digest);
            
            // The hash digests from the left and right nodes are concatenated
            // into 'digest' ready to be hashed.
            //
            // Yes, there are far safer ways to do this but come on, now, shush.

            strcpy(digest, previous_layer[previous_layer_left_index]->sha256_digest);
            strcat(digest, previous_layer[previous_layer_right_index]->sha256_digest);

            cakelog("concatenated digest is: %s", digest);

            // New Node is created with calls to sha256() and hexidigest()
            //
            // A call to hexidigest is not strictly necessary at this point and
            // is even inefficient, but this is a small experimental program and
            // it's good to be able to observe things properly in the debug log.
            // Ordinarily, this only needs to be done when the root node is
            // being displayed

            n = new_node(previous_layer[previous_layer_left_index], 
                         previous_layer[previous_layer_right_index], 
                         hexdigest(sha256(digest)));

        }
        else {

            // There is an odd number of Nodes and the final Node needs to be
            // duplicated, but otherwise the process is the same
            
            cakelog("only have left node available");
            
            cakelog("left node addr: %p, left node digest: [%s]", previous_layer[previous_layer_left_index], previous_layer[previous_layer_left_index]->sha256_digest);

            strcpy(digest, previous_layer[previous_layer_left_index]->sha256_digest);
            strcat(digest, previous_layer[previous_layer_left_index]->sha256_digest);

            cakelog("new node concatenated digest is: %s", digest);

            n = new_node(previous_layer[previous_layer_left_index], 
                         previous_layer[previous_layer_left_index], 
                         hexdigest(sha256(digest)));
        }

        // Add the new Node to the next empty slot in the layer

        next_layer[next_layer_index] = n;
    
        cakelog("added node at address %p to next_layer with an index of %ld", n, next_layer_index);
        next_layer_index++;

        previous_layer_left_index = previous_layer_right_index + 1;
    }
    
    // The recursive call where the next layer becomes the previous layer

    return build_merkle_tree(next_layer, next_layer_index);
}

// The program uses the Cakelog logger
// (https://github.com/chris-j-akers/cakelog)) which outputs timestamped
// information to a log file, but this is optional as logging slows the program
// down, especially if flush is forced.
//
// Usage (assuming the executable is called 'mtree') is: 
//
//      mtree [-d|-f] <datafile>
//
// Where <datafile> is the name of an input file that contains a list of words
// on each line, -d is a request to trace output to a file, and -f is to trace
// output to a file but also force Cakelog to flush the file each time it's
// written to (can add considerable processing time).
//
// The datafile should be a file of words or text separated by a newline
// character. There are some examples in the ./test-data folder of this repo.

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

    printf("reading file %s\n", argv[optind]);

    char *words = read_data_file(argv[optind]);
    long word_count = get_word_count(words);

    printf("read %ld words into buffer\n", word_count);

    printf("building leaves...\n");

    Node **leaves = build_leaves(words);

    printf("building tree ...\n");
    Node *root = build_merkle_tree(leaves, word_count);

    // Finally
    printf("\n");
    printf("================================================================================\n");
    printf("Root digest is: %s\n", root->sha256_digest);
    printf("================================================================================\n");

    cakelog_stop();

}