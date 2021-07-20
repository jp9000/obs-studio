project(obs-amf)

set(_CXX_STANDARD 17)
set(_CXX_EXTENSIONS OFF)



set(obs-amf_libamf_SOURCES

	)

set(obs-amf_libamf_HEADERS
	AMF/include/components/Ambisonic2SRenderer.h
	AMF/include/components/AudioCapture.h
	AMF/include/components/ColorSpace.h
	AMF/include/components/Component.h
	AMF/include/components/ComponentCaps.h
	AMF/include/components/DisplayCapture.h
	AMF/include/components/FFMPEGAudioConverter.h
	AMF/include/components/FFMPEGAudioDecoder.h
	AMF/include/components/FFMPEGAudioEncoder.h
	AMF/include/components/FFMPEGComponents.h
	AMF/include/components/FFMPEGFileDemuxer.h
	AMF/include/components/FFMPEGFileMuxer.h
	AMF/include/components/FFMPEGVideoDecoder.h
	AMF/include/components/MediaSource.h
	AMF/include/components/VideoCapture.h
	AMF/include/components/VideoConverter.h
	AMF/include/components/VideoDecoderUVD.h
	AMF/include/components/VideoEncoderHEVC.h
	AMF/include/components/VideoEncoderVCE.h
	AMF/include/components/VideoStitch.h
	AMF/include/components/ZCamLiveStream.h

	AMF/include/core/AudioBuffer.h
	AMF/include/core/Buffer.h
	AMF/include/core/Compute.h
	AMF/include/core/ComputeFactory.h
	AMF/include/core/Context.h
	AMF/include/core/Data.h
	AMF/include/core/Debug.h
	AMF/include/core/Dump.h
	AMF/include/core/Factory.h
	AMF/include/core/Interface.h
	AMF/include/core/Plane.h
	AMF/include/core/Platform.h
	AMF/include/core/PropertyStorage.h
	AMF/include/core/PropertyStorageEx.h
	AMF/include/core/Result.h
	AMF/include/core/Surface.h
	AMF/include/core/Trace.h
	AMF/include/core/Variant.h
	AMF/include/core/Version.h
	)

set(obs-amf_SOURCES
	amf-h264-encoder.cpp
	amf-hevc-encoder.cpp
	amf-encoder.cpp
	amf-trace-writer.cpp
	obs-amf.cpp
	obs-amf-plugin-main.cpp)

set(MODULE_DESCRIPTION "AMD Encoder")
	configure_file(${CMAKE_SOURCE_DIR}/cmake/winrc/obs-module.rc.in obs-amf.rc)
	list(APPEND obs-amf_SOURCES
		obs-amf.rc)

set(obs-amf_HEADERS
	amf-encoder.hpp
	amf-trace-writer.hpp
	obs-amf.hpp)

add_library(obs-amf MODULE
	${obs-amf_SOURCES}
	${obs-amf_HEADERS}
	${obs-amf_libamf_SOURCES}
	${obs-amf_libamf_HEADERS}
	)
target_link_libraries(obs-amf
	libobs
	d3d9
	d3d11
	dxva2
	dxgi
	dxguid
	)

target_compile_definitions(obs-amf PRIVATE DX11_D3D)
set_target_properties(obs-amf PROPERTIES FOLDER "plugins")

source_group("obs-amf\\Source Files" FILES ${obs-amf_SOURCES})
source_group("obs-amf\\Header Files" FILES ${obs-amf_HEADERS})
source_group("libamf\\Source Files" FILES ${obs-amf_libamf_SOURCES})
source_group("libamf\\Header Files" FILES ${obs-amf_libamf_HEADERS})

install_obs_plugin(obs-amf)