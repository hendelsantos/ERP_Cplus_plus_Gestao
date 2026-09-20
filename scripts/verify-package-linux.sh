#!/usr/bin/env bash
set -euo pipefail
project_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
if [[ $# != 1 || ! -f "$1" ]]; then
    echo "Uso: scripts/verify-package-linux.sh caminho/pacote.deb" >&2
    exit 2
fi
package_context="$(mktemp -d)"
trap 'rm -rf -- "$package_context"' EXIT
cp -- "$1" "$package_context/"
cp -- "$project_root/packaging/linux/verify-installed.py" "$package_context/"
docker build --file "$project_root/packaging/linux/Verify.Dockerfile" "$package_context"
