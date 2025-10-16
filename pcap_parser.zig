const std = @import("std");
const fs = std.fs;
const mem = std.mem;

const packet_buffer_size = 65536;
const reader_buffer_size = 64 * 1024;

const ParseArgsError = error{ InvalidArgs, OutOfMemory };

// PCAP file structures
const PcapGlobalHeader = extern struct {
    magic_number: u32,
    version_major: u16,
    version_minor: u16,
    thiszone: i32,
    sigfigs: u32,
    snaplen: u32,
    network: u32,
};

const PcapPacketHeader = extern struct {
    ts_sec: u32,
    ts_usec: u32,
    incl_len: u32,
    orig_len: u32,
};

const EthernetHeader = extern struct {
    dest_mac: [6]u8,
    src_mac: [6]u8,
    ethertype: u16,
};

const IPv4Header = extern struct {
    version_ihl: u8,
    dscp_ecn: u8,
    total_length: u16,
    identification: u16,
    flags_fragment: u16,
    ttl: u8,
    protocol: u8,
    checksum: u16,
    src_ip: [4]u8,
    dest_ip: [4]u8,
};

const ICMPHeader = extern struct {
    type: u8,
    code: u8,
    checksum: u16,
    identifier: u16,
    sequence: u16,
};

const UDPHeader = extern struct {
    src_port: u16,
    dest_port: u16,
    length: u16,
    checksum: u16,
};

const TCPHeader = extern struct {
    src_port: u16,
    dest_port: u16,
    seq_num: u32,
    ack_num: u32,
    data_offset_reserved_flags: u16,
    window: u16,
    checksum: u16,
    urgent_pointer: u16,
};

/// Streaming PCAP file reader with fixed buffer (no runtime allocation)
const PcapReader = struct {
    file: fs.File,
    reader: fs.File.Reader,
    global_header: PcapGlobalHeader,
    packet_buffer: []u8,

    /// Initialize a PCAP reader from a file with a pre-allocated buffer
    pub fn init(file: fs.File, packet_buffer: []u8, reader_buffer: []u8) !PcapReader {
        var reader = file.reader(reader_buffer);
        var header_buf: [@sizeOf(PcapGlobalHeader)]u8 = undefined;
        const header_read = try reader.interface.readSliceShort(&header_buf);
        if (header_read != header_buf.len) {
            return error.InvalidPcapFile;
        }
        const global_header = mem.bytesAsValue(PcapGlobalHeader, &header_buf).*;

        return PcapReader{
            .file = file,
            .reader = reader,
            .global_header = global_header,
            .packet_buffer = packet_buffer,
        };
    }

    /// Read the next packet from the PCAP file into the internal buffer
    /// Returns null when EOF is reached
    /// The returned slice is valid until the next call to nextPacket
    pub fn nextPacket(self: *PcapReader) !?struct {
        header: PcapPacketHeader,
        data: []const u8,
    } {
        var packet_header_buf: [@sizeOf(PcapPacketHeader)]u8 = undefined;
        const header_read = try self.reader.interface.readSliceShort(&packet_header_buf);
        if (header_read == 0) {
            return null;
        }
        if (header_read != packet_header_buf.len) {
            return error.TruncatedPacket;
        }
        const packet_header = mem.bytesAsValue(PcapPacketHeader, &packet_header_buf).*;

        // Check if packet fits in our buffer
        if (packet_header.incl_len > self.packet_buffer.len) {
            return error.PacketTooLarge;
        }

        // Read packet data into pre-allocated buffer
        self.reader.interface.readSliceAll(self.packet_buffer[0..packet_header.incl_len]) catch |err| switch (err) {
            error.EndOfStream => return error.TruncatedPacket,
            error.ReadFailed => return err,
        };

        return .{
            .header = packet_header,
            .data = self.packet_buffer[0..packet_header.incl_len],
        };
    }

    pub fn printGlobalHeader(self: *const PcapReader) void {
        std.debug.print("=== PCAP File Header ===\n", .{});
        std.debug.print("Magic Number: 0x{x}\n", .{self.global_header.magic_number});
        std.debug.print("Version: {}.{}\n", .{ self.global_header.version_major, self.global_header.version_minor });
        std.debug.print("Network: {}\n", .{self.global_header.network});
        std.debug.print("Snaplen: {}\n", .{self.global_header.snaplen});
        std.debug.print("\n", .{});
    }
};

fn printHexPayload(data: []const u8, max_bytes: usize) void {
    const bytes_to_print = if (data.len > max_bytes) max_bytes else data.len;
    std.debug.print("Payload ({d} bytes):\n", .{data.len});

    var i: usize = 0;
    while (i < bytes_to_print) : (i += 16) {
        std.debug.print("{x:0>4}: ", .{i});

        // Print hex bytes
        var j = i;
        while (j < i + 16 and j < bytes_to_print) : (j += 1) {
            std.debug.print("{x:0>2} ", .{data[j]});
        }

        // Padding for alignment
        while (j < i + 16) : (j += 1) {
            std.debug.print("   ", .{});
        }

        std.debug.print("  ", .{});

        // Print ASCII representation
        j = i;
        while (j < i + 16 and j < bytes_to_print) : (j += 1) {
            const c = data[j];
            if (c >= 32 and c < 127) {
                std.debug.print("{c}", .{c});
            } else {
                std.debug.print(".", .{});
            }
        }

        std.debug.print("\n", .{});
    }

    if (data.len > max_bytes) {
        std.debug.print("... ({d} more bytes)\n", .{data.len - max_bytes});
    }
}

fn processPacket(packet_index: usize, header: PcapPacketHeader, data: []const u8) void {
    std.debug.print("=== Packet index {d} ===\n", .{packet_index});
    std.debug.print("Timestamp: {d}.{d:0>6} seconds\n", .{
        header.ts_sec,
        header.ts_usec,
    });
    std.debug.print("Captured Length: {d} bytes\n", .{header.incl_len});
    std.debug.print("Original Length: {d} bytes\n", .{header.orig_len});

    const packet_len = data.len;

    if (packet_len >= @sizeOf(EthernetHeader)) {
        const eth_header = mem.bytesAsValue(EthernetHeader, data[0..@sizeOf(EthernetHeader)]);
        std.debug.print("\n--- Ethernet Header ---\n", .{});
        std.debug.print("Source MAC: {x:0>2}:{x:0>2}:{x:0>2}:{x:0>2}:{x:0>2}:{x:0>2}\n", .{
            eth_header.src_mac[0], eth_header.src_mac[1], eth_header.src_mac[2],
            eth_header.src_mac[3], eth_header.src_mac[4], eth_header.src_mac[5],
        });
        std.debug.print("Dest MAC:   {x:0>2}:{x:0>2}:{x:0>2}:{x:0>2}:{x:0>2}:{x:0>2}\n", .{
            eth_header.dest_mac[0], eth_header.dest_mac[1], eth_header.dest_mac[2],
            eth_header.dest_mac[3], eth_header.dest_mac[4], eth_header.dest_mac[5],
        });
        const is_multicast_mac = (eth_header.dest_mac[0] & 0x01) == 0x01;
        if (is_multicast_mac) {
            std.debug.print("Multicast destination MAC detected\n", .{});
        }
        std.debug.print("EtherType: 0x{x}\n", .{@byteSwap(eth_header.ethertype)});

        // Parse IP header
        if (packet_len >= @sizeOf(EthernetHeader) + @sizeOf(IPv4Header)) {
            const ip_header = mem.bytesAsValue(IPv4Header, data[@sizeOf(EthernetHeader) .. @sizeOf(EthernetHeader) + @sizeOf(IPv4Header)]);

            std.debug.print("\n--- IPv4 Header ---\n", .{});
            std.debug.print("Version: {d}\n", .{(ip_header.version_ihl >> 4) & 0x0f});
            const ip_header_length = (@as(usize, ip_header.version_ihl & 0x0f)) * 4;
            std.debug.print("Header Length: {d} bytes\n", .{ip_header_length});
            std.debug.print("Total Length: {d}\n", .{@byteSwap(ip_header.total_length)});
            std.debug.print("TTL: {d}\n", .{ip_header.ttl});
            std.debug.print("Protocol: {d} ", .{ip_header.protocol});

            if (ip_header.protocol == 1) {
                std.debug.print("(ICMP)\n", .{});
            } else if (ip_header.protocol == 6) {
                std.debug.print("(TCP)\n", .{});
            } else if (ip_header.protocol == 17) {
                std.debug.print("(UDP)\n", .{});
            } else {
                std.debug.print("(Other)\n", .{});
            }

            std.debug.print("Source IP: {d}.{d}.{d}.{d}\n", .{
                ip_header.src_ip[0], ip_header.src_ip[1],
                ip_header.src_ip[2], ip_header.src_ip[3],
            });
            std.debug.print("Dest IP: {d}.{d}.{d}.{d}\n", .{
                ip_header.dest_ip[0], ip_header.dest_ip[1],
                ip_header.dest_ip[2], ip_header.dest_ip[3],
            });
            const is_multicast_ip = (ip_header.dest_ip[0] & 0xf0) == 0xe0;
            if (is_multicast_ip) {
                std.debug.print("Multicast destination IP detected\n", .{});
            }

            const ip_payload_offset = @sizeOf(EthernetHeader) + ip_header_length;
            if (packet_len >= ip_payload_offset) {
                const remaining_len = packet_len - ip_payload_offset;

                // Parse TCP header if present
                if (ip_header.protocol == 6 and remaining_len >= @sizeOf(TCPHeader)) {
                    const tcp_header = mem.bytesAsValue(TCPHeader, data[ip_payload_offset .. ip_payload_offset + @sizeOf(TCPHeader)]);
                    const data_offset = ((@byteSwap(tcp_header.data_offset_reserved_flags) >> 12) & 0x0f) * 4;
                    const flags_field = @byteSwap(tcp_header.data_offset_reserved_flags) & 0x01ff;
                    std.debug.print("\n--- TCP Header ---\n", .{});
                    std.debug.print("Source Port: {d}\n", .{@byteSwap(tcp_header.src_port)});
                    std.debug.print("Dest Port: {d}\n", .{@byteSwap(tcp_header.dest_port)});
                    std.debug.print("Sequence Number: {d}\n", .{@byteSwap(tcp_header.seq_num)});
                    std.debug.print("Ack Number: {d}\n", .{@byteSwap(tcp_header.ack_num)});
                    std.debug.print("Data Offset: {d} bytes\n", .{data_offset});
                    std.debug.print("Flags: 0x{x}\n", .{flags_field});
                    std.debug.print("Window Size: {d}\n", .{@byteSwap(tcp_header.window)});
                }

                // Parse UDP header if present
                if (ip_header.protocol == 17 and remaining_len >= @sizeOf(UDPHeader)) {
                    const udp_header = mem.bytesAsValue(UDPHeader, data[ip_payload_offset .. ip_payload_offset + @sizeOf(UDPHeader)]);
                    const udp_len = @byteSwap(udp_header.length);
                    std.debug.print("\n--- UDP Header ---\n", .{});
                    std.debug.print("Source Port: {d}\n", .{@byteSwap(udp_header.src_port)});
                    std.debug.print("Dest Port: {d}\n", .{@byteSwap(udp_header.dest_port)});
                    std.debug.print("Length: {d}\n", .{udp_len});
                }
            }

            // Parse ICMP if present
            if (ip_header.protocol == 1 and packet_len >= @sizeOf(EthernetHeader) + @sizeOf(IPv4Header) + @sizeOf(ICMPHeader)) {
                const icmp_header = mem.bytesAsValue(ICMPHeader, data[@sizeOf(EthernetHeader) + @sizeOf(IPv4Header) .. @sizeOf(EthernetHeader) + @sizeOf(IPv4Header) + @sizeOf(ICMPHeader)]);

                std.debug.print("\n--- ICMP Header ---\n", .{});
                std.debug.print("Type: {d} ", .{icmp_header.type});

                if (icmp_header.type == 8) {
                    std.debug.print("(Echo Request)\n", .{});
                } else if (icmp_header.type == 0) {
                    std.debug.print("(Echo Reply)\n", .{});
                } else {
                    std.debug.print("(Other)\n", .{});
                }

                std.debug.print("Code: {d}\n", .{icmp_header.code});
                std.debug.print("Sequence: {d}\n", .{@byteSwap(icmp_header.sequence)});

                // Print ICMP payload
                const icmp_payload_start = @sizeOf(EthernetHeader) + @sizeOf(IPv4Header) + @sizeOf(ICMPHeader);
                if (packet_len > icmp_payload_start) {
                    std.debug.print("\n", .{});
                    const payload = data[icmp_payload_start..];
                    printHexPayload(payload, 256);
                }
            }
        }
    }

    // Print full packet payload as hex dump
    std.debug.print("\n--- Full Packet Hex Dump ---\n", .{});
    printHexPayload(data, 256);

    std.debug.print("\n", .{});
}

const ParsedArgs = struct {
    pcap_path: []const u8,
    detail_indices: std.ArrayList(usize),
};

const DecoderBuffers = struct {
    packet: []u8,
    reader: []u8,
};

const DecodeStats = struct {
    packet_count: usize,
    total_bytes: usize,
    elapsed_ms: u64,
};

fn printUsage(program_name: []const u8) void {
    std.debug.print("Usage: {s} [-i index ...] <pcap_file>\n", .{program_name});
}

fn parseArguments(allocator: std.mem.Allocator, args: []const [:0]u8) ParseArgsError!ParsedArgs {
    const program_name = std.mem.sliceTo(args[0], 0);

    if (args.len < 2) {
        printUsage(program_name);
        return ParseArgsError.InvalidArgs;
    }

    var detail_indices = std.ArrayList(usize){};
    errdefer detail_indices.deinit(allocator);

    var pcap_path_opt: ?[]const u8 = null;
    var arg_index: usize = 1;

    while (arg_index < args.len) {
        const arg_nt = args[arg_index];
        const arg = std.mem.sliceTo(arg_nt, 0);

        if (mem.eql(u8, arg, "-i")) {
            arg_index += 1;
            if (arg_index >= args.len) {
                std.debug.print("Missing index value after -i\n", .{});
                printUsage(program_name);
                return ParseArgsError.InvalidArgs;
            }

            const index_nt = args[arg_index];
            const index_str = std.mem.sliceTo(index_nt, 0);
            const parsed_index = std.fmt.parseUnsigned(usize, index_str, 10) catch {
                std.debug.print("Invalid index for -i: {s}\n", .{index_str});
                return ParseArgsError.InvalidArgs;
            };
            try detail_indices.append(allocator, parsed_index);

            arg_index += 1;
            continue;
        }

        if (arg.len > 0 and arg[0] == '-') {
            std.debug.print("Unknown option: {s}\n", .{arg});
            printUsage(program_name);
            return ParseArgsError.InvalidArgs;
        }

        if (pcap_path_opt != null) {
            std.debug.print("Multiple PCAP files specified.\n", .{});
            printUsage(program_name);
            return ParseArgsError.InvalidArgs;
        }

        pcap_path_opt = arg;
        arg_index += 1;
    }

    if (pcap_path_opt == null) {
        printUsage(program_name);
        return ParseArgsError.InvalidArgs;
    }

    return ParsedArgs{
        .pcap_path = pcap_path_opt.?,
        .detail_indices = detail_indices,
    };
}

fn allocateBuffers(allocator: std.mem.Allocator) !DecoderBuffers {
    const packet = try allocator.alloc(u8, packet_buffer_size);
    errdefer allocator.free(packet);

    const reader = try allocator.alloc(u8, reader_buffer_size);

    return DecoderBuffers{
        .packet = packet,
        .reader = reader,
    };
}

fn shouldPrintPacket(index: usize, detail_indices: []const usize) bool {
    for (detail_indices) |detail_index| {
        if (detail_index == index) return true;
    }
    return false;
}

fn decodePackets(reader: *PcapReader, detail_indices: []const usize) !DecodeStats {
    const should_print_details = detail_indices.len > 0;
    var packet_count: usize = 0;
    var total_packet_bytes: usize = 0;
    const start_ms = std.time.milliTimestamp();

    while (try reader.nextPacket()) |packet| {
        if (should_print_details and shouldPrintPacket(packet_count, detail_indices)) {
            processPacket(packet_count, packet.header, packet.data);
        }

        packet_count += 1;
        total_packet_bytes += packet.data.len;
    }

    const end_ms = std.time.milliTimestamp();
    const elapsed_ms_signed = if (end_ms >= start_ms) end_ms - start_ms else start_ms - end_ms;
    const elapsed_ms: u64 = @intCast(elapsed_ms_signed);

    return DecodeStats{
        .packet_count = packet_count,
        .total_bytes = total_packet_bytes,
        .elapsed_ms = elapsed_ms,
    };
}

fn printSummary(stats: DecodeStats) void {
    const elapsed_seconds = stats.elapsed_ms / 1000;
    const elapsed_millis = stats.elapsed_ms % 1000;
    const average_packet_length: usize = if (stats.packet_count == 0)
        0
    else
        stats.total_bytes / stats.packet_count;
    const packets_per_sec: f64 = if (stats.elapsed_ms == 0 or stats.packet_count == 0)
        0.0
    else blk: {
        const seconds = @as(f64, @floatFromInt(stats.elapsed_ms)) / 1000.0;
        break :blk @as(f64, @floatFromInt(stats.packet_count)) / seconds;
    };

    std.debug.print("Total packets processed: {d}\n", .{stats.packet_count});
    std.debug.print("Elapsed time: {d}.{d:0>3} seconds\n", .{
        elapsed_seconds,
        elapsed_millis,
    });
    std.debug.print("Average packet length: {d} bytes\n", .{average_packet_length});
    std.debug.print("Packets/sec: {d:.3}\n", .{packets_per_sec});
}

pub fn main() !void {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    const args = try std.process.argsAlloc(allocator);
    defer std.process.argsFree(allocator, args);

    var parsed = parseArguments(allocator, args) catch |err| {
        if (err == ParseArgsError.InvalidArgs) return;
        return err;
    };
    defer parsed.detail_indices.deinit(allocator);

    // Open the PCAP file
    const file = try fs.cwd().openFile(parsed.pcap_path, .{});
    defer file.close();

    const buffers = try allocateBuffers(allocator);
    defer allocator.free(buffers.packet);
    defer allocator.free(buffers.reader);

    var reader = try PcapReader.init(file, buffers.packet, buffers.reader);
    if (parsed.detail_indices.items.len > 0) {
        reader.printGlobalHeader();
    }

    const stats = try decodePackets(&reader, parsed.detail_indices.items);
    printSummary(stats);
}
