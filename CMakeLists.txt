# This file is part of qawno.
#
# qawno is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# qawno is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with qawno. If not, see <http://www.gnu.org/licenses/>.

cmake_minimum_required(VERSION 3.16)

project(Qawno VERSION 2.1.0 LANGUAGES CXX)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

list(APPEND
     CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Build against whichever Qt is installed: Qt6 is preferred, Qt5 still works.
# Svg lets QIcon render the toolbar's .svg icons (pulls in the qsvg image
# plugin and lets macdeployqt bundle it).
find_package(QT NAMES Qt6 Qt5 COMPONENTS Core Widgets Svg Network REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core Widgets Svg Network REQUIRED)

# Qt6 moved QTextCodec (needed for Windows-1251 scripts) into Core5Compat.
if(QT_VERSION_MAJOR EQUAL 6)
  find_package(Qt6 COMPONENTS Core5Compat REQUIRED)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# Qt6 requires C++17; Qt5 is happy with it too.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# The .ui files wire actions to slots resolved at runtime. Qt6's uic defaults
# to compile-time pointer-to-member connections, which fail to build. Emit
# string-based connections (Qt5 behaviour) instead.
set(CMAKE_AUTOUIC_OPTIONS --connections string)

configure_file(qawno.h.in qawno.h @ONLY)

# Telemetry: events POST to a Supabase Edge Function that holds the DB key
# server-side, so no key is baked into the binary. A default function URL is
# compiled into TelemetryClient.cpp; override it with QAWNO_TELEMETRY_URL in
# cmake/Telemetry.env (gitignored) for staging/dev endpoints.
set(QAWNO_TELEMETRY_ENV "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Telemetry.env")
if(EXISTS "${QAWNO_TELEMETRY_ENV}")
  file(STRINGS "${QAWNO_TELEMETRY_ENV}" _telemetry_lines)
  foreach(_line IN LISTS _telemetry_lines)
    if(_line MATCHES "^QAWNO_TELEMETRY_URL=(.+)$")
      add_compile_definitions(QAWNO_TELEMETRY_URL="${CMAKE_MATCH_1}")
    endif()
  endforeach()
endif()

set(HEADERS
  src/AboutDialog.h
  src/Compiler.h
  src/CompilerSettingsDialog.h
  src/ServerSettingsDialog.h
  src/SettingsDialog.h
  src/IosToggle.h
  src/EditorWidget.h
  src/FindDialog.h
  src/GoToDialog.h
  src/MainWindow.h
  src/OutputWidget.h
  src/Server.h
  src/StatusBar.h
  src/SyntaxHighlighter.h
  src/UpdateChecker.h
  src/UpdatePromptDialog.h
  src/Installer.h
  src/Spinner.h
  src/IconLoader.h
  src/LocaleManager.h
  src/TelemetryClient.h
  src/ConsentDialog.h
)

set(SOURCES
  src/AboutDialog.cpp
  src/Compiler.cpp
  src/CompilerSettingsDialog.cpp
  src/ServerSettingsDialog.cpp
  src/SettingsDialog.cpp
  src/IosToggle.cpp
  src/EditorWidget.cpp
  src/FindDialog.cpp
  src/GoToDialog.cpp
  src/main.cpp
  src/MainWindow.cpp
  src/OutputWidget.cpp
  src/Server.cpp
  src/StatusBar.cpp
  src/SyntaxHighlighter.cpp
  src/UpdateChecker.cpp
  src/UpdatePromptDialog.cpp
  src/Installer.cpp
  src/Spinner.cpp
  src/IconLoader.cpp
  src/LocaleManager.cpp
  src/TelemetryClient.cpp
  src/ConsentDialog.cpp
)

# qawno.rc is a Windows resource script (the .exe icon); it must not be fed to
# the compiler on other platforms.
if(WIN32)
  list(APPEND SOURCES qawno.rc)
endif()

set(UI_SOURCES
  src/CompilerSettingsDialog.ui
  src/ServerSettingsDialog.ui
  src/GoToDialog.ui
  src/MainWindow.ui
)

qt_wrap_ui(UI_HEADERS ${UI_SOURCES} OPTIONS --connections string)
qt_add_resources(EMBEDDED_RESOURCES qawno.qrc)

# Translations: compile each .ts → .qm into the build dir, then bundle them
# via a generated qrc so LocaleManager can load ":/i18n/qawno_<code>.qm".
file(GLOB TS_FILES "${CMAKE_CURRENT_SOURCE_DIR}/i18n/qawno_*.ts")
if(TS_FILES)
  # lrelease ships in the same bin dir as qmake — derive from Qt6::qmake's
  # imported location to avoid relying on PATH.
  get_target_property(_qmake_loc Qt${QT_VERSION_MAJOR}::qmake IMPORTED_LOCATION)
  get_filename_component(_qt_bin "${_qmake_loc}" DIRECTORY)
  find_program(LRELEASE_EXECUTABLE lrelease HINTS "${_qt_bin}" REQUIRED)
  set(QM_FILES "")
  set(QM_RESOURCE_LIST "")
  foreach(_ts ${TS_FILES})
    get_filename_component(_base "${_ts}" NAME_WE)
    set(_qm "${CMAKE_CURRENT_BINARY_DIR}/${_base}.qm")
    add_custom_command(
      OUTPUT "${_qm}"
      COMMAND "${LRELEASE_EXECUTABLE}" -silent "${_ts}" -qm "${_qm}"
      DEPENDS "${_ts}"
      COMMENT "Compiling translation ${_base}.qm"
      VERBATIM
    )
    list(APPEND QM_FILES "${_qm}")
    list(APPEND QM_RESOURCE_LIST "    <file alias=\"i18n/${_base}.qm\">${_qm}</file>")
  endforeach()
  string(REPLACE ";" "\n" QM_RESOURCE_BODY "${QM_RESOURCE_LIST}")
  file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/qawno_i18n.qrc"
       "<RCC>\n  <qresource>\n${QM_RESOURCE_BODY}\n  </qresource>\n</RCC>\n")
  qt_add_resources(I18N_RESOURCES "${CMAKE_CURRENT_BINARY_DIR}/qawno_i18n.qrc")
  list(APPEND EMBEDDED_RESOURCES ${I18N_RESOURCES})
  add_custom_target(qawno_translations ALL DEPENDS ${QM_FILES})
endif()

if(APPLE)
  # Assemble a .iconset with the names iconutil expects, then convert to .icns.
  # iconutil validates each file's pixel size, so map our PNGs to the matching
  # slots (the 1024px image becomes the 512x512@2x retina variant).
  set(ICON_SRC "${CMAKE_CURRENT_SOURCE_DIR}/assets/images")
  set(ICONSET_DIR "${CMAKE_CURRENT_BINARY_DIR}/qawno.iconset")
  set(ICNS_FILE "${CMAKE_CURRENT_BINARY_DIR}/qawno.icns")
  set(RESORUCES "${ICNS_FILE}")
  add_custom_command(
    OUTPUT ${ICNS_FILE}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${ICONSET_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy "${ICON_SRC}/icon_16x16.png"     "${ICONSET_DIR}/icon_16x16.png"
    COMMAND ${CMAKE_COMMAND} -E copy "${ICON_SRC}/icon_32x32.png"     "${ICONSET_DIR}/icon_16x16@2x.png"
    COMMAND ${CMAKE_COMMAND} -E copy "${ICON_SRC}/icon_32x32.png"     "${ICONSET_DIR}/icon_32x32.png"
    COMMAND ${CMAKE_COMMAND} -E copy "${ICON_SRC}/icon_128x128.png"   "${ICONSET_DIR}/icon_128x128.png"
    COMMAND ${CMAKE_COMMAND} -E copy "${ICON_SRC}/icon_256x256.png"   "${ICONSET_DIR}/icon_128x128@2x.png"
    COMMAND ${CMAKE_COMMAND} -E copy "${ICON_SRC}/icon_256x256.png"   "${ICONSET_DIR}/icon_256x256.png"
    COMMAND ${CMAKE_COMMAND} -E copy "${ICON_SRC}/icon_512x512.png"   "${ICONSET_DIR}/icon_256x256@2x.png"
    COMMAND ${CMAKE_COMMAND} -E copy "${ICON_SRC}/icon_512x512.png"   "${ICONSET_DIR}/icon_512x512.png"
    COMMAND ${CMAKE_COMMAND} -E copy "${ICON_SRC}/icon_1024x1024.png" "${ICONSET_DIR}/icon_512x512@2x.png"
    COMMAND iconutil --convert icns --output "${ICNS_FILE}" "${ICONSET_DIR}"
    DEPENDS
      "${ICON_SRC}/icon_16x16.png"
      "${ICON_SRC}/icon_32x32.png"
      "${ICON_SRC}/icon_128x128.png"
      "${ICON_SRC}/icon_256x256.png"
      "${ICON_SRC}/icon_512x512.png"
      "${ICON_SRC}/icon_1024x1024.png"
    COMMENT "Generating qawno.icns"
    VERBATIM
  )
else()
  set(RESOURCES assets/images/qawno.ico)
endif()

add_executable(qawno
  ${SOURCES}
  ${HEADERS}
  ${UI_HEADERS}
  ${EMBEDDED_RESOURCES}
  ${RESORUCES}
)

if(APPLE)
  set_target_properties(qawno PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "Qawno"
    MACOSX_BUNDLE_GUI_IDENTIFIER "io.openmultiplayer.qawno"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_ICON_FILE "qawno.icns"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/cmake/QawnoInfo.plist.in"
    RESOURCE "${ICNS_FILE}"
  )
endif()

target_link_libraries(qawno
  Qt${QT_VERSION_MAJOR}::Core
  Qt${QT_VERSION_MAJOR}::Widgets
  Qt${QT_VERSION_MAJOR}::Svg
  Qt${QT_VERSION_MAJOR}::Network
)

if(QT_VERSION_MAJOR EQUAL 6)
  target_link_libraries(qawno Qt6::Core5Compat)
endif()

if(WIN32)
  if(MSVC)
    set_property(TARGET qawno APPEND_STRING PROPERTY
                 LINK_FLAGS " /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
  elseif(MINGW)
    set_property(TARGET qawno APPEND_STRING PROPERTY
                 LINK_FLAGS " -Wl,-subsystem,windows")
  endif()
endif()

if(UNIX AND NOT APPLE)
  set(INSTALL_BINARY_DIR bin)
  set(INSTALL_LIBRARY_DIR lib)
else()
  set(INSTALL_BINARY_DIR .)
  set(INSTALL_LIBRARY_DIR .)
endif()

install(
  TARGETS qawno
  RUNTIME DESTINATION ${INSTALL_BINARY_DIR}
  BUNDLE DESTINATION ${INSTALL_BINARY_DIR}
  LIBRARY DESTINATION ${INSTALL_LIBRARY_DIR}
)

if(UNIX AND NOT APPLE)
  install(FILES ${RESOURCES} DESTINATION share)
endif()

if(WIN32 OR APPLE)
  get_target_property(QMAKE_EXECUTABLE Qt${QT_VERSION_MAJOR}::qmake IMPORTED_LOCATION)
  get_filename_component(QT5_BIN_DIR "${QMAKE_EXECUTABLE}" DIRECTORY)
endif()

if(WIN32)
  set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
  include(InstallRequiredSystemLibraries)
  install(FILES ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS} DESTINATION .)

  find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT5_BIN_DIR}")
  if(NOT WINDEPLOYQT_EXECUTABLE)
    message(FATAL_ERROR "Could not determine location of windeployqt")
  endif()

  set(INSTALL_APP_DIR "${CMAKE_CURRENT_BINARY_DIR}/app")
  install(CODE "
      file(REMOVE_RECURSE \"${INSTALL_APP_DIR}\")
      file(MAKE_DIRECTORY \"${INSTALL_APP_DIR}\")
      file(COPY \"${CMAKE_CURRENT_BINARY_DIR}/qawno.exe\"
           DESTINATION \"${INSTALL_APP_DIR}\")
  ")
  install(CODE "
    execute_process(
      COMMAND ${WINDEPLOYQT_EXECUTABLE}
              --no-opengl-sw
              --no-quick-import
              --no-svg
              --no-translations
              --no-webkit2
              \"${INSTALL_APP_DIR}\")
  ")
  install(DIRECTORY "${INSTALL_APP_DIR}/" DESTINATION .)
endif()

if(APPLE)
  set_target_properties(qawno PROPERTIES OUTPUT_NAME Qawno)

  # Bundle the native pawncc (+ libpawnc.dylib) into
  # Qawno.app/Contents/Resources/native/ at build time so Qawno can deploy a
  # copy into the user's project qawno/native/ folder on compile. No Wine.
  add_custom_command(TARGET qawno POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
      "$<TARGET_FILE_DIR:qawno>/../Resources/native"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/bin/darwin/pawncc"
      "$<TARGET_FILE_DIR:qawno>/../Resources/native/pawncc"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/bin/darwin/libpawnc.dylib"
      "$<TARGET_FILE_DIR:qawno>/../Resources/native/libpawnc.dylib"
    COMMENT "Bundling native pawncc compiler"
  )

  find_program(MACDEPLOYQT_EXECUTABLE macdeployqt HINTS "${QT5_BIN_DIR}")
  if(NOT MACDEPLOYQT_EXECUTABLE)
    message(FATAL_ERROR "Could not determine location of macdeployqt")
  endif()

  set(INSTALL_APP_DIR "${CMAKE_CURRENT_BINARY_DIR}/qawno.app")
  install(CODE "
    execute_process(
      COMMAND ${MACDEPLOYQT_EXECUTABLE} \"${INSTALL_APP_DIR}\")
  ")
  install(DIRECTORY "${INSTALL_APP_DIR}"
          DESTINATION .
          USE_SOURCE_PERMISSIONS)
endif()

set(CPACK_PACKAGE_NAME Qawno)
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_VENDOR Zeex)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A simple Pawn editor")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt")
set(CPACK_PACKAGE_INSTALL_DIRECTORY Qawno)

if(WIN32)
  if(NOT CPACK_GENERATOR)
    set(CPACK_GENERATOR WIX)
  endif()
  set(CPACK_WIX_PRODUCT_ICON
      "${CMAKE_CURRENT_SOURCE_DIR}/assets/images/qawno.ico")
  set(CPACK_WIX_PRODUCT_GUID "E5FCD5B0-D4F3-4F4C-B7E1-89BB99706556")
  set(CPACK_WIX_UPGRADE_GUID "1CB3E5B2-C719-4A65-8AF1-B5C19783C779")
  set(CPACK_WIX_UI_DIALOG
      "${CMAKE_CURRENT_SOURCE_DIR}/assets/images/msi_dialog.bmp")
  set(CPACK_WIX_UI_BANNER
      "${CMAKE_CURRENT_SOURCE_DIR}/assets/images/msi_banner.bmp")
elseif(CMAKE_SYSTEM_NAME EQUAL Linux)
  if(NOT CPACK_GENERATOR)
    set(CPACK_GENERATOR TGZ)
  endif()
elseif(APPLE)
  if(NOT CPACK_GENERATOR)
    set(CPACK_GENERATOR DragNDrop)
  endif()
  set(CPACK_BUNDLE_ICON qawno.icns)
endif()

include(CPack)
