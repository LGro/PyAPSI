FROM python:3.9.12-slim-bullseye

RUN apt-get update -q && apt-get install -q -y build-essential tar curl zip unzip git pkg-config cmake

RUN git clone https://github.com/microsoft/vcpkg /tmp/vcpkg

RUN ./tmp/vcpkg/bootstrap-vcpkg.sh && \
    export CXXFLAGS="$CXXFLAGS -fPIC" && \
    ./tmp/vcpkg/vcpkg install seal[no-throw-tran] kuku log4cplus cppzmq flatbuffers jsoncpp apsi

RUN /tmp/vcpkg/vcpkg integrate install

ENV VCPKG_ROOT_DIR=/tmp/vcpkg

RUN mkdir /tmp/pyapsi
COPY ./ /tmp/pyapsi
WORKDIR /tmp/pyapsi

RUN pip install poetry && \
    poetry install && \
    poetry run pip install --verbose .

CMD ["poetry", "run", "pytest", "tests"]
