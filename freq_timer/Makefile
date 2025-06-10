.PHONY: all test run bench clean install build help

# Default target
all: build

# Show help
help:
	@echo "Available targets:"
	@echo "  make build           - Build release library with C headers"
	@echo "  make test            - Run basic tests (Rust + C FFI)"
	@echo "  make test-consistency - Run comprehensive consistency tests"
	@echo "  make test-all        - Run all tests including consistency"
	@echo "  make run             - Build and run example programs"
	@echo "  make bench           - Run performance benchmarks"
	@echo "  make analyze         - Generate CSV data and performance plots"
	@echo "  make plots           - Generate plots from existing CSV data"
	@echo "  make clean           - Clean build artifacts"
	@echo "  make install         - Install library system-wide (requires sudo)"

# Build release library with C headers
build:
	@echo "Building freq_timer library..."
	cargo build --release
	@echo "Library built at target/release/"
	@ls -la target/release/libfreq_timer.*

# Run all tests (Rust + C FFI)
test: build
	@echo "Running Rust tests..."
	cargo test
	@echo "\nBuilding and running C interface tests..."
	@if [ -f tests/c_interface_test.c ]; then \
		$(CC) tests/c_interface_test.c -Iinclude -Ltarget/release -lfreq_timer -lpthread -o test_c && \
		LD_LIBRARY_PATH=target/release ./test_c && \
		rm -f test_c; \
	else \
		echo "C interface test not found, skipping..."; \
	fi

# Run comprehensive consistency tests
test-consistency: build
	@echo "Running Rust consistency tests..."
	cargo test --test consistency_tests
	@echo "\nBuilding and running C consistency tests..."
	@if [ -f tests/c_consistency_test.c ]; then \
		$(CC) tests/c_consistency_test.c -Iinclude -Ltarget/release -lfreq_timer -lpthread -lm -o test_consistency_c && \
		LD_LIBRARY_PATH=target/release ./test_consistency_c && \
		rm -f test_consistency_c; \
	else \
		echo "C consistency test not found, skipping..."; \
	fi

# Run all tests including consistency
test-all: test test-consistency

# Run example programs
run: build
	@echo "Running Rust example..."
	@if [ -f examples/rust_usage.rs ]; then \
		cargo run --example rust_usage; \
	else \
		echo "Rust example not found, skipping..."; \
	fi
	@echo "\nRunning C example..."
	@if [ -f examples/basic_usage.c ]; then \
		$(CC) examples/basic_usage.c -Iinclude -Ltarget/release -lfreq_timer -lpthread -o example_c && \
		LD_LIBRARY_PATH=target/release ./example_c && \
		rm -f example_c; \
	else \
		echo "C example not found, skipping..."; \
	fi

# Run performance benchmarks
bench:
	@echo "Running performance benchmarks..."
	cargo bench
	@echo "Benchmark results saved to target/criterion/"

# Generate CSV data and create performance analysis plots
analyze: build
	@echo "Generating comprehensive performance analysis..."
	@python3 analyze_timer_data.py --generate-data
	@echo "Analysis complete! Check the generated PNG files."

# Generate plots from existing CSV data
plots:
	@echo "Generating plots from existing CSV data..."
	@python3 analyze_timer_data.py
	@echo "Plots generated! Check the PNG files."

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	cargo clean
	rm -f test_c example_c csv_test
	rm -f *.csv *.png
	rm -rf include/*.h

# Install library system-wide (optional)
install: build
	@echo "Installing freq_timer library..."
	@echo "This requires sudo access to install to /usr/local"
	@if [ -f target/release/libfreq_timer.so ]; then \
		sudo cp target/release/libfreq_timer.so /usr/local/lib/; \
	fi
	@if [ -f target/release/libfreq_timer.a ]; then \
		sudo cp target/release/libfreq_timer.a /usr/local/lib/; \
	fi
	@if [ -f include/freq_timer.h ]; then \
		sudo cp include/freq_timer.h /usr/local/include/; \
	fi
	@echo "Running ldconfig..."
	@sudo ldconfig
	@echo "Installation complete!"

# Debug build
debug:
	cargo build
	@echo "Debug library built at target/debug/"