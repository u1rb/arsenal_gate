#!/bin/bash

# Function to cleanup on script exit
cleanup() {
    local exit_code=$?
    if [ -n "$MOUNT_POINT" ] && mountpoint -q "$MOUNT_POINT"; then
        fusermount -u "$MOUNT_POINT"
    fi
    # Remove temporary directories only if they exist
    [ -d "$UPPER_DIR" ] && rm -rf "$UPPER_DIR"
    [ -d "$WORK_DIR" ] && rm -rf "$WORK_DIR"
    exit $exit_code
}

# Set up trap for cleanup
trap cleanup EXIT

# Parse arguments
while getopts "w:" opt; do
    case $opt in
        w)
            SOURCE_DIR=$(realpath "$OPTARG")
            ;;
        \?)
            echo "Usage: $0 [-w work_dir] command [args...]"
            exit 1
            ;;
    esac
done

# Remove the parsed options from the argument list
shift $((OPTIND-1))

# Check if command and work directory are provided
if [ $# -eq 0 ] || [ -z "$SOURCE_DIR" ]; then
    echo "Error: Both work directory and command are required"
    echo "Usage: $0 -w work_dir command [args...]"
    exit 1
fi

# Check if source directory exists
if [ ! -d "$SOURCE_DIR" ]; then
    echo "Error: Source directory does not exist: $SOURCE_DIR"
    exit 1
fi

# Check if fuse-overlayfs is installed
if ! command -v fuse-overlayfs &> /dev/null; then
    echo "Error: fuse-overlayfs is not installed"
    exit 1
fi

# Create temporary directories for overlay filesystem
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
TEMP_BASE="/tmp/snapshot_${TIMESTAMP}_$$"
UPPER_DIR="${TEMP_BASE}/upper"
WORK_DIR="${TEMP_BASE}/work"
MOUNT_POINT="${TEMP_BASE}/merged"

mkdir -p "$UPPER_DIR" "$WORK_DIR" "$MOUNT_POINT"

# Mount overlay filesystem
if ! fuse-overlayfs -o lowerdir="$SOURCE_DIR",upperdir="$UPPER_DIR",workdir="$WORK_DIR" "$MOUNT_POINT"; then
    echo "Error: Failed to mount overlay filesystem"
    exit 1
fi

# Run the command in the snapshot
echo "Created snapshot of $SOURCE_DIR at $MOUNT_POINT"
echo "Running command in snapshot..."
echo "Starting at: $(date)"

# Execute the command in the snapshot directory
(cd "$MOUNT_POINT" && "$@")
COMMAND_EXIT_CODE=$?

echo "Finished at: $(date)"
echo "Command exit code: $COMMAND_EXIT_CODE"

# Cleanup is handled by the trap

exit $COMMAND_EXIT_CODE
