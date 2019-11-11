FROM debian:10
LABEL maintainer="mario.bielert@tu-dresden.de"

RUN useradd -m metricq
RUN apt-get update && apt-get install -y git cmake tzdata libprotobuf-dev protobuf-compiler build-essential libssl-dev libfreeipmi-dev alien

COPY --chown=metricq:metricq . /home/metricq/metricq-source-hdeem

WORKDIR /home/metricq/metricq-source-hdeem
RUN ln -s /usr/lib /usr/lib64
RUN alien *.rpm
RUN dpkg -i *.deb

USER metricq
RUN mkdir build

WORKDIR /home/metricq/metricq-source-hdeem/build
RUN cmake -DCMAKE_BUILD_TYPE=Release .. && make -j 2

ENTRYPOINT ["/home/metricq/metricq-source-hdeem/build/metricq-source-hdeem"]
CMD ["--token", "metricq-source-hdeem", "--server", "amqps://guest@guest:localhost"]
