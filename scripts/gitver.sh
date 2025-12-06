#!/bin/bash
set -e

# Get git version (example: 0.1.0-4-ga7ef0fa)
GIT_VERSION="$(git describe --tags --always)"

# Replace '-' with '+' to satisfy Debian version rules
# Example: 0.1.0-4-ga7ef0fa → 0.1.0+4+ga7ef0fa
DEB_VERSION="${GIT_VERSION//-/+}"

# Print final version (CMake will capture this)
echo "$DEB_VERSION"
