// The binary-trees benchmark: the C baseline of programs/binary_trees.at
// (one malloc per box, as the Astra version allocates one per Box.new).
#include <stdio.h>
#include <stdlib.h>

typedef struct Tree {
    int is_node;
    struct Tree *left, *right;
} Tree;

static Tree *leaf(void) {
    Tree *t = malloc(sizeof *t);
    t->is_node = 0;
    return t;
}

static Tree *make(int depth) {
    Tree *t = malloc(sizeof *t);
    t->is_node = 1;
    t->left = depth == 0 ? leaf() : make(depth - 1);
    t->right = depth == 0 ? leaf() : make(depth - 1);
    return t;
}

static long check(const Tree *t) {
    return t->is_node ? 1 + check(t->left) + check(t->right) : 0;
}

static void drop(Tree *t) {
    if (t->is_node) {
        drop(t->left);
        drop(t->right);
    }
    free(t);
}

int main(void) {
    int max_depth = 18;
    Tree *stretch = make(max_depth + 1);
    printf("stretch tree of depth %d\t check: %ld\n", max_depth + 1, check(stretch));
    drop(stretch);
    Tree *long_lived = make(max_depth);
    for (int depth = 4; depth <= max_depth; depth += 2) {
        long iterations = 1L << (max_depth - depth + 4);
        long sum = 0;
        for (long i = 0; i < iterations; i++) {
            Tree *t = make(depth);
            sum += check(t);
            drop(t);
        }
        printf("%ld\t trees of depth %d\t check: %ld\n", iterations, depth, sum);
    }
    printf("long lived tree of depth %d\t check: %ld\n", max_depth, check(long_lived));
    drop(long_lived);
    return 0;
}
