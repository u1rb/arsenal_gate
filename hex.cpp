#include <cstddef> // For size_t
#include <cstdint> // For uint8_t
#include <vector>  // For LUT init and example usage
#include <stdexcept> // For runtime_error in LUT init if needed
#include <iostream> // For example usage
#include <string>   // For example usage
#include <iomanip>  // For std::hex in example usage


namespace HighPerfHex {

    namespace { // Anonymous namespace for internal linkage details

        // Encoding: 0-15 maps to '0'-'9', 'A'-'F'
        // Using static const for compile-time constants with internal linkage.
        static const uint8_t hex_encode_lut[16] = {
            '0', '1', '2', '3', '4', '5', '6', '7',
            '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
        };

        // Decoding: ASCII maps to 0-15, or a sentinel (16) for invalid chars
        // Use 16 as sentinel: easy check (val >= 16) -> invalid
        // Initialized using an immediately-invoked lambda expression (IILE) for safety
        // and to keep initialization logic close to the table definition.
        static const std::vector<uint8_t> hex_decode_lut_vec = []{
            std::vector<uint8_t> lut(256, 16); // Initialize all to invalid (16)
            try {
                for (uint8_t i = 0; i < 10; ++i) lut.at('0' + i) = i;      // 0-9
                for (uint8_t i = 0; i < 6; ++i) {
                     uint8_t upper_idx = 'A' + i;
                     uint8_t lower_idx = 'a' + i;
                     if (upper_idx >= lut.size() || lower_idx >= lut.size()) {
                         // This should theoretically not happen with standard ASCII
                         throw std::out_of_range("LUT index out of bounds during init");
                     }
                     lut.at(upper_idx) = 10 + i; // A-F
                     lut.at(lower_idx) = 10 + i; // a-f
                }
            } catch (const std::out_of_range& e) {
                 // Handle error during initialization - maybe log or rethrow
                 // For simplicity here, we might let it terminate if LUT init fails criticaly
                 throw std::runtime_error("Failed to initialize hex decode LUT");
            }

            return lut;
        }(); // Immediately invoke lambda to initialize

        // Get a C-style array pointer from the vector for direct lookup access
        // This assumes the vector 'hex_decode_lut_vec' lives for the program duration (which it does as a static)
         static const uint8_t* const hex_decode_lut = hex_decode_lut_vec.data();

    } // end anonymous namespace

    /**
     * @brief Encodes binary data into hexadecimal representation (uppercase).
     * @param src_data Pointer to the source binary data. Must not be null.
     * @param src_size Size of the source data in bytes.
     * @param dst_data Pointer to the destination buffer for the hex string. Must not be null.
     * @param dst_max_size Maximum size of the destination buffer. Must be >= src_size * 2.
     * @return The number of bytes written to dst_data (always src_size * 2 on success),
     *         or 0 if input pointers are null or dst_max_size is insufficient.
     */
    size_t hex_encode(const uint8_t * src_data, size_t src_size,
                      uint8_t * dst_data, size_t dst_max_size)
    {
        // --- Input Validation ---
        if (!src_data || !dst_data) {
            #ifndef NDEBUG // Optional: Print error in debug builds
            // std::cerr << "HexEncode Error: Null pointer provided." << std::endl;
            #endif
            return 0;
        }
        // Check for potential overflow when calculating required size
        if (src_size > SIZE_MAX / 2) {
             #ifndef NDEBUG
             // std::cerr << "HexEncode Error: Source size too large, potential overflow." << std::endl;
             #endif
            return 0; // Avoid overflow
        }
        const size_t required_dst_size = src_size * 2;
        if (dst_max_size < required_dst_size) {
             #ifndef NDEBUG
             // std::cerr << "HexEncode Error: Destination buffer too small. Need " << required_dst_size << ", have " << dst_max_size << std::endl;
             #endif
            return 0;
        }

        // --- Encoding Loop ---
        // Use separate pointers for source end and destination to clarify loop condition
        const uint8_t* const src_end = src_data + src_size;
        uint8_t* dst_ptr = dst_data;

        while (src_data < src_end) {
            const uint8_t byte = *src_data++; // Cache byte, increment source pointer
            // Use the LUT for efficient conversion
            *dst_ptr++ = hex_encode_lut[byte >> 4];   // High nibble
            *dst_ptr++ = hex_encode_lut[byte & 0x0F]; // Low nibble
        }

        return required_dst_size; // Return bytes written
    }

    /**
     * @brief Decodes a hexadecimal string (uppercase or lowercase) into binary data.
     * @param src_data Pointer to the source hex string. Must not be null.
     * @param src_size Size of the source hex string in bytes (must be even).
     * @param dst_data Pointer to the destination buffer for the binary data. Must not be null.
     * @param dst_max_size Maximum size of the destination buffer. Must be >= src_size / 2.
     * @return The number of bytes written to dst_data (always src_size / 2 on success),
     *         or 0 if input pointers are null, src_size is odd, dst_max_size is insufficient,
     *         or an invalid hex character is found.
     */
    size_t hex_decode(const uint8_t * src_data, size_t src_size,
                      uint8_t * dst_data, size_t dst_max_size)
    {
        // --- Input Validation ---
        if (!src_data || !dst_data) {
             #ifndef NDEBUG
             // std::cerr << "HexDecode Error: Null pointer provided." << std::endl;
             #endif
            return 0;
        }
        if (src_size % 2 != 0) {
             #ifndef NDEBUG
             // std::cerr << "HexDecode Error: Source size (" << src_size << ") must be even." << std::endl;
             #endif
            return 0; // Hex string must have an even number of characters
        }
         // Prevent division by zero if src_size is 0 (though loop handles this)
         if (src_size == 0) {
             return 0; // Nothing to decode, 0 bytes written
         }
        const size_t required_dst_size = src_size / 2;
        if (dst_max_size < required_dst_size) {
             #ifndef NDEBUG
            //  std::cerr << "HexDecode Error: Destination buffer too small. Need " << required_dst_size << ", have " << dst_max_size << std::endl;
             #endif
            return 0;
        }

        // --- Decoding Loop ---
        const uint8_t* const src_end = src_data + src_size;
        uint8_t* dst_ptr = dst_data;

        while (src_data < src_end) {
            // Prefetching characters might help on some architectures, but adds complexity.
            // Keeping it simple here for clarity.
            const uint8_t high_char = *src_data++;
            const uint8_t low_char = *src_data++;

            // Use the LUT for fast conversion and validation
            const uint8_t high_nibble = hex_decode_lut[high_char];
            const uint8_t low_nibble = hex_decode_lut[low_char];

            // Check for invalid characters using the sentinel value (16 or higher)
            // Combining check might offer slight optimization opportunity for compiler
            if ((high_nibble | low_nibble) >= 16) {
                 #ifndef NDEBUG
                 // std::cerr << "HexDecode Error: Invalid hex character encountered ('"
                 //           << static_cast<char>(high_nibble >= 16 ? high_char : low_char)
                 //           << "')." << std::endl;
                 #endif
                return 0; // Invalid hex character encountered
            }

            // Combine valid nibbles into a byte
            *dst_ptr++ = (high_nibble << 4) | low_nibble;
        }

        return required_dst_size; // Return bytes written
    }

} // namespace HighPerfHex


// --- Example Usage ---
int main() {
    // --- Encode Example ---
    std::vector<uint8_t> binary_data = {0xDE, 0xAD, 0xBE, 0xEF, 0x12, 0x34, 0x00, 0xFF, 'A'};
    std::vector<uint8_t> hex_encoded_data(binary_data.size() * 2); // Allocate exact space

    size_t encoded_size = HighPerfHex::hex_encode(
        binary_data.data(),
        binary_data.size(),
        hex_encoded_data.data(),
        hex_encoded_data.size()
    );

    if (encoded_size > 0) {
        std::cout << "Original Binary (" << binary_data.size() << " bytes): ";
        for (uint8_t b : binary_data) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
        }
        std::cout << std::dec << std::endl; // Switch back to decimal output

        std::cout << "Hex Encoded   (" << encoded_size << " bytes): ";
        // Convert uint8_t vector to string for easy printing
        std::string hex_str(hex_encoded_data.begin(), hex_encoded_data.end());
        std::cout << hex_str << std::endl;
    } else {
        std::cerr << "Hex encoding failed!" << std::endl;
    }

    // --- Decode Example ---
    std::string hex_string_to_decode = "DEADBEEF123400ff41"; // Matches the binary_data above
    // Test lowercase and invalid chars
    std::string hex_string_lower = "deadbeef";
    std::string hex_string_invalid = "DEADBEEF123G"; // 'G' is invalid
    std::string hex_string_odd = "ABC"; // Odd length

    std::vector<uint8_t> decoded_data(hex_string_to_decode.length() / 2);

    size_t decoded_size = HighPerfHex::hex_decode(
        reinterpret_cast<const uint8_t*>(hex_string_to_decode.data()),
        hex_string_to_decode.length(),
        decoded_data.data(),
        decoded_data.size()
    );

    if (decoded_size > 0) {
        std::cout << "Hex Decoded   (" << decoded_size << " bytes) from '" << hex_string_to_decode << "': ";
         for (uint8_t b : decoded_data) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
        }
        std::cout << std::dec << std::endl;

        // Verify correctness
        if (decoded_data == binary_data) {
            std::cout << "Decode successful and matches original!" << std::endl;
        } else {
             std::cerr << "Decode mismatch!" << std::endl;
        }
    } else {
        std::cerr << "Hex decoding failed for '" << hex_string_to_decode << "'!" << std::endl;
    }

    // --- Test Lowercase Decode ---
     std::vector<uint8_t> decoded_lower(hex_string_lower.length() / 2);
     decoded_size = HighPerfHex::hex_decode(
        reinterpret_cast<const uint8_t*>(hex_string_lower.data()),
        hex_string_lower.length(),
        decoded_lower.data(),
        decoded_lower.size()
     );
     if (decoded_size > 0) {
          std::cout << "Lowercase decode successful for '" << hex_string_lower << "'" << std::endl;
     } else {
          std::cerr << "Lowercase decode failed for '" << hex_string_lower << "'!" << std::endl;
     }


    // --- Test Invalid Decode ---
    std::vector<uint8_t> decoded_invalid_data(hex_string_invalid.length() / 2); // Size might be calculated wrong, but function should fail before using it fully
     decoded_size = HighPerfHex::hex_decode(
        reinterpret_cast<const uint8_t*>(hex_string_invalid.data()),
        hex_string_invalid.length(),
        decoded_invalid_data.data(),
        decoded_invalid_data.size()
    );
     if (decoded_size == 0) {
        std::cout << "Correctly failed decoding invalid string: '" << hex_string_invalid << "'" << std::endl;
    } else {
        std::cerr << "Incorrectly decoded invalid string!" << std::endl;
    }

    // --- Test Odd Length Decode ---
     std::vector<uint8_t> decoded_odd_data(1); // Buffer size doesn't matter much here
     decoded_size = HighPerfHex::hex_decode(
        reinterpret_cast<const uint8_t*>(hex_string_odd.data()),
        hex_string_odd.length(),
        decoded_odd_data.data(),
        decoded_odd_data.size()
    );
      if (decoded_size == 0) {
        std::cout << "Correctly failed decoding odd length string: '" << hex_string_odd << "'" << std::endl;
    } else {
        std::cerr << "Incorrectly decoded odd length string!" << std::endl;
    }


    return 0;
}
