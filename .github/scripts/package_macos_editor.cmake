cmake_minimum_required(VERSION 3.24)

if(NOT CMAKE_HOST_APPLE)
    message(FATAL_ERROR "The macOS app bundle must be packaged on macOS.")
endif()
if(NOT DEFINED APP_BUNDLE OR NOT IS_DIRECTORY "${APP_BUNDLE}")
    message(FATAL_ERROR "APP_BUNDLE must point to the built HurricanMapEditor.app bundle.")
endif()

include(BundleUtilities)
set(BU_CHMOD_BUNDLE_ITEMS ON)
set(BU_COPY_FULL_FRAMEWORK_CONTENTS ON)

message(STATUS "Bundling non-system runtime libraries into ${APP_BUNDLE}")
fixup_bundle("${APP_BUNDLE}" "" "${SEARCH_DIRS}")
verify_app("${APP_BUNDLE}")
