#!/bin/bash
set -ueo pipefail

cd `dirname $0`

cargo b 
cargo test 