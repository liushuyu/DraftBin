#!/bin/bash -e

# $1: file name
function reconstruct_xcode_img() {
  TMP="$(mktemp -d -p .)"

  echo 'Extracting files...'
  pushd "$TMP"
  bsdtar -xf "$1"
  # save disk space
  rm "$1"

  python2 ../unscramble.py Content
  echo 'Decompressing xz segments...'

  for segment in *part*.xz; do
    # remove empty files
    if [[ "x$(du $segment | cut -f 1)" == 'x0' ]]; then
      rm "$segment"
    else
      unxz "$segment"
    fi
  done
  echo "Reconstrcting image file..."
  cat *.cpio > "$2/Xcode_$XCODE_VER.cpio"

  echo "Cleaning up..."
  rm -f *part*.cpio
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
echo 'Building LLVM compiler runtime...'
./build_compiler_rt.sh

echo "Done. Your toolchain is built at ${OC_SYSROOT}"
echo 'Please run the commands above as root to complete the installation'
