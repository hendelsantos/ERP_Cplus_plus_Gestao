# Build against the same distribution used by the first supported package.
FROM ubuntu:24.04 AS build
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build dpkg-dev libssl-dev qt6-base-dev qt6-declarative-dev \
    libqt6sql6-sqlite qml6-module-qtqml qml6-module-qtqml-models qml6-module-qtqml-workerscript \
    qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-layouts \
    qml6-module-qtquick-templates qml6-module-qtquick-window \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY desktop/ desktop/
COPY packaging/ packaging/
COPY docs/distribuicao.md docs/distribuicao.md
RUN cmake -S desktop -B /build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr \
    && cmake --build /build --parallel 4 \
    && ctest --test-dir /build --output-on-failure \
    && cd /build && cpack -G DEB
FROM scratch AS artifact
COPY --from=build /build/*.deb /
