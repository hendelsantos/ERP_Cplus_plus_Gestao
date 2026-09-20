#!/usr/bin/env bash
set -euo pipefail
project_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$project_root"
# Artifact export only: no service is deployed, and no host package is installed.
docker build --file packaging/linux/Dockerfile --target artifact \
    --output "type=local,dest=$project_root/dist" .

package_version="$(sed -n 's/^project(MHStore VERSION \([^ ]*\).*/\1/p' desktop/CMakeLists.txt)"
package_arch="$(docker run --rm ubuntu:24.04 dpkg --print-architecture)"
"$project_root/scripts/verify-package-linux.sh" "$project_root/dist/mhstore_${package_version}_${package_arch}.deb"
