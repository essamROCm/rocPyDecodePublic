FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive
ARG ROCM_SERIES=10.1
ARG GPU_TARGET=gfx1250
ARG ROCM_BUILD=20260821-32431166035
ARG ROCM_PACKAGE_VERSION=10.1.0~20260821-32431166035

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        wget \
    && rm -rf /var/lib/apt/lists/*

RUN echo "deb [trusted=yes] https://rocm.nightlies.amd.com/packages-multi-arch/deb/${ROCM_BUILD} stable main" \
        > /etc/apt/sources.list.d/rocm-nightly.list \
    && apt-get update \
    && apt-get install -y --no-install-recommends \
        "amdrocm-core-sdk${ROCM_SERIES}-${GPU_TARGET}=${ROCM_PACKAGE_VERSION}" \
        "amdrocm-decode-dev${ROCM_SERIES}=${ROCM_PACKAGE_VERSION}" \
        "amdrocm-decode-test${ROCM_SERIES}=${ROCM_PACKAGE_VERSION}" \
        build-essential \
        cmake \
        git \
        libavcodec-dev \
        libavformat-dev \
        libavutil-dev \
        libdlpack-dev \
        libnuma-dev \
        libswscale-dev \
        ninja-build \
        pkg-config \
        python3.12 \
        python3.12-dev \
        python3.12-venv \
    && rm -rf /var/lib/apt/lists/*

RUN python3.12 -m venv /opt/venv \
    && /opt/venv/bin/python -m pip install --no-cache-dir --upgrade pip setuptools wheel \
    && /opt/venv/bin/python -m pip install --no-cache-dir numpy pybind11 pytest

ENV VIRTUAL_ENV=/opt/venv
ENV ROCM_PATH=/opt/rocm
ENV ROCM_HOME=/opt/rocm
ENV PATH=/opt/venv/bin:/opt/rocm/bin:/opt/rocm/lib/llvm/bin:${PATH}
ENV CMAKE_PREFIX_PATH=/opt/rocm/lib/cmake:/opt/venv/lib/python3.12/site-packages/pybind11/share/cmake/pybind11
ENV LD_LIBRARY_PATH=/opt/rocm/lib

RUN hipconfig --full \
    && test -d /opt/rocm/share/rocdecode/utils \
    && test -d /opt/rocm/share/rocdecode/test \
    && test -d /opt/rocm/share/rocdecode/video

WORKDIR /workspace/EssamWork/ROCPYDECODE

CMD ["bash"]
