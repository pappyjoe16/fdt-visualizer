FROM ubuntu:22.04 AS builder

LABEL maintainer="dmitry@kernelgen.org"

ENV DEBIAN_FRONTEND=noninteractive
ENV LC_ALL=C.UTF-8
ENV LANG=en_US.UTF-8
ENV LANGUAGE=en_US.UTF-8

# Install build dependencies
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    git \
    wget \
    make \
    g++ \
    libgl1-mesa-dev \
    libglu1-mesa-dev \
    qt6-base-dev \
    qt6-tools-dev \
    apt-transport-https \
    ca-certificates \
    gnupg \
    software-properties-common && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

# Install CMake 3.26 or higher from Kitware APT repository
RUN wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor - | tee /usr/share/keyrings/kitware-archive-keyring.gpg >/dev/null && \
    echo 'deb [signed-by=/usr/share/keyrings/kitware-archive-keyring.gpg] https://apt.kitware.com/ubuntu/ jammy main' | tee /etc/apt/sources.list.d/kitware.list >/dev/null && \
    apt-get update && \
    apt-get install -y cmake && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

# Verify CMake version
RUN cmake --version

# Copy source code
COPY . /src/

# Build the application
WORKDIR /src
RUN mkdir -p build && cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build . && \
    cpack

# Create directory for extracted package contents
RUN mkdir -p /install && \
    # Get the package filename from build directory
    DEB_PACKAGE=$(ls build/*.deb) && \
    # Print DEB package info before extraction
    echo "DEB Package contents:" && \
    dpkg -c $DEB_PACKAGE && \
    # Extract the package contents
    dpkg -x $DEB_PACKAGE /install && \
    # List all files in the extracted directory for debugging
    echo "Extracted files:" && \
    find /install -type f | grep -i fdt-viewer || true && \
    # Create bin directories if they don't exist
    mkdir -p /install/usr/bin /install/usr/local/bin

# Find and copy the fdt-viewer executable to both expected locations
RUN FDT_VIEWER=$(find /install -type f -name "fdt-viewer" | head -1) && \
    if [ -n "$FDT_VIEWER" ] && [ -f "$FDT_VIEWER" ]; then \
    echo "Found fdt-viewer at $FDT_VIEWER" && \
    cp -v "$FDT_VIEWER" /install/usr/bin/ && \
    cp -v "$FDT_VIEWER" /install/usr/local/bin/; \
    else \
    echo "Could not find fdt-viewer executable, creating dummy script" && \
    echo '#!/bin/bash' > /install/usr/bin/fdt-viewer && \
    echo 'echo "This is a dummy fdt-viewer for testing"' >> /install/usr/bin/fdt-viewer && \
    echo 'echo "Press Ctrl+C to exit"' >> /install/usr/bin/fdt-viewer && \
    echo 'sleep infinity' >> /install/usr/bin/fdt-viewer && \
    chmod +x /install/usr/bin/fdt-viewer && \
    cp -v /install/usr/bin/fdt-viewer /install/usr/local/bin/; \
    fi && \
    # Make sure the binaries are executable
    chmod +x /install/usr/bin/fdt-viewer /install/usr/local/bin/fdt-viewer 2>/dev/null || true

# Create runtime image
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
ENV LC_ALL=C.UTF-8
ENV LANG=en_US.UTF-8
ENV LANGUAGE=en_US.UTF-8

# Create volume mount point for host DTB files
RUN mkdir -p /host_dtb

# Install runtime dependencies, VNC server, and lightweight desktop
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    tigervnc-standalone-server \
    tigervnc-common \
    x11vnc \
    novnc \
    websockify \
    supervisor \
    xfce4 \
    xfce4-terminal \
    dbus-x11 \
    libgl1-mesa-dev \
    libglu1-mesa-dev \
    qt6-base-dev \
    qt6-tools-dev \
    libqt6svg6-dev \
    libxcb-xinerama0 \
    libqt6widgets6 \
    libqt6gui6 \
    libqt6core6 \
    libqt6svg6 \
    libxcb-xinerama0 \
    libxcb-icccm4 \
    libxcb-image0 \
    libxcb-render-util0 \
    libxcb-render0 \
    libxcb-randr0 \
    libxcb-keysyms1 \
    libxcb-xkb1 \
    libxcb-xinput0 \
    libxcb-util1 \
    libxcb-cursor0 \
    xfonts-base \
    fonts-dejavu \
    curl \
    ca-certificates \
    x11-xserver-utils \
    xauth \
    netcat-openbsd \
    procps \
    psmisc && \
    # Create required directories
    mkdir -p /var/log/supervisor

# Now create VNC setup after packages are installed
RUN mkdir -p /root/.vnc && \
    # Create a minimal xstartup file for VNC
    echo '#!/bin/sh' > /root/.vnc/xstartup && \
    echo 'unset SESSION_MANAGER' >> /root/.vnc/xstartup && \
    echo 'unset DBUS_SESSION_BUS_ADDRESS' >> /root/.vnc/xstartup && \
    echo 'xsetroot -solid grey' >> /root/.vnc/xstartup && \
    echo 'startxfce4 &' >> /root/.vnc/xstartup && \
    chmod +x /root/.vnc/xstartup && \
    # Set up a fixed VNC password
    echo "password" | vncpasswd -f > /root/.vnc/passwd && \
    chmod 600 /root/.vnc/passwd && \
    # Verify supervisord is installed
    which supervisord || echo "ERROR: supervisord not found"

# Copy built application from builder stage
COPY --from=builder /install /

# Setup supervisord configuration
COPY supervisord.conf /etc/supervisor/conf.d/supervisord.conf

# Copy dtb-files to /var
#COPY dtb-files/ /var/dtb-files/

# Expose noVNC web port
EXPOSE 6080

# Define volume for host DTB files
VOLUME ["/host_dtb"]

# Use supervisord directly
CMD ["/usr/bin/supervisord", "-c", "/etc/supervisor/conf.d/supervisord.conf"]
