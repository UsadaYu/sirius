# sirius-0.1.1

---



# 1 Build and Compile

## 1.1 CMake

```shell
# --- Configure ---
cmake \
-S . \
-B build \
-DCMAKE_BUILD_TYPE=Release \
-DBUILD_SHARED_LIBS=ON \
-DCMAKE_INSTALL_PREFIX="/usr/local" \
\
-DSIRIUS_TEST_ENABLE=ON

# --- Compile and Install ---
cmake --build build --target install

# --- Run Tests ---
ctest --test-dir build --verbose
```



## 1.2 Meson

```shell
# --- Configure ---
meson \
setup builddir \
-Dbuildtype=release \
-Ddefault_library=shared \
-Dprefix="/usr/local" \
\
-Dtest-enable=true

# --- Compile and Install ---
meson compile -C builddir install

# --- Run Tests ---
meson test -C builddir --verbose
```



