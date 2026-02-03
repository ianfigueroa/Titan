# CompilerWarnings.cmake - Strict compiler flags and sanitizer support

function(set_project_warnings target)
    set(GCC_CLANG_WARNINGS
        -Wall
        -Wextra
        -Wpedantic
        -Werror
        -Wshadow
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Wcast-align
        -Wunused
        -Woverloaded-virtual
        -Wconversion
        -Wsign-conversion
        -Wnull-dereference
        -Wdouble-promotion
        -Wformat=2
        -Wimplicit-fallthrough
    )

    set(MSVC_WARNINGS
        /W4
        /WX
        /permissive-
        /w14640  # thread-unsafe static member initialization
        /w14242  # conversion, possible loss of data
        /w14254  # conversion, possible loss of data
        /w14263  # member function does not override
        /w14265  # class has virtual functions but dtor is not virtual
        /w14287  # unsigned/negative constant mismatch
        /w14296  # expression is always true/false
        /w14311  # pointer truncation
        /w14545  # expression before comma evaluates to function
        /w14546  # function call before comma missing argument list
        /w14547  # operator before comma has no effect
        /w14549  # operator before comma has no effect
        /w14555  # expression has no effect
        /w14619  # pragma warning invalid
        /w14640  # thread-unsafe static member initialization
        /w14826  # conversion is sign-extended
        /w14905  # wide string literal cast to LPSTR
        /w14906  # string literal cast to LPWSTR
        /w14928  # illegal copy-initialization
    )

    if(MSVC)
        target_compile_options(${target} PRIVATE ${MSVC_WARNINGS})
    else()
        target_compile_options(${target} PRIVATE ${GCC_CLANG_WARNINGS})
    endif()
endfunction()

function(enable_sanitizers target)
    if(NOT MSVC)
        target_compile_options(${target} PRIVATE
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
        )
        target_link_options(${target} PRIVATE
            -fsanitize=address,undefined
        )
    endif()
endfunction()
