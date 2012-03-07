IF (WITH_DOCS)
  FIND_PACKAGE(Doxygen)
  SET(DOXYGEN_ERR "doxygen not installed, not generating documentation")
ELSE (WITH_DOCS)
  SET(DOXYGEN_ERR "doxygen not configured to be built, not generating documentation")
  SET(DOXYGEN_FOUND FALSE)
ENDIF (WITH_DOCS)

IF (DOXYGEN_FOUND)
  SET (YAJL_VERSION ${YAJL_MAJOR}.${YAJL_MINOR}.${YAJL_MICRO})
  SET(yajlDirName yajl-${YAJL_VERSION})
  SET(docPath
      "${CMAKE_CURRENT_BINARY_DIR}/${yajlDirName}/share/doc/${yajlDirName}")
  MESSAGE("** using doxygen at: ${doxygenPath}")
  MESSAGE("** documentation output to: ${docPath}")

  CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/src/YAJL.dxy
                 ${CMAKE_CURRENT_BINARY_DIR}/YAJL.dxy @ONLY)

  FILE(MAKE_DIRECTORY "${docPath}")

  ADD_CUSTOM_TARGET(yajl-doc
                    ${DOXYGEN_EXECUTABLE} YAJL.dxy   
                    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
ELSE (DOXYGEN_FOUND)
  IF (WITH_DOCS)
    MESSAGE("!! doxygen not found, not generating documentation")
  ENDIF (WITH_DOCS)
 
  ADD_CUSTOM_TARGET(
    yajl-doc
    echo ${DOXYGEN_ERR}
  )
ENDIF (DOXYGEN_FOUND)

IF(NOT TARGET doc)
  ADD_CUSTOM_TARGET(doc)
ENDIF(NOT TARGET doc)
ADD_DEPENDENCIES(doc yajl-doc)
