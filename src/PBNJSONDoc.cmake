FIND_PACKAGE(Doxygen REQUIRED)
    
IF(NOT TARGET doc-api)
    ADD_CUSTOM_TARGET(doc-api)
ENDIF(NOT TARGET doc-api)

IF (DOXYGEN_FOUND)
	SET(DOC_DIRNAME pbnjson-${PBNJSON_VERSION})
	SET(PJ_DOC_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${DOC_DIRNAME}/share/doc/${DOC_DIRNAME}")
	SET(PJ_DOC_C_API_OUT "${PJ_DOC_OUTPUT}/c_api")
	SET(PJ_DOC_CXX_API_OUT "${PJ_DOC_OUTPUT}/cxx_api")
	SET(PJ_DOC_C_SRC_OUT "${PJ_DOC_OUTPUT}/c_internal")
	SET(PJ_DOC_CXX_SRC_OUT "${PJ_DOC_OUTPUT}/cxx_internal")
	
	MESSAGE(STATUS "** doxygen: ${DOXYGEN_PATH}")
	MESSAGE(STATUS "** documentation output to: ${PJ_DOC_OUTPUT}")
	
    MACRO(DOC_TARGET CUSTOM_TARGET LANG DOXY_FILENAME)
        CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/${DOXY_FILENAME} ${CMAKE_CURRENT_BINARY_DIR}/${DOXY_FILENAME} @ONLY)

        ADD_CUSTOM_TARGET(pbnjson-doc-${CUSTOM_TARGET}_${LANG}
            ${DOXYGEN_EXECUTABLE} ${DOXY_FILENAME}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Compiling ${CUSTOM_TARGET} documentation for ${LANG}")
    ENDMACRO(DOC_TARGET DOXY_FILENAME)

    #ADD_CUSTOM_TARGET(pbnjson-doc-api ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/PBNJSON_Public.dxy WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    #ADD_CUSTOM_TARGET(pbnjson-doc-src ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/PBNSJON_Internal.dxy WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    
    IF(NOT TARGET doc-src)
        ADD_CUSTOM_TARGET(doc-src)
    ENDIF(NOT TARGET doc-src)
    
    DOC_TARGET(src c PBNJSON_InternalC.dxy)
    DOC_TARGET(src cxx PBNJSON_InternalCXX.dxy)
    DOC_TARGET(api c PBNJSON_PublicC.dxy)
    DOC_TARGET(api cxx PBNJSON_PublicCXX.dxy)
    
    ADD_DEPENDENCIES(doc-api pbnjson-doc-api_c pbnjson-doc-api_cxx)
    ADD_DEPENDENCIES(doc-src pbnjson-doc-src_c pbnjson-doc-src_cxx)
    ADD_DEPENDENCIES(pbnjson-doc-src_c pbnjson-doc-api_c)
    ADD_DEPENDENCIES(pbnjson-doc-src_cxx pbnjson-doc-api_cxx)
    
    FOREACH(DOC_OUTPUT_DIR ${PJ_DOC_C_API_OUT} ${PJ_DOC_CXX_API_OUT} ${PJ_DOC_C_SRC_OUT} ${PJ_DOC_CXX_SRC_OUT})
	    FILE(MAKE_DIRECTORY "${DOC_OUTPUT_DIR}")
	ENDFOREACH(DOC_OUTPUT_DIR)

ELSE (DOXYGEN_FOUND)
	MESSAGE(STATUS "!! doxygen not found, not generating documentation")     
	ADD_CUSTOM_TARGET(pbnjson-doc echo "doxygen not installed, no documentation")
	ADD_DEPENDENCIES(doc-api pbnjson-doc)
ENDIF (DOXYGEN_FOUND)

IF(NOT TARGET doc)
    ADD_CUSTOM_TARGET(doc)
ENDIF(NOT TARGET doc)

ADD_DEPENDENCIES(doc doc-api)
