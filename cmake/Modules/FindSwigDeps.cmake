if(DEFINED SWIGDIR)
	list(APPEND CMAKE_PREFIX_PATH "${SWIGDIR}")
elseif(DEFINED ENV{SWIGDIR})
	list(APPEND CMAKE_PREFIX_PATH "$ENV{SWIGDIR}")
endif()

if(WIN32)
	IF(CMAKE_SIZEOF_VOID_P EQUAL 8)
		SET(_LIB_SUFFIX 64)
	ELSE()
		SET(_LIB_SUFFIX 32)
	ENDIF()

	FIND_PATH(SWIG_DIR
		NAMES swigrun.i
		HINTS
			ENV SwigPath${_LIB_SUFFIX}
			ENV SwigPath
			ENV DepsPath${_LIB_SUFFIX}
			ENV DepsPath
			${SwigPath${_LIB_SUFFIX}}
			${SwigPath}
			${DepsPath${_LIB_SUFFIX}}
			${DepsPath}
			${_PYTHON_INCLUDE_DIRS}
		PATH_SUFFIXES
			../swig/Lib
			swig/Lib
			)

	find_program(SWIG_EXECUTABLE
		NAMES swig
		HINTS
			ENV SwigPath${_LIB_SUFFIX}
			ENV SwigPath
			ENV DepsPath${_LIB_SUFFIX}
			ENV DepsPath
			${SwigPath${_LIB_SUFFIX}}
			${SwigPath}
			${DepsPath${_LIB_SUFFIX}}
			${DepsPath}
			${_PYTHON_INCLUDE_DIRS}
		PATH_SUFFIXES
			../swig
			swig
			)
endif()

find_package(SWIG QUIET 2)
