/* Hand-written stand-in for CMake's generate_export_header() output.
   liquidgov_http and the CMake liquidgov_platform target both link miniz
   in statically (no shared library, no cross-DSO visibility needed), so
   every export macro is a no-op — the same thing generate_export_header
   would emit for a static build. Do not regenerate this from a shared
   build; that would add dllexport/visibility attributes objdump won't
   need here. */
#ifndef MINIZ_EXPORT_H
#define MINIZ_EXPORT_H

#define MINIZ_EXPORT
#define MINIZ_NO_EXPORT

#endif
