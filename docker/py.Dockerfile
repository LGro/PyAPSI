ARG PYTHON_VERSION
FROM python:$PYTHON_VERSION-slim-bullseye

RUN apt-get update -q && apt-get install -q -y build-essential git cmake patchelf

RUN mkdir /tmp/vcpkg
COPY --from=pyapsi:base /tmp/vcpkg /tmp/vcpkg

RUN mkdir /tmp/pyapsi
COPY --from=pyapsi:base /tmp/pyapsi /tmp/pyapsi

ENV VCPKG_ROOT_DIR=/tmp/vcpkg

WORKDIR /tmp/pyapsi

RUN pip install poetry
RUN poetry install
RUN poetry run python setup.py bdist_wheel
RUN poetry run auditwheel repair --plat manylinux_2_31_x86_64 dist/*.whl
RUN poetry run pip install wheelhouse/*.whl

CMD ["poetry", "run", "pytest", "tests"]
