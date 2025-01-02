#!/bin/bash
set -e  # Exit on error

# Create test directory and files
mkdir -p test_data
rm -rf test_data/*

TEST_DB="test_data/test.sqlite"
rm -f $TEST_DB  # Clean start

# Create test files for hashFiles
mkdir -p test_data/project
echo '{"dependencies": {}}' > test_data/project/package-lock.json
echo "Initial content:"
cat test_data/project/package-lock.json

# Test 1: Simple key cache miss
echo -e "\nTest 1: Simple key cache miss"
if python cmd_cache.py --db $TEST_DB --action get --key "test-key-1"; then
    echo "❌ Expected cache miss but got hit"
    exit 1
else
    echo "✅ Cache miss as expected"
fi

# Test 2: Set and get simple key
echo -e "\nTest 2: Set and get simple key"
python cmd_cache.py --db $TEST_DB --action set --key "test-key-1"
if python cmd_cache.py --db $TEST_DB --action get --key "test-key-1"; then
    echo "✅ Cache hit as expected"
else
    echo "❌ Expected cache hit but got miss"
    exit 1
fi

# Test 3: hashFiles pattern cache miss
echo -e "\nTest 3: hashFiles pattern cache miss"
TEST_KEY="123-hashFiles('test_data/project/package-lock.json')"
if python cmd_cache.py --db $TEST_DB --action get --key "$TEST_KEY"; then
    echo "❌ Expected cache miss but got hit"
    exit 1
else
    echo "✅ Cache miss as expected"
fi

# Test 4: Set and get with hashFiles pattern
echo -e "\nTest 4: Set and get with hashFiles pattern"
python cmd_cache.py --db $TEST_DB --action set --key "$TEST_KEY"
if python cmd_cache.py --db $TEST_DB --action get --key "$TEST_KEY"; then
    echo "✅ Cache hit as expected"
else
    echo "❌ Expected cache hit but got miss"
    exit 1
fi

# Test 5: Modify file and verify cache invalidation
echo -e "\nTest 5: Modify file and verify cache invalidation"
sleep 1  # Ensure file modification time changes
echo '{"dependencies": {"test": "1.0.0"}}' > test_data/project/package-lock.json
echo "Modified content:"
cat test_data/project/package-lock.json
if python cmd_cache.py --db $TEST_DB --action get --key "$TEST_KEY"; then
    echo "❌ Expected cache miss after file modification but got hit"
    exit 1
else
    echo "✅ Cache miss as expected after file modification"
fi

echo -e "\n✨ All tests passed!"

# Cleanup
# rm -rf test_data 
