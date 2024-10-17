import os
from fsspec.implementations.cached import CachingFileSystem
import fsspec
from filelock import FileLock, Timeout

def setup_cached_gcs_filesystem(cache_dir: str, cache_max_size_gb: int = 20) -> CachingFileSystem:
    # (Same as before)
    os.makedirs(cache_dir, exist_ok=True)
    gcs_fs = fsspec.filesystem('gcs')
    cached_fs = CachingFileSystem(
        fs=gcs_fs,
        cache_storage=cache_dir,
        cache_max_size=int(cache_max_size_gb * 1e9),
        same_names=False,
        expiry_times=None
    )
    return cached_fs

def download_from_gcs(cached_fs: CachingFileSystem, gcs_path: str, local_path: str, lock_timeout: int = 10):
    """
    Downloads a file or directory from GCS to the local filesystem using the cached filesystem.
    Implements file-level locking to handle concurrent access.

    Parameters:
    - cached_fs (CachingFileSystem): The cached filesystem object.
    - gcs_path (str): The GCS path to the file or directory.
    - local_path (str): The local path where the file or directory will be saved.
    - lock_timeout (int): Maximum time to wait for acquiring the lock (in seconds).
    """
    lock_path = f"{local_path}.lock"
    lock = FileLock(lock_path, timeout=lock_timeout)

    try:
        with lock:
            if cached_fs.isdir(gcs_path):
                cached_fs.get(gcs_path, local_path, recursive=True)
                print(f"Directory '{gcs_path}' has been downloaded to '{local_path}'.")
            elif cached_fs.isfile(gcs_path):
                os.makedirs(os.path.dirname(local_path), exist_ok=True)
                cached_fs.get(gcs_path, local_path)
                print(f"File '{gcs_path}' has been downloaded to '{local_path}'.")
            else:
                print(f"The path '{gcs_path}' does not exist in GCS.")
    except Timeout:
        print(f"Could not acquire lock for '{local_path}' within {lock_timeout} seconds.")

def main():
    # (Same as before)
    cache_directory = "./gcs_cache"
    cached_gcs_fs = setup_cached_gcs_filesystem(cache_dir=cache_directory, cache_max_size_gb=20)

    examples = [
        {
            "gcs_path": "your-bucket-name/path/to/your-file.csv",
            "local_path": "downloads/your-file.csv"
        },
        {
            "gcs_path": "your-bucket-name/path/to/your-directory/",
            "local_path": "downloads/your-directory/"
        }
    ]

    # Example with concurrent downloads using threading
    from concurrent.futures import ThreadPoolExecutor

    with ThreadPoolExecutor(max_workers=5) as executor:
        futures = [
            executor.submit(download_from_gcs, cached_gcs_fs, ex["gcs_path"], ex["local_path"])
            for ex in examples
        ]
        for future in futures:
            future.result()

if __name__ == "__main__":
    main()