#!/bin/bash

# ======================================================================
# sync_data: Synchronize directories from a source server to multiple targets
# ======================================================================

# -----------------------
# Configuration Section
# -----------------------

# Define your target servers here. Replace with actual server hostnames or IPs.
TARGET_SERVERS=("server1.example.com" "server2.example.com" "server3.example.com")
# Alternatively, you can read from a file:
# TARGET_SERVERS=($(cat /path/to/target_servers.txt))

# -----------------------
# Function Definitions
# -----------------------

# Function to display usage information
usage() {
    echo "Usage: $0 --source <source_server> --targets <target_servers|ALL> --dir <directory>"
    echo
    echo "Options:"
    echo "  --source     The hostname or IP of the source server."
    echo "  --targets    Comma-separated list of target servers or 'ALL' to target all defined servers."
    echo "  --dir        The directory path to synchronize."
    exit 1
}

# Function to perform rsync to a single target
sync_to_target() {
    local source="$1"
    local target="$2"
    local directory="$3"

    echo "----------------------------------------"
    echo "Syncing '$directory' from '$source' to '$target'..."
    
    # Execute rsync command
    rsync -avz --perms --owner --group -e ssh "${source}:${directory}/" "${target}:${directory}/"

    if [ $? -eq 0 ]; then
        echo "✅ Successfully synced to $target."
    else
        echo "❌ Failed to sync to $target."
    fi
}

# -----------------------
# Argument Parsing
# -----------------------

# Ensure at least 6 arguments are provided
if [ "$#" -lt 6 ]; then
    echo "Error: Insufficient arguments provided."
    usage
fi

# Parse command-line arguments
while [[ "$#" -gt 0 ]]; do
    case "$1" in
        --source)
            SOURCE_SERVER="$2"
            shift 2
            ;;
        --targets)
            TARGETS_INPUT="$2"
            shift 2
            ;;
        --dir)
            DIRECTORY="$2"
            shift 2
            ;;
        *)
            echo "Error: Unknown option '$1'"
            usage
            ;;
    esac
done

# Validate mandatory arguments
if [[ -z "$SOURCE_SERVER" ]] || [[ -z "$TARGETS_INPUT" ]] || [[ -z "$DIRECTORY" ]]; then
    echo "Error: Missing required arguments."
    usage
fi

# -----------------------
# Determine Target Servers
# -----------------------

if [[ "$TARGETS_INPUT" == "ALL" ]]; then
    TARGET_LIST=("${TARGET_SERVERS[@]}")
else
    # Split the comma-separated list into an array
    IFS=',' read -r -a TARGET_LIST <<< "$TARGETS_INPUT"
fi

# -----------------------
# Execute Synchronization
# -----------------------

echo "Starting synchronization..."
echo "Source Server : $SOURCE_SERVER"
echo "Target Servers: ${TARGET_LIST[@]}"
echo "Directory      : $DIRECTORY"
echo

for TARGET in "${TARGET_LIST[@]}"; do
    sync_to_target "$SOURCE_SERVER" "$TARGET" "$DIRECTORY"
done

echo
echo "Synchronization process completed."

exit 0