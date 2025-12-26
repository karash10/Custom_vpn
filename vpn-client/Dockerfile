# Use "Bookworm" (Debian 12) which is the current stable version
FROM python:3.9-bookworm

# Install networking tools and GCC
RUN apt-get update && apt-get install -y \
    iproute2 \
    iputils-ping \
    tcpdump \
    gcc \
    libc6-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY client.c .

# Compile the C code
RUN gcc client.c -o vpnclient

CMD ["tail", "-f", "/dev/null"]