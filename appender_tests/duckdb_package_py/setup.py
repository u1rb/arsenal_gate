from setuptools import setup, find_packages
import os

# Package metadata
NAME = "duckdb_package"
VERSION = "1.2.0"
DESCRIPTION = "DuckDB shared library package"
AUTHOR = "DuckDB Team"
AUTHOR_EMAIL = "info@duckdb.org"

setup(
    name=NAME,
    version=VERSION,
    packages=find_packages(),
    package_data={
        'duckdb_package': ['lib/*.so', 'bin/*'],
    },
    include_package_data=True,
    description=DESCRIPTION,
    author=AUTHOR,
    author_email=AUTHOR_EMAIL,
    entry_points={
        'console_scripts': [
            'duckdb-setup-env=duckdb_package.run:run_command',
        ],
    },
    classifiers=[
        "Programming Language :: Python :: 3",
        "License :: OSI Approved :: MIT License",
        "Operating System :: POSIX :: Linux",
    ],
    python_requires=">=3.6",
) 