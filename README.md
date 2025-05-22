# fty-automatic-group

Agent fty-automatic-group to manage automatic group.

## How to build

To build, run:

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=usr -DBUILD_TESTING=On ..
make
sudo make install
```

## How to run

To run fty-automatic-group project:

* from within the source tree, run:

```bash
./build/server/fty-automatic-group
```

For the other options available, refer to the manual page of fty-automatic-group

* from an installed base, using systemd, run:

```bash
systemctl start fty-automatic-group
```
