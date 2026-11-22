function(deadnec_configure_project target)
    target_compile_features(${target} PUBLIC cxx_std_20)

    set_target_properties(${target} PROPERTIES
    
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    
    )
endfunction()