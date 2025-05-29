# Background Compilation with PID Monitoring

You need to run a long-running compile script without timing out. Follow this pattern:

## Step 1: Start Background Compilation
Run the compile script in the background and capture its PID:

```bash
# Start compilation in background and capture PID
./compile.sh > compile.log 2>&1 & BUILD_PID=$!; echo "Build started with PID: $BUILD_PID"
```

## Step 2: Monitor Process Status
Check if the process is still running (run this command to poll status):

```bash
# Check if build is still running - run this command to poll status
if kill -0 $BUILD_PID 2>/dev/null; then echo "Build still running... ($(date))"; tail -n 2 compile.log; sleep 30; echo "Run this command again to continue monitoring"; else echo "Build process completed"; fi
```

## Step 3: Check Exit Status and Results
Once the process completes, check its exit status:

```bash
# Wait for process and get exit code, then show results
wait $BUILD_PID; EXIT_CODE=$?; echo "Build finished with exit code: $EXIT_CODE"; tail -n 10 compile.log
```

## Alternative: All-in-One Command
For maximum compactness, combine everything into a single command chain:

```bash
# Complete build monitoring in one line
./compile.sh > compile.log 2>&1 & BUILD_PID=$!; echo "Build PID: $BUILD_PID"; while kill -0 $BUILD_PID 2>/dev/null; do echo "Running..."; sleep 10; done; wait $BUILD_PID; echo "Exit code: $?"; tail compile.log
```

## Key Benefits:
- **No timeout issues**: Uses PID monitoring instead of waiting synchronously
- **Reliable completion detection**: Monitors actual process status with `kill -0`
- **Exit code verification**: Captures and reports the actual return value
- **Compact commands**: All operations condensed to one-liners
- **Real-time feedback**: Shows progress without blocking

## Usage Pattern:
1. Start background process and capture PID immediately
2. Use `kill -0` to check if process is still alive (non-blocking)
3. Use `wait` to get the actual exit code after completion
4. Process results based on exit status

This pattern works for any long-running process and provides reliable completion detection through process monitoring.
