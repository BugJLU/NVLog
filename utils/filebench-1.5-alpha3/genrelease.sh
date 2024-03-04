#!/bin/bash -x

# Run from within git repo to generate a release tarball
version="`grep AC_INIT configure.ac | grep -Po '\[\K[^\]]*' | tail -1`"


# Step 1: Generate auto-tool scripts
libtoolize || die
aclocal || die
autoheader || die
automake -a || die
autoconf || die

# Step 2: Copy into a properly named directory
cd ..
cp --dereference -Rvf filebench filebench-$version

# Step 3: Remove git repo metadata
cd filebench-$version
rm -Rvf .git .gitignore
cd ..

# Step 4: Generate tarballs
tar -czvf filebench-$version.tar.gz filebench-$version
zip -r filebench-$version.zip filebench-$version
