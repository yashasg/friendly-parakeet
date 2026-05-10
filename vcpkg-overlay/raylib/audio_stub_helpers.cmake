function(raylib_write_audio_disabled_stub source_path header)
    file(WRITE "${source_path}/src/external/${header}" "# audio disabled")
endfunction()
