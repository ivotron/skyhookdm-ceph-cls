#!/bin/bash

docker run -di --name test -v /home/travis/build/uccross/skyhookdb-ceph:/ceph cephbuilder/ceph:${RELEASE}
docker exec test /bin/bash -c "mkdir build && cd build && cmake .."
docker exec test /bin/bash -c "cd build && make -j2 ceph-osd osdc librados cls_tabular"
# to run vstart, we additionally need: 
#   make j2 ceph-authtool ceph-mon monmaptool crushtool
#   cd /ceph/build/src/pybind && make -j2