const std = @import("std");
const Allocator = std.mem.Allocator;
const btree = @import("btree_2.zig");
const BTree = btree.BTree;

pub const Side = enum {
    Buy,
    Sell,
};

// Fixed-point price type with 4 decimal places (e.g., 123.4567)
pub const FixedPrice = struct {
    value: u64, // stored as integer with 4 decimal places (e.g., 123.4567 -> 1234567)

    const SCALE: u64 = 10000;

    // Create from integral and fractional parts (e.g., 123, 4567 -> 123.4567)
    pub fn init(integral_part: u64, fractional_part: u64) FixedPrice {
        return FixedPrice{
            .value = integral_part * SCALE + fractional_part,
        };
    }

    // Create from raw value
    pub fn fromRaw(raw_value: u64) FixedPrice {
        return FixedPrice{ .value = raw_value };
    }

    // Create from f64 (for convenience in tests, avoid in production)
    pub fn fromFloat(price: f64) FixedPrice {
        return FixedPrice{
            .value = @intFromFloat(price * @as(f64, @floatFromInt(SCALE))),
        };
    }

    // Get raw u64 value
    pub fn raw(self: FixedPrice) u64 {
        return self.value;
    }

    // Get integral part
    pub fn integral(self: FixedPrice) u64 {
        return self.value / SCALE;
    }

    // Get fractional part (4 digits)
    pub fn fractional(self: FixedPrice) u64 {
        return self.value % SCALE;
    }

    // Convert to f64 for display (avoid for calculations)
    pub fn toFloat(self: FixedPrice) f64 {
        return @as(f64, @floatFromInt(self.value)) / @as(f64, @floatFromInt(SCALE));
    }
};

// Fixed-size ticker type for low-cost copying (max 8 chars)
pub const Ticker = struct {
    data: [8]u8,
    len: u8,

    pub fn init(s: []const u8) Ticker {
        var ticker = Ticker{
            .data = [_]u8{0} ** 8,
            .len = @intCast(@min(s.len, 8)),
        };
        @memcpy(ticker.data[0..ticker.len], s[0..ticker.len]);
        return ticker;
    }

    pub fn slice(self: *const Ticker) []const u8 {
        return self.data[0..self.len];
    }

    pub fn eql(self: *const Ticker, other: *const Ticker) bool {
        return self.len == other.len and std.mem.eql(u8, self.slice(), other.slice());
    }
};

// Fixed-size exchange type for low-cost copying (max 8 chars)
pub const Exchange = struct {
    data: [8]u8,
    len: u8,

    pub fn init(s: []const u8) Exchange {
        var exchange = Exchange{
            .data = [_]u8{0} ** 8,
            .len = @intCast(@min(s.len, 8)),
        };
        @memcpy(exchange.data[0..exchange.len], s[0..exchange.len]);
        return exchange;
    }

    pub fn slice(self: *const Exchange) []const u8 {
        return self.data[0..self.len];
    }

    pub fn eql(self: *const Exchange, other: *const Exchange) bool {
        return self.len == other.len and std.mem.eql(u8, self.slice(), other.slice());
    }
};

// Composite key for organizing orders by exchange and ticker
pub const BookKey = struct {
    exchange: Exchange,
    ticker: Ticker,

    pub fn init(exchange: []const u8, ticker: []const u8) BookKey {
        return BookKey{
            .exchange = Exchange.init(exchange),
            .ticker = Ticker.init(ticker),
        };
    }

    pub fn eql(self: BookKey, other: BookKey) bool {
        return self.exchange.eql(&other.exchange) and self.ticker.eql(&other.ticker);
    }

    pub fn hash(self: BookKey) u64 {
        var hasher = std.hash.Wyhash.init(0);
        hasher.update(self.exchange.slice());
        hasher.update(self.ticker.slice());
        return hasher.final();
    }
};

const BookKeyContext = struct {
    pub fn hash(_: BookKeyContext, key: BookKey) u64 {
        return key.hash();
    }

    pub fn eql(_: BookKeyContext, a: BookKey, b: BookKey) bool {
        return a.eql(b);
    }
};

pub const Order = struct {
    id: u64,
    exchange: Exchange,
    ticker: Ticker,
    nic_ts: u64,
    side: Side,
    price: FixedPrice,
    qty: u32,

    pub fn init(
        id: u64,
        exchange: []const u8,
        ticker: []const u8,
        nic_ts: u64,
        side: Side,
        price: FixedPrice,
        qty: u32,
    ) Order {
        return Order{
            .id = id,
            .exchange = Exchange.init(exchange),
            .ticker = Ticker.init(ticker),
            .nic_ts = nic_ts,
            .side = side,
            .price = price,
            .qty = qty,
        };
    }
};

// Price level with sorted orders
pub const PriceLevel = struct {
    order_ids: BTree(u64, 5), // BTree of order IDs for sorted access (FIFO at each price)
    orders: std.AutoHashMap(u64, Order), // HashMap for O(1) order lookup
    allocator: Allocator,

    pub fn init(allocator: Allocator) !PriceLevel {
        return PriceLevel{
            .order_ids = try BTree(u64, 5).init(allocator, 128),
            .orders = std.AutoHashMap(u64, Order).init(allocator),
            .allocator = allocator,
        };
    }

    pub fn deinit(self: *PriceLevel) void {
        self.order_ids.deinit();
        self.orders.deinit();
    }

    pub fn addOrder(self: *PriceLevel, order: Order) !void {
        try self.order_ids.insert(order.id);
        try self.orders.put(order.id, order);
    }

    pub fn deleteOrder(self: *PriceLevel, order_id: u64) bool {
        if (self.orders.remove(order_id)) {
            self.order_ids.delete(order_id);
            return true;
        }
        return false;
    }

    pub fn getOrder(self: *PriceLevel, order_id: u64) ?*Order {
        return self.orders.getPtr(order_id);
    }

    pub fn isEmpty(self: *PriceLevel) bool {
        return self.orders.count() == 0;
    }
};

// Price level book with sorted bids and asks
pub const PriceLevelBook = struct {
    // price -> PriceLevel (with sorted orders)
    price_levels: std.AutoHashMap(u64, PriceLevel),

    // BTrees for maintaining sorted price levels
    bid_prices: BTree(u64, 5),  // sorted ascending (iterate reverse for high-to-low)
    ask_prices: BTree(u64, 5),  // sorted ascending (iterate forward for low-to-high)

    allocator: Allocator,

    pub fn init(allocator: Allocator) !PriceLevelBook {
        return PriceLevelBook{
            .price_levels = std.AutoHashMap(u64, PriceLevel).init(allocator),
            .bid_prices = try BTree(u64, 5).init(allocator, 1024),
            .ask_prices = try BTree(u64, 5).init(allocator, 1024),
            .allocator = allocator,
        };
    }

    pub fn deinit(self: *PriceLevelBook) void {
        var iter = self.price_levels.valueIterator();
        while (iter.next()) |level| {
            level.deinit();
        }
        self.price_levels.deinit();
        self.bid_prices.deinit();
        self.ask_prices.deinit();
    }

    fn getOrCreatePriceLevel(self: *PriceLevelBook, price: u64) !*PriceLevel {
        const entry = try self.price_levels.getOrPut(price);
        if (!entry.found_existing) {
            entry.value_ptr.* = try PriceLevel.init(self.allocator);
        }
        return entry.value_ptr;
    }

    pub fn addOrder(self: *PriceLevelBook, order: Order) !void {
        const price_value = order.price.raw();
        const price_level = try self.getOrCreatePriceLevel(price_value);
        try price_level.addOrder(order);

        // Add price to appropriate BTree
        if (order.side == Side.Buy) {
            try self.bid_prices.insert(price_value);
        } else {
            try self.ask_prices.insert(price_value);
        }
    }

    pub fn deleteOrder(self: *PriceLevelBook, order_id: u64, price: FixedPrice, side: Side) !void {
        const price_value = price.raw();

        if (self.price_levels.getPtr(price_value)) |price_level| {
            if (price_level.deleteOrder(order_id)) {
                // If no more orders at this price, remove price level
                if (price_level.isEmpty()) {
                    if (side == Side.Buy) {
                        self.bid_prices.delete(price_value);
                    } else {
                        self.ask_prices.delete(price_value);
                    }
                    // Deinit the PriceLevel and remove from HashMap
                    price_level.deinit();
                    _ = self.price_levels.remove(price_value);
                }
            } else {
                return error.OrderNotFound;
            }
        } else {
            return error.PriceLevelNotFound;
        }
    }

    pub fn modifyOrder(self: *PriceLevelBook, order_id: u64, price: FixedPrice, new_qty: u32) !void {
        const price_value = price.raw();

        if (self.price_levels.getPtr(price_value)) |price_level| {
            if (price_level.getOrder(order_id)) |order| {
                order.qty = new_qty;
            } else {
                return error.OrderNotFound;
            }
        } else {
            return error.PriceLevelNotFound;
        }
    }

    pub fn getOrder(self: *PriceLevelBook, order_id: u64, price: FixedPrice) ?*Order {
        const price_value = price.raw();
        if (self.price_levels.getPtr(price_value)) |price_level| {
            return price_level.getOrder(order_id);
        }
        return null;
    }
};

pub const OrderBook = struct {
    books: std.HashMap(BookKey, PriceLevelBook, BookKeyContext, std.hash_map.default_max_load_percentage),
    allocator: Allocator,

    pub fn init(allocator: Allocator) OrderBook {
        return OrderBook{
            .books = std.HashMap(BookKey, PriceLevelBook, BookKeyContext, std.hash_map.default_max_load_percentage).init(allocator),
            .allocator = allocator,
        };
    }

    pub fn deinit(self: *OrderBook) void {
        var iter = self.books.valueIterator();
        while (iter.next()) |book| {
            book.deinit();
        }
        self.books.deinit();
    }

    fn getOrCreateBook(self: *OrderBook, key: BookKey) !*PriceLevelBook {
        const entry = try self.books.getOrPut(key);
        if (!entry.found_existing) {
            entry.value_ptr.* = try PriceLevelBook.init(self.allocator);
        }
        return entry.value_ptr;
    }

    pub fn add_order(
        self: *OrderBook,
        exchange: []const u8,
        ticker: []const u8,
        id: u64,
        nic_ts: u64,
        side: Side,
        price: FixedPrice,
        qty: u32,
    ) !void {
        const key = BookKey.init(exchange, ticker);
        const book = try self.getOrCreateBook(key);
        const order = Order.init(id, exchange, ticker, nic_ts, side, price, qty);
        try book.addOrder(order);
    }

    pub fn delete_order(
        self: *OrderBook,
        exchange: []const u8,
        ticker: []const u8,
        id: u64,
        price: FixedPrice,
        side: Side,
    ) !void {
        const key = BookKey.init(exchange, ticker);
        if (self.books.getPtr(key)) |book| {
            try book.deleteOrder(id, price, side);
        } else {
            return error.BookNotFound;
        }
    }

    pub fn modify_order(
        self: *OrderBook,
        exchange: []const u8,
        ticker: []const u8,
        id: u64,
        price: FixedPrice,
        qty: u32,
    ) !void {
        const key = BookKey.init(exchange, ticker);
        if (self.books.getPtr(key)) |book| {
            try book.modifyOrder(id, price, qty);
        } else {
            return error.BookNotFound;
        }
    }

    pub fn get_order(self: *OrderBook, exchange: []const u8, ticker: []const u8, id: u64, price: FixedPrice) ?*Order {
        const key = BookKey.init(exchange, ticker);
        if (self.books.getPtr(key)) |book| {
            return book.getOrder(id, price);
        }
        return null;
    }
};

pub fn main() !void {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var orderbook = OrderBook.init(allocator);
    defer orderbook.deinit();

    // Test add_order
    std.debug.print("\n=== Testing add_order ===\n", .{});
    const price = FixedPrice.init(123, 4500); // 123.4500
    try orderbook.add_order("XNAS", "AAPL", 1, 123, Side.Buy, price, 10);
    std.debug.print("Added order: id=1, exchange=XNAS, ticker=AAPL, side=Buy, price=123.4500, qty=10\n", .{});

    // Test get_order
    if (orderbook.get_order("XNAS", "AAPL", 1, price)) |order| {
        std.debug.print("Retrieved order: id={d}, price={d:.4}, qty={d}\n", .{ order.id, order.price.toFloat(), order.qty });
    }

    // Test modify_order
    std.debug.print("\n=== Testing modify_order ===\n", .{});
    try orderbook.modify_order("XNAS", "AAPL", 1, price, 5);
    std.debug.print("Modified order: id=1, new_qty=5\n", .{});

    if (orderbook.get_order("XNAS", "AAPL", 1, price)) |order| {
        std.debug.print("After modification: id={d}, qty={d}\n", .{ order.id, order.qty });
    }

    // Test delete_order
    std.debug.print("\n=== Testing delete_order ===\n", .{});
    try orderbook.delete_order("XNAS", "AAPL", 1, price, Side.Buy);
    std.debug.print("Deleted order: id=1\n", .{});

    // Try to get deleted order
    if (orderbook.get_order("XNAS", "AAPL", 1, price)) |_| {
        std.debug.print("Error: Order should have been deleted\n", .{});
    } else {
        std.debug.print("Order successfully deleted (not found)\n", .{});
    }

    // Test error case - delete non-existent order
    std.debug.print("\n=== Testing error handling ===\n", .{});
    const bad_price = FixedPrice.init(999, 0);
    orderbook.delete_order("XNAS", "AAPL", 999, bad_price, Side.Buy) catch |err| {
        std.debug.print("Expected error caught: {}\n", .{err});
    };

    // Run benchmarks
    std.debug.print("\n=== BENCHMARKS ===\n", .{});

    // Benchmark add_order
    var bench_orderbook = OrderBook.init(allocator);
    defer bench_orderbook.deinit();

    const num_orders: usize = 10000;
    var timer = try std.time.Timer.start();

    for (0..num_orders) |i| {
        // Use realistic number of price levels (~50)
        const bench_price = FixedPrice.init(100 + (i % 50), 0);
        try bench_orderbook.add_order(
            "XNAS",
            "AAPL",
            i,
            @intCast(i),
            if (i % 2 == 0) Side.Buy else Side.Sell,
            bench_price,
            @intCast(i % 100 + 1),
        );
    }

    const add_elapsed = timer.read();
    const add_ns_per_op = add_elapsed / num_orders;
    std.debug.print("Add {d} orders: {d}ms ({d} ns/op)\n", .{
        num_orders,
        add_elapsed / std.time.ns_per_ms,
        add_ns_per_op,
    });

    // Benchmark get_order
    timer.reset();
    var found_count: usize = 0;
    for (0..num_orders) |i| {
        const bench_price = FixedPrice.init(100 + (i % 50), 0);
        if (bench_orderbook.get_order("XNAS", "AAPL", i, bench_price)) |_| {
            found_count += 1;
        }
    }

    const get_elapsed = timer.read();
    const get_ns_per_op = get_elapsed / num_orders;
    std.debug.print("Get {d} orders: {d}ms ({d} ns/op) [found: {d}]\n", .{
        num_orders,
        get_elapsed / std.time.ns_per_ms,
        get_ns_per_op,
        found_count,
    });

    // Benchmark modify_order
    timer.reset();
    for (0..num_orders) |i| {
        const bench_price = FixedPrice.init(100 + (i % 50), 0);
        bench_orderbook.modify_order("XNAS", "AAPL", i, bench_price, @intCast((i % 50) + 1)) catch {};
    }

    const modify_elapsed = timer.read();
    const modify_ns_per_op = modify_elapsed / num_orders;
    std.debug.print("Modify {d} orders: {d}ms ({d} ns/op)\n", .{
        num_orders,
        modify_elapsed / std.time.ns_per_ms,
        modify_ns_per_op,
    });

    // Benchmark delete_order
    timer.reset();
    for (0..num_orders) |i| {
        const bench_price = FixedPrice.init(100 + (i % 50), 0);
        bench_orderbook.delete_order(
            "XNAS",
            "AAPL",
            i,
            bench_price,
            if (i % 2 == 0) Side.Buy else Side.Sell,
        ) catch {};
    }

    const delete_elapsed = timer.read();
    const delete_ns_per_op = delete_elapsed / num_orders;
    std.debug.print("Delete {d} orders: {d}ms ({d} ns/op)\n", .{
        num_orders,
        delete_elapsed / std.time.ns_per_ms,
        delete_ns_per_op,
    });

    // Benchmark with random events
    std.debug.print("\n=== Random Events Benchmarks ===\n", .{});

    var prng = std.Random.DefaultPrng.init(42);
    var rand = prng.random();

    // Pre-generate events for add benchmark
    var add_active_orders = std.AutoHashMap(u64, OrderInfo).init(allocator);
    defer add_active_orders.deinit();

    var add_next_id: u64 = 1;
    const num_random_adds: usize = 10000;

    var add_events = std.ArrayList(RandomEvent){};
    defer add_events.deinit(allocator);

    for (0..num_random_adds) |_| {
        const event = try generateRandomEvent(rand, &add_next_id, &add_active_orders, allocator);
        if (event.event_type == .Add) {
            try add_events.append(allocator, event);
            try add_active_orders.put(event.id, OrderInfo{ .price = event.price, .side = event.side });
        }
    }

    // Benchmark random add_order
    var add_orderbook = OrderBook.init(allocator);
    defer add_orderbook.deinit();

    timer.reset();
    for (add_events.items) |event| {
        try add_orderbook.add_order(
            event.exchange,
            event.ticker,
            event.id,
            event.nic_ts,
            event.side,
            event.price,
            event.qty,
        );
    }

    const add_random_elapsed = timer.read();
    const add_random_ns_per_op = add_random_elapsed / add_events.items.len;
    std.debug.print("Add {d} random orders: {d}ms ({d} ns/op)\n", .{
        add_events.items.len,
        add_random_elapsed / std.time.ns_per_ms,
        add_random_ns_per_op,
    });

    // Pre-generate delete operations (from previously added orders)
    // Note: We'll delete the orders we added earlier
    var delete_events = std.ArrayList(RandomEvent){};
    defer delete_events.deinit(allocator);

    for (add_events.items) |event| {
        try delete_events.append(allocator, event); // Reuse the add events for deletion
    }

    // Benchmark random delete_order
    timer.reset();
    for (delete_events.items) |event| {
        try add_orderbook.delete_order(event.exchange, event.ticker, event.id, event.price, event.side);
    }

    const delete_random_elapsed = timer.read();
    const delete_random_ns_per_op = if (delete_events.items.len > 0) delete_random_elapsed / delete_events.items.len else 0;
    std.debug.print("Delete {d} random orders: {d}ms ({d} ns/op)\n", .{
        delete_events.items.len,
        delete_random_elapsed / std.time.ns_per_ms,
        delete_random_ns_per_op,
    });

    // Pre-generate events for modify benchmark
    var modify_active_orders = std.AutoHashMap(u64, OrderInfo).init(allocator);
    defer modify_active_orders.deinit();

    var modify_next_id: u64 = 1;
    const num_setup_orders: usize = 10000;

    var modify_setup_events = std.ArrayList(RandomEvent){};
    defer modify_setup_events.deinit(allocator);

    // Generate setup events
    for (0..num_setup_orders) |_| {
        const event = try generateRandomEvent(rand, &modify_next_id, &modify_active_orders, allocator);
        if (event.event_type == .Add) {
            try modify_setup_events.append(allocator, event);
            try modify_active_orders.put(event.id, OrderInfo{ .price = event.price, .side = event.side });
        }
    }

    // Setup orders for modification
    var modify_orderbook = OrderBook.init(allocator);
    defer modify_orderbook.deinit();

    for (modify_setup_events.items) |event| {
        try modify_orderbook.add_order(
            event.exchange,
            event.ticker,
            event.id,
            event.nic_ts,
            event.side,
            event.price,
            event.qty,
        );
    }

    // Pre-generate modify operations
    var modify_events = std.ArrayList(RandomEvent){};
    defer modify_events.deinit(allocator);

    for (modify_setup_events.items) |event| {
        var modified_event = event;
        modified_event.qty = rand.intRangeAtMost(u32, 1, 1000);
        try modify_events.append(allocator, modified_event);
    }

    // Benchmark random modify_order
    timer.reset();
    for (modify_events.items) |event| {
        try modify_orderbook.modify_order(event.exchange, event.ticker, event.id, event.price, event.qty);
    }

    const modify_random_elapsed = timer.read();
    const modify_random_ns_per_op = if (modify_events.items.len > 0) modify_random_elapsed / modify_events.items.len else 0;
    std.debug.print("Modify {d} random orders: {d}ms ({d} ns/op)\n", .{
        modify_events.items.len,
        modify_random_elapsed / std.time.ns_per_ms,
        modify_random_ns_per_op,
    });

    // Pre-generate events for mixed operations benchmark
    var mixed_active_orders = std.AutoHashMap(u64, OrderInfo).init(allocator);
    defer mixed_active_orders.deinit();

    var mixed_next_id: u64 = 1;
    const num_mixed_ops: usize = 50000;

    var mixed_events = std.ArrayList(RandomEvent){};
    defer mixed_events.deinit(allocator);

    for (0..num_mixed_ops) |_| {
        const event = try generateRandomEvent(rand, &mixed_next_id, &mixed_active_orders, allocator);
        try mixed_events.append(allocator, event);

        // Track state for event generation
        switch (event.event_type) {
            .Add => {
                try mixed_active_orders.put(event.id, OrderInfo{ .price = event.price, .side = event.side });
            },
            .Delete => {
                _ = mixed_active_orders.remove(event.id);
            },
            .Modify => {},
        }
    }

    // Benchmark mixed operations
    var mixed_orderbook = OrderBook.init(allocator);
    defer mixed_orderbook.deinit();

    timer.reset();
    for (mixed_events.items) |event| {
        switch (event.event_type) {
            .Add => {
                mixed_orderbook.add_order(
                    event.exchange,
                    event.ticker,
                    event.id,
                    event.nic_ts,
                    event.side,
                    event.price,
                    event.qty,
                ) catch {};
            },
            .Delete => {
                mixed_orderbook.delete_order(event.exchange, event.ticker, event.id, event.price, event.side) catch {};
            },
            .Modify => {
                mixed_orderbook.modify_order(event.exchange, event.ticker, event.id, event.price, event.qty) catch {};
            },
        }
    }

    const mixed_elapsed = timer.read();
    const mixed_ns_per_op = mixed_elapsed / mixed_events.items.len;
    std.debug.print("Mixed {d} operations: {d}ms ({d} ns/op)\n", .{
        mixed_events.items.len,
        mixed_elapsed / std.time.ns_per_ms,
        mixed_ns_per_op,
    });
}

// ===== UNIT TESTS =====

const testing = std.testing;

test "FixedPrice initialization and conversion" {
    const price1 = FixedPrice.init(123, 4500); // 123.4500
    try testing.expectEqual(@as(u64, 1234500), price1.raw());
    try testing.expectEqual(@as(u64, 123), price1.integral());
    try testing.expectEqual(@as(u64, 4500), price1.fractional());

    const price2 = FixedPrice.fromRaw(2500000); // 250.0000
    try testing.expectEqual(@as(u64, 250), price2.integral());
    try testing.expectEqual(@as(u64, 0), price2.fractional());

    const price3 = FixedPrice.fromFloat(99.99);
    try testing.expectEqual(@as(u64, 99), price3.integral());
    // Floating point conversion may not be exact
    try testing.expect(price3.fractional() >= 9899 and price3.fractional() <= 9901);
}

test "Ticker initialization and comparison" {
    const ticker1 = Ticker.init("AAPL");
    const ticker2 = Ticker.init("AAPL");
    const ticker3 = Ticker.init("MSFT");

    try testing.expect(ticker1.eql(&ticker2));
    try testing.expect(!ticker1.eql(&ticker3));
    try testing.expectEqualStrings("AAPL", ticker1.slice());
}

test "Exchange initialization and comparison" {
    const exchange1 = Exchange.init("XNAS");
    const exchange2 = Exchange.init("XNAS");
    const exchange3 = Exchange.init("NYSE");

    try testing.expect(exchange1.eql(&exchange2));
    try testing.expect(!exchange1.eql(&exchange3));
    try testing.expectEqualStrings("XNAS", exchange1.slice());
}

test "BookKey hash and equality" {
    const key1 = BookKey.init("XNAS", "AAPL");
    const key2 = BookKey.init("XNAS", "AAPL");
    const key3 = BookKey.init("NYSE", "AAPL");
    const key4 = BookKey.init("XNAS", "MSFT");

    try testing.expect(key1.eql(key2));
    try testing.expect(!key1.eql(key3));
    try testing.expect(!key1.eql(key4));
    try testing.expectEqual(key1.hash(), key2.hash());
}

test "OrderBook add and retrieve order" {
    var orderbook = OrderBook.init(testing.allocator);
    defer orderbook.deinit();

    const price = FixedPrice.init(123, 4500); // 123.4500
    try orderbook.add_order("XNAS", "AAPL", 1, 123, Side.Buy, price, 10);

    const order = orderbook.get_order("XNAS", "AAPL", 1, price);
    try testing.expect(order != null);
    try testing.expectEqual(@as(u64, 1), order.?.id);
    try testing.expectEqual(@as(u64, 1234500), order.?.price.raw());
    try testing.expectEqual(@as(u32, 10), order.?.qty);
    try testing.expectEqual(Side.Buy, order.?.side);
}

test "OrderBook modify order" {
    var orderbook = OrderBook.init(testing.allocator);
    defer orderbook.deinit();

    const price = FixedPrice.init(123, 4500);
    try orderbook.add_order("XNAS", "AAPL", 1, 123, Side.Buy, price, 10);
    try orderbook.modify_order("XNAS", "AAPL", 1, price, 5);

    const order = orderbook.get_order("XNAS", "AAPL", 1, price);
    try testing.expect(order != null);
    try testing.expectEqual(@as(u32, 5), order.?.qty);
}

test "OrderBook delete order" {
    var orderbook = OrderBook.init(testing.allocator);
    defer orderbook.deinit();

    const price = FixedPrice.init(123, 4500);
    try orderbook.add_order("XNAS", "AAPL", 1, 123, Side.Buy, price, 10);
    try orderbook.delete_order("XNAS", "AAPL", 1, price, Side.Buy);

    const order = orderbook.get_order("XNAS", "AAPL", 1, price);
    try testing.expect(order == null);
}

test "OrderBook multiple exchanges and tickers" {
    var orderbook = OrderBook.init(testing.allocator);
    defer orderbook.deinit();

    const price1 = FixedPrice.init(123, 4500);
    const price2 = FixedPrice.init(250, 0);
    const price3 = FixedPrice.init(123, 5000);

    try orderbook.add_order("XNAS", "AAPL", 1, 123, Side.Buy, price1, 10);
    try orderbook.add_order("XNAS", "MSFT", 2, 124, Side.Sell, price2, 20);
    try orderbook.add_order("NYSE", "AAPL", 3, 125, Side.Buy, price3, 15);

    try testing.expect(orderbook.get_order("XNAS", "AAPL", 1, price1) != null);
    try testing.expect(orderbook.get_order("XNAS", "MSFT", 2, price2) != null);
    try testing.expect(orderbook.get_order("NYSE", "AAPL", 3, price3) != null);
    try testing.expect(orderbook.get_order("XNAS", "AAPL", 2, price1) == null);
}

test "OrderBook error handling - modify non-existent order" {
    var orderbook = OrderBook.init(testing.allocator);
    defer orderbook.deinit();

    const price = FixedPrice.init(999, 0);
    const result = orderbook.modify_order("XNAS", "AAPL", 999, price, 10);
    try testing.expectError(error.BookNotFound, result);
}

test "OrderBook error handling - delete non-existent order" {
    var orderbook = OrderBook.init(testing.allocator);
    defer orderbook.deinit();

    const price = FixedPrice.init(999, 0);
    const result = orderbook.delete_order("XNAS", "AAPL", 999, price, Side.Buy);
    try testing.expectError(error.BookNotFound, result);
}

// ===== RANDOM EVENT GENERATION =====

pub const EventType = enum {
    Add,
    Delete,
    Modify,
};

pub const RandomEvent = struct {
    event_type: EventType,
    exchange: []const u8,
    ticker: []const u8,
    id: u64,
    nic_ts: u64,
    side: Side,
    price: FixedPrice,
    qty: u32,
};

pub const OrderInfo = struct {
    price: FixedPrice,
    side: Side,
};

pub fn generateRandomEvent(rand: std.Random, next_id: *u64, active_orders: *std.AutoHashMap(u64, OrderInfo), allocator: Allocator) !RandomEvent {
    _ = allocator;
    const exchanges = [_][]const u8{ "XNAS", "NYSE", "BATS", "ARCA" };
    const tickers = [_][]const u8{ "AAPL", "MSFT", "GOOGL", "AMZN", "TSLA", "META", "NVDA", "AMD" };

    const exchange = exchanges[rand.intRangeAtMost(usize, 0, exchanges.len - 1)];
    const ticker = tickers[rand.intRangeAtMost(usize, 0, tickers.len - 1)];

    // Bias towards Add if few active orders
    const has_active_orders = active_orders.count() > 0;
    const event_type = if (!has_active_orders) EventType.Add else rand.enumValue(EventType);

    const id: u64 = blk: {
        if (event_type == .Add) {
            const new_id = next_id.*;
            next_id.* += 1;
            break :blk new_id;
        } else if (has_active_orders) {
            // Pick a random active order
            const order_count = active_orders.count();
            const target_idx = rand.intRangeAtMost(usize, 0, order_count - 1);
            var iter = active_orders.keyIterator();
            var idx: usize = 0;
            while (iter.next()) |key| : (idx += 1) {
                if (idx == target_idx) break :blk key.*;
            }
            break :blk 1; // fallback
        } else {
            break :blk 1; // fallback for no active orders
        }
    };

    // Generate price with 4 decimal places: 50.00 to 500.00
    const integral = rand.intRangeAtMost(u64, 50, 500);
    const fractional = rand.intRangeAtMost(u64, 0, 9999);
    const price = FixedPrice.init(integral, fractional);
    const side = rand.enumValue(Side);

    // Get price/side from active order for Delete/Modify
    const order_info = if (event_type != .Add)
        (active_orders.get(id) orelse OrderInfo{ .price = price, .side = side })
    else
        OrderInfo{ .price = price, .side = side };

    return RandomEvent{
        .event_type = event_type,
        .exchange = exchange,
        .ticker = ticker,
        .id = id,
        .nic_ts = rand.int(u64),
        .side = order_info.side,
        .price = if (event_type == .Add) price else order_info.price,
        .qty = rand.intRangeAtMost(u32, 1, 1000),
    };
}

test "Random event generation" {
    var prng = std.Random.DefaultPrng.init(42);
    const rand = prng.random();

    var active_orders = std.AutoHashMap(u64, OrderInfo).init(testing.allocator);
    defer active_orders.deinit();

    var next_id: u64 = 1;
    const event = try generateRandomEvent(rand, &next_id, &active_orders, testing.allocator);

    try testing.expect(event.price.integral() >= 50 and event.price.integral() <= 500);
    try testing.expect(event.qty >= 1 and event.qty <= 1000);
}

// ===== FUZZING TEST =====

test "Fuzz test with random events" {
    var prng = std.Random.DefaultPrng.init(12345);
    const rand = prng.random();

    var orderbook = OrderBook.init(testing.allocator);
    defer orderbook.deinit();

    var active_orders = std.AutoHashMap(u64, OrderInfo).init(testing.allocator);
    defer active_orders.deinit();

    var next_id: u64 = 1;
    var add_count: u32 = 0;
    var delete_count: u32 = 0;
    var modify_count: u32 = 0;

    // Generate 1000 random events
    for (0..1000) |_| {
        const event = try generateRandomEvent(rand, &next_id, &active_orders, testing.allocator);

        switch (event.event_type) {
            .Add => {
                orderbook.add_order(
                    event.exchange,
                    event.ticker,
                    event.id,
                    event.nic_ts,
                    event.side,
                    event.price,
                    event.qty,
                ) catch {};
                // Track active order
                try active_orders.put(event.id, OrderInfo{ .price = event.price, .side = event.side });
                add_count += 1;
            },
            .Delete => {
                orderbook.delete_order(event.exchange, event.ticker, event.id, event.price, event.side) catch {};
                // Remove from active orders
                _ = active_orders.remove(event.id);
                delete_count += 1;
            },
            .Modify => {
                orderbook.modify_order(event.exchange, event.ticker, event.id, event.price, event.qty) catch {};
                modify_count += 1;
            },
        }
    }

    std.debug.print("\nFuzz test completed: {d} adds, {d} deletes, {d} modifies\n", .{ add_count, delete_count, modify_count });
}

// ===== BENCHMARK =====

test "Benchmark add_order performance" {
    var orderbook = OrderBook.init(testing.allocator);
    defer orderbook.deinit();

    const num_orders = 10000;
    var timer = try std.time.Timer.start();

    for (0..num_orders) |i| {
        const integral = 100 + (i % 1000);
        const price = FixedPrice.init(integral, 0);
        try orderbook.add_order(
            "XNAS",
            "AAPL",
            i,
            @intCast(i),
            if (i % 2 == 0) Side.Buy else Side.Sell,
            price,
            @intCast(i % 100 + 1),
        );
    }

    const elapsed = timer.read();
    const ns_per_op = elapsed / num_orders;

    std.debug.print("\nAdded {d} orders in {d}ms ({d}ns per order)\n", .{
        num_orders,
        elapsed / std.time.ns_per_ms,
        ns_per_op,
    });
}

test "Benchmark mixed operations performance" {
    var prng = std.Random.DefaultPrng.init(77777);
    const rand = prng.random();

    var orderbook = OrderBook.init(testing.allocator);
    defer orderbook.deinit();

    var active_orders = std.AutoHashMap(u64, OrderInfo).init(testing.allocator);
    defer active_orders.deinit();

    var next_id: u64 = 1;
    const num_events = 50000;
    var timer = try std.time.Timer.start();

    for (0..num_events) |_| {
        const event = try generateRandomEvent(rand, &next_id, &active_orders, testing.allocator);

        switch (event.event_type) {
            .Add => {
                orderbook.add_order(
                    event.exchange,
                    event.ticker,
                    event.id,
                    event.nic_ts,
                    event.side,
                    event.price,
                    event.qty,
                ) catch {};
                active_orders.put(event.id, OrderInfo{ .price = event.price, .side = event.side }) catch {};
            },
            .Delete => {
                orderbook.delete_order(event.exchange, event.ticker, event.id, event.price, event.side) catch {};
                _ = active_orders.remove(event.id);
            },
            .Modify => {
                orderbook.modify_order(event.exchange, event.ticker, event.id, event.price, event.qty) catch {};
            },
        }
    }

    const elapsed = timer.read();
    const ns_per_op = elapsed / num_events;

    std.debug.print("\nProcessed {d} mixed operations in {d}ms ({d}ns per operation)\n", .{
        num_events,
        elapsed / std.time.ns_per_ms,
        ns_per_op,
    });
}
