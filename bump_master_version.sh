#!/bin/bash

# This script needs to be edited to bump old develop version to new master version for new release.
# - run this script in develop branch 
# - after running this script merge develop into master
# - after running this script and merging develop into master, run bump_develop_version.sh in master and
#   merge master into develop

OLD_HIPBLAS_VERSION="13.5.0"
NEW_HIPBLAS_VERSION="12.6.0"

sed -i "s/${OLD_HIPBLAS_VERSION}/${NEW_HIPBLAS_VERSION}/g" CMakeLists.txt
