FROM redhat/ubi8:8.8

RUN yum install -y \
    curl tar \
    gcc \
    openssl-devel perl

RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y

RUN source $HOME/.cargo/env && \
    cargo install code2prompt

ENTRYPOINT ["/root/.cargo/bin/code2prompt"]