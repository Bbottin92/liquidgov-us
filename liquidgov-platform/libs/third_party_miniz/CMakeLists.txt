# Vendored miniz (zip/deflate) — https://github.com/richgel999/miniz, MIT licensed.
add_library(third_party_miniz
    miniz/miniz.c
    miniz/miniz_tdef.c
    miniz/miniz_tinfl.c
    miniz/miniz_zip.c
)
target_include_directories(third_party_miniz PUBLIC miniz)
