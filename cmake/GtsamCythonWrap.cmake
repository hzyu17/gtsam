# Check Cython version, need to be >=0.25.2
# Unset these cached variables to avoid surprises when the python/cython
# in the current environment are different from the cached!
unset(PYTHON_EXECUTABLE CACHE)
unset(CYTHON_EXECUTABLE CACHE)
find_package(Cython 0.25.2 REQUIRED)

# User-friendly Cython wrapping and installing function.
# Builds a Cython module from the provided interface_header.
# For example, for the interface header gtsam.h,
# this will build the wrap module 'gtsam'.
#
# Arguments:
#
# interface_header:  The relative path to the wrapper interface definition file.
# extra_imports: extra header to import in the Cython pxd file.
#                For example, to use Cython gtsam.pxd in your own module,
#        use "from gtsam cimport *"
# install_path: destination to install the library
# libs: libraries to link with
# dependencies: Dependencies which need to be built before the wrapper
function(wrap_and_install_library_cython interface_header extra_imports install_path libs dependencies)
  # Paths for generated files
  get_filename_component(module_name "${interface_header}" NAME_WE)
  set(generated_files_path "${PROJECT_BINARY_DIR}/cython/${module_name}")
  wrap_library_cython("${interface_header}" "${generated_files_path}" "${extra_imports}" "${libs}" "${dependencies}")
  install_cython_wrapped_library("${interface_header}" "${generated_files_path}" "${install_path}")
endfunction()

function(set_up_required_cython_packages)
  # Set up building of cython module
  find_package(PythonLibs 2.7 REQUIRED)
  include_directories(${PYTHON_INCLUDE_DIRS})
  find_package(NumPy REQUIRED)
  include_directories(${NUMPY_INCLUDE_DIRS})
endfunction()

# Convert pyx to cpp by executing cython
# This is the first step to compile cython from the command line
# as described at: http://cython.readthedocs.io/en/latest/src/reference/compilation.html
#
# Arguments:
#    - target:  The specified target for this step
#    - pyx_file:   The input pyx_file in full *absolute* path
#    - generated_cpp:   The output cpp file in full absolute path
#    - include_dirs:   Directories to include when executing cython
function(pyx_to_cpp target pyx_file generated_cpp include_dirs)
  foreach(dir ${include_dirs})
    set(includes_for_cython ${includes_for_cython}  -I ${dir})
  endforeach()

  add_custom_command(
    OUTPUT ${generated_cpp}
    COMMAND
      ${CYTHON_EXECUTABLE} -X boundscheck=False -a -v --fast-fail --cplus ${includes_for_cython} ${pyx_file} -o ${generated_cpp}
    VERBATIM)
  add_custom_target(${target} ALL DEPENDS ${generated_cpp})
endfunction()

# Build the cpp file generated by converting pyx using cython
# This is the second step to compile cython from the command line
# as described at: http://cython.readthedocs.io/en/latest/src/reference/compilation.html
#
# Arguments:
#    - target:  The specified target for this step
#    - cpp_file:   The input cpp_file in full *absolute* path
#    - output_lib_we:   The output lib filename only (without extension)
#    - output_dir:   The output directory
function(build_cythonized_cpp target cpp_file output_lib_we output_dir)
  add_library(${target} MODULE ${cpp_file})
  if(APPLE)
    set(link_flags "-undefined dynamic_lookup")
  endif()
  set_target_properties(${target} PROPERTIES COMPILE_FLAGS "-w" LINK_FLAGS "${link_flags}"
    OUTPUT_NAME ${output_lib_we} PREFIX "" LIBRARY_OUTPUT_DIRECTORY ${output_dir})
endfunction()

# Cythonize a pyx from the command line as described at
# http://cython.readthedocs.io/en/latest/src/reference/compilation.html
# Arguments:
#    - target:        The specified target
#    - pyx_file:      The input pyx_file in full *absolute* path
#    - output_lib_we: The output lib filename only (without extension)
#    - output_dir:    The output directory
#    - include_dirs:  Directories to include when executing cython
#    - libs:          Libraries to link with
#    - interface_header: For dependency. Any update in interface header will re-trigger cythonize
function(cythonize target pyx_file output_lib_we output_dir include_dirs libs interface_header dependencies)
  get_filename_component(pyx_path "${pyx_file}" DIRECTORY)
  get_filename_component(pyx_name "${pyx_file}" NAME_WE)
  set(generated_cpp "${output_dir}/${pyx_name}.cpp")

  set_up_required_cython_packages()
  pyx_to_cpp(${target}_pyx2cpp ${pyx_file} ${generated_cpp} "${include_dirs}")

  # Late dependency injection, to make sure this gets called whenever the interface header is updated
  # See: https://stackoverflow.com/questions/40032593/cmake-does-not-rebuild-dependent-after-prerequisite-changes
  add_custom_command(OUTPUT ${generated_cpp} DEPENDS ${interface_header} ${pyx_file} APPEND)
  if (NOT "${dependencies}" STREQUAL "")
    add_dependencies(${target}_pyx2cpp "${dependencies}")
  endif()

  build_cythonized_cpp(${target} ${generated_cpp} ${output_lib_we} ${output_dir})
  if (NOT "${libs}" STREQUAL "")
    target_link_libraries(${target} "${libs}")
  endif()
  add_dependencies(${target} ${target}_pyx2cpp)
endfunction()

# Internal function that wraps a library and compiles the wrapper
function(wrap_library_cython interface_header generated_files_path extra_imports libs dependencies)
  # Wrap codegen interface
  # Extract module path and name from interface header file name
  # wrap requires interfacePath to be *absolute*
  get_filename_component(interface_header "${interface_header}" ABSOLUTE)
  get_filename_component(module_path "${interface_header}" PATH)
  get_filename_component(module_name "${interface_header}" NAME_WE)

  # Wrap module to Cython pyx
  message(STATUS "Cython wrapper generating ${module_name}.pyx")
  set(generated_pyx "${generated_files_path}/${module_name}.pyx")
  file(MAKE_DIRECTORY "${generated_files_path}")
  add_custom_command(
    OUTPUT ${generated_pyx}
    DEPENDS ${interface_header} wrap
    COMMAND
        wrap --cython ${module_path} ${module_name} ${generated_files_path} "${extra_imports}"
    VERBATIM
    WORKING_DIRECTORY ${generated_files_path}/../)
  add_custom_target(cython_wrap_${module_name}_pyx ALL DEPENDS ${generated_pyx})
  if(NOT "${dependencies}" STREQUAL "")
    add_dependencies(cython_wrap_${module_name}_pyx ${dependencies})
  endif()

  message(STATUS "Cythonize and build ${module_name}.pyx")
  get_property(include_dirs DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY INCLUDE_DIRECTORIES)
  cythonize(cythonize_${module_name} ${generated_pyx} ${module_name}
    ${generated_files_path} "${include_dirs}" "${libs}" ${interface_header} cython_wrap_${module_name}_pyx)

  # distclean
  add_custom_target(wrap_${module_name}_cython_distclean
      COMMAND cmake -E remove_directory ${generated_files_path})
endfunction()

# Internal function that installs a wrap toolbox
function(install_cython_wrapped_library interface_header generated_files_path install_path)
  get_filename_component(module_name "${interface_header}" NAME_WE)

    # NOTE: only installs .pxd and .pyx and binary files (not .cpp) - the trailing slash on the directory name
  # here prevents creating the top-level module name directory in the destination.
  message(STATUS "Installing Cython Toolbox to ${install_path}") #${GTSAM_CYTHON_INSTALL_PATH}")
  if(GTSAM_BUILD_TYPE_POSTFIXES)
    foreach(build_type ${CMAKE_CONFIGURATION_TYPES})
      string(TOUPPER "${build_type}" build_type_upper)
      if(${build_type_upper} STREQUAL "RELEASE")
        set(build_type_tag "") # Don't create release mode tag on installed directory
      else()
        set(build_type_tag "${build_type}")
      endif()
      # Split up filename to strip trailing '/' in GTSAM_CYTHON_INSTALL_PATH if there is one
      get_filename_component(location "${install_path}" PATH)
      get_filename_component(name "${install_path}" NAME)
      install(DIRECTORY "${generated_files_path}/" DESTINATION "${location}/${name}${build_type_tag}"
          CONFIGURATIONS "${build_type}"
          PATTERN "build" EXCLUDE
          PATTERN "CMakeFiles" EXCLUDE
          PATTERN "Makefile" EXCLUDE
          PATTERN "*.cmake" EXCLUDE
          PATTERN "*.cpp" EXCLUDE
          PATTERN "*.py" EXCLUDE)
    endforeach()
  else()
    install(DIRECTORY "${generated_files_path}/" DESTINATION ${install_path}
        PATTERN "build" EXCLUDE
        PATTERN "CMakeFiles" EXCLUDE
        PATTERN "Makefile" EXCLUDE
        PATTERN "*.cmake" EXCLUDE
        PATTERN "*.cpp" EXCLUDE
        PATTERN "*.py" EXCLUDE)
  endif()
endfunction()

# Helper function to install Cython scripts and handle multiple build types where the scripts
# should be installed to all build type toolboxes
#
# Arguments:
#  source_directory: The source directory to be installed. "The last component of each directory
#                    name is appended to the destination directory but a trailing slash may be
#                    used to avoid this because it leaves the last component empty."
#                    (https://cmake.org/cmake/help/v3.3/command/install.html?highlight=install#installing-directories)
#  dest_directory: The destination directory to install to.
#  patterns: list of file patterns to install
function(install_cython_scripts source_directory dest_directory patterns)
  set(patterns_args "")
  set(exclude_patterns "")

  foreach(pattern ${patterns})
    list(APPEND patterns_args PATTERN "${pattern}")
  endforeach()
  if(GTSAM_BUILD_TYPE_POSTFIXES)
    foreach(build_type ${CMAKE_CONFIGURATION_TYPES})
      string(TOUPPER "${build_type}" build_type_upper)
      if(${build_type_upper} STREQUAL "RELEASE")
        set(build_type_tag "") # Don't create release mode tag on installed directory
      else()
        set(build_type_tag "${build_type}")
      endif()
      # Split up filename to strip trailing '/' in GTSAM_CYTHON_INSTALL_PATH if there is one
      get_filename_component(location "${dest_directory}" PATH)
      get_filename_component(name "${dest_directory}" NAME)
      install(DIRECTORY "${source_directory}" DESTINATION "${location}/${name}${build_type_tag}" CONFIGURATIONS "${build_type}"
            FILES_MATCHING ${patterns_args} PATTERN "${exclude_patterns}" EXCLUDE)
    endforeach()
  else()
    install(DIRECTORY "${source_directory}" DESTINATION "${dest_directory}" FILES_MATCHING ${patterns_args} PATTERN "${exclude_patterns}" EXCLUDE)
  endif()

endfunction()

# Helper function to install specific files and handle multiple build types where the scripts
# should be installed to all build type toolboxes
#
# Arguments:
#  source_files: The source files to be installed.
#  dest_directory: The destination directory to install to.
function(install_cython_files source_files dest_directory)

  if(GTSAM_BUILD_TYPE_POSTFIXES)
    foreach(build_type ${CMAKE_CONFIGURATION_TYPES})
      string(TOUPPER "${build_type}" build_type_upper)
      if(${build_type_upper} STREQUAL "RELEASE")
        set(build_type_tag "") # Don't create release mode tag on installed directory
      else()
        set(build_type_tag "${build_type}")
      endif()
      # Split up filename to strip trailing '/' in GTSAM_CYTHON_INSTALL_PATH if there is one
      get_filename_component(location "${dest_directory}" PATH)
      get_filename_component(name "${dest_directory}" NAME)
      install(FILES "${source_files}" DESTINATION "${location}/${name}${build_type_tag}" CONFIGURATIONS "${build_type}")
    endforeach()
  else()
    install(FILES "${source_files}" DESTINATION "${dest_directory}")
  endif()

endfunction()

