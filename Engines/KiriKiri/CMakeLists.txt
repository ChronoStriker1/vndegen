# Builds the KiriKiri engine (krkrsdl3) for VNDegen. scripts/kirikiri-engine.sh drives it.
#   macOS: KiriKiriHelper, an executable VNDegen runs a game in.
#   iOS:   KiriKiri.framework, which exports only the vnkk_* functions of vndegen/vnkk.h.
# Rendering is software compositing presented through SDL3's renderer (Metal), with no OpenGL and
# no JIT. Adapted from krkrsdl3_build's CMakeLists.txt and cmake/*.txt; see UPSTREAM.md.
cmake_minimum_required(VERSION 3.28)

set(VCPKG_OVERLAY_TRIPLETS ${CMAKE_CURRENT_LIST_DIR}/triplets)
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(CMAKE_OSX_DEPLOYMENT_TARGET 18.0)
else()
    set(CMAKE_OSX_DEPLOYMENT_TARGET 15.0)
endif()
set(CMAKE_OSX_ARCHITECTURES arm64)

project(KiriKiri LANGUAGES C CXX OBJC OBJCXX)

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_OBJC_VISIBILITY_PRESET hidden)
set(CMAKE_OBJCXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

# The engine's fallback font, used when a game ships none. krkrsdl3_build keeps it in Res/.
set(KIRIKIRI_DOWNLOADS ${CMAKE_BINARY_DIR} CACHE PATH "Folder downloaded build inputs are cached in")
set(KIRIKIRI_FONT ${KIRIKIRI_DOWNLOADS}/DroidSansFallback.ttf)
file(DOWNLOAD
    https://raw.githubusercontent.com/krkrsdl3/krkrsdl3_build/6059f7857bf6c56bad33a781ec0fc2cc5151b86a/Res/DroidSansFallback.ttf
    ${KIRIKIRI_FONT}
    EXPECTED_HASH SHA256=76aafe35ec7b42188704ac45c8e9cdcc023296c9fa1e786c8f14e0fc1b7c048f)

set(KIRIKIRI_CORE_SOURCES
    krkrsdl3/core/archive/TextStream.cpp
    krkrsdl3/core/archive/TVPArchive.cpp
    krkrsdl3/core/archive/TVPStorage.cpp
    krkrsdl3/core/archive/UtilStreams.cpp
    krkrsdl3/core/archive/XP3Archive.cpp
    krkrsdl3/core/archive/encoding/gbk2unicode.c
    krkrsdl3/core/archive/encoding/jis2unicode.c
    krkrsdl3/core/main/NativeEventQueue.cpp
    krkrsdl3/core/main/SystemControl.cpp
    krkrsdl3/core/main/TVPApplication.cpp
    krkrsdl3/core/main/TVPEvent.cpp
    krkrsdl3/core/main/TVPSystem.cpp
    krkrsdl3/core/main/TVPWindow.cpp
    krkrsdl3/core/main/WindowManager.cpp
    krkrsdl3/core/media/font/CharacterData.cpp
    krkrsdl3/core/media/font/FreeTypeFontRasterizer.cpp
    krkrsdl3/core/media/font/FreeTypeImp.cpp
    krkrsdl3/core/media/font/PrerenderedFont.cpp
    krkrsdl3/core/media/font/TVPFont.cpp
    krkrsdl3/core/media/image/BitmapInfomation.cpp
    krkrsdl3/core/media/image/GraphicsLoadThread.cpp
    krkrsdl3/core/media/image/LoadBMP.cpp
    krkrsdl3/core/media/image/LoadTLG.cpp
    krkrsdl3/core/media/image/SaveTLG5.cpp
    krkrsdl3/core/media/image/SaveTLG6.cpp
    krkrsdl3/core/media/image/TVPGraphicsLoader.cpp
    krkrsdl3/core/media/movie/krmovie.cpp
    krkrsdl3/core/media/sound/RIFFWaveDecoder.cpp
    krkrsdl3/core/media/sound/TVPWaveManager.cpp
    krkrsdl3/core/media/sound/WaveDecodeThread.cpp
    krkrsdl3/core/media/sound/WaveLoopManager.cpp
    krkrsdl3/core/media/sound/WaveSegmentQueue.cpp
    krkrsdl3/core/msg/TVPDebug.cpp
    krkrsdl3/core/msg/TVPMsg.cpp
    krkrsdl3/core/render/gl/blend_function.cpp
    krkrsdl3/core/render/gl/tvpgl.cpp
    krkrsdl3/core/render/ComplexRect.cpp
    krkrsdl3/core/render/DrawDevice.cpp
    krkrsdl3/core/render/LayerBitmap.cpp
    krkrsdl3/core/render/LayerManager.cpp
    krkrsdl3/core/render/LayerTreeOwner.cpp
    krkrsdl3/core/render/RenderManager.cpp
    krkrsdl3/core/render/TVPTrans.cpp
    krkrsdl3/core/render/TVPColor.cpp
    krkrsdl3/core/render/TVPCompositor.cpp
    krkrsdl3/core/render/backend/SWRenderBackend.cpp
    krkrsdl3/core/script/tjsNativeAsyncTrigger.cpp
    krkrsdl3/core/script/tjsNativeBasicDrawDevice.cpp
    krkrsdl3/core/script/tjsNativeBitmap.cpp
    krkrsdl3/core/script/tjsNativeBitmapLayerTreeOwner.cpp
    krkrsdl3/core/script/tjsNativeCDDASoundBuffer.cpp
    krkrsdl3/core/script/tjsNativeClipboard.cpp
    krkrsdl3/core/script/tjsNativeDebug.cpp
    krkrsdl3/core/script/tjsNativeFont.cpp
    krkrsdl3/core/script/tjsNativeImageFunction.cpp
    krkrsdl3/core/script/tjsNativeKAGParser.cpp
    krkrsdl3/core/script/tjsNativeLayer.cpp
    krkrsdl3/core/script/tjsNativeMenuItem.cpp
    krkrsdl3/core/script/tjsNativeMIDISoundBuffer.cpp
    krkrsdl3/core/script/tjsNativePad.cpp
    krkrsdl3/core/script/tjsNativePhaseVocoder.cpp
    krkrsdl3/core/script/tjsNativePlugins.cpp
    krkrsdl3/core/script/tjsNativeRect.cpp
    krkrsdl3/core/script/tjsNativeScript.cpp
    krkrsdl3/core/script/tjsNativeStorages.cpp
    krkrsdl3/core/script/tjsNativeSystem.cpp
    krkrsdl3/core/script/tjsNativeTimer.cpp
    krkrsdl3/core/script/tjsNativeVideoOverlay.cpp
    krkrsdl3/core/script/tjsNativeWaveSoundBuffer.cpp
    krkrsdl3/core/script/tjsNativeWindow.cpp
    krkrsdl3/core/script/TVPScript.cpp
    krkrsdl3/core/utils/PhaseVocoderDSP.cpp
    krkrsdl3/core/utils/Random.cpp
    krkrsdl3/core/utils/RealFFT.cpp
    krkrsdl3/core/utils/ResampleImage.cpp
    krkrsdl3/core/utils/TVPSettings.cpp
    krkrsdl3/core/utils/TVPTimer.cpp
    krkrsdl3/core/utils/VelocityTracker.cpp
    krkrsdl3/core/utils/WaveFormatConverter.cpp
    krkrsdl3/core/utils/WeightFunctor.cpp
    krkrsdl3/core/utils/math/md5.c
)

set(KIRIKIRI_PLUGINS_SOURCES
    krkrsdl3/plugins/ncbind/ncbind.cpp
    krkrsdl3/plugins/addFont.cpp
    krkrsdl3/plugins/AlphaMovie.cpp
    krkrsdl3/plugins/csvParser.cpp
    krkrsdl3/plugins/dirlist.cpp
    krkrsdl3/plugins/expat.cpp
    krkrsdl3/plugins/fftgraph.cpp
    krkrsdl3/plugins/fstat.cpp
    krkrsdl3/plugins/FuncStubs.cpp
    krkrsdl3/plugins/getabout.cpp
    krkrsdl3/plugins/getSample.cpp
    krkrsdl3/plugins/gfxEffect.cpp
    krkrsdl3/plugins/gfxFire.cpp
    krkrsdl3/plugins/json.cpp
    krkrsdl3/plugins/LayerExAreaAverage.cpp
    krkrsdl3/plugins/LayerExBase.cpp
    krkrsdl3/plugins/LayerExBTOA.cpp
    krkrsdl3/plugins/LayerExImage.cpp
    krkrsdl3/plugins/LayerExMovie.cpp
    krkrsdl3/plugins/LayerExPerspective.cpp
    krkrsdl3/plugins/LayerExRaster.cpp
    krkrsdl3/plugins/LayerExSave.cpp
    krkrsdl3/plugins/lzfs.cpp
    krkrsdl3/plugins/motionPlayer.cpp
    krkrsdl3/plugins/PackinOne.cpp
    krkrsdl3/plugins/saveStruct.cpp
    krkrsdl3/plugins/scriptsEx.cpp
    krkrsdl3/plugins/shrinkCopy.cpp
    krkrsdl3/plugins/textrender.cpp
    krkrsdl3/plugins/TVPPlugin.cpp
    krkrsdl3/plugins/varfile.cpp
    krkrsdl3/plugins/win32dialog.cpp
    krkrsdl3/plugins/windowEx.cpp
    krkrsdl3/plugins/wuopus.cpp
    krkrsdl3/plugins/wutcwf.cpp
    krkrsdl3/plugins/wuvorbis.cpp
    krkrsdl3/plugins/xp3filter.cpp
    krkrsdl3/plugins/DrawDeviceD3D/DrawDeviceD3D.cpp
    krkrsdl3/plugins/DrawDeviceD3D/DrawDeviceD3D_reg.cpp
    krkrsdl3/plugins/DrawDeviceD3D/D3DEmotePlayer.cpp
    krkrsdl3/plugins/emoteplayer/emotefile.cpp
    krkrsdl3/plugins/emoteplayer/emoteplayer.cpp
    krkrsdl3/plugins/emoteplayer/emoteplayerclass.cpp
    krkrsdl3/plugins/emoteplayer/emoterunner.cpp
    krkrsdl3/plugins/extrans/extrans.cpp
    krkrsdl3/plugins/extrans/mosaic.cpp
    krkrsdl3/plugins/extrans/ripple.cpp
    krkrsdl3/plugins/extrans/rotatebase.cpp
    krkrsdl3/plugins/extrans/rotatetrans.cpp
    krkrsdl3/plugins/extrans/turn.cpp
    krkrsdl3/plugins/extrans/turntrans_table.cpp
    krkrsdl3/plugins/extrans/wave.cpp
    krkrsdl3/plugins/KAGParserEx/KAG.cpp
    krkrsdl3/plugins/KAGParserEx/KAGParserEx.cpp
    krkrsdl3/plugins/ExtKAGParser/ExtKAG.cpp
    krkrsdl3/plugins/ExtKAGParser/ExtKAGParser.cpp
    krkrsdl3/plugins/ExtKAGParser/TJSAry.cpp
    krkrsdl3/plugins/ExtKAGParser/TJSDic.cpp
    krkrsdl3/plugins/LayerExDraw/LayerExDraw.cpp
    krkrsdl3/plugins/psbfile/PSB.cpp
    krkrsdl3/plugins/psbfile/PSBData.cpp
    krkrsdl3/plugins/psbfile/PSBFile.cpp
    krkrsdl3/plugins/psbfile/PSBMedia.cpp
    krkrsdl3/plugins/sqlite3/extend.cpp
    krkrsdl3/plugins/sqlite3/sqlite3.c
    krkrsdl3/plugins/sqlite3/sqlite3.cpp
    krkrsdl3/plugins/sqlite3/xp3_vfs.cpp
    krkrsdl3/plugins/Kirikiroid2/kirikiroid2.cpp
    krkrsdl3/plugins/Kirikiroid2/minizip/unzip.c
    krkrsdl3/plugins/Kirikiroid2/minizip/ioapi.cpp
    krkrsdl3/plugins/Kirikiroid2/TARArchive.cpp
    krkrsdl3/plugins/Kirikiroid2/ZIPArchive.cpp
    krkrsdl3/plugins/Kirikiroid2/7zArchive.cpp
    krkrsdl3/plugins/Kirikiroid2/7zip/7zArcIn.c
    krkrsdl3/plugins/Kirikiroid2/7zip/7zDec.c
    krkrsdl3/plugins/Kirikiroid2/7zip/7zBuf.c
    krkrsdl3/plugins/Kirikiroid2/7zip/7zBuf2.c
    krkrsdl3/plugins/Kirikiroid2/7zip/7zStream.c
    krkrsdl3/plugins/Kirikiroid2/7zip/Aes.c
    krkrsdl3/plugins/Kirikiroid2/7zip/AesOpt.c
    krkrsdl3/plugins/Kirikiroid2/7zip/Alloc.c
    krkrsdl3/plugins/Kirikiroid2/7zip/Bcj2.c
    krkrsdl3/plugins/Kirikiroid2/7zip/Bcj2Enc.c
    krkrsdl3/plugins/Kirikiroid2/7zip/Blake2s.c
    krkrsdl3/plugins/Kirikiroid2/7zip/Bra.c
    krkrsdl3/plugins/Kirikiroid2/7zip/Bra86.c
    krkrsdl3/plugins/Kirikiroid2/7zip/BraIA64.c
    krkrsdl3/plugins/Kirikiroid2/7zip/BwtSort.c
    krkrsdl3/plugins/Kirikiroid2/7zip/CpuArch.c
    krkrsdl3/plugins/Kirikiroid2/7zip/Delta.c
    krkrsdl3/plugins/Kirikiroid2/7zip/HuffEnc.c
    krkrsdl3/plugins/Kirikiroid2/7zip/LzFind.c
    krkrsdl3/plugins/Kirikiroid2/7zip/LzFindMt.c
    krkrsdl3/plugins/Kirikiroid2/7zip/LzFindOpt.c
    krkrsdl3/plugins/Kirikiroid2/7zip/LzmaDec.c
    krkrsdl3/plugins/Kirikiroid2/7zip/Lzma2Dec.c
    krkrsdl3/plugins/Kirikiroid2/7zip/Lzma2DecMt.c
    krkrsdl3/plugins/Kirikiroid2/7zip/Lzma2Enc.c
    krkrsdl3/plugins/Kirikiroid2/7zip/LzmaEnc.c
    krkrsdl3/plugins/Kirikiroid2/7zip/LzmaLib.c
    krkrsdl3/plugins/Kirikiroid2/7zip/MtCoder.c
    krkrsdl3/plugins/Kirikiroid2/7zip/MtDec.c
    krkrsdl3/plugins/Kirikiroid2/7zip/Ppmd7.c
    krkrsdl3/plugins/Kirikiroid2/7zip/Ppmd7aDec.c
    krkrsdl3/plugins/Kirikiroid2/7zip/Ppmd7Dec.c
    krkrsdl3/plugins/Kirikiroid2/7zip/Ppmd7Enc.c
    krkrsdl3/plugins/Kirikiroid2/7zip/Ppmd8.c
    krkrsdl3/plugins/Kirikiroid2/7zip/Ppmd8Dec.c
    krkrsdl3/plugins/Kirikiroid2/7zip/Ppmd8Enc.c
    krkrsdl3/plugins/Kirikiroid2/7zip/Sha1.c
    krkrsdl3/plugins/Kirikiroid2/7zip/Sha1Opt.c
    krkrsdl3/plugins/Kirikiroid2/7zip/Sha256.c
    krkrsdl3/plugins/Kirikiroid2/7zip/Sha256Opt.c
    krkrsdl3/plugins/Kirikiroid2/7zip/Sort.c
    krkrsdl3/plugins/Kirikiroid2/7zip/SwapBytes.c
    krkrsdl3/plugins/Kirikiroid2/7zip/Threads.c
    krkrsdl3/plugins/Kirikiroid2/7zip/Xz.c
    krkrsdl3/plugins/Kirikiroid2/7zip/XzDec.c
    krkrsdl3/plugins/Kirikiroid2/7zip/XzEnc.c
    krkrsdl3/plugins/Kirikiroid2/7zip/XzIn.c
    krkrsdl3/plugins/Kirikiroid2/7zip/7zCrc.c
    krkrsdl3/plugins/Kirikiroid2/7zip/7zCrcOpt.c
    krkrsdl3/plugins/Kirikiroid2/7zip/XzCrc64.c
    krkrsdl3/plugins/Kirikiroid2/7zip/XzCrc64Opt.c
    krkrsdl3/plugins/Kirikiroid2/7zip/Xxh64.c
    krkrsdl3/plugins/Kirikiroid2/7zip/ZstdDec.c
    krkrsdl3/plugins/Kirikiroid2/7zip/Md5.c
    krkrsdl3/plugins/Kirikiroid2/7zip/Sha3.c
    krkrsdl3/plugins/Kirikiroid2/7zip/Sha512.c
    krkrsdl3/plugins/Kirikiroid2/7zip/Sha512Opt.c
    krkrsdl3/plugins/Kirikiroid2/LoadJPEG.cpp
    krkrsdl3/plugins/Kirikiroid2/LoadPNG.cpp
    krkrsdl3/plugins/Kirikiroid2/LoadWEBP.cpp
)

set(KIRIKIRI_TJS2_SOURCES
    krkrsdl3/tjs2/tjs.cpp
    krkrsdl3/tjs2/tjs.tab.cpp
    krkrsdl3/tjs2/tjsArray.cpp
    krkrsdl3/tjs2/tjsBinarySerializer.cpp
    krkrsdl3/tjs2/tjsByteCodeLoader.cpp
    krkrsdl3/tjs2/tjsCompileControl.cpp
    krkrsdl3/tjs2/tjsConfig.cpp
    krkrsdl3/tjs2/tjsConstArrayData.cpp
    krkrsdl3/tjs2/tjsDate.cpp
    krkrsdl3/tjs2/tjsdate.tab.cpp
    krkrsdl3/tjs2/tjsDateParser.cpp
    krkrsdl3/tjs2/tjsDebug.cpp
    krkrsdl3/tjs2/tjsDictionary.cpp
    krkrsdl3/tjs2/tjsDisassemble.cpp
    krkrsdl3/tjs2/tjsError.cpp
    krkrsdl3/tjs2/tjsException.cpp
    krkrsdl3/tjs2/tjsGlobalStringMap.cpp
    krkrsdl3/tjs2/tjsInterCodeExec.cpp
    krkrsdl3/tjs2/tjsInterCodeGen.cpp
    krkrsdl3/tjs2/tjsInterface.cpp
    krkrsdl3/tjs2/tjsLex.cpp
    krkrsdl3/tjs2/tjsMath.cpp
    krkrsdl3/tjs2/tjsMessage.cpp
    krkrsdl3/tjs2/tjsMT19937ar-cok.cpp
    krkrsdl3/tjs2/tjsNamespace.cpp
    krkrsdl3/tjs2/tjsNative.cpp
    krkrsdl3/tjs2/tjsObject.cpp
    krkrsdl3/tjs2/tjsObjectExtendable.cpp
    krkrsdl3/tjs2/tjsOctPack.cpp
    krkrsdl3/tjs2/tjspp.tab.cpp
    krkrsdl3/tjs2/tjsRandomGenerator.cpp
    krkrsdl3/tjs2/tjsRegExp.cpp
    krkrsdl3/tjs2/tjsScriptBlock.cpp
    krkrsdl3/tjs2/tjsScriptCache.cpp
    krkrsdl3/tjs2/tjsString.cpp
    krkrsdl3/tjs2/tjsUtils.cpp
    krkrsdl3/tjs2/tjsVariant.cpp
    krkrsdl3/tjs2/tjsVariantString.cpp
)

set(KIRIKIRI_ENVIRON_SOURCES
    krkrsdl3/environ/Platform.cpp
    krkrsdl3/environ/apple/apple_core.mm
    krkrsdl3/environ/unix/unix_file.cpp
    krkrsdl3/environ/sdl3/sdl3_app.cpp
    krkrsdl3/environ/sdl3/sdl3_audio.cpp
    krkrsdl3/environ/sdl3/sdl3_dialog.cpp
    krkrsdl3/environ/sdl3/sdl3_file.cpp
    krkrsdl3/environ/sdl3/sdl3_input.cpp
    krkrsdl3/environ/sdl3/sdl3_thread.cpp
    krkrsdl3/environ/sdl3/sdl3_utils.cpp
)
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    list(APPEND KIRIKIRI_ENVIRON_SOURCES krkrsdl3/environ/iOS/ios_core.mm)
else()
    list(APPEND KIRIKIRI_ENVIRON_SOURCES krkrsdl3/environ/macos/macos_core.mm)
endif()

# An object library keeps every object file, so the plugins' static registrations survive linking.
add_library(kirikiri_engine OBJECT
    vndegen/ciphers.cpp
    ${KIRIKIRI_CORE_SOURCES}
    ${KIRIKIRI_ENVIRON_SOURCES}
    ${KIRIKIRI_PLUGINS_SOURCES}
    ${KIRIKIRI_TJS2_SOURCES}
)
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    target_compile_definitions(kirikiri_engine PUBLIC _KRKRSDL3_IOS=1)
else()
    target_compile_definitions(kirikiri_engine PUBLIC _KRKRSDL3_MACOS=1)
endif()
target_compile_definitions(kirikiri_engine PUBLIC _KRKRSDL3_USE_SDL3=1)

set(KRKRSDL krkrsdl3)
target_include_directories(kirikiri_engine PUBLIC
    ${KRKRSDL}/core/archive
    ${KRKRSDL}/core/main
    ${KRKRSDL}/core/media/font
    ${KRKRSDL}/core/media/image
    ${KRKRSDL}/core/media/movie
    ${KRKRSDL}/core/media/sound
    ${KRKRSDL}/core/msg
    ${KRKRSDL}/core/render
    ${KRKRSDL}/core/script
    ${KRKRSDL}/core/utils
    ${KRKRSDL}/core/utils/math
    ${KRKRSDL}/environ
    ${KRKRSDL}/plugins
    ${KRKRSDL}/tjs2
    vndegen
)

find_package(ZLIB REQUIRED)
find_package(PNG REQUIRED)
find_package(Freetype REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(JPEG REQUIRED)
find_package(libjpeg-turbo CONFIG REQUIRED)
find_package(Vorbis CONFIG REQUIRED)
find_package(WebP CONFIG REQUIRED)
find_package(oniguruma CONFIG REQUIRED)
find_package(Opus CONFIG REQUIRED)
find_package(OpusFile CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
find_package(SDL3_ttf CONFIG REQUIRED)
find_package(plutovg CONFIG REQUIRED)
target_link_libraries(kirikiri_engine PUBLIC
    ZLIB::ZLIB
    PNG::PNG
    Freetype::Freetype
    glm::glm-header-only
    JPEG::JPEG
    $<IF:$<TARGET_EXISTS:libjpeg-turbo::turbojpeg>,libjpeg-turbo::turbojpeg,libjpeg-turbo::turbojpeg-static>
    Vorbis::vorbis
    Vorbis::vorbisfile
    Vorbis::vorbisenc
    WebP::webp
    WebP::webpdecoder
    WebP::webpdemux
    oniguruma::onig
    Opus::opus
    OpusFile::opusfile
    SDL3::SDL3
    SDL3_ttf::SDL3_ttf
    plutovg::plutovg
    "-framework Foundation"
    "-framework CoreGraphics"
    "-framework QuartzCore"
    "-framework Metal"
    "-framework AudioToolbox"
    "-framework GameController"
)

set_source_files_properties(vndegen/vnkk.mm PROPERTIES COMPILE_OPTIONS -fobjc-arc)

if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    target_link_libraries(kirikiri_engine PUBLIC
        "-framework UIKit"
        "-framework AVFoundation"
    )
    add_library(KiriKiri SHARED vndegen/vnkk.mm vndegen/vnkk.h)
    target_link_libraries(KiriKiri PRIVATE kirikiri_engine)
    set_target_properties(KiriKiri PROPERTIES
        FRAMEWORK TRUE
        PUBLIC_HEADER vndegen/vnkk.h
        MACOSX_FRAMEWORK_IDENTIFIER com.chronostriker1.vndegen.KiriKiri
        MACOSX_FRAMEWORK_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/vndegen/Info.plist
    )
    # Only the C API is visible. SDL3 and the rest stay private, so they cannot clash with another
    # copy of SDL in the same app.
    target_link_options(KiriKiri PRIVATE
        -Wl,-exported_symbols_list,${CMAKE_CURRENT_SOURCE_DIR}/vndegen/vnkk.exports
        -Wl,-dead_strip
    )
    add_custom_command(TARGET KiriKiri POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${KIRIKIRI_FONT} $<TARGET_BUNDLE_DIR:KiriKiri>/)
else()
    target_link_libraries(kirikiri_engine PUBLIC
        "-framework AppKit"
        "-framework IOKit"
        "-framework ForceFeedback"
        "-framework Carbon"
    )
    add_executable(KiriKiriHelper vndegen/helper_main.cpp vndegen/vnkk.mm)
    target_link_libraries(KiriKiriHelper PRIVATE kirikiri_engine)
    target_link_options(KiriKiriHelper PRIVATE -Wl,-dead_strip)
    add_custom_command(TARGET KiriKiriHelper POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${KIRIKIRI_FONT} $<TARGET_FILE_DIR:KiriKiriHelper>/Res/DroidSansFallback.ttf)

    # Checks cipher detection on synthetic archives. Built on demand, after scripts/kirikiri-engine.sh:
    #   cmake --build build/kirikiri/.build/mac --target KiriKiriCipherTest
    #   build/kirikiri/.build/mac/KiriKiriCipherTest
    add_executable(KiriKiriCipherTest EXCLUDE_FROM_ALL vndegen/cipher_test.cpp vndegen/vnkk.mm)
    target_link_libraries(KiriKiriCipherTest PRIVATE kirikiri_engine)
endif()
