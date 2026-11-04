// Binary trees: the Go baseline of programs/binary_trees.of.
package main

import "fmt"

type Tree struct {
	left, right *Tree
}

func makeTree(depth int) *Tree {
	if depth == 0 {
		return &Tree{left: &Tree{}, right: &Tree{}}
	}
	return &Tree{left: makeTree(depth - 1), right: makeTree(depth - 1)}
}

func check(tree *Tree) int64 {
	if tree.left == nil {
		return 0
	}
	return 1 + check(tree.left) + check(tree.right)
}

func main() {
	maxDepth := 18
	stretch := makeTree(maxDepth + 1)
	fmt.Printf("stretch tree of depth %d\t check: %d\n", maxDepth+1, check(stretch))
	longLived := makeTree(maxDepth)
	for depth := 4; depth <= maxDepth; depth += 2 {
		iterations := int64(1) << (maxDepth - depth + 4)
		var sum int64
		for i := int64(0); i < iterations; i++ {
			sum += check(makeTree(depth))
		}
		fmt.Printf("%d\t trees of depth %d\t check: %d\n", iterations, depth, sum)
	}
	fmt.Printf("long lived tree of depth %d\t check: %d\n", maxDepth, check(longLived))
}
