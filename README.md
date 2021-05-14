# Merkle Trees in C

- [Merkle Trees in C](#merkle-trees-in-c)
  - [Introduction](#introduction)
  - [Merkle Trees](#merkle-trees)
    - [Building a Merkle Tree](#building-a-merkle-tree)
    - [Why Merkle Trees?](#why-merkle-trees)
  - [Running mtree](#running-mtree)
  - [Changing the Data](#changing-the-data)
  - [How Does it Work?](#how-does-it-work)
    - [Input Data](#input-data)
    - [Program Flow](#program-flow)

---
## Introduction

There's plenty of detailed information about Merkle Trees all over the interwebs and it's not going to be regurgitated, here. A simple online search will turn up any number of articles with varying levels of detail (and, frankly, quality). As usual, [Wikipedia](https://en.wikipedia.org/wiki/Merkle_tree) is as good a place to start as any. 

The problem Merkle Trees try to solve is that of data integrity. Imagine a large dataset maintained by multiple, decentralised parties who each store their own copy. They must all make sure they have exactly the same copy. How?

One solution is for each party to constantly receive copies of everyone else's dataset and run byte-by-byte comparisons against their own. Obviously, though, this is horrifically inefficient. The bandwidth costs alone make this solution prohibitive.

A more practical way is for a hashing algorithm to be run against each dataset and the resulting digest sent to each party for comparison. This will be a much smaller value (for instance, the SHA256 hashing algorithm is just 32-bytes in size). If one party has computed a different hash from their dataset when compared to another party, clearly they are out of sync and work must be done to remediate.




Note that, this exercise does not particularly care what happens



 If compared to  compared can be run against the entire dataset and resulting digest can be compared with the hash digests of the other datasets. If they are the same, then no action is required. If they are different, then the parties can work together to plug the differences.

There is still one further problem, though. What if the dataset is particularly big? In order to bring 




This is a fairly basic implementation of a Merkle Tree in C using the words in a plain-text English dictionary file as data-blocks. Frankly, why anyone would want to do this in real-life ... well, you got me, but using this list of words instead of actual blocks of binary data, which I'd have to generate or fake, means 1) I have a ready-made, easily accessible and meaty set of data-blocks using little to no effort (over 450,000 words in my file, so 450,000 blocks) and 2) it's easy to watch what the program is doing with only a debugger, an online hash function and plenty of logging. The purpose of this exercise is as a self-learning tool to deep-dive the structure of a Merkle Tree and the mechanism for building it. The data is not important.

## Merkle Trees


I'm not going to regurgitate a lot of this material here, but I will talk about how a Merkle Tree is constructed and why they're a good idea for data verification.

### Building a Merkle Tree

To build a Merkle Tree a dataset must be split into blocks and a hash function run against each of those blocks. The resulting hash digests are stored in a list of nodes that form the bottom layer, or leaves, of the Merkle Tree. This layer is split into pairs of nodes and the hash digests stored in each left and right pair concatenated. The result of the two concatenated values is hashed again, and this hash forms the hash digest of the node in the layer above, and in between, the concatenated pair. 

For a layer with an odd number of nodes (that is, a layer that cannot be split into left and right pairs evenly), the last node is duplicated and used as the right hand node of the final pair.

Each layer of the tree is built in this way until there’s only one node left - the root node. The data in this root node contains the hash digest of the entire tree and acts as a kind of fingerprint for all the data in that tree.

### Why Merkle Trees?

Imagine a large dataset being maintained by multiple parties and each party must make sure that it has exactly the same copy of this dataset. We *could* get each party to run a byte-by-byte comparison against everyone else's dataset, but this would be time consuming and inefficient, especially if the dataset is liable to change frequently.

Instead, each party builds a Merkle Tree out of their own version of the dataset, takes the 32-byte hash (or even the 64-byte hexadecimal string) from the root of their Merkle Tree and compares this root hash digest with everyone else's. Much smaller, much faster, less complicated. Any differences at all in the dataset, no matter how minor, will result in a very different root hash digest. Something I'm going to prove when I run `mtree`, below.

## Running mtree

To get the root hash of our data set, we build and run `mtree`, passing in the location of our data file.

![]({{ site.url }}{{ site.baseurl }}site-assets/images/merkle-trees/run-the-program-1.png)

As you can see, the root hash of our dataset (466,549 words) is:
```
bc4550eaefb5c8cc2ea917f3533b1e4635ffa232555de1d80f82634514223a35
```
This hash digest will never change no matter how many times I run the program against the same set of data, and being only 64-characters long (or 32-bytes internally) this 'fingerprint' from our dataset, can be easily passed to other parties and used in comparison with their own Merkle Tree root hash to confirm they have exactly the same data. What's more, I'm using the SHA256 hash algorithm (from the OpenSSL library), a well-known standard, so as long as other parties are also using the SHA256 hash algorithm when building their tree they don't even need to use my `mtree`, they can write their own program in whichever language they want.

To prove this I ran a few examples where I changed the dataset slightly to see what happened.

## Changing the Data

First, say I erased the word: `War` from the file (Lennon would be proud!)

![]({{ site.url }}{{ site.baseurl }}site-assets/images/merkle-trees/run-the-program-2.png)

If we run the program again, the hash result is not just different, but completely different.

![]({{ site.url }}{{ site.baseurl }}site-assets/images/merkle-trees/run-the-program-3.png)

I can also put `War` back into the file in exactly the same place and I will get my original root hash digest.

Even changing a single letter in our dataset will result in a very different root hash digest. For instance, adding an extra `n` to the word `banana`.

![]({{ site.url }}{{ site.baseurl }}site-assets/images/merkle-trees/run-the-program-4.png)

Still, a very different hash result:

![]({{ site.url }}{{ site.baseurl }}site-assets/images/merkle-trees/run-the-program-5.png)

So, this is the power of Merkle Trees. No matter how big your dataset is, you can reduce it to a single hash value and that value can be used to compare with other datasets to verify they are identical.

This data-structure is used heavily in blockchains and can also be used to verify large data transfers, especially when those transfers split the data up into blocks.

## How Does it Work?

This section talks about the implementation of `mtree` and the test data used.

### Input Data

The input data was downloaded from: [https://github.com/dwyl/english-words](https://github.com/dwyl/english-words).

It's a plain text file with each word on it's own line (or, to put it another way, a list of words separated by a newline (`\n`) character).

There are 466,549 words in the file.

### Program Flow

Basically, I wanted the program to:

1. Read in the data file of words
1. Build a list of leaves out of those words (the bottom layer of the tree)
1. Build the Merkle Tree from the list of leaves
1. Print out the hash digest from the top node of the tree

I'm not going to be printing or walking the tree in this exercise, just building it. A future post will deal with these functions.
 
For hashing, I use the [OpenSSL Library](https://www.openssl.org/source/) SHA256 functions.