#!/usr/bin/env bash
set -euo pipefail
project_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
qt_root="${MHSTORE_QT_ROOT:-$HOME/.local/share/mhstore-qt}"
if [[ -d "$qt_root/usr/lib/x86_64-linux-gnu/cmake/Qt6" ]]; then
    export CMAKE_PREFIX_PATH="$qt_root/usr${CMAKE_PREFIX_PATH:+:$CMAKE_PREFIX_PATH}"
    export LD_LIBRARY_PATH="$qt_root/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
    export QT_PLUGIN_PATH="$qt_root/usr/lib/x86_64-linux-gnu/qt6/plugins${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}"
    export QML_IMPORT_PATH="$qt_root/usr/lib/x86_64-linux-gnu/qt6/qml${QML_IMPORT_PATH:+:$QML_IMPORT_PATH}"
fi
command="${1:-build}"
if [[ $# -gt 0 ]]; then shift; fi
case "$command" in
    configure) exec cmake -S "$project_root/desktop" -B "$project_root/build" "$@" ;;
    build)
        cmake -S "$project_root/desktop" -B "$project_root/build"
        exec cmake --build "$project_root/build" --parallel 4 "$@"
        ;;
    test) exec ctest --test-dir "$project_root/build" --output-on-failure "$@" ;;
    run) exec "$project_root/build/bin/MHStore" "$@" ;;
    *) echo 'Uso: scripts/dev.sh {configure|build|test|run}' >&2; exit 2 ;;
esac
