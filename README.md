# Merkle Trees in C

---

- [Merkle Trees in C](#merkle-trees-in-c)
  - [Introduction](#introduction)
  - [Building a Merkle Tree](#building-a-merkle-tree)
  - [Merkle Trees](#merkle-trees)
  - [Running mtree](#running-mtree)
  - [Changing the Data](#changing-the-data)
  - [How Does it Work?](#how-does-it-work)
    - [Input Data](#input-data)
    - [Program Flow](#program-flow)

---
## Introduction

There's plenty of detailed information about Merkle Trees all over the interwebs and it's not going to be regurgitated, here. A simple online search will turn up any number of articles with varying levels of detail (and, frankly, quality). As usual, [Wikipedia](https://en.wikipedia.org/wiki/Merkle_tree) is as good a place to start as any. There's also a particularly good one by Marc Clifton at [The Code Project](https://www.codeproject.com/Articles/1176140/Understanding-Merkle-Trees-Why-use-them-who-uses-t).

Broadly, the problem Merkle Trees try to solve is that of data integrity and consistency. Imagine a large dataset maintained by multiple, untrusting, decentralised parties who each store their own version. All these versions must remain in sync. How?

One solution is for each party to constantly receive copies of everyone else's dataset and run byte-by-byte comparisons against their own. Obviously, though, this is horrifically inefficient. The bandwidth costs alone can make this solution prohibitive. They could mitigate this by agreeing to do run a synchronisation just once per day or once per month, but what if this isn't frequent enough? What if the application the parties are serving requires near real-time consistency?

A more practical way is for a hashing algorithm to be run against each dataset and the resulting digest sent to every party for comparison. This will be a much smaller value (for instance, the SHA256 hashing algorithm is just 32-bytes in size). If one party has computed a different hash from their dataset when compared to another party, clearly they are out of sync and work must be done to remediate.

There is still one problem, though. While parties are better off with this solution because datasets with matching hash values need no work at all, for hash values that don't match, large amounts of bandwidth and processor time are still being used to locate and patch differences in datasets. All a mismatched hash does is tell a party there is a problem **somewhere** in their data. They've still got to scan it all to find it. This is a waste if the differences are tiny.

The next stage, then, is to split the dataset into blocks and compute a hash value for each block. Now, every party has a list of hashes to compare and it only needs to re-synchronise blocks where the hash comparisons fail. There are still multiple hashes flying around the system, though, and while it's, no doubt, faster, parties must maintain lists of which hashes represent which blocks and maintain these lists.

So, the final step? A Merkle Tree.

## Building a Merkle Tree

A list of blocks and their associated hashes have been prepared. The hashes are extracted into a list which forms the bottom layer, or leaves, of the Merkle Tree. The list is then split into pairs of hashes and the left and right hashes are concatenated, then hashed again. These new hashes form the next layer up of the tree. This process continues until there is only one hash left, known as the root hash.

For any layer with an odd number of hashes (that is, a layer that cannot be split into left and right pairs evenly), the last hash is duplicated and used as the right hand hash pair.

---

## Merkle Trees


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