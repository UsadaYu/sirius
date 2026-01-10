# sirius-0.1.0

---

# 1 Build

## 1.1 CMake

```shell
# --- Configure ---
cmake \
-S . \
-B build \
-DCMAKE_BUILD_TYPE=Release \
-DBUILD_SHARED_LIBS=ON \
-DSIRIUS_TEST_ENABLE=ON \
-DCMAKE_INSTALL_PREFIX="/usr/local"

# --- Build ---
cmake --build build --target install

# --- Run Tests ---
cd build ; ctest ; cd -
```



## 1.2 Meson

```shell
# Not yet
```



