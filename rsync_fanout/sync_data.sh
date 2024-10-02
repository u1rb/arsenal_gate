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
    echo "Usage: $0 --action <rsync|check> --source <source_server|LOCAL> --targets <target_servers|ALL> --dir <directory>"
    echo
    echo "Options:"
    echo "  --action     Operation mode: 'rsync' to synchronize or 'check' for a dry run."
    echo "  --source     The hostname or IP of the source server or 'LOCAL' for local synchronization."
    echo "  --targets    Comma-separated list of target servers or 'ALL' to target all defined servers."
    echo "  --dir        The directory path to synchronize."
    echo
    echo "Examples:"
    echo "  $0 --action rsync --source LOCAL --targets ALL --dir /export/data/curated"
    echo "  $0 --action check --source source.example.com --targets server1.example.com,server2.example.com --dir /export/data/curated"
    exit 1
}

get_rsync_command() {
    local action="$1"
    local source="$2"
    local target="$3"
    local directory="$4"

    # Construct rsync command based on action
    if [[ "$action" == "rsync" ]]; then
        RSYNC_COMMAND=$(cat <<EOF
            rsync -avz --perms --owner --group $directory/ "$target:$directory/"
EOF
)
    elif [[ "$action" == "check" ]]; then
        RSYNC_COMMAND=$(cat <<EOF
            rsync -avzn --perms --owner --group $directory/ "$target:$directory/" && \
            rsync -avzn --perms --owner --group --stats $directory/ "$target:$directory/"
EOF
)
    else
        echo "❌ Invalid action: $action"
        return 1
    fi

    echo "$RSYNC_COMMAND"
}

create_remote_directory() {
    local target="$1"
    local directory="$2"

    cmd="ssh $target \"mkdir -p $directory\""
    echo "Executing: $cmd"
    eval $cmd
    if [[ $? -ne 0 ]]; then
        echo "❌ Failed to create directory '$directory' on $target."
        return 1
    fi
}

ssh_ping() {
    local target="$1"
    ssh "$target" "exit"
    if [[ $? -ne 0 ]]; then
        echo "❌ Cannot connect to server '$target'."
        return 1
    fi
}


# Function to perform rsync to a single target
sync_to_target() {
    local action="$1"        # 'rsync' or 'check'
    local source="$2"        # Source server or 'LOCAL'
    local target="$3"        # Target server
    local directory="$4"     # Directory to synchronize


    ssh_ping "$target"
    create_remote_directory "$target" "$directory"

    RSYNC_COMMAND=$(get_rsync_command "$action" "$source" "$target" "$directory")

    if [[ "$source" == "LOCAL" ]]; then
        echo "Executing: $RSYNC_COMMAND"
        RSYNC_OUTPUT=$(eval $RSYNC_COMMAND 2>&1)
        echo "$RSYNC_OUTPUT"
    else
        ssh_ping "$source"
        echo "Executing: $RSYNC_COMMAND"
        RSYNC_OUTPUT=$(ssh $source $RSYNC_COMMAND 2>&1)
        echo "$RSYNC_OUTPUT"
    fi

    if [[ "$action" == "rsync" ]]; then
        if [[ $? -ne 0 ]]; then
            echo "❌ Failed to $action '$directory' to/from '$source' and '$target'."
            return 1
        else
            echo "✅ Successfully completed '$action' to $target."
        fi
    elif [[ "$action" == "check" ]]; then
        if [[ $? -ne 0 ]]; then
            echo "❌ Dry run for '$directory' to/from '$source' and '$target' encountered errors."
            return 1
        else
            FILE_COUNT=$(echo "$RSYNC_OUTPUT" | grep -E 'Number of (regular )?files transferred:' | awk -F': ' '{print $2}')
            echo "FILE_COUNT: $FILE_COUNT"
            
            if [[ $FILE_COUNT -eq 0 ]]; then
                echo "✅ All files are already synced."
                return 0
            else
                echo "❌ $FILE_COUNT files would be transferred."
                return 1
            fi
        fi
    fi

    return 0
}

# -----------------------
# Argument Parsing
# -----------------------

# Ensure at least 8 arguments are provided (including --action)
if [ "$#" -lt 8 ]; then
    echo "Error: Insufficient arguments provided."
    usage
fi

# Initialize variables
ACTION=""
SOURCE_SERVER=""
TARGETS_INPUT=""
DIRECTORY=""

# Parse command-line arguments
while [[ "$#" -gt 0 ]]; do
    case "$1" in
        --action)
            ACTION="$2"
            shift 2
            ;;
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
if [[ -z "$ACTION" ]] || [[ -z "$SOURCE_SERVER" ]] || [[ -z "$TARGETS_INPUT" ]] || [[ -z "$DIRECTORY" ]]; then
    echo "Error: Missing required arguments."
    usage
fi

# Validate action
if [[ "$ACTION" != "rsync" && "$ACTION" != "check" ]]; then
    echo "Error: Invalid action '$ACTION'. Must be 'rsync' or 'check'."
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
# Execute Synchronization or Check
# -----------------------

echo "Starting '$ACTION' process..."
echo "Action         : $ACTION"
echo "Source Server  : $SOURCE_SERVER"
echo "Target Servers : ${TARGET_LIST[@]}"
echo "Directory      : $DIRECTORY"
echo "----------------------------------------"

for TARGET in "${TARGET_LIST[@]}"; do
    sync_to_target "$ACTION" "$SOURCE_SERVER" "$TARGET" "$DIRECTORY"
done

echo
echo "Process '$ACTION' completed."
exit 0