#!/usr/bin/env bash
set -euo pipefail

cd /tmp/mesa

mkdir -p /usr/local/libexec
cp bindgen-switch-wrapper.sh /usr/local/libexec/bindgen
cp rustc-switch-wrapper.sh /usr/local/libexec/rustc
cp bindgen-atomic-shim.h /usr/local/libexec/bindgen-atomic-shim.h
chmod +x /usr/local/libexec/bindgen /usr/local/libexec/rustc
export MESA_SWITCH_RUSTC=/opt/rust/cargo/bin/rustc
export MESA_SWITCH_BINDGEN=/opt/rust/cargo/bin/bindgen
export MESA_SWITCH_RUST_TARGET=aarch64-unknown-linux-gnu
export PATH="/usr/local/libexec:/tmp/mesa/builddir-native/src/compiler/clc:/tmp/mesa/builddir-native/src/compiler/spirv:$PATH"

cp src/gallium/winsys/nouveau/drm/nouveau.h /opt/devkitpro/portlibs/switch/include/
cp src/nouveau/headers/nv_device_info.h /opt/devkitpro/portlibs/switch/include/

if [ ! -d /usr/lib/llvm-15/lib/clang/15/include ]; then
    ln -s /usr/lib/llvm-15/lib/clang/15.0.6/include /usr/lib/llvm-15/lib/clang/15/include
fi

if [ ! -f /opt/devkitpro/portlibs/switch/lib/libdl.a ]; then
    printf 'void __mesa_switch_stub_lib(void) {}\n' > /tmp/stub_posix.c
    /opt/devkitpro/devkitA64/bin/aarch64-none-elf-gcc -c /tmp/stub_posix.c -o /tmp/stub_posix.o
    for library in dl rt util; do
        /opt/devkitpro/devkitA64/bin/aarch64-none-elf-ar rcs \
            "/opt/devkitpro/portlibs/switch/lib/lib${library}.a" /tmp/stub_posix.o
    done
fi

meson setup builddir-native \
    -Dvulkan-drivers= -Dgallium-drivers= -Dshader-cache=true -Dplatforms= \
    -Dglx=disabled -Degl=disabled -Dopengl=false -Dgles1=disabled -Dgles2=disabled \
    -Dtools=[] -Dllvm=enabled -Dmesa-clc=enabled -Dprecomp-compiler=enabled \
    -Dinstall-mesa-clc=true
ninja -C builddir-native src/compiler/clc/mesa_clc src/compiler/spirv/vtn_bindgen2

mesa_build_id=$(find src/nouveau src/compiler src/vulkan -type f ! -name '*.orig' -print0 | sort -z | xargs -0 sha256sum | sha256sum | cut -d' ' -f1)
meson setup builddir-switch \
    --cross-file switch_cross_file.txt --native-file switch_native_file.txt \
    --buildtype=release -Doptimization=1 -Dnvk-build-id="$mesa_build_id" \
    -Db_lto=false -Db_ndebug=true -Dvulkan-drivers=nouveau -Dgallium-drivers=nouveau \
    -Dshader-cache=true -Dgallium-rusticl=false -Dplatforms=switch -Dglx=disabled \
    -Degl=disabled -Dopengl=false -Dgles1=disabled -Dgles2=disabled -Dllvm=disabled \
    -Dshared-glapi=disabled -Dshared-llvm=disabled -Dmesa-clc=system \
    -Dprecomp-compiler=system -Dcpp_rtti=false
ninja -C builddir-switch \
    src/nouveau/vulkan/libnvk.a src/nouveau/vulkan/libvulkan.a \
    src/vulkan/util/libvulkan_util.a src/compiler/nir/libnir.a \
    src/compiler/libcompiler.a src/compiler/spirv/libvtn.a src/util/libxmlconfig.a \
    src/nouveau/compiler/libnak.a src/nouveau/compiler/libnak_rs.a \
    src/nouveau/nil/libnil.a src/nouveau/nil/liblibnil_format_table.a \
    src/nouveau/mme/libnouveau_mme.a src/nouveau/winsys/libnouveau_ws.a \
    src/nouveau/headers/libnvidia_headers_c.a src/compiler/rust/libcompiler_c_helpers.a \
    src/util/libmesa_util.a src/util/libmesa_util_simd.a src/util/blake3/libblake3.a \
    src/c11/impl/libmesa_util_c11.a

install -D builddir-switch/src/nouveau/vulkan/libvulkan.a /opt/switch/lib/libvulkan.a
/opt/devkitpro/devkitA64/bin/aarch64-none-elf-objdump -f /opt/switch/lib/libvulkan.a | awk '
    /file format/ {
        count++
        if ($NF != "elf64-littleaarch64") {
            print "Unexpected Mesa Vulkan archive member: " $0 > "/dev/stderr"
            bad = 1
        }
    }
    END {
        if (count == 0 || bad) exit 1
        print "Validated " count " AArch64 objects in libvulkan.a"
    }
'
install -d /opt/switch/include
cp -r include/vulkan include/vk_video /opt/switch/include/
install -D VulkanConfig.cmake /opt/switch/lib/cmake/Vulkan/VulkanConfig.cmake
