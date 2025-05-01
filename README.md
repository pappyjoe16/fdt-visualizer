# FDT Viewer
Flattened Device Tree Viewer written in Qt.

The main app without dockerlized is in https://github.com/dev-0x7C6/fdt-viewer 

#### Supported files
* \*.dtb - devicetree blob
* \*.dtbo - devicetree overlay blob
* \*.itb - fit image container

#### Features
* Quick search for single or multiple device-trees
* Show embedded inner device-tree data

#### Installation
Build the docker image using "docker build -t fdt-viewer-web -f Dockerfile . --no-cache"
To run the image you need to include the .dtb file to be mounted in /host_dtb 
    docker run -d -p 6080:6080 -v path/to/dtb/files:/host_dtb fdt-viewer-web

Once the docker image is running 
Open your browser http://localhost:6080/vnc.html

