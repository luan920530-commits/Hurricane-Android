# ---------------------------------------------------------------------------------------
# Copia los datos del juego a los assets del APK.
#
# Se ejecuta como "cmake -P" desde el target hurrican_assets (ver CMakeLists.txt) para que
# el APK se pueda construir directamente con Android Studio o ./gradlew sin pasos manuales.
#
# Copia incremental: solo se recopian los ficheros que cambiaron, asi que en las siguientes
# compilaciones es practicamente instantaneo.
# ---------------------------------------------------------------------------------------

if(NOT DEFINED HURRICAN_SOURCE OR NOT DEFINED ASSETS_DIR)
    message(FATAL_ERROR "Uso: cmake -DHURRICAN_SOURCE=<dir> -DASSETS_DIR=<dir> -P copy_assets.cmake")
endif()

foreach(subdir data lang)
    set(src "${HURRICAN_SOURCE}/${subdir}")
    if(NOT IS_DIRECTORY "${src}")
        message(FATAL_ERROR "No se encuentra '${src}'")
    endif()
    file(COPY "${src}" DESTINATION "${ASSETS_DIR}")
endforeach()

message(STATUS "Assets del juego listos en ${ASSETS_DIR}")
