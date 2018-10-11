#!/bin/bash -e

# $1: file name
function reconstruct_xcode_img() {
  source ./extract_image.sh
  make_sdk_tbl "$1"
}

if [[ "x$SLIENT_RUNNING" != 'x' ]]; then
  STDOUT="$(readlink -f osxcross_build.log)"
  echo "Running in less verbose mode. Detailed log: ${STDOUT}"
else
  STDOUT='/dev/stdout'
fi

if [[ "x$XCODE_VER" == 'x' ]]; then
  export XCODE_VER='9.2'
  echo "Xcode version not specified, using version $XCODE_VER"
fi

echo 'Downloading Xcode package...'
python3 fetch-xcode.py

bash download_xcode$XCODE_VER.sh >> "${STDOUT}"

echo 'Making SDK tarball...'
reconstruct_xcode_img "$(readlink -f Command_Line_Tools_macOS_10.13_for_Xcode_${XCODE_VER}.dmg)"

echo 'Cloning osxcross repository...'
if [[ -d osxcross ]]; then
  rm -rf osxcross
fi

git clone --depth=50 https://github.com/tpoechtrager/osxcross/
cd osxcross
patch -Np1 -i ../0002-make-prefix-changeable.patch

mv ../MacOSX10.*.sdk.tar.* ./tarballs/

if [[ "x${OC_SYSROOT}" == 'x' ]]; then
  OC_SYSROOT="$(readlink -f ./target)"
fi

set +e
echo 'Build initial toolchain (will fail)'
if UNATTENDED=1 ./build.sh >> "${STDOUT}"; then
  echo "That's strange... This should fail though..."
fi
set -e

echo "Toolchain will be installed to ${OC_SYSROOT}"

echo 'Building TAPI library...'
git clone https://github.com/tpoechtrager/apple-libtapi.git
cd apple-libtapi
git checkout -f 2.0.0
INSTALLPREFIX="${OC_SYSROOT}" ./build.sh >> "${STDOUT}"
INSTALLPREFIX="${OC_SYSROOT}" ./install.sh >> "${STDOUT}"
cd ..

echo 'Building newer version of cctools...'
git clone https://github.com/tpoechtrager/cctools-port.git
pushd cctools-port/cctools/
./configure --prefix="${OC_SYSROOT}" --target=x86_64-apple-darwin17 --with-libtapi="${OC_SYSROOT}" >> "${STDOUT}"
make -j$(nproc) >> "${STDOUT}"
make install >> "${STDOUT}"
popd

echo 'Building LLVM dsymutil...'
./build_llvm_dsymutil.sh >> "${STDOUT}"

if [[ "x${XCODE_NORT}" != 'x' ]]; then
  echo "Skipped building compiler runtime."
  exit
fi

RT_BUILD_LOG="$(mktemp --suffix='.log' -p .)"
echo 'Building LLVM compiler runtime...'
if [[ "${STDOUT}" == '/dev/stdout' ]]; then
  ./build_compiler_rt.sh | tee "${RT_BUILD_LOG}"
else
  ./build_compiler_rt.sh > "${RT_BUILD_LOG}"
  cat "${RT_BUILD_LOG}" >> "${STDOUT}"
fi

echo "Done. Your toolchain is built at ${OC_SYSROOT}"

set +e
if which perl; then
  perl -0777 -ne '/hand to install compiler-rt:\n(.*)/sg && print $1' < "${RT_BUILD_LOG}" > install_rt.sh
  echo 'Please run install_rt.sh as root manually to install LLVM runtime. This operation may corrupt your LLVM installation.'
  exit 0
fi
echo 'Please run the commands above as root to complete the installation'
