#!/bin/bash -e

# $1: file name
function reconstruct_xcode_img() {
  TMP="$(mktemp -d -p .)"

  echo 'Extracting files...'
  pushd "$TMP"
  bsdtar -xf "$1"
  # save disk space
  rm "$1"

  echo 'Reconstrcting image file...'
  python2 ../unscramble.py Content "$2/Xcode_$XCODE_VER.cpio"

  echo "Cleaning up..."
  popd
  rm -rf "$TMP"
}

if [[ "x$XCODE_VER" == 'x' ]]; then
  export XCODE_VER='9.2'
  echo "Xcode version not specified, using version $XCODE_VER"
fi

echo 'Downloading Xcode package...'
python3 fetch-xcode.py

bash download_xcode$XCODE_VER.sh

reconstruct_xcode_img "$(readlink -f Xcode_$XCODE_VER.xip)" "$(pwd)"

echo 'Expanding archive...'
cpio -i < Xcode_$XCODE_VER.cpio

rm -f Xcode_$XCODE_VER.cpio

echo 'Cloning osxcross repository...'
git clone --depth=50 https://github.com/tpoechtrager/osxcross/
cd osxcross
patch -Np1 -i ../0001-add-10.13-support.patch

echo 'Making SDK tarball...'
XCODEDIR="$(readlink -f ../Xcode.app)" ./tools/gen_sdk_package.sh
mv MacOSX10.13.sdk.tar.* ./tarballs/

set +e
echo 'Build initial toolchain (will fail)'
if UNATTENDED=1 ./build.sh; then
  echo "That's strange... This should fail though..."
fi
set -e

OC_SYSROOT="$(readlink -f ./target)"

echo 'Building TAPI library...'
git clone https://github.com/tpoechtrager/apple-libtapi.git
cd apple-libtapi
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
