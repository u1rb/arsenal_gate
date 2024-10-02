# sync_data

Check and sync data with remote servers.

## Prerequisites

Case 1: Local to remote server
- rsync is installed on all machines
- you can ssh from local to remote server without password

Case 2: Remote server A to remote server (B,C)
- rsync is installed on all machines
- you can ssh from local to A, from A to B/C without password


## Config

--targests If you want to use "ALL" in --targets, you need to change the TARGET_SERVERS in the script.

For --source, if you use LOCAL, it means you want to sync from local to remote server. If you use other server IP_OR_HOSTNAME, it means you want to sync from server A to server B/C.

--action rsync: do the file sync
--action check: only check if the files are synced, do not sync


```bash
## return 0 if all files are synced, return 1 if there are files to be synced
bash sync_data.sh --action check --source LOCAL --targets SERVER_IP_OR_HOSTNAME --dir "~/path/to/sync"

## rsync files to server
bash sync_data.sh --action rsync --source LOCAL --targets SERVER_IP_OR_HOSTNAME --dir "~/path/to/sync"

## rsync files from server A to server B/C
bash sync_data.sh --action rsync --source server_a --targets server_b,server_c --dir "~/path/to/sync"
```
