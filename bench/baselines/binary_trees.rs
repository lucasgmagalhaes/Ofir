// Binary trees: the Rust baseline of programs/binary_trees.of.
struct Tree {
    children: Option<(Box<Tree>, Box<Tree>)>,
}

fn make(depth: u32) -> Box<Tree> {
    let children = if depth == 0 {
        (Box::new(Tree { children: None }), Box::new(Tree { children: None }))
    } else {
        (make(depth - 1), make(depth - 1))
    };
    Box::new(Tree { children: Some(children) })
}

fn check(tree: &Tree) -> i64 {
    match &tree.children {
        None => 0,
        Some((left, right)) => 1 + check(left) + check(right),
    }
}

fn main() {
    let max_depth = 18;
    let stretch = make(max_depth + 1);
    println!("stretch tree of depth {}\t check: {}", max_depth + 1, check(&stretch));
    drop(stretch);
    let long_lived = make(max_depth);
    for depth in (4..=max_depth).step_by(2) {
        let iterations = 1_i64 << (max_depth - depth + 4);
        let mut sum = 0;
        for _ in 0..iterations {
            sum += check(&make(depth));
        }
        println!("{}\t trees of depth {}\t check: {}", iterations, depth, sum);
    }
    println!("long lived tree of depth {}\t check: {}", max_depth, check(&long_lived));
}
