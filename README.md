srsRAN
======

[![Build Status](https://github.com/srsran/srsRAN_4G/actions/workflows/ccpp.yml/badge.svg?branch=master)](https://github.com/srsran/srsRAN_4G/actions/workflows/ccpp.yml)
[![CodeQL](https://github.com/srsran/srsRAN_4G/actions/workflows/codeql.yml/badge.svg?branch=master)](https://github.com/srsran/srsRAN_4G/actions/workflows/codeql.yml)
[![Coverity](https://scan.coverity.com/projects/28268/badge.svg)](https://scan.coverity.com/projects/srsran_4g_agpl)

srsRAN is an open source 4G software radio suite developed by [SRS](http://www.srs.io). For 5G RAN, see our new O-RAN CU/DU solution - [srsRAN Project](https://www.github.com/srsran/srsran_project).

See the [srsRAN 4G project pages](https://www.srsran.com) for information, guides and project news.

The srsRAN suite includes:
  * srsUE - a full-stack SDR 4G UE application with prototype 5G features
  * srsENB - a full-stack SDR 4G eNodeB application
  * srsEPC - a light-weight 4G core network implementation with MME, HSS and S/P-GW

For application features, build instructions and user guides see the [srsRAN 4G documentation](https://docs.srsran.com/projects/4g/).

For license details, see LICENSE file.

For building the application 
cd srsRAN_4G
mkdir build
cd build \n
cmake .. \
  -DENABLE_ZMQ=ON \
  -DENABLE_EXAMPLES=ON \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_FLAGS="-Werror -Wno-error=array-bounds" \
  -DCMAKE_CXX_FLAGS="-Werror -Wno-error=array-bounds"
make -j$(nproc)

Afterwards once it has succeffuly been build you need to relocate the default profile or make your own to the src folder of the srsue binary.
mkdir -p default_config
file ./srsue/ue.conf.example
ls -l ./srsue/ue.conf.example
./srsue --config=default_config/ue.conf

Also after you have build it you can run cell search in the build folder using:
 ./lib/examples/cell_search -t

Support
=======

Mailing list: https://lists.srsran.com/mailman/listinfo/srsran-users

