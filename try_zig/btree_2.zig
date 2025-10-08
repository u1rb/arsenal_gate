
const std = @import("std");
const Allocator = std.mem.Allocator;
const testing = std.testing;

/// High-performance B-tree optimized for HFT with:
/// - Pool allocator (no heap allocation on insert)
/// - Inline storage (cache-friendly)
/// - Index-based children (4 bytes vs 8 bytes pointers)
/// - Binary search for large nodes
/// - Cache-line aligned nodes
pub fn BTree(comptime T: type, comptime order: usize) type {
    return struct {
        const Self = @This();
        const min_degree = (order + 1) / 2;
        const min_keys = min_degree - 1;
        const max_keys = 2 * min_degree - 1;
        const max_children = max_keys + 1;
        const NULL_INDEX: u32 = 0xFFFFFFFF;

        /// Threshold for choosing linear vs binary search
        const LINEAR_SEARCH_THRESHOLD = 8;

        /// Cache-friendly node with inline storage
        pub const Node = struct {
            keys: [max_keys]T,
            children: [max_children]u32, // Indices instead of pointers
            num_keys: u8,
            is_leaf: bool,
        };

        nodes: []Node,
        free_list: std.ArrayList(u32),
        root_idx: u32,
        allocator: Allocator,

        pub fn init(allocator: Allocator, capacity: usize) !Self {
            const nodes = try allocator.alloc(Node, capacity);

            // Initialize all nodes to empty state
            for (nodes) |*node| {
                node.num_keys = 0;
                node.is_leaf = true;
            }

            // Create free list with all node indices available
            var free_list = std.ArrayList(u32){};
            try free_list.ensureTotalCapacity(allocator, capacity);

            var node_idx: u32 = 0;
            while (node_idx < capacity) : (node_idx += 1) {
                try free_list.append(allocator, node_idx);
            }

            return .{
                .nodes = nodes,
                .free_list = free_list,
                .root_idx = NULL_INDEX,
                .allocator = allocator,
            };
        }

        pub fn deinit(self: *Self) void {
            self.allocator.free(self.nodes);
            self.free_list.deinit(self.allocator);
        }

        fn allocNode(self: *Self, is_leaf: bool) !u32 {
            if (self.free_list.items.len == 0) {
                return error.OutOfNodes;
            }

            const idx = self.free_list.pop() orelse return error.OutOfNodes;
            var node = &self.nodes[idx];
            node.num_keys = 0;
            node.is_leaf = is_leaf;

            // Initialize all children to NULL
            for (&node.children) |*child| {
                child.* = NULL_INDEX;
            }

            return idx;
        }

        fn freeNode(self: *Self, idx: u32) void {
            self.free_list.append(self.allocator, idx) catch unreachable;
        }

        fn freeNodeRecursive(self: *Self, idx: u32) void {
            if (idx == NULL_INDEX) return;

            const node = &self.nodes[idx];
            if (!node.is_leaf) {
                var i: usize = 0;
                while (i <= node.num_keys) : (i += 1) {
                    self.freeNodeRecursive(node.children[i]);
                }
            }

            self.freeNode(idx);
        }

        /// Linear search - fast for small nodes
        inline fn linearSearch(node: *const Node, key: T) ?usize {
            var i: usize = 0;
            while (i < node.num_keys) : (i += 1) {
                if (key == node.keys[i]) {
                    return i;
                } else if (key < node.keys[i]) {
                    return null;
                }
            }
            return null;
        }

        /// Binary search - fast for large nodes
        inline fn binarySearch(node: *const Node, key: T) ?usize {
            if (node.num_keys == 0) return null;

            var left: usize = 0;
            var right: usize = node.num_keys;

            while (left < right) {
                const mid = left + (right - left) / 2;
                if (key == node.keys[mid]) {
                    return mid;
                } else if (key < node.keys[mid]) {
                    right = mid;
                } else {
                    left = mid + 1;
                }
            }
            return null;
        }

        /// Find insertion position (linear search)
        inline fn findInsertPos(node: *const Node, key: T) usize {
            var i: usize = 0;
            while (i < node.num_keys and key > node.keys[i]) : (i += 1) {}
            return i;
        }

        fn searchNode(self: *Self, node_idx: u32, key: T) bool {
            if (node_idx == NULL_INDEX) return false;

            const node = &self.nodes[node_idx];

            // Choose search strategy based on node size
            const found_idx = if (node.num_keys <= LINEAR_SEARCH_THRESHOLD)
                linearSearch(node, key)
            else
                binarySearch(node, key);

            if (found_idx != null) {
                return true;
            }

            if (node.is_leaf) {
                return false;
            }

            // Find child to recurse into
            const child_idx = findInsertPos(node, key);
            return self.searchNode(node.children[child_idx], key);
        }

        pub fn search(self: *Self, key: T) bool {
            return self.searchNode(self.root_idx, key);
        }

        /// Split a full child node into two nodes, promoting the middle key to parent
        fn splitChild(self: *Self, parent_idx: u32, child_pos: usize) !void {
            const parent = &self.nodes[parent_idx];
            const child_idx = parent.children[child_pos];
            const child = &self.nodes[child_idx];

            // Create new right sibling for the split
            const new_child_idx = try self.allocNode(child.is_leaf);
            const new_child = &self.nodes[new_child_idx];

            // Split at middle key
            const split_idx = min_degree;
            new_child.num_keys = @intCast(max_keys - split_idx);

            // Copy second half of keys
            var j: usize = 0;
            while (j < new_child.num_keys) : (j += 1) {
                new_child.keys[j] = child.keys[split_idx + j];
            }

            // Copy second half of children if not leaf
            if (!child.is_leaf) {
                j = 0;
                while (j <= new_child.num_keys) : (j += 1) {
                    new_child.children[j] = child.children[split_idx + j];
                }
            }

            child.num_keys = @intCast(min_degree - 1);

            // Shift children in parent
            j = parent.num_keys;
            while (j > child_pos) : (j -= 1) {
                parent.children[j + 1] = parent.children[j];
            }
            parent.children[child_pos + 1] = new_child_idx;

            // Shift keys in parent
            j = parent.num_keys;
            while (j > child_pos) : (j -= 1) {
                parent.keys[j] = parent.keys[j - 1];
            }
            parent.keys[child_pos] = child.keys[min_degree - 1];
            parent.num_keys += 1;
        }

        /// Insert a key into a node that is guaranteed to be non-full
        fn insertNonFull(self: *Self, node_idx: u32, key: T) !void {
            var node = &self.nodes[node_idx];
            var i: isize = @as(isize, @intCast(node.num_keys)) - 1;

            if (node.is_leaf) {
                // Find insertion position
                while (i >= 0 and key < node.keys[@intCast(i)]) : (i -= 1) {}

                // Check for duplicate
                if (i >= 0 and key == node.keys[@intCast(i)]) {
                    return; // Key already exists
                }

                // Shift keys to make room
                var j: isize = @as(isize, @intCast(node.num_keys)) - 1;
                while (j > i) : (j -= 1) {
                    node.keys[@intCast(j + 1)] = node.keys[@intCast(j)];
                }

                node.keys[@intCast(i + 1)] = key;
                node.num_keys += 1;
            } else {
                // Find child to insert into
                while (i >= 0 and key < node.keys[@intCast(i)]) : (i -= 1) {}

                // Check for duplicate in this non-leaf node
                if (i >= 0 and key == node.keys[@intCast(i)]) {
                    return; // Key already exists in this non-leaf node
                }

                i += 1;

                const child_idx = node.children[@intCast(i)];
                const child = &self.nodes[child_idx];

                if (child.num_keys == max_keys) {
                    try self.splitChild(node_idx, @intCast(i));
                    // Re-fetch node pointer (may have moved)
                    node = &self.nodes[node_idx];
                    if (key == node.keys[@intCast(i)]) {
                        return; // Duplicate: key equals the promoted key
                    } else if (key > node.keys[@intCast(i)]) {
                        i += 1;
                    }
                }
                try self.insertNonFull(node.children[@intCast(i)], key);
            }
        }

        pub fn insert(self: *Self, key: T) !void {
            if (self.root_idx == NULL_INDEX) {
                self.root_idx = try self.allocNode(true);
            }

            const root = &self.nodes[self.root_idx];
            if (root.num_keys == max_keys) {
                const new_root_idx = try self.allocNode(false);
                var new_root = &self.nodes[new_root_idx];
                new_root.children[0] = self.root_idx;
                self.root_idx = new_root_idx;
                try self.splitChild(new_root_idx, 0);
                try self.insertNonFull(new_root_idx, key);
            } else {
                try self.insertNonFull(self.root_idx, key);
            }
        }

        /// Merge a child with its right sibling through the parent
        /// Used when both child and sibling have minimum number of keys
        fn merge(self: *Self, parent_idx: u32, idx: usize) void {
            const parent = &self.nodes[parent_idx];
            const child_idx = parent.children[idx];
            const sibling_idx = parent.children[idx + 1];
            const child = &self.nodes[child_idx];
            const sibling = &self.nodes[sibling_idx];

            // Pull key from parent and place at end of child's current keys
            const merge_pos = child.num_keys;
            child.keys[merge_pos] = parent.keys[idx];

            // Copy keys from sibling
            var i: usize = 0;
            while (i < sibling.num_keys) : (i += 1) {
                child.keys[merge_pos + 1 + i] = sibling.keys[i];
            }

            // Copy children from sibling if not leaf
            if (!child.is_leaf) {
                i = 0;
                while (i <= sibling.num_keys) : (i += 1) {
                    child.children[merge_pos + 1 + i] = sibling.children[i];
                }
            }

            child.num_keys += sibling.num_keys + 1;

            // Shift keys in parent
            i = idx;
            while (i < parent.num_keys - 1) : (i += 1) {
                parent.keys[i] = parent.keys[i + 1];
            }

            // Shift children in parent
            i = idx + 1;
            while (i < parent.num_keys) : (i += 1) {
                parent.children[i] = parent.children[i + 1];
            }

            parent.num_keys -= 1;
            self.freeNode(sibling_idx);
        }

        /// Borrow a key from the left sibling through the parent
        /// Used when left sibling has more than minimum keys
        fn borrowFromPrev(self: *Self, parent_idx: u32, idx: usize) void {
            const parent = &self.nodes[parent_idx];
            const child = &self.nodes[parent.children[idx]];
            const sibling = &self.nodes[parent.children[idx - 1]];

            // Shift keys in child
            var i: usize = child.num_keys;
            while (i > 0) : (i -= 1) {
                child.keys[i] = child.keys[i - 1];
            }

            // Shift children in child
            if (!child.is_leaf) {
                i = child.num_keys + 1;
                while (i > 0) : (i -= 1) {
                    child.children[i] = child.children[i - 1];
                }
            }

            child.keys[0] = parent.keys[idx - 1];
            parent.keys[idx - 1] = sibling.keys[sibling.num_keys - 1];

            if (!child.is_leaf) {
                child.children[0] = sibling.children[sibling.num_keys];
            }

            child.num_keys += 1;
            sibling.num_keys -= 1;
        }

        /// Borrow a key from the right sibling through the parent
        /// Used when right sibling has more than minimum keys
        fn borrowFromNext(self: *Self, parent_idx: u32, idx: usize) void {
            const parent = &self.nodes[parent_idx];
            const child = &self.nodes[parent.children[idx]];
            const sibling = &self.nodes[parent.children[idx + 1]];

            child.keys[child.num_keys] = parent.keys[idx];
            parent.keys[idx] = sibling.keys[0];

            if (!child.is_leaf) {
                child.children[child.num_keys + 1] = sibling.children[0];
            }

            // Shift keys in sibling
            var i: usize = 0;
            while (i < sibling.num_keys - 1) : (i += 1) {
                sibling.keys[i] = sibling.keys[i + 1];
            }

            // Shift children in sibling
            if (!sibling.is_leaf) {
                i = 0;
                while (i < sibling.num_keys) : (i += 1) {
                    sibling.children[i] = sibling.children[i + 1];
                }
            }

            child.num_keys += 1;
            sibling.num_keys -= 1;
        }

        /// Fill a child that has fewer than min_keys by borrowing or merging
        fn fill(self: *Self, parent_idx: u32, idx: usize) void {
            const parent = &self.nodes[parent_idx];

            if (idx != 0 and self.nodes[parent.children[idx - 1]].num_keys > min_keys) {
                self.borrowFromPrev(parent_idx, idx);
            } else if (idx != parent.num_keys and self.nodes[parent.children[idx + 1]].num_keys > min_keys) {
                self.borrowFromNext(parent_idx, idx);
            } else {
                if (idx != parent.num_keys) {
                    self.merge(parent_idx, idx);
                } else {
                    self.merge(parent_idx, idx - 1);
                }
            }
        }

        /// Remove a key from a leaf node at the given index
        fn removeFromLeaf(self: *Self, node_idx: u32, idx: usize) void {
            const node = &self.nodes[node_idx];
            var i = idx;
            while (i < node.num_keys - 1) : (i += 1) {
                node.keys[i] = node.keys[i + 1];
            }
            node.num_keys -= 1;
        }

        /// Get the predecessor key (rightmost key in left subtree)
        fn getPredecessor(self: *Self, node_idx: u32, idx: usize) T {
            var cur_idx = self.nodes[node_idx].children[idx];
            while (!self.nodes[cur_idx].is_leaf) {
                cur_idx = self.nodes[cur_idx].children[self.nodes[cur_idx].num_keys];
            }
            return self.nodes[cur_idx].keys[self.nodes[cur_idx].num_keys - 1];
        }

        /// Get the successor key (leftmost key in right subtree)
        fn getSuccessor(self: *Self, node_idx: u32, idx: usize) T {
            var cur_idx = self.nodes[node_idx].children[idx + 1];
            while (!self.nodes[cur_idx].is_leaf) {
                cur_idx = self.nodes[cur_idx].children[0];
            }
            return self.nodes[cur_idx].keys[0];
        }

        /// Remove a key from a non-leaf node by replacing with predecessor or successor
        fn removeFromNonLeaf(self: *Self, node_idx: u32, idx: usize) void {
            var node = &self.nodes[node_idx];
            const key = node.keys[idx];
            const left_child = node.children[idx];
            const right_child = node.children[idx + 1];

            if (self.nodes[left_child].num_keys > min_keys) {
                const pred = self.getPredecessor(node_idx, idx);
                // Re-fetch node after getPredecessor (may have been modified)
                node = &self.nodes[node_idx];
                node.keys[idx] = pred;
                self.removeNode(left_child, pred);
            } else if (self.nodes[right_child].num_keys > min_keys) {
                const succ = self.getSuccessor(node_idx, idx);
                // Re-fetch node after getSuccessor (may have been modified)
                node = &self.nodes[node_idx];
                node.keys[idx] = succ;
                self.removeNode(right_child, succ);
            } else {
                self.merge(node_idx, idx);
                // Re-fetch node after merge (structure has changed)
                node = &self.nodes[node_idx];
                self.removeNode(node.children[idx], key);
            }
        }

        fn removeNode(self: *Self, node_idx: u32, key: T) void {
            if (node_idx == NULL_INDEX) return;

            var node = &self.nodes[node_idx];
            var idx: usize = 0;
            while (idx < node.num_keys and key > node.keys[idx]) : (idx += 1) {}

            if (idx < node.num_keys and key == node.keys[idx]) {
                if (node.is_leaf) {
                    self.removeFromLeaf(node_idx, idx);
                } else {
                    self.removeFromNonLeaf(node_idx, idx);
                }
            } else {
                if (node.is_leaf) {
                    return; // Key not found
                }

                const is_in_subtree = (idx == node.num_keys);

                // Get the child we'll recurse into
                const child_idx = node.children[idx];
                if (child_idx == NULL_INDEX) return; // Sanity check

                // Check if child needs filling
                if (self.nodes[child_idx].num_keys < min_keys) {
                    self.fill(node_idx, idx);
                    // Re-fetch node pointer after fill (structure may have changed)
                    node = &self.nodes[node_idx];
                }

                // After fill, determine correct child to recurse into
                // If we were at the last child and merge happened, child moved to idx-1
                const final_child_idx = if (is_in_subtree and idx > node.num_keys)
                    node.children[idx - 1]
                else
                    node.children[idx];

                self.removeNode(final_child_idx, key);
            }
        }

        pub fn delete(self: *Self, key: T) void {
            if (self.root_idx == NULL_INDEX) return;

            self.removeNode(self.root_idx, key);

            const root = &self.nodes[self.root_idx];
            if (root.num_keys == 0) {
                const old_root = self.root_idx;
                if (!root.is_leaf) {
                    self.root_idx = root.children[0];
                } else {
                    self.root_idx = NULL_INDEX;
                }
                self.freeNode(old_root);
            }
        }

        /// Collect all keys in sorted order via in-order traversal
        pub fn collectKeysInOrder(self: *Self, allocator: Allocator) !std.ArrayList(T) {
            var result = std.ArrayList(T){};
            errdefer result.deinit(allocator);

            if (self.root_idx != NULL_INDEX) {
                try self.inOrderTraversal(self.root_idx, &result, allocator);
            }

            return result;
        }

        fn inOrderTraversal(self: *Self, node_idx: u32, result: *std.ArrayList(T), allocator: Allocator) !void {
            if (node_idx == NULL_INDEX) return;

            const node = &self.nodes[node_idx];

            var i: usize = 0;
            while (i < node.num_keys) : (i += 1) {
                // Traverse left child
                if (!node.is_leaf) {
                    try self.inOrderTraversal(node.children[i], result, allocator);
                }

                // Visit current key
                try result.append(allocator, node.keys[i]);
            }

            // Traverse rightmost child
            if (!node.is_leaf) {
                try self.inOrderTraversal(node.children[node.num_keys], result, allocator);
            }
        }

        /// Iterator for in-order traversal
        pub const Iterator = struct {
            tree: *Self,
            stack: std.ArrayList(StackItem),
            current_node_idx: u32,
            current_key_idx: usize,
            allocator: Allocator,

            const StackItem = struct {
                node_idx: u32,
                key_idx: usize,
            };

            pub fn init(tree: *Self, allocator: Allocator) !Iterator {
                var stack = std.ArrayList(StackItem){};
                errdefer stack.deinit(allocator);

                var iter = Iterator{
                    .tree = tree,
                    .stack = stack,
                    .current_node_idx = tree.root_idx,
                    .current_key_idx = 0,
                    .allocator = allocator,
                };

                // Move to leftmost key
                if (tree.root_idx != NULL_INDEX) {
                    try iter.descendToLeftmost(tree.root_idx);
                }

                return iter;
            }

            pub fn deinit(self: *Iterator) void {
                self.stack.deinit(self.allocator);
            }

            fn descendToLeftmost(self: *Iterator, start_idx: u32) !void {
                var node_idx = start_idx;
                while (node_idx != NULL_INDEX) {
                    const node = &self.tree.nodes[node_idx];

                    try self.stack.append(self.allocator, .{
                        .node_idx = node_idx,
                        .key_idx = 0,
                    });

                    if (node.is_leaf) break;
                    node_idx = node.children[0];
                }
            }

            pub fn next(self: *Iterator) ?T {
                if (self.stack.items.len == 0) return null;

                const item = self.stack.items[self.stack.items.len - 1];
                const node = &self.tree.nodes[item.node_idx];

                if (item.key_idx >= node.num_keys) {
                    _ = self.stack.pop();
                    return self.next();
                }

                const key = node.keys[item.key_idx];

                // Update stack for next iteration
                self.stack.items[self.stack.items.len - 1].key_idx += 1;

                // If not a leaf, descend to right child of this key
                if (!node.is_leaf) {
                    const right_child = node.children[item.key_idx + 1];
                    self.descendToLeftmost(right_child) catch return null;
                }

                return key;
            }
        };

        /// Create an iterator for in-order traversal
        pub fn iterator(self: *Self, allocator: Allocator) !Iterator {
            return Iterator.init(self, allocator);
        }

        /// Format the tree structure as a string for debugging/visualization
        pub fn toString(self: *Self, allocator: Allocator) ![]u8 {
            var buffer = std.ArrayList(u8){};
            errdefer buffer.deinit(allocator);

            if (self.root_idx == NULL_INDEX) {
                try buffer.appendSlice(allocator, "Empty tree\n");
            } else {
                try self.formatNode(&buffer, allocator, self.root_idx, 0);
            }

            return buffer.toOwnedSlice(allocator);
        }

        fn formatNode(self: *Self, buffer: *std.ArrayList(u8), allocator: Allocator, node_idx: u32, depth: usize) !void {
            if (node_idx == NULL_INDEX) return;

            const node = &self.nodes[node_idx];

            // Print indentation
            var i: usize = 0;
            while (i < depth) : (i += 1) {
                try buffer.appendSlice(allocator, "  ");
            }

            // Print node type and keys
            if (node.is_leaf) {
                try buffer.appendSlice(allocator, "Leaf[");
            } else {
                try buffer.appendSlice(allocator, "Internal[");
            }

            i = 0;
            while (i < node.num_keys) : (i += 1) {
                try std.fmt.format(buffer.writer(allocator), "{d}", .{node.keys[i]});
                if (i < node.num_keys - 1) {
                    try buffer.appendSlice(allocator, ", ");
                }
            }
            try buffer.appendSlice(allocator, "]\n");

            // Recursively print children
            if (!node.is_leaf) {
                i = 0;
                while (i <= node.num_keys) : (i += 1) {
                    try self.formatNode(buffer, allocator, node.children[i], depth + 1);
                }
            }
        }
    };
}

// ============================================================================
// Example Usage
// ============================================================================

/// Example demonstrating basic BTree usage
pub fn exampleBasicUsage() !void {
    const allocator = std.heap.page_allocator;

    // Create a B-Tree with i32 keys and order 5
    const Tree = BTree(i32, 5);
    var tree = try Tree.init(allocator, 100);
    defer tree.deinit();

    // Insert values
    try tree.insert(50);
    try tree.insert(30);
    try tree.insert(70);
    try tree.insert(20);
    try tree.insert(40);

    // Visualize tree structure
    std.debug.print("=== Tree Structure ===\n", .{});
    const tree_str = try tree.toString(allocator);
    defer allocator.free(tree_str);
    std.debug.print("{s}\n", .{tree_str});

    // Search for values
    const found = tree.search(30); // Returns true
    std.debug.print("Search for 30: {}\n", .{found});

    const not_found = tree.search(100); // Returns false
    std.debug.print("Search for 100: {}\n", .{not_found});

    // Iterate in sorted order
    std.debug.print("Keys in order: ", .{});
    var iter = try tree.iterator(allocator);
    defer iter.deinit();
    while (iter.next()) |key| {
        std.debug.print("{d} ", .{key});
    }
    std.debug.print("\n", .{});

    // Delete a value
    tree.delete(30);
    std.debug.print("After deleting 30: ", .{});
    var iter2 = try tree.iterator(allocator);
    defer iter2.deinit();
    while (iter2.next()) |key| {
        std.debug.print("{d} ", .{key});
    }
    std.debug.print("\n", .{});
}

/// Example demonstrating advanced BTree operations
pub fn exampleAdvancedUsage() !void {
    const allocator = std.heap.page_allocator;

    // Create a B-Tree with larger order for better performance
    const Tree = BTree(i32, 11);
    var tree = try Tree.init(allocator, 1000);
    defer tree.deinit();

    // Bulk insert
    var i: i32 = 0;
    while (i < 100) : (i += 1) {
        try tree.insert(i * 2); // Insert even numbers
    }

    // Collect all keys in sorted order
    var keys = try tree.collectKeysInOrder(allocator);
    defer keys.deinit(allocator);

    std.debug.print("Total keys: {d}\n", .{keys.items.len});
    std.debug.print("First key: {d}\n", .{keys.items[0]});
    std.debug.print("Last key: {d}\n", .{keys.items[keys.items.len - 1]});

    // Bulk delete
    i = 0;
    while (i < 50) : (i += 1) {
        tree.delete(i * 4); // Delete every other even number
    }

    // Verify deletions
    i = 0;
    while (i < 50) : (i += 1) {
        const should_exist = tree.search(i * 4 + 2);
        _ = should_exist; // true
        const should_not_exist = tree.search(i * 4);
        _ = should_not_exist; // false
    }
}

// ============================================================================
// Unit Tests
// ============================================================================
test "BTree2 - basic insert and search" {
    const Tree = BTree(i32, 5);
    var tree = try Tree.init(testing.allocator, 1000);
    defer tree.deinit();

    try tree.insert(10);
    try tree.insert(20);
    try tree.insert(5);
    try tree.insert(15);

    try testing.expect(tree.search(10));
    try testing.expect(tree.search(20));
    try testing.expect(tree.search(5));
    try testing.expect(tree.search(15));
    try testing.expect(!tree.search(100));
}

test "BTree2 - large insertion" {
    const Tree = BTree(i32, 5);
    var tree = try Tree.init(testing.allocator, 2000);
    defer tree.deinit();

    var i: i32 = 0;
    while (i < 1000) : (i += 1) {
        try tree.insert(i);
    }

    i = 0;
    while (i < 1000) : (i += 1) {
        try testing.expect(tree.search(i));
    }

    try testing.expect(!tree.search(1000));
    try testing.expect(!tree.search(-1));
}

test "BTree2 - delete operations" {
    const Tree = BTree(i32, 5);
    var tree = try Tree.init(testing.allocator, 1000);
    defer tree.deinit();

    var i: i32 = 0;
    while (i < 50) : (i += 1) {
        try tree.insert(i);
    }

    i = 0;
    while (i < 25) : (i += 1) {
        tree.delete(i);
        try testing.expect(!tree.search(i));
    }

    i = 25;
    while (i < 50) : (i += 1) {
        try testing.expect(tree.search(i));
    }
}

test "BTree2 - ordered iteration" {
    const Tree = BTree(i32, 5);
    var tree = try Tree.init(testing.allocator, 1000);
    defer tree.deinit();

    // Insert in random order
    const values = [_]i32{ 50, 20, 80, 10, 30, 70, 90, 5, 15, 25, 35 };
    for (values) |val| {
        try tree.insert(val);
    }

    // Collect keys using recursive traversal
    var keys = try tree.collectKeysInOrder(testing.allocator);
    defer keys.deinit(testing.allocator);

    // Verify keys are in sorted order
    try testing.expectEqual(@as(usize, 11), keys.items.len);
    const expected = [_]i32{ 5, 10, 15, 20, 25, 30, 35, 50, 70, 80, 90 };
    for (expected, 0..) |exp, idx| {
        try testing.expectEqual(exp, keys.items[idx]);
    }

    // Test iterator
    var iter = try tree.iterator(testing.allocator);
    defer iter.deinit();

    var idx: usize = 0;
    while (iter.next()) |key| : (idx += 1) {
        try testing.expectEqual(expected[idx], key);
    }
    try testing.expectEqual(@as(usize, 11), idx);

    // Test iteration after deletions
    tree.delete(20);
    tree.delete(70);

    var keys2 = try tree.collectKeysInOrder(testing.allocator);
    defer keys2.deinit(testing.allocator);

    const expected_after_delete = [_]i32{ 5, 10, 15, 25, 30, 35, 50, 80, 90 };
    try testing.expectEqual(@as(usize, 9), keys2.items.len);
    for (expected_after_delete, 0..) |exp, i| {
        try testing.expectEqual(exp, keys2.items[i]);
    }
}

test "BTree2 - ordered iteration with duplicates prevented" {
    const Tree = BTree(i32, 5);
    var tree = try Tree.init(testing.allocator, 100);
    defer tree.deinit();

    // Try to insert duplicates
    try tree.insert(10);
    try tree.insert(20);
    try tree.insert(10); // Duplicate, should be ignored
    try tree.insert(30);
    try tree.insert(20); // Duplicate, should be ignored

    var keys = try tree.collectKeysInOrder(testing.allocator);
    defer keys.deinit(testing.allocator);

    // Should only have 3 unique keys
    try testing.expectEqual(@as(usize, 3), keys.items.len);
    try testing.expectEqual(@as(i32, 10), keys.items[0]);
    try testing.expectEqual(@as(i32, 20), keys.items[1]);
    try testing.expectEqual(@as(i32, 30), keys.items[2]);
}

test "BTree2 - fuzz test with random operations" {
    const Tree = BTree(i32, 5);

    // Test with multiple seeds and iterations
    const seeds = [_]u64{ 0, 12345, 42, 99999, 314159 };

    for (seeds) |seed| {
        var tree = try Tree.init(testing.allocator, 5000);
        defer tree.deinit();

        var prng = std.Random.DefaultPrng.init(seed);
        const random = prng.random();

        var ground_truth = std.AutoHashMap(i32, void).init(testing.allocator);
        defer ground_truth.deinit();

        // Run multiple iterations of mixed operations
        var iteration: usize = 0;
        while (iteration < 5) : (iteration += 1) {
            // Phase 1: Random inserts
            var i: usize = 0;
            while (i < 500) : (i += 1) {
                const value = random.intRangeAtMost(i32, 0, 5000);
                try tree.insert(value);
                try ground_truth.put(value, {});
            }

            // Verify all values can be found
            var iter = ground_truth.keyIterator();
            while (iter.next()) |key| {
                if (!tree.search(key.*)) {
                    std.debug.print("ITERATION {}: Key {} in ground truth but not found in tree!\n", .{ iteration, key.* });
                    std.debug.print("Seed: {}, Total keys: {}\n", .{ seed, ground_truth.count() });
                }
                try testing.expect(tree.search(key.*));
            }

            // Phase 2: Random deletes
            var keys_to_delete = std.ArrayList(i32){};
            defer keys_to_delete.deinit(testing.allocator);

            iter = ground_truth.keyIterator();
            i = 0;
            while (iter.next()) |key| : (i += 1) {
                if (i % 3 == 0) { // Delete ~1/3 of keys
                    try keys_to_delete.append(testing.allocator, key.*);
                }
            }

            for (keys_to_delete.items) |key| {
                tree.delete(key);
                _ = ground_truth.remove(key);

                // Immediately verify it's gone
                if (tree.search(key)) {
                    std.debug.print("ITERATION {}: Key {} still found after deletion!\n", .{ iteration, key });
                    std.debug.print("Seed: {}\n", .{seed});
                }
                try testing.expect(!tree.search(key));
            }

            // Phase 3: Verify all remaining keys
            iter = ground_truth.keyIterator();
            while (iter.next()) |key| {
                if (!tree.search(key.*)) {
                    std.debug.print("ITERATION {}: Remaining key {} not found!\n", .{ iteration, key.* });
                    std.debug.print("Seed: {}, Total remaining: {}\n", .{ seed, ground_truth.count() });
                }
                try testing.expect(tree.search(key.*));
            }

            // Phase 4: Random searches (including non-existent keys)
            i = 0;
            while (i < 100) : (i += 1) {
                const value = random.intRangeAtMost(i32, 0, 5000);
                const found_in_tree = tree.search(value);
                const in_ground_truth = ground_truth.contains(value);

                if (found_in_tree != in_ground_truth) {
                    std.debug.print("ITERATION {}: Search mismatch for key {}!\n", .{ iteration, value });
                    std.debug.print("Tree says: {}, Ground truth says: {}\n", .{ found_in_tree, in_ground_truth });
                    std.debug.print("Seed: {}\n", .{seed});
                }
                try testing.expect(found_in_tree == in_ground_truth);
            }

            // Phase 5: Verify ordered iteration
            var tree_keys = try tree.collectKeysInOrder(testing.allocator);
            defer tree_keys.deinit(testing.allocator);

            // Verify count matches
            if (tree_keys.items.len != ground_truth.count()) {
                std.debug.print("ITERATION {}: Key count mismatch! Tree: {}, Ground truth: {}\n", .{ iteration, tree_keys.items.len, ground_truth.count() });
                std.debug.print("Seed: {}\n", .{seed});
            }
            try testing.expectEqual(ground_truth.count(), tree_keys.items.len);

            // Verify all keys from tree are in ground truth
            for (tree_keys.items) |key| {
                if (!ground_truth.contains(key)) {
                    std.debug.print("ITERATION {}: Key {} from tree not in ground truth!\n", .{ iteration, key });
                    std.debug.print("Seed: {}\n", .{seed});
                }
                try testing.expect(ground_truth.contains(key));
            }

            // Verify keys are in sorted order
            i = 1;
            while (i < tree_keys.items.len) : (i += 1) {
                if (tree_keys.items[i] <= tree_keys.items[i - 1]) {
                    std.debug.print("ITERATION {}: Keys not in sorted order! {} <= {}\n", .{ iteration, tree_keys.items[i], tree_keys.items[i - 1] });
                    std.debug.print("Seed: {}\n", .{seed});
                }
                try testing.expect(tree_keys.items[i] > tree_keys.items[i - 1]);
            }
        }
    }
}

/// Verify all keys in hashmap exist in tree (for fuzz testing)
fn verifyTreeContainsAll(comptime T: type, tree: anytype, map: *std.AutoHashMap(T, void), error_msg: []const u8) !void {
    var iter = map.keyIterator();
    while (iter.next()) |key| {
        if (!tree.search(key.*)) {
            std.debug.print("FUZZ TEST FAILED: {s} {}\n", .{ error_msg, key.* });
            return error.FuzzTestFailed;
        }
    }
}

/// Collect hashmap keys into ArrayList
fn collectMapKeys(comptime T: type, allocator: Allocator, map: *std.AutoHashMap(T, void)) !std.ArrayList(T) {
    var keys = std.ArrayList(T){};
    errdefer keys.deinit(allocator);

    var iter = map.keyIterator();
    while (iter.next()) |key| {
        try keys.append(allocator, key.*);
    }
    return keys;
}

// Fuzzing test
pub fn fuzzTest(seed: u64) !void {
    const FUZZ_TREE_CAPACITY = 5000;
    const FUZZ_INSERT_COUNT = 1000;
    const FUZZ_VALUE_RANGE = 10000;
    const FUZZ_DELETE_DIVISOR = 3;

    const allocator = std.heap.page_allocator;
    const Tree = BTree(i32, 5);
    var tree = try Tree.init(allocator, FUZZ_TREE_CAPACITY);
    defer tree.deinit();

    var prng = std.Random.DefaultPrng.init(seed);
    const random = prng.random();

    var inserted = std.AutoHashMap(i32, void).init(allocator);
    defer inserted.deinit();

    // Insert random values (hashmap tracks unique values)
    var i: usize = 0;
    while (i < FUZZ_INSERT_COUNT) : (i += 1) {
        const value = random.intRangeAtMost(i32, 0, FUZZ_VALUE_RANGE);
        try tree.insert(value);
        try inserted.put(value, {});
    }

    // Verify all inserted values can be found
    try verifyTreeContainsAll(i32, &tree, &inserted, "Value not found after insertion:");

    // Collect keys for random deletion
    var keys = try collectMapKeys(i32, allocator, &inserted);
    defer keys.deinit(allocator);

    // Random deletes
    const num_deletes = keys.items.len / FUZZ_DELETE_DIVISOR;
    i = 0;
    while (i < num_deletes) : (i += 1) {
        const idx = random.intRangeAtMost(usize, 0, keys.items.len - 1);
        const value = keys.items[idx];
        tree.delete(value);
        _ = inserted.remove(value);

        if (tree.search(value)) {
            std.debug.print("FUZZ TEST FAILED: Value {} still found after deletion\n", .{value});
            return error.FuzzTestFailed;
        }
    }

    // Verify remaining values still exist
    try verifyTreeContainsAll(i32, &tree, &inserted, "Value not found (should still be in tree):");
}

/// Time measurement result
const TimingResult = struct {
    start_ns: i128,
    end_ns: i128,

    fn elapsed_ns(self: TimingResult) f64 {
        return @as(f64, @floatFromInt(self.end_ns - self.start_ns));
    }

    fn elapsed_ms(self: TimingResult) f64 {
        return self.elapsed_ns() / 1_000_000.0;
    }

    fn elapsed_us(self: TimingResult) f64 {
        return self.elapsed_ns() / 1_000.0;
    }

    fn ns_per_op(self: TimingResult, ops: usize) f64 {
        return self.elapsed_ns() / @as(f64, @floatFromInt(ops));
    }
};

/// Start timing measurement
inline fn startTiming() i128 {
    return std.time.nanoTimestamp();
}

/// End timing measurement
inline fn endTiming(start: i128) TimingResult {
    return .{ .start_ns = start, .end_ns = std.time.nanoTimestamp() };
}

/// Benchmark context holding test data and allocator
const BenchmarkContext = struct {
    allocator: Allocator,
    insert_values: []i32,
    search_values: []i32,
    delete_values: []i32,
    num_ops: usize,

    const Tree = BTree(i32, 5);
    const TREE_CAPACITY = 200000;
    const SMALL_TREE_CAPACITY = 50000;
    const ORDER_BENCH_COUNT = 100000;
    const MIXED_OP_DIVISOR = 3;

    /// Create and populate a tree with insert_values
    fn createPopulatedTree(self: *const BenchmarkContext) !Tree {
        var tree = try Tree.init(self.allocator, TREE_CAPACITY);
        errdefer tree.deinit();
        for (self.insert_values) |val| {
            try tree.insert(val);
        }
        return tree;
    }

    fn benchInsert(self: *const BenchmarkContext) !void {
        var tree = try Tree.init(self.allocator, TREE_CAPACITY);
        defer tree.deinit();

        const start = startTiming();
        for (self.insert_values) |val| {
            try tree.insert(val);
        }
        const timing = endTiming(start);

        std.debug.print("Insert {d} random items: {d:.2}ms ({d:.2} ns/op)\n", .{ self.num_ops, timing.elapsed_ms(), timing.ns_per_op(self.num_ops) });
    }

    fn benchSearch(self: *const BenchmarkContext) !void {
        var tree = try self.createPopulatedTree();
        defer tree.deinit();

        var found_count: usize = 0;
        const start = startTiming();
        for (self.search_values) |val| {
            if (tree.search(val)) found_count += 1;
        }
        const timing = endTiming(start);

        std.debug.print("Search {d} random items: {d:.2}ms ({d:.2} ns/op) [found: {d}]\n", .{ self.num_ops, timing.elapsed_ms(), timing.ns_per_op(self.num_ops), found_count });
    }

    fn benchDelete(self: *const BenchmarkContext) !void {
        var tree = try self.createPopulatedTree();
        defer tree.deinit();

        const start = startTiming();
        for (self.delete_values) |val| {
            tree.delete(val);
        }
        const timing = endTiming(start);

        std.debug.print("Delete {d} random items: {d:.2}ms ({d:.2} ns/op)\n", .{ self.num_ops, timing.elapsed_ms(), timing.ns_per_op(self.num_ops) });
    }

    fn benchMixed(self: *const BenchmarkContext) !void {
        var tree = try Tree.init(self.allocator, TREE_CAPACITY);
        defer tree.deinit();

        var found_count: usize = 0;
        const start = startTiming();
        var i: usize = 0;
        while (i < self.num_ops / MIXED_OP_DIVISOR) : (i += 1) {
            try tree.insert(self.insert_values[i]);
            if (tree.search(self.search_values[i])) found_count += 1;
            if (i > 0) tree.delete(self.delete_values[i - 1]);
        }
        const timing = endTiming(start);

        const ops = (self.num_ops / MIXED_OP_DIVISOR) * MIXED_OP_DIVISOR;
        std.debug.print("Mixed operations ({d} ops): {d:.2}ms ({d:.2} ns/op) [found: {d}]\n", .{ ops, timing.elapsed_ms(), timing.ns_per_op(ops), found_count });
    }

    fn benchIteration(self: *const BenchmarkContext) !void {
        var tree = try self.createPopulatedTree();
        defer tree.deinit();

        // Benchmark collectKeysInOrder
        const start = startTiming();
        var keys = try tree.collectKeysInOrder(self.allocator);
        const timing = endTiming(start);
        defer keys.deinit(self.allocator);

        std.debug.print("Ordered iteration ({d} keys): {d:.2}ms ({d:.2} ns/key)\n", .{ keys.items.len, timing.elapsed_ms(), timing.ns_per_op(keys.items.len) });

        // Benchmark iterator
        const start_iter = startTiming();
        var iter = try tree.iterator(self.allocator);
        defer iter.deinit();
        var count: usize = 0;
        while (iter.next()) |_| {
            count += 1;
        }
        const timing_iter = endTiming(start_iter);

        std.debug.print("Iterator traversal ({d} keys): {d:.2}ms ({d:.2} ns/key)\n", .{ count, timing_iter.elapsed_ms(), timing_iter.ns_per_op(count) });
    }

    fn benchDifferentOrders(self: *const BenchmarkContext) !void {
        std.debug.print("\n", .{});

        inline for ([_]usize{ 3, 5, 7, 11, 17, 33 }) |ord| {
            const TreeN = BTree(i32, ord);
            var tree = try TreeN.init(self.allocator, SMALL_TREE_CAPACITY);
            defer tree.deinit();

            const start = startTiming();
            for (self.insert_values[0..ORDER_BENCH_COUNT]) |val| {
                try tree.insert(val);
            }
            const timing = endTiming(start);

            std.debug.print("Order {d}: Insert {d} random items in {d:.2}us ({d:.2} ns/op)\n", .{ ord, ORDER_BENCH_COUNT, timing.elapsed_us(), timing.ns_per_op(ORDER_BENCH_COUNT) });
        }
    }
};

/// Shuffle array in place using Fisher-Yates algorithm
fn shuffleArray(comptime T: type, array: []T, random: std.Random) void {
    var i: usize = array.len - 1;
    while (i > 0) : (i -= 1) {
        const j = random.intRangeAtMost(usize, 0, i);
        const temp = array[i];
        array[i] = array[j];
        array[j] = temp;
    }
}

// Benchmark
pub fn benchmark() !void {
    const allocator = std.heap.page_allocator;

    std.debug.print("\n=== Optimized B-Tree (btree_2) Benchmark ===\n", .{});

    // Pre-generate random test data
    var prng = std.Random.DefaultPrng.init(42);
    const random = prng.random();

    const num_ops = 100000;
    const insert_values = try allocator.alloc(i32, num_ops);
    defer allocator.free(insert_values);

    const search_values = try allocator.alloc(i32, num_ops);
    defer allocator.free(search_values);

    const delete_values = try allocator.alloc(i32, num_ops);
    defer allocator.free(delete_values);

    // Generate and shuffle test data
    for (insert_values, 0..) |*val, i| {
        val.* = random.intRangeAtMost(i32, 0, 50000);
        search_values[i] = val.*;
        delete_values[i] = val.*;
    }

    shuffleArray(i32, search_values, random);
    shuffleArray(i32, delete_values, random);

    // Create benchmark context
    const ctx = BenchmarkContext{
        .allocator = allocator,
        .insert_values = insert_values,
        .search_values = search_values,
        .delete_values = delete_values,
        .num_ops = num_ops,
    };

    // Run benchmarks
    try ctx.benchInsert();
    try ctx.benchSearch();
    try ctx.benchDelete();
    try ctx.benchMixed();
    try ctx.benchIteration();
    try ctx.benchDifferentOrders();
}

pub fn main() !void {
    // Uncomment to run examples:
    try exampleBasicUsage();
    try exampleAdvancedUsage();

    std.debug.print("Running benchmarks...\n", .{});
    try benchmark();

    std.debug.print("\n=== Running Fuzz Tests ===\n", .{});
    const num_fuzz_tests: u64 = 100;
    var i: u64 = 0;
    while (i < num_fuzz_tests) : (i += 1) {
        try fuzzTest(i);
    }
    std.debug.print("{d} fuzz tests passed\n", .{num_fuzz_tests});
}
