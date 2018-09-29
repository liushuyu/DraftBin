#!/bin/bash -e

# $1: file name
function reconstruct_xcode_img() {
  source ./extract_image.sh
  make_sdk_tbl "$1"
}

if [[ "x$XCODE_VER" == 'x' ]]; then
  export XCODE_VER='9.2'
  echo "Xcode version not specified, using version $XCODE_VER"
fi

echo 'Downloading Xcode package...'
python3 fetch-xcode.py

bash download_xcode$XCODE_VER.sh

echo 'Making SDK tarball...'
reconstruct_xcode_img "$(readlink -f Command_Line_Tools_macOS_10.13_for_Xcode_${XCODE_VER}.dmg)"

echo 'Cloning osxcross repository...'
git clone --depth=50 https://github.com/tpoechtrager/osxcross/
cd osxcross
patch -Np1 -i ../0002-make-prefix-changeable.patch

mv ../MacOSX10.13.sdk.tar.* ./tarballs/

if [[ "x${OC_SYSROOT}" == 'x' ]]; then
  OC_SYSROOT="$(readlink -f ./target)"
fi

set +e
echo 'Build initial toolchain (will fail)'
if UNATTENDED=1 ./build.sh; then
  echo "That's strange... This should fail though..."
fi
set -e

echo "Toolchain will be installed to ${OC_SYSROOT}"

echo 'Building TAPI library...'
git clone https://github.com/tpoechtrager/apple-libtapi.git
cd apple-libtapi
git checkout -f 2.0.0
INSTALLPREFIX="${OC_SYSROOT}" ./build.sh
INSTALLPREFIX="${OC_SYSROOT}" ./install.sh
cd ..

echo 'Building newer version of cctools...'
git clone https://github.com/tpoechtrager/cctools-port.git
pushd cctools-port/cctools/
./configure --prefix="${OC_SYSROOT}" --target=x86_64-apple-darwin17 --with-libtapi="${OC_SYSROOT}"
make && make install
popd

echo 'Building LLVM dsymutil...'
./build_llvm_dsymutil.sh

RT_BUILD_LOG="$(mktemp --suffix='.log' -p .)"
echo 'Building LLVM compiler runtime...'
./build_compiler_rt.sh | tee "${RT_BUILD_LOG}"

echo "Done. Your toolchain is built at ${OC_SYSROOT}"

set +e
if which perl; then
  perl -0777 -ne '/hand to install compiler-rt:\n(.*)/sg && print $1' < "${RT_BUILD_LOG}" > install_rt.sh
  echo 'Please run install_rt.sh as root manually to install LLVM runtime. This operation may corrupt your LLVM installation.'
  exit 0
fi
echo 'Please run the commands above as root to complete the installation'
