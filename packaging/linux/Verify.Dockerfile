FROM ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive
COPY *.deb /packages/
# No Qt, compiler or project sources are present before installing the package.
RUN apt-get update && apt-get install -y --no-install-recommends /packages/*.deb \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --create-home --uid 10001 tester
COPY verify-installed.py /verify-installed.py
RUN python3 /verify-installed.py
