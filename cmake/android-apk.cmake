# SPDX-License-Identifier: AGPL-3.0-only
function(calculator_add_android_apk target)
  set(CALCULATOR_ANDROID_FRAMEWORK_RES_APK "${MANDK_ROOT}/tools/framework-res.apk" CACHE FILEPATH "AOSP framework resource APK")
  set(CALCULATOR_ANDROID_APKSIGNER_JAR "${MANDK_ROOT}/tools/apksigner.jar" CACHE FILEPATH "Source-built apksigner JAR")
  set(CALCULATOR_ANDROID_TARGET_API 35 CACHE STRING "APK target API")
  set(CALCULATOR_ANDROID_MIN_API 21 CACHE STRING "APK minimum API")
  set(CALCULATOR_ANDROID_VERSION_CODE 1 CACHE STRING "Monotonic APK version code")
  option(CALCULATOR_ANDROID_TEST_KEY "Generate a test signing key and test-signed APK" ON)
  set(CALCULATOR_ANDROID_LIBRARY_DIRS "" CACHE STRING "Additional Android runtime library search directories")
  if(CALCULATOR_ANDROID_MIN_API LESS 21 OR CALCULATOR_ANDROID_TARGET_API LESS CALCULATOR_ANDROID_MIN_API)
    message(FATAL_ERROR "Android packaging requires 21 <= CALCULATOR_ANDROID_MIN_API <= CALCULATOR_ANDROID_TARGET_API")
  endif()
  # Android reads APK signature scheme v2 from 7.0 (API 24): below it, only
  # the v1 (JAR) signature is checked, and an APK without one is refused.
  if(CALCULATOR_ANDROID_MIN_API LESS 24)
    set(v1_signing true)
  else()
    set(v1_signing false)
  endif()
  foreach(input CALCULATOR_ANDROID_FRAMEWORK_RES_APK CALCULATOR_ANDROID_APKSIGNER_JAR)
    if(NOT EXISTS "${${input}}")
      message(FATAL_ERROR "Set ${input} to an existing file (currently '${${input}}')")
    endif()
  endforeach()
  find_program(CALCULATOR_AAPT2 aapt2 REQUIRED NO_CMAKE_FIND_ROOT_PATH)
  find_program(CALCULATOR_ZIPALIGN zipalign REQUIRED NO_CMAKE_FIND_ROOT_PATH)
  find_program(CALCULATOR_JAVA java REQUIRED NO_CMAKE_FIND_ROOT_PATH)
  find_program(CALCULATOR_READELF NAMES llvm-readelf readelf REQUIRED NO_CMAKE_FIND_ROOT_PATH)
  find_package(Python3 COMPONENTS Interpreter REQUIRED)

  get_target_property(sdl_source SDL3::SDL3 SOURCE_DIR)
  if(NOT EXISTS "${sdl_source}/build-scripts/native-activity/dex/CMakeLists.txt")
    message(FATAL_ERROR "Android packaging requires the native SDL fork built from source")
  endif()
  cme_declare_port(NAME sdl-native-dex PROVIDES sdl-native-dex VERSION 1.0
    SOURCE_DIR "${sdl_source}/build-scripts/native-activity/dex" MACHINE build
    PROGRAMS "sdl-native-dex=sdl::native-dex" TARGETS sdl::native-dex)
  find_package(sdl-native-dex REQUIRED)

  set(out "${CMAKE_BINARY_DIR}/apk")
  file(MAKE_DIRECTORY "${out}")
  set(CALCULATOR_ANDROID_MANIFEST "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../android/AndroidManifest.xml"
      CACHE FILEPATH "Application manifest")
  set(manifest "${CALCULATOR_ANDROID_MANIFEST}")
  set(packager "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../tools/android_apk.py")
  # Source-built shared dependencies, if any, can live anywhere below the
  # build tree. SDL is not one: it is linked into the application statically.
  set(search --library-dir "${CMAKE_BINARY_DIR}")
  foreach(dir IN LISTS CMAKE_PREFIX_PATH CALCULATOR_ANDROID_LIBRARY_DIRS)
    list(APPEND search --library-dir "${dir}")
  endforeach()
  add_custom_command(OUTPUT "${out}/classes.dex"
    COMMAND "${sdl-native-dex_PROGRAM}" --out "${out}/classes.dex" --library "$<TARGET_FILE_BASE_NAME:${target}>"
    DEPENDS "${sdl-native-dex_PROGRAM}" VERBATIM)
  # The resources and manifest of each variant: the debug one linked with
  # --debug-mode, which puts android:debuggable="true" in its manifest --
  # what Android's own debugging goes by: run-as into the app's data,
  # lldb-server or a JDWP debugger attached to its process, its heap and
  # native allocations profiled. The stripped one is not debuggable.
  foreach(variant stripped debug)
    set(debug_mode)
    if(variant STREQUAL "debug")
      set(debug_mode --debug-mode)
    endif()
    file(MAKE_DIRECTORY "${out}/${variant}")
    add_custom_command(OUTPUT "${out}/${variant}/calculator-resources.apk"
      COMMAND "${CALCULATOR_AAPT2}" link -o "${out}/${variant}/calculator-resources.apk"
        --manifest "${manifest}" -I "${CALCULATOR_ANDROID_FRAMEWORK_RES_APK}"
        --min-sdk-version "${CALCULATOR_ANDROID_MIN_API}" --target-sdk-version "${CALCULATOR_ANDROID_TARGET_API}"
        --version-code "${CALCULATOR_ANDROID_VERSION_CODE}" --version-name "${PROJECT_VERSION}"
        ${debug_mode}
      DEPENDS "${manifest}" "${CALCULATOR_ANDROID_FRAMEWORK_RES_APK}" VERBATIM)
  endforeach()
  if(CMAKE_STRIP)
    set(calculator_strip "${CMAKE_STRIP}")
  else()
    find_program(calculator_strip NAMES llvm-strip llvm-strip-23 REQUIRED NO_CMAKE_FIND_ROOT_PATH)
  endif()
  set(test_keystore "${PROJECT_SOURCE_DIR}/android/test-signing.p12")
  foreach(variant stripped debug)
    set(variant_out "${out}/${variant}")
    file(MAKE_DIRECTORY "${variant_out}")
    set(strip_args)
    if(variant STREQUAL "stripped")
      set(strip_args --strip-tool "${calculator_strip}")
    endif()
    add_custom_command(OUTPUT "${variant_out}/calculator-unsigned.apk"
      COMMAND "${Python3_EXECUTABLE}" "${packager}"
        --resources "${variant_out}/calculator-resources.apk" --dex "${out}/classes.dex"
        --library "$<TARGET_FILE:${target}>" ${strip_args}
        ${search} --readelf "${CALCULATOR_READELF}" --abi "${CALCULATOR_ANDROID_ABI}" --out "${variant_out}/calculator-unaligned.apk"
      COMMAND "${CALCULATOR_ZIPALIGN}" -f 4 "${variant_out}/calculator-unaligned.apk" "${variant_out}/calculator-unsigned.apk"
      COMMAND "${CALCULATOR_ZIPALIGN}" -c 4 "${variant_out}/calculator-unsigned.apk"
      DEPENDS ${target} "${out}/classes.dex" "${variant_out}/calculator-resources.apk" "${packager}"
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../android/abis.json"
      VERBATIM COMMAND_EXPAND_LISTS)
    add_custom_target(calculator-apk-${variant}-unsigned DEPENDS "${variant_out}/calculator-unsigned.apk")
    if(CALCULATOR_ANDROID_TEST_KEY)
      # One public test key for both variants and every revision, so either
      # variant can update an existing installation without deleting its data.
      add_custom_command(OUTPUT "${variant_out}/calculator-test-signed.apk"
        COMMAND "${CALCULATOR_JAVA}" -jar "${CALCULATOR_ANDROID_APKSIGNER_JAR}" sign
          --ks "${test_keystore}" --ks-type PKCS12 --ks-key-alias androiddebugkey --ks-pass pass:android
          --key-pass pass:android --min-sdk-version "${CALCULATOR_ANDROID_MIN_API}"
          --v1-signing-enabled ${v1_signing}
          --v2-signing-enabled true --v3-signing-enabled true
          --out "${variant_out}/calculator-test-signed.apk" "${variant_out}/calculator-unsigned.apk"
        COMMAND "${CALCULATOR_JAVA}" -jar "${CALCULATOR_ANDROID_APKSIGNER_JAR}" verify
          --min-sdk-version "${CALCULATOR_ANDROID_MIN_API}" "${variant_out}/calculator-test-signed.apk"
        DEPENDS "${variant_out}/calculator-unsigned.apk" "${test_keystore}" "${CALCULATOR_ANDROID_APKSIGNER_JAR}" VERBATIM)
      add_custom_target(calculator-apk-${variant} DEPENDS "${variant_out}/calculator-test-signed.apk")
    else()
      add_custom_target(calculator-apk-${variant} DEPENDS "${variant_out}/calculator-unsigned.apk")
    endif()
  endforeach()
  # Preserve the existing paths for callers expecting the compact APK.
  add_custom_command(OUTPUT "${out}/calculator-unsigned.apk"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${out}/stripped/calculator-unsigned.apk" "${out}/calculator-unsigned.apk"
    DEPENDS "${out}/stripped/calculator-unsigned.apk" VERBATIM)
  add_custom_target(calculator-apk-unsigned DEPENDS "${out}/calculator-unsigned.apk")
  if(CALCULATOR_ANDROID_TEST_KEY)
    add_custom_command(OUTPUT "${out}/calculator-test-signed.apk"
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${out}/stripped/calculator-test-signed.apk" "${out}/calculator-test-signed.apk"
      DEPENDS "${out}/stripped/calculator-test-signed.apk" VERBATIM)
    add_custom_target(calculator-apk DEPENDS "${out}/calculator-test-signed.apk" calculator-apk-debug)
  else()
    add_custom_target(calculator-apk DEPENDS calculator-apk-unsigned calculator-apk-debug)
  endif()
endfunction()
