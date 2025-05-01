#!/bin/bash
set -e

echo "Starting VNC web container..."

# Kill any existing VNC sessions
pkill -f "Xvnc|Xtigervnc|x11vnc" || true

# Make sure .vnc directory has proper permissions
mkdir -p /root/.vnc
chmod 700 /root/.vnc

# Verify fdt-viewer executable
echo "Verifying application executable:"
if [ -x /usr/bin/fdt-viewer ]; then
    echo "Found fdt-viewer at /usr/bin/fdt-viewer"
elif [ -x /usr/local/bin/fdt-viewer ]; then
    echo "Found fdt-viewer at /usr/local/bin/fdt-viewer"
    # Create a symlink for consistency
    ln -sf /usr/local/bin/fdt-viewer /usr/bin/fdt-viewer
else
    echo "WARNING: fdt-viewer executable not found in standard locations!"
    # Find it anywhere in the filesystem
    FDT_VIEWER=$(find / -name "fdt-viewer" -type f -executable 2>/dev/null | head -1)
    if [ -n "$FDT_VIEWER" ]; then
        echo "Found fdt-viewer at $FDT_VIEWER"
        ln -sf "$FDT_VIEWER" /usr/bin/fdt-viewer
    else
        echo "ERROR: Could not find fdt-viewer executable anywhere."
        # Create a dummy script for testing purposes
        echo "Creating dummy fdt-viewer script for testing"
        echo '#!/bin/bash' > /usr/bin/fdt-viewer
        echo 'echo "This is a dummy fdt-viewer for testing"' >> /usr/bin/fdt-viewer
        echo 'echo "Press Ctrl+C to exit"' >> /usr/bin/fdt-viewer
        echo 'sleep infinity' >> /usr/bin/fdt-viewer
        chmod +x /usr/bin/fdt-viewer
    fi
fi

echo "Starting supervisord..."
exec /usr/bin/supervisord -c /etc/supervisor/conf.d/supervisord.conf
