from typing import Dict, List, Optional
import subprocess
import os
import asyncio
import sys
import fcntl
import paramiko

from loguru import logger


class Cli:
    workdir: Optional[str] = None
    env: Dict[str, str] = {}
    commands: List[str] = []
    remote_host: Optional[str] = None
    ssh_client: Optional[paramiko.SSHClient] = None

    def with_workdir(self, workdir: str) -> "Cli":
        self.workdir = workdir
        return self

    def with_env(self, env: Dict[str, str]) -> "Cli":
        self.env.update(env)
        return self

    def with_cmd(self, cmd: str) -> "Cli":
        self.commands.append(cmd)
        return self

    def with_remote_ssh(self, hostname: str) -> "Cli":
        self.remote_host = hostname

        # Setup SSH client
        self.ssh_client = paramiko.SSHClient()
        self.ssh_client.set_missing_host_key_policy(paramiko.AutoAddPolicy())

        # Try to load system host keys
        try:
            self.ssh_client.load_system_host_keys()
        except Exception as e:
            logger.error(f"Failed to load system host keys: {e}")

        # Connect using SSH config
        config = paramiko.SSHConfig()
        try:
            with open(os.path.expanduser("~/.ssh/config")) as f:
                config.parse(f)
        except Exception as e:
            logger.error(f"Failed to load SSH config: {e}")

        host_config = config.lookup(hostname)

        # Get connection details from SSH config
        hostname = host_config.get("hostname", hostname)
        username = host_config.get("user")
        key_filename = host_config.get("identityfile", [None])[0]
        port = int(host_config.get("port", 22))

        try:
            self.ssh_client.connect(
                hostname=hostname,
                username=username,
                key_filename=key_filename,
                port=port,
            )
        except Exception as e:
            logger.error(f"Failed to connect to {hostname}: {str(e)}")

        return self

    def read_file(self, filename: str) -> bytes:
        filepath = os.path.join(self.workdir, filename)

        if self.remote_host and self.ssh_client:
            sftp = self.ssh_client.open_sftp()
            with sftp.file(filepath, "rb") as f:
                content = f.read()
            sftp.close()
            return content
        else:
            if not os.path.exists(filepath):
                raise FileNotFoundError(f"File not found: {filepath}")
            with open(filepath, "rb") as f:
                return f.read()

    def write_file(self, filename: str, content: bytes) -> None:
        filepath = os.path.join(self.workdir, filename)

        if self.remote_host and self.ssh_client:
            sftp = self.ssh_client.open_sftp()
            with sftp.file(filepath, "wb") as f:
                f.write(content)
            sftp.close()
        else:
            with open(filepath, "wb") as f:
                f.write(content)

    def _configure_logger(self):
        logger.remove()  # Remove any existing handlers

        if self.plain_logging:
            # Plain logging - just print the message
            logger.add(
                lambda msg: print(msg.record["message"], end="\n", flush=True),
                filter=lambda record: "stdout" in record["extra"],
            )
            logger.add(
                lambda msg: print(
                    msg.record["message"], end="\n", flush=True, file=sys.stderr
                ),
                filter=lambda record: "stderr" in record["extra"],
            )
        else:
            # Colored logging with green for stdout and red for stderr
            logger.add(
                lambda msg: print(
                    f"\033[32m{msg.record['message']}\033[0m", end="\n", flush=True
                ),
                filter=lambda record: "stdout" in record["extra"],
            )
            logger.add(
                lambda msg: print(
                    f"\033[31m{msg.record['message']}\033[0m",
                    end="\n",
                    flush=True,
                    file=sys.stderr,
                ),
                filter=lambda record: "stderr" in record["extra"],
            )

    def with_log(self, plain: bool = False) -> "Cli":
        self.plain_logging = plain
        self._configure_logger()
        return self

    def _set_non_blocking(self, pipe):
        fd = pipe.fileno()
        flags = fcntl.fcntl(fd, fcntl.F_GETFL)
        fcntl.fcntl(fd, fcntl.F_SETFL, flags | os.O_NONBLOCK)

    def _process_output(self, proc: subprocess.Popen) -> None:
        self._set_non_blocking(proc.stdout)
        self._set_non_blocking(proc.stderr)

        while proc.poll() is None:
            try:
                stdout_line = proc.stdout.readline() if proc.stdout else None
                if stdout_line:
                    logger.bind(stdout=True).info(stdout_line.decode().strip())
            except (IOError, BlockingIOError):
                pass

            try:
                stderr_line = proc.stderr.readline() if proc.stderr else None
                if stderr_line:
                    logger.bind(stderr=True).error(stderr_line.decode().strip())
            except (IOError, BlockingIOError):
                pass

        # Read any remaining output
        for line in proc.stdout:
            logger.bind(stdout=True).info(line.decode().strip())
        for line in proc.stderr:
            logger.bind(stderr=True).error(line.decode().strip())

    async def _run_remote_cmd(self, cmd: str) -> None:
        if not self.ssh_client:
            raise Exception("SSH client not initialized")

        # Prepare command with environment variables and working directory
        full_cmd = ""
        if self.env:
            env_str = " ".join(f"{k}={v}" for k, v in self.env.items())
            full_cmd += f"export {env_str}; "
        if self.workdir:
            full_cmd += f"cd {self.workdir}; "
        full_cmd += cmd

        stdin, stdout, stderr = self.ssh_client.exec_command(full_cmd)

        async def read_output(stream, is_stdout=True):
            while True:
                line = await asyncio.to_thread(stream.readline)
                if not line:
                    break
                if is_stdout:
                    logger.bind(stdout=True).info(line.strip())
                else:
                    logger.bind(stderr=True).error(line.strip())

        # Use asyncio.gather to wait for both streams
        await asyncio.gather(read_output(stdout, True), read_output(stderr, False))

        exit_status = stdout.channel.recv_exit_status()
        if exit_status != 0:
            raise subprocess.CalledProcessError(exit_status, cmd)

    def run_seq(self) -> None:
        if self.remote_host:
            for cmd in self.commands:
                asyncio.run(self._run_remote_cmd(cmd))
            return

        merged_env = {**os.environ.copy(), **self.env}

        for cmd in self.commands:
            proc = subprocess.Popen(
                cmd,
                shell=True,
                cwd=self.workdir,
                env=merged_env,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                bufsize=1,
                universal_newlines=False,
            )
            self._process_output(proc)

            if proc.returncode != 0:
                raise subprocess.CalledProcessError(proc.returncode, cmd)

    async def _run_cmd_async(self, cmd: str) -> None:
        if self.remote_host:
            await self._run_remote_cmd(cmd)
            return

        merged_env = {**os.environ.copy(), **self.env}

        proc = await asyncio.create_subprocess_shell(
            cmd,
            cwd=self.workdir,
            env=merged_env,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
        )

        async def read_stream(stream, is_stdout=True):
            while True:
                line = await stream.readline()
                if not line:
                    break
                if is_stdout:
                    logger.bind(stdout=True).info(line.decode().strip())
                else:
                    logger.bind(stderr=True).error(line.decode().strip())

        await asyncio.gather(
            read_stream(proc.stdout, True), read_stream(proc.stderr, False)
        )

        await proc.wait()
        if proc.returncode != 0:
            raise subprocess.CalledProcessError(proc.returncode, cmd)

    def run_con(self) -> None:
        async def main():
            tasks = [self._run_cmd_async(cmd) for cmd in self.commands]
            await asyncio.gather(*tasks)

        asyncio.run(main())

    def __del__(self):
        if self.ssh_client:
            self.ssh_client.close()
