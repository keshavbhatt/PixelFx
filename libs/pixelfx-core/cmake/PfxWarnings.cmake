# Adds the library's warning set to a target.
# Usage: pfx_set_warnings(<target>)
function(pfx_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /w14640 /w14265 /w14062)
        if(PFX_WERROR)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
            -Wnon-virtual-dtor -Wold-style-cast -Woverloaded-virtual
            -Wnull-dereference -Wdouble-promotion -Wformat=2
        )
        if(PFX_WERROR)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()
