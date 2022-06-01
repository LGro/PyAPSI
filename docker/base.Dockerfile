FROM debian:bullseye

RUN apt-get update -q && apt-get install -q -y build-essential tar curl zip unzip git pkg-config cmake

RUN git clone https://github.com/microsoft/vcpkg /tmp/vcpkg
RUN /tmp/vcpkg/bootstrap-vcpkg.sh
RUN /tmp/vcpkg/vcpkg install seal[no-throw-tran] kuku log4cplus cppzmq flatbuffers jsoncpp

RUN git clone https://github.com/microsoft/apsi /tmp/apsi
# This is a hack to disable AVX2 use, which causes issues during dynamic linking
RUN sed -i "s/-D_AVX2_/-D_AVX_/g" /tmp/apsi/CMakeLists.txt
RUN sed -i "s/_AVX2.S/.S/g" /tmp/apsi/common/apsi/fourq/amd64/CMakeLists.txt
