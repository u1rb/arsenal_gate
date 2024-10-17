import paramiko
import sys

def read_credentials(file_path):
    """
    Reads SSH credentials from a file.
    Each line should be in the format: hostname,username,password
    """
    credentials = []
    try:
        with open(file_path, 'r') as f:
            for line in f:
                parts = line.strip().split(',')
                if len(parts) != 3:
                    print(f"Skipping invalid line: {line.strip()}")
                    continue
                hostname, username, password = parts
                credentials.append({
                    'hostname': hostname.strip(),
                    'username': username.strip(),
                    'password': password.strip()
                })
    except FileNotFoundError:
        print(f"Credentials file not found: {file_path}")
        sys.exit(1)
    return credentials

def execute_ssh_command(hostname, username, password, command):
    """
    Connects to the SSH server and executes the given command.
    """
    try:
        # Initialize the SSH client
        client = paramiko.SSHClient()
        client.set_missing_host_key_policy(paramiko.AutoAddPolicy())

        print(f"Connecting to {hostname}...")
        client.connect(hostname=hostname, username=username, password=password, timeout=10)

        print(f"Executing command on {hostname}: {command}")
        stdin, stdout, stderr = client.exec_command(command)

        # Read command output and errors
        output = stdout.read().decode()
        errors = stderr.read().decode()

        if output:
            print(f"Output from {hostname}:\n{output}")
        if errors:
            print(f"Errors from {hostname}:\n{errors}")

    except paramiko.AuthenticationException:
        print(f"Authentication failed for {username}@{hostname}.")
    except paramiko.SSHException as sshException:
        print(f"Unable to establish SSH connection to {hostname}: {sshException}")
    except Exception as e:
        print(f"Exception occurred while connecting to {hostname}: {e}")
    finally:
        client.close()
        print(f"Connection to {hostname} closed.\n")

def main():
    if len(sys.argv) != 4:
        print("Usage: python ssh_execute.py <credentials_file> <command> <optional: port>")
        print("Example: python ssh_execute.py credentials.txt 'ls -la' 22")
        sys.exit(1)

    credentials_file = sys.argv[1]
    command = sys.argv[2]
    port = int(sys.argv[3]) if len(sys.argv) == 4 else 22

    credentials = read_credentials(credentials_file)

    for cred in credentials:
        execute_ssh_command(
            hostname=cred['hostname'],
            username=cred['username'],
            password=cred['password'],
            command=command
        )

if __name__ == "__main__":
    main()