#!/bin/bash

# Default session name prefix
SESSION_PREFIX="task"
# Default shell
DEFAULT_SHELL="zsh"

# Parse arguments
while getopts "w:s:" opt; do
    case $opt in
        w)
            WORKDIR="$OPTARG"
            ;;
        s)
            SHELL_TYPE="$OPTARG"
            ;;
        \?)
            echo "Usage: $0 [-w work_dir] [-s shell_type] command [args...]"
            echo "shell_type can be 'bash' or 'zsh'"
            exit 1
            ;;
    esac
done

# Remove the parsed options from the argument list
shift $((OPTIND-1))

# Check if command is provided
if [ $# -eq 0 ]; then
    echo "Error: No command provided"
    echo "Usage: $0 [-w work_dir] [-s shell_type] command [args...]"
    echo "shell_type can be 'bash' or 'zsh'"
    exit 1
fi

# Set shell type
SHELL_TYPE=${SHELL_TYPE:-$DEFAULT_SHELL}
case $SHELL_TYPE in
    "bash")
        SHELL_PATH=$(which bash)
        ;;
    "zsh")
        SHELL_PATH=$(which zsh)
        if [ -z "$SHELL_PATH" ]; then
            echo "Error: zsh not found. Please install zsh or use bash"
            exit 1
        fi
        ;;
    *)
        echo "Error: Invalid shell type. Use 'bash' or 'zsh'"
        exit 1
        ;;
esac

# Generate unique session name
SESSION_NAME="${SESSION_PREFIX}_$(date +%Y%m%d_%H%M%S)"

# Create new tmux session with specified shell
if [ -n "$WORKDIR" ]; then
    # Create work directory if it doesn't exist
    mkdir -p "$WORKDIR"
    # Start tmux with specified working directory and shell
    tmux new-session -d -s "$SESSION_NAME" -c "$WORKDIR" "$SHELL_PATH"
else
    # Start tmux in current directory with specified shell
    tmux new-session -d -s "$SESSION_NAME" "$SHELL_PATH"
fi

# Send the command to the tmux session
tmux send-keys -t "$SESSION_NAME" "echo 'Starting at: $(date)'" C-m
tmux send-keys -t "$SESSION_NAME" "$*" C-m
tmux send-keys -t "$SESSION_NAME" "echo 'Finished at: $(date)'" C-m

echo "Started tmux session: $SESSION_NAME"
echo "To attach to the session: tmux attach -t $SESSION_NAME"
echo "To list all sessions: tmux ls"
