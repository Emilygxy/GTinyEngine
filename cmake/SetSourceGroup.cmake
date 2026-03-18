#
# Provide `set_source_group_for_files()` for Visual Studio IDE grouping.
#
# Usage:
#   set_source_group_for_files("${SOURCES}")
#   set_source_group_for_files("${HEADERS}")
#

function(set_source_group_for_files files)
    foreach(_file IN LISTS files)
        if(NOT _file)
            continue()
        endif()

        # Normalize path separators to forward slashes (CMake's internal form).
        file(TO_CMAKE_PATH "${_file}" _file_norm)

        # Make absolute for consistent relative path computation.
        if(NOT IS_ABSOLUTE "${_file_norm}")
            get_filename_component(_file_abs "${_file_norm}" ABSOLUTE)
        else()
            set(_file_abs "${_file_norm}")
        endif()

        file(TO_CMAKE_PATH "${_file_abs}" _file_abs_norm)

        # Compute path relative to the source root and use its directory as the group name.
        file(RELATIVE_PATH _rel_path "${CMAKE_SOURCE_DIR}" "${_file_abs_norm}")
        get_filename_component(_rel_dir "${_rel_path}" DIRECTORY)

        # VS source_group uses backslashes to express nested groups.
        if(_rel_dir STREQUAL "")
            set(_group_name "Source Files")
        else()
            string(REPLACE "/" "\\" _group_name "${_rel_dir}")
        endif()

        # Register the file into the computed group.
        source_group("${_group_name}" FILES "${_file_abs_norm}")
    endforeach()
endfunction()

