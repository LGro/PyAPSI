FROM python:3.9.12-slim-bullseye

RUN apt-get update && apt-get install -y build-essential tar curl zip unzip git pkg-config

RUN git clone https://github.com/microsoft/vcpkg /tmp/vcpkg

RUN ./tmp/vcpkg/bootstrap-vcpkg.sh && \
    export CXXFLAGS="$CXXFLAGS -fPIC" && \
    ./tmp/vcpkg/vcpkg install seal[no-throw-tran] kuku log4cplus cppzmq flatbuffers jsoncpp apsi

ENV VCPKG_INSTALLED_DIR=/tmp/vcpkg/installed

COPY . /tmp/pyapsi

RUN cd /tmp/pyapsi && \
    pip install poetry && \
    poetry install && \
    poetry run pip install --verbose .
