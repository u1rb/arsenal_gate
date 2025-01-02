import os
import hashlib
import glob
import re


def hash_files(pattern):
    """Calculate SHA-256 hash of files matching the given pattern.
    
    Args:
        pattern: Glob pattern to match files
        
    Returns:
        str: SHA-256 hash of concatenated file contents, or None if no files found
    """
    matching_files = glob.glob(pattern)
    if not matching_files:
        return None
        
    # Sort files for deterministic ordering
    matching_files.sort()
    
    # Calculate combined hash of all matching files
    hasher = hashlib.sha256()
    for filepath in matching_files:
        if not os.path.isfile(filepath):
            continue
        with open(filepath, 'rb') as f:
            # Read and update hash in chunks for memory efficiency
            for chunk in iter(lambda: f.read(4096), b''):
                hasher.update(chunk)
                
    return hasher.hexdigest()


def eval_key(key):
    """Process a key, replacing hashFiles patterns with actual hashes.
    
    Args:
        key: String potentially containing hashFiles('pattern') to be evaluated
        
    Returns:
        str: Key with hashFiles calls replaced with actual file hashes
    """
    def replace_hash(match):
        pattern = match.group(1)
        file_hash = hash_files(pattern)
        return file_hash if file_hash is not None else match.group(0)
    
    return re.sub(r"hashFiles\('([^']+)'\)", replace_hash, key)


def test_eval_key():
    os.system("rm -rf test_data/unit_test")
    os.system("mkdir -p test_data/unit_test")
    os.system("echo '{\"dependencies\": {}}' > test_data/unit_test/package-lock.json")
    os.system("cat test_data/unit_test/package-lock.json")
    os.system("sha256sum test_data/unit_test/package-lock.json")

    key = "123-hashFiles('test_data/unit_test/package-lock.json')"
    processed_key = eval_key(key)
    assert (
        processed_key
        == "123-7f465bedbfe41e5d50268ae7e87e88d8fa74c852cd38bece322bcac4e25b5218"
    )

    key = "123-hashFiles('test_data/unit_test/*-lock.json')"
    processed_key = eval_key(key)
    assert (
        processed_key
        == "123-7f465bedbfe41e5d50268ae7e87e88d8fa74c852cd38bece322bcac4e25b5218"
    )
