FROM python:3.9.12-slim-bullseye

RUN apt-get update -q && apt-get install -q -y build-essential tar curl zip unzip git pkg-config cmake
RUN pip install poetry

RUN git clone https://github.com/microsoft/vcpkg /tmp/vcpkg
RUN /tmp/vcpkg/bootstrap-vcpkg.sh
RUN /tmp/vcpkg/vcpkg install seal[no-throw-tran] kuku log4cplus cppzmq flatbuffers jsoncpp
RUN /tmp/vcpkg/vcpkg integrate install
ENV VCPKG_ROOT_DIR=/tmp/vcpkg

RUN mkdir /tmp/pyapsi
COPY ./ /tmp/pyapsi
WORKDIR /tmp/pyapsi

RUN mkdir /tmp/pyapsi/external
RUN git clone https://github.com/microsoft/apsi /tmp/pyapsi/external/apsi
# This is a hack to disable AVX2 use, which causes issues during dynamic linking
RUN sed -i "s/-D_AVX2_/-D_AVX_/g" /tmp/pyapsi/external/apsi/CMakeLists.txt
RUN sed -i "s/_AVX2.S/.S/g" /tmp/pyapsi/external/apsi/common/apsi/fourq/amd64/CMakeLists.txt

RUN poetry install
RUN poetry run pip install --verbose .

RUN poetry run python setup.py bdist_wheel

CMD ["poetry", "run", "pytest", "tests"]
