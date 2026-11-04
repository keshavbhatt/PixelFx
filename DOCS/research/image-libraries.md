# PixelFx rewrite: image processing library research and core stack recommendation

> Note (2026-11-04): this report calls the library "pixelcore" and suggests its own repository; the owner chose the name libpixelfx (`libs/pixelfx-core`, namespace `pfx`) inside the monorepo (ADR-001). The decisions taken from this report are in ADR-003, which departs from section 4.2 on one point: libvips drives the pipeline (our operations run inside it) while the GPU path is deferred.

Date: 2026-11-04. Facts were checked against official release pages, package indexes and docs on that date; where a source could not be reached it is flagged. Version numbers move fast, so treat them as "as of Nov 2026".

Context recap: the old PixelFx (Qt5, MIT) shelled out to a Go CLI (`pixel`, a wrapper over github.com/disintegration/gift) through `QProcess("bash -c ./pixel <flags> file")`, round-tripping whole files per filter chain (src/gift.cpp). The rewrite is Qt 6.11 / C++20 / CMake / Qt Widgets, GPL-3.0-or-later with a proprietary licensing module, built with the KDE neon `kde-qt6-core24-sdk` snap and run against `kf6-core24`; Flathub (KDE runtime 6.x) is a second target, Windows and macOS later. The owner wants a reusable open-source core image library (own repo, usable without Qt, thin Qt adapter), non-destructive adjustments, looks/presets, user-defined operation stacks saved as files, batch, geometry, format conversion, metadata preservation, RAW (nice to have), HEIC/AVIF/WebP/JXL, ICC, 16-bit/float pipelines and fast (ideally GPU) preview.

---

## 1. Summary of the recommendation

Build the core library ("pixelcore", working name) as an own float32 linear-light processing pipeline on top of a small set of mature, permissively licensed C libraries, instead of adopting any single big framework as the engine:

| Layer | Choice | Why |
|---|---|---|
| Pixel engine (adjustments, curves, HSL, local contrast, grain, vignette, LUT) | Own C++20 code, float32 planar/interleaved RGBA, linear Rec.2020 (or linear ProPhoto) working space, SIMD via Google Highway, tiles + thread pool | Every "retouching" op the app needs is missing or incomplete in all candidate libraries; the pipeline must be ours to be GPU-mirrorable and preset-stable |
| Decode/encode, resampling, geometry, big-image streaming, batch | libvips 8.18/8.19 (LGPL-2.1-or-later) | Demand-driven tiled pipeline, Lanczos3, affine/mapim warps, ICC import/export, loaders for JPEG/PNG/TIFF/WebP/HEIF/AVIF/JXL/EXR/RAW behind one API, bounded memory, no GPU but excellent CPU scaling |
| Colour management | LittleCMS2 2.19+ (MIT core; do NOT ship fast_float/threaded plugins, they are GPL-3) | ICC v2/v4, float transforms, device links; babl/OCIO are not ICC CMMs |
| Metadata | Exiv2 0.28.x (GPL-2.0-or-later) for read+write of Exif/IPTC/XMP in JPEG/PNG/TIFF/WebP and XMP sidecars; isolated behind a metadata interface so it can be swapped | Only complete C++ option; licence is fine for a GPL-3 app but see section 4.3 for the core-library licence consequence |
| RAW (optional feature) | LibRaw 0.22.x (LGPL-2.1 or CDDL) | Decoding to 16-bit linear camera RGB with matrices; demosaic packs are dead and not needed |
| Codecs (direct use only where libvips is not enough) | libjpeg-turbo, libpng, libtiff, libwebp, libheif (+libde265, dav1d/libaom), libavif, libjxl, OpenEXR | All BSD/MIT/LGPL except x265 (GPL-2): use kvazaar or skip HEIC encode |
| GPU preview | Qt RHI (QRhiWidget + QRhiComputePipeline or fragment passes, Vulkan GLSL baked with qsb), with the CPU pipeline as the mandatory reference implementation | One shader source for Vulkan/Metal/D3D/OpenGL; works in Flatpak/snap through Mesa; OpenCL is dead on macOS and painful to package |
| Preset / filter files | Own JSON, versioned, ordered list of operations with explicit working space, LUT references and optional node graph | Importers for Lightroom XMP (approximate), RawTherapee .pp3 (approximate), .cube/.3dl/HaldCLUT (exact) |

Not recommended as the engine: GEGL (right architecture, but GLib/GObject API, GPL-3 for the useful ops, no curves/HSL, OpenCL half-maintained), OpenCV (whole-image, no colour management, Lanczos4 only, 10 MB for core+imgproc, 4.6 in noble), ImageMagick/GraphicsMagick (whole-image, 2 and 1 maintainers, IM has no vcpkg port, GM has no HDRI float), OpenImageIO (great I/O and colour conversion but OCIO-centric and whole-buffer, 2.4 in noble, heavy deps), Halide (superb kernel compiler but LLVM at build time and a second toolchain), Skia (compositor, not an image engine), CImg/stb (single-maintainer or too small). Reusing darktable/RawTherapee code would make the core library GPL-3 and is practically hard; treat them as algorithm references only.

---

## 2. Candidate libraries, evaluated

All version/licence/package claims below come from the cited pages, checked 2026-11-04. "noble" = Ubuntu 24.04, the base of both the kde-neon core24 snaps and this project's SDK.

### 2.1 libvips (C API and vips8 C++ API)

- Version: 8.18.7 (2026-09-26, "Latest"); 8.19.0-rc1 published 2026-11-01. https://github.com/libvips/libvips/releases
- Licence: LGPL-2.1-or-later. https://github.com/libvips/libvips/blob/master/LICENSE
- Maintenance: daily commits, four point releases Jul-Sep 2026, three core maintainers (jcupitt, kleisauke, lovell) plus security researchers. https://github.com/libvips/libvips/commits/master
- Pixel model: VipsBandFormat uchar/char/ushort/short/uint/int/float/double/complex; VipsInterpretation includes sRGB, scRGB (linear float), RGB16, LAB, LCH, XYZ, OKLAB, OKLCH, HSV, CMYK. https://www.libvips.org/API/current/enum.BandFormat.html , https://www.libvips.org/API/current/libvips-colour.html
- Colour management: vips_icc_import/export/transform via lcms2 with intent, BPC, PCS, depth 8/16 (float output not documented). https://www.libvips.org/API/current/method.Image.icc_transform.html . 8.19 adds CICP/HDR handling and UltraHDR gain maps.
- Performance model: demand-driven graph of side-effect-free partial images, evaluated in tiles through VipsRegion; "horizontal" threading where each worker runs a whole tile through the entire pipeline; memory bounded by region buffers; SIMD through Highway with runtime dispatch. https://www.libvips.org/API/current/how-it-works.html
- Threading: VipsImage immutable and shareable; VipsRegion must stay on one thread; worker count = CPU count, override with vips_concurrency_set() / VIPS_CONCURRENCY. https://www.libvips.org/API/current/using-threads.html
- Interactive preview primitive: vips_sink_screen(in, out, mask, tile_w, tile_h, max_tiles, priority, notify_fn) renders in background threads, returns cached tiles immediately, marks readiness in a mask image, and calls notify_fn from a worker (marshal to the UI thread with g_idle_add or a Qt queued signal). https://www.libvips.org/API/current/method.Image.sink_screen.html
- Caches: operation cache (100 ops default), vips_tilecache (128x128 tiles, max_tiles, random/sequential, threaded, persistent). https://www.libvips.org/API/current/method.Image.tilecache.html
- Retouch-relevant ops: vips_resize (default VIPS_KERNEL_LANCZOS3; nearest, linear, cubic, mitchell, lanczos2/3, mks2013/2021), vips_affine, vips_similarity, vips_rotate, vips_mapim (arbitrary warp incl. perspective), interpolators nearest/bilinear/bicubic/lbb/nohalo/vsqbs, vips_gaussblur, vips_sharpen (unsharp in Lab L), conv/convsep, sobel/scharr/canny, vips_median (rank), histogram ops (hist_find, hist_equal, hist_local CLAHE, hist_match, stdif), maplut (8/16-bit index LUTs), FFT via fftw. No bilateral, no non-local-means, no curves/HSL/tone ops beyond linear/gamma. https://www.libvips.org/API/current/libvips-resample.html , https://www.libvips.org/API/current/libvips-convolution.html , https://www.libvips.org/API/current/libvips-histogram.html
- GPU: none (no option in meson_options.txt, nothing in release notes). https://raw.githubusercontent.com/libvips/libvips/master/meson_options.txt
- C++ API: `#include <vips/vips8>`, namespace vips, VImage with refcounting, VError exceptions, VOption for optional args, `pkg-config vips-cpp`; C++14 minimum since 8.18. https://www.libvips.org/API/current/cpp/index.html
- Loaders and deps (all optional meson features, can be dlopen modules): libjpeg(-turbo)/mozjpeg, libpng (preferred over libspng since 8.18), libtiff, libwebp, libheif, libjxl, OpenJPEG, OpenEXR (read), LibRaw (dcrawload), libultrahdr, libexif, lcms2, fftw3, highway, cgif+libimagequant, librsvg, poppler/PDFium, pangocairo, libarchive, matio, cfitsio, nifti, OpenSlide, ImageMagick/GraphicsMagick fallback. Built-in: nsgif, ppm, qoi (8.19), radiance, analyze. Required: glib, expat. https://raw.githubusercontent.com/libvips/libvips/master/README.md
- Size: Debian libvips42t64 8.18.7 installed 3.5 MB (.deb 1.3 MB); official Windows "web" dev zip with all DLLs 10.8 MB, "all" 18.8 MB. https://packages.debian.org/sid/amd64/libvips42t64 , https://github.com/libvips/build-win64-mxe/releases
- Windows/macOS: official MinGW-w64 Windows zips, Homebrew vips 8.18.7, vcpkg libvips 8.18.7 (features cplusplus, exif, jpeg, lcms, png, tiff, webp, rsvg, pangocairo; no heif/jxl/highway features in the port), Conan stale at 8.16. https://www.libvips.org/install.html , https://vcpkg.io/en/package/libvips.html
- noble: libvips42t64 8.15.1-1.1build4 (universe), linked against libheif1, libjxl0.7, libwebp7, libmagickcore-6.q16, libopenslide, poppler, librsvg, cgif, libspng, lcms2, OpenEXR 3.1, libhwy1. https://packages.ubuntu.com/noble/libvips42t64

Assessment: the best fit for I/O, resampling, geometry, memory-bounded batch and large-image streaming. It is not a retouching engine (no tone/colour grading primitives, 8/16-bit index LUTs only, no GPU), but its float band format and scRGB interpretation let us hand float tiles to our own ops. The user's remark that libvips "can also be powerful" is right in exactly this sense: it should carry everything around the adjustment math, and even the adjustment math can run inside a libvips pipeline through a custom VipsOperation (section 5.2).

### 2.2 OpenCV (core + imgproc only)

- Version: 5.0.0 (2026-06-06); 4.14.0 (2026-07-19) on the 4.x LTS line. https://github.com/opencv/opencv/releases
- Licence: Apache-2.0. https://github.com/opencv/opencv/blob/5.x/LICENSE
- Maintenance: multiple merges per day, large team.
- 5.0 changes: C++17, C API removed, new depths CV_16BF/CV_32U/CV_64U/CV_64S/CV_Bool, module reshuffle (ml/gapi to contrib, calib3d split, features2d renamed features), warpAffine/warpPerspective/remap reworked. https://raw.githubusercontent.com/wiki/opencv/opencv/OpenCV-5.md
- Model: whole-image cv::Mat, 8U/16U/16S/32F/64F/16F (+bf16 etc. in 5.x), universal intrinsics SIMD, parallel_for_ (TBB/OpenMP/pthreads), HAL plug-ins. No colour management, no ICC, no linear-light pipeline. OpenCL T-API (UMat) still present and maintained but not a focus; transparent CPU fallback. https://github.com/opencv/opencv/blob/5.x/modules/core/include/opencv2/core/ocl.hpp
- Ops: resize (INTER_LANCZOS4 only, no lanczos3), warpAffine/warpPerspective/remap, GaussianBlur, medianBlur (3/5 for 16U/32F, larger only 8U), bilateralFilter (8U/32F, 1 or 3 ch), filter2D, CLAHE (8U/16U), LUT (256 entries, 8-bit index only), no unsharp primitive. photo module: fastNlMeansDenoising 8U (16U with NORM_L1), inpaint, edgePreservingFilter/detailEnhance (8UC3 only), Tonemap on 32FC3. https://github.com/opencv/opencv/blob/5.x/modules/photo/include/opencv2/photo.hpp (docs.opencv.org refused automated fetch; verify depth tables at https://docs.opencv.org/5.x/d4/d86/group__imgproc__filter.html)
- Size: Debian 4.10 core 4.3 MB + imgproc 5.6 MB + photo 0.7 MB, so about 10 MB for core+imgproc. BUILD_LIST=core,imgproc,photo and WITH_QT=OFF trims the build. https://packages.debian.org/sid/amd64/libopencv-core410 , https://packages.debian.org/sid/amd64/libopencv-imgproc410
- Windows/macOS: official Windows installer (186 MB), Homebrew opencv 5.0.0, vcpkg opencv4 4.14.0 (no opencv5 port yet), Conan 4.14.0. https://github.com/microsoft/vcpkg/blob/master/ports/opencv4/vcpkg.json
- noble: 4.6.0+dfsg-13.1ubuntu1 (imgproc installed 4.9 MB amd64). https://packages.ubuntu.com/noble/libopencv-imgproc406t64

Assessment: a reasonable optional dependency for a few specific algorithms (non-local-means denoise, bilateral, inpaint, perspective solving from corner points). Not an engine: 8-bit-centric photo module, no ICC, no lanczos3, whole-image model. Note that the kf6-core24 content snap already carries a neon rebuild of OpenCV 4.10 (full module set), so on the snap it would cost nothing extra; on Flathub it would be a ~10 MB module.

### 2.3 ImageMagick 7 / Magick++

- Version: 7.1.2-35 (2026-10-31), releases roughly every two weeks. https://github.com/ImageMagick/ImageMagick/releases
- Licence: ImageMagick License (Apache-2.0 derived, "compatible with the GPL v3"). https://imagemagick.org/license/
- Maintenance: daily commits, effectively two maintainers (Cristy, dlemstra); constant security advisories.
- Model: whole-image pixel cache (heap/mmap/disk), quantum depth fixed at build (default Q16-HDRI = float storage), OpenMP inside algorithms, optional OpenCL for a small op set (blur, resize, modulate, ...), lcms2 for profiles. https://imagemagick.org/architecture/ , https://imagemagick.org/opencl/
- Ops: resize with many filters (Lanczos, Robidoux, ...), Distort Perspective, UnsharpMask, AdaptiveSharpen, Kuwahara, WaveletDenoise, CLAHE, Hald CLUT (HaldClutImage), CLUT, FunctionImage, sigmoidal contrast. No bilateral/NLM.
- Size: Debian libmagickcore-7.q16hdri-10 7.5 MB (+ coders); Arch full install 22.5 MB. https://packages.debian.org/sid/amd64/libmagickcore-7.q16hdri-10
- Windows/macOS: official Windows installers, Homebrew, Conan 7.1.2.35; no vcpkg port. https://imagemagick.org/download/
- noble: still ImageMagick 6 (8:6.9.12.98), Q16 and separate Q16HDRI packages. https://packages.ubuntu.com/noble/libmagick++-6.q16-9t64

Assessment: very capable for conversion but large, whole-image, security-heavy; noble ships IM6 only. Not recommended as a dependency of the core.

### 2.4 GraphicsMagick

- Version: 1.3.49 (2026-10-08). https://sourceforge.net/projects/graphicsmagick/files/graphicsmagick/
- Licence: MIT-style. https://sourceforge.net/p/graphicsmagick/code/ci/default/tree/Copyright.txt?format=raw
- Maintenance: three fix-only releases in 12 months, essentially one maintainer (Bob Friesenhahn). https://sourceforge.net/p/graphicsmagick/code/ci/default/tree/ChangeLog?format=raw
- Model: whole-image, Q8/Q16/Q32 integer only (no HDRI float build), OpenMP, no GPU, lcms2 profiles, Hald CLUT, no perspective distort, no CLAHE, no bilateral/NLM.
- Size: Debian 3.8 MB; Windows binaries stop at 1.3.46; vcpkg port stale at 1.3.45. https://github.com/microsoft/vcpkg/blob/master/ports/graphicsmagick/vcpkg.json
- noble: 1.4+really1.3.42. https://packages.ubuntu.com/noble/libgraphicsmagick++-q16-12t64

Assessment: this is what Photoflare uses (Qt 6 + Magick++). Stable API but integer-only and single-maintainer; not a fit for a float pipeline.

### 2.5 GEGL + babl

- Versions: GEGL 0.4.72 (2026-09-07), babl 0.1.128 (2026-08-07). https://download.gimp.org/pub/gegl/0.4/ , https://download.gimp.org/pub/babl/0.1/
- Licence: GEGL library LGPL-3.0-or-later; operations under operations/common-gpl3+ (shadows-highlights, lens-distortion, channel-mixer, ...) are GPL-3.0-or-later; babl LGPL-3.0-or-later. https://raw.githubusercontent.com/GNOME/gegl/master/COPYING.LESSER , https://gegl.org/operations/gegl-shadows-highlights.html , https://github.com/microsoft/vcpkg/blob/master/ports/gegl/vcpkg.json
- Maintenance: five releases each in 13 months, small GIMP-centric team. https://github.com/GNOME/gegl/commits/master
- Model: C/GObject graph (GeglNode/GeglBuffer), lazy, tile-cached with disk swap, mipmap preview rendering, GEGL_THREADS, OpenCL opt-in (revived in GSoC 2024, headers to CL 3.0 in 0.4.72, partial op coverage). babl formats like "RGBA float" (linear) vs "R'G'B'A u8", ICC matrix/TRC spaces via babl_space_from_icc. https://gegl.org/environment.html , https://gegl.org/babl/
- Photo ops present: exposure, levels, brightness-contrast, hue-chroma, saturation, vibrance (0.4.68), color-temperature, noise-reduction, unsharp-mask, bilateral-filter, median-blur, gaussian-blur, vignette, high-pass, HDR tone mappers; GPL3+: shadows-highlights, lens-distortion, channel-mixer. Missing in GEGL proper: curves, hue-saturation (HSL), color-balance; those live in GIMP's app/operations as GPL-3 gimp:* ops. https://github.com/GNOME/gegl/tree/master/operations/common , https://github.com/GNOME/gimp/tree/master/app/operations
- Deps: babl, GLib/GObject/GIO, json-glib, libpng, libjpeg, plus many optional. Meson build; MSVC and relocatable bundles since 0.4.66-0.4.72.
- noble: libgegl-0.4-0t64 0.4.48, libbabl-0.1-0 0.1.108 (both old). https://packages.ubuntu.com/noble/libgegl-0.4-0t64
- Windows/macOS: vcpkg gegl 0.4.72 (MinGW only), MSYS2, Homebrew; GIMP proves cross-platform builds.

Assessment: the only true non-destructive tile graph with photo ops, but adopting it pulls GLib/GObject into a library meant to be Qt-free and dependency-light, the ops we need most are GPL-3 or in GIMP, GPU is OpenCL-only and partial, and a Qt-side adapter would be awkward. Keep as an algorithm reference (its ops are small, readable C files).

### 2.6 OpenImageIO + OpenColorIO

- OIIO 3.2.2.0 (2026-10-28), monthly patch releases, Apache-2.0; Boost no longer required since 3.0; OCIO >= 2.3 is a required dependency. https://github.com/AcademySoftwareFoundation/OpenImageIO/releases , https://github.com/AcademySoftwareFoundation/OpenImageIO/blob/main/INSTALL.md
- OCIO 2.6.0 (2026-10-01), BSD-3-Clause, Vulkan GPU shader support since 2.5, built-in ACES configs; it is not an ICC CMM. https://github.com/AcademySoftwareFoundation/OpenColorIO/releases
- OIIO API: ImageBuf (uint8..double, half, float), thread-safe tiled ImageCache with memory limit, ImageBufAlgo with nthreads (resize, warp, rotate, colorconvert via OCIO, unsharp_mask, median_filter, convolve, demosaic, contrast_remap, histogram...). Whole-buffer, no lazy ROI graph, no photo ops. https://openimageio.readthedocs.io/en/latest/imagebufalgo.html
- Deps: Imath, OpenEXR, libtiff, OCIO, libjpeg, fmt, robin-map, plus optional format libs; roughly 10-20 MB plus OpenEXR/OCIO.
- noble: libopenimageio2.4t64 2.4.17 (Boost era), libopencolorio2.1t64 2.1.3 (too old for OIIO 3). https://packages.ubuntu.com/search?keywords=openimageio&searchon=names&suite=noble&section=all
- vcpkg openimageio 3.2.1.2, opencolorio 2.6.0.

Assessment: VFX-grade I/O and OCIO colour but heavier than libvips, whole-buffer, no sink_screen-style preview, and OCIO's config model is overkill for an ICC-based photo app. OCIO could still be an optional "look" importer (it parses .cube/.3dl/.clf and offers tetrahedral LUT interpolation on CPU and GPU), but a 3D LUT evaluator is ~200 lines, so this is not worth the dependency.

### 2.7 Halide

- Version 22.1.0 (2026-10-31), MIT; LLVM 21-23 at build time; official tarballs 160-185 MB; AOT runtime < 1 MB; backends CUDA/OpenCL/Metal/D3D12/Vulkan (beta)/WebGPU. https://github.com/halide/Halide/releases , https://github.com/halide/Halide/blob/main/doc/Vulkan.md
- noble: libhalide17 17.0.1 (old). vcpkg halide 21.0.0 (builds LLVM).

Assessment: the strongest single-source CPU-SIMD+GPU story, and its apps/ contains local_laplacian, bilateral_grid, camera_pipe and unsharp demos that match our needs. Cost: a second compiler toolchain in CI for every platform and a generator step in CMake. Worth a later experiment for the two or three expensive ops (local contrast, denoise) once the Highway CPU path and Qt RHI shader path exist; not for v1.

### 2.8 Skia

- Milestone branches (chrome/m157 current, m158 in progress), BSD-3-Clause, GN build, vcpkg port m148. SkColorType includes RGBA_F16 and RGBA_F32, SkColorSpace via skcms, SkRuntimeEffect/SkSL for user shaders, Ganesh (GL/Vulkan/Metal/D3D) and Graphite (Metal/Vulkan/Dawn) with Ganesh slated for deprecation. https://github.com/google/skia/blob/main/RELEASE_NOTES.md , https://skia.org/docs/user/sksl/
- Assessment: compositor and rasteriser, not an image engine; would duplicate Qt's own scene graph; GN build is alien to a CMake/snap project. No.

### 2.9 CImg

- v.4.0.7 (2026-10-27), header-only, CeCILL-C or CeCILL 2.1 (both GPL-compatible), one maintainer, no ICC/SIMD/GPU, whole-image, OpenMP loops; has bilateral/guided/anisotropic blur and Lanczos resize. https://github.com/GreycLab/CImg/tags
- Assessment: handy for prototyping an algorithm, not for the shipping core.

### 2.10 stb_image family

- stb_image 2.30, stb_image_write 1.16, stb_image_resize2 2.18 (2026-08-02 snapshot), public domain or MIT, 8/16-bit and float HDR load, resize2 has SIMD and float/half types but no Lanczos; no ICC, no EXIF. https://github.com/nothings/stb
- Assessment: fine for the test suite and tooling, not for the product (no colour management, no metadata).

### 2.11 LittleCMS2

- 2.20 (2026-10-31); 2.19 (2026-04) added CMake. https://github.com/mm2/Little-CMS/releases
- Licence: core MIT. The fast_float and multithreaded plugins are GPL-3.0-or-later with a paid commercial licence; do not vendor them into a library you want to keep permissive. https://littlecms.com/plugin/ , https://sources.debian.org/src/lcms2/2.17-1/plugins/README.1ST
- API: cmsCreateTransform + cmsDoTransformLineStride; float formats TYPE_RGB_FLT etc. are slower than 16-bit unless fast_float is loaded; share a transform across threads with cmsFLAGS_NOCACHE or create one per thread. https://www.mail-archive.com/lcms-user@lists.sourceforge.net/msg03044.html
- Device links: cmsTransform2DeviceLink() (there is no cmsCreateDeviceLink).
- noble: liblcms2-2 2.14; vcpkg lcms 2.19.1.

### 2.12 Exiv2 vs libexif vs exempi

- Exiv2 0.28.9 (2026-08-30), GPL-2.0-or-later, no commercial licence any more (withdrawn around 0.26). https://github.com/Exiv2/exiv2/releases , https://github.com/Exiv2/exiv2/wiki/Commercial-License
- 0.28 API: C++17, unique_ptr, ImageFactory::open, Error with ErrorCode enum. BMFF (HEIF/AVIF/CR3/JXL-in-BMFF) is read-only: BmffImage::writeMetadata throws kerWritingImageFormatUnsupported; bare JXL codestreams not handled. Read+write: JPEG, PNG, TIFF, WebP, PSD, JP2, XMP sidecars. crs namespace registered (Xmp.crs.*). https://raw.githubusercontent.com/Exiv2/exiv2/main/src/bmffimage.cpp , https://exiv2.org/tags-xmp-crs.html
- noble: libexiv2-27 0.27.6 (AutoPtr API, source-incompatible with 0.28). Prebuilt MSVC/macOS binaries upstream; vcpkg 0.28.9. https://packages.ubuntu.com/noble/libexiv2-27
- libexif 0.6.26 (2026-04-14), LGPL-2.1-or-later, Exif-only, JPEG-only, C. https://github.com/libexif/libexif/releases
- exempi 2.6.6 (Adobe XMP SDK, BSD-3), XMP only, no vcpkg port. noble libexempi8 2.6.5.
- LibRaw exposes parsed Exif/makernotes for raw files only.

Assessment: Exiv2 is the only library that preserves Exif+IPTC+XMP across JPEG/PNG/TIFF/WebP writes. For HEIF/AVIF/JXL output the app must re-embed metadata through the codec APIs (libheif heif_context_add_exif_metadata / XMP, libavif avifImageSetMetadataExif/XMP, libjxl JxlEncoderAddBox "Exif"/"xml "), which darktable also does because Exiv2 cannot. Isolate Exiv2 behind a `MetadataStore` interface so the core library's licence story stays clean (section 4.3).

### 2.13 LibRaw

- 0.22.3 (2026-10-26); LGPL-2.1-only OR CDDL-1.0. https://github.com/LibRaw/LibRaw/releases , https://github.com/LibRaw/LibRaw/blob/master/README.md
- Demosaic packs (GPL2/GPL3: AMaZE, LMMSE, VCD...) were deprecated in 0.19 and are abandoned; DHT (user_qual 11) and AAHD (12) are in core; no camera needs a pack. https://third-party-mirror.googlesource.com/libraw/+/refs/heads/master/README.demosaic-packs
- API: one LibRaw object per file, several objects in parallel allowed, OpenMP inside; output_bps=16, gamm {1,1}, no_auto_bright, use_camera_wb/use_camera_matrix, output_color 0..8 (raw/sRGB/Adobe/Wide/ProPhoto/XYZ/ACES/DCI-P3/Rec2020); 1284 cameras, CR3 and DNG (optional Adobe DNG SDK 1.7 for JXL-in-DNG). https://www.libraw.org/docs/API-datastruct.html , https://www.libraw.org/node/2861
- noble: libraw23t64 0.21.2 (different SONAME); vcpkg libraw 0.22.2.

### 2.14 libheif, libavif, libjxl, libwebp and the other codecs

- libheif 1.23.7 (2026-10-26, security release), LGPL-3.0-or-later; plugin system since 1.14 (WITH_x_PLUGIN, LIBHEIF_PLUGIN_PATH); 10/12-bit via heif_chroma_interleaved_RRGGBB_LE; ICC (prof/rICC) and nclx profile APIs. HEVC decode via libde265 (LGPL-3, 1.1.3), HEVC encode via x265 (GPL-2) or kvazaar (BSD-3). https://github.com/strukturag/libheif/releases , https://github.com/strukturag/libheif/blob/master/README.md
- noble libheif1 1.17.6-1ubuntu4.9 (main) depends on libheif-plugin-aomdec | dav1d; libheif-plugin-libde265 was demoted to Suggests in 1ubuntu4.3 (Feb 2026, LP#2142762) and libheif-plugin-x265 to Suggests in 1ubuntu4. So a stock noble install decodes AVIF but decodes HEIC only when the libde265 plugin is installed; the snap must stage libheif-plugin-libde265 explicitly. https://packages.ubuntu.com/noble/libheif1 , https://changelogs.ubuntu.com/changelogs/pool/main/libh/libheif/libheif_1.17.6-1ubuntu4.9/changelog
- libavif 1.4.2 (2026-05-26), BSD-2; gain maps, 8/10/12-bit, ICC and CICP; noble 1.0.4 (no gain maps). https://github.com/AOMediaCodec/libavif/releases
- libjxl 0.12.0 (2026-07-01), BSD-3 + patent grant; JxlEncoderAddJPEGFrame lossless JPEG transcoding, FLOAT/UINT16/FLOAT16 pixel types, libjxl_threads runner; depends on highway, brotli, lcms2 (libjxl_cms). noble libjxl0.7 0.7.0 is 2022-era and must not be used. https://github.com/libjxl/libjxl/releases , https://packages.ubuntu.com/noble/libjxl0.7
- libwebp 1.6.0 (2025-06-30), BSD-3, 8-bit only, 16383 px limit; noble 1.3.2. https://github.com/webmproject/libwebp/tags
- libjpeg-turbo 3.2.1 (2026-10-27), IJG+BSD-3+Zlib, 8/12/16-bit in one build; noble 2.1.5. https://github.com/libjpeg-turbo/libjpeg-turbo/releases
- libpng 1.6.59 (2026-09-28), libpng-2.0; noble 1.6.43. https://github.com/pnggroup/libpng/tags
- libtiff 4.7.2 (2026), libtiff licence; noble 4.5.1. https://libtiff.gitlab.io/libtiff/releases/v4.7.2.html
- OpenEXR 3.5.3 (2026-10-31), BSD-3, zstd and HTJ2K compression since 3.5; noble 3.1.5. https://github.com/AcademySoftwareFoundation/openexr/releases
- libspng 0.7.4 (2023, dormant), BSD-2; libvips now prefers libpng. https://github.com/randy408/libspng/releases

All have current vcpkg ports (lcms, exiv2, libraw, libheif, libde265, x265, kvazaar, aom, dav1d, svt-av1, libavif, libjxl, libwebp, libjpeg-turbo, libpng, tiff, openexr); exempi does not.

### 2.15 Darktable and RawTherapee code reuse

- darktable: GPL-3.0-or-later, C, fixed-order iop pixelpipe (src/develop/pixelpipe_hb.c) with modify_roi_in/out, 4x float32 per pixel, linear Rec.2020 working profile, preview pipe (downsampled) vs full pipe (visible ROI only) vs export pipe, tiling for memory-hungry modules, optional OpenCL kernels per module with CPU fallback. History stored in XMP sidecars as `darktable:history` with `darktable:params` hex dumps of C structs (plus gzip+base64 "gz" blobs). Styles are `.dtstyle` XML with the same opaque blobs. https://raw.githubusercontent.com/darktable-org/darktable/master/doc/pdf/hacking.tex , https://docs.darktable.org/usermanual/development/en/darkroom/pixelpipe/the-pixelpipe-and-module-order/ , https://docs.darktable.org/usermanual/development/en/overview/sidecar-files/sidecar/
- RawTherapee: GPL-3.0-or-later, C++, rtengine with ImProcCoordinator and Pipeline/Stage enums, float gamma-1.0 pipeline, ProPhoto working profile by default, OpenMP, no GPU, `.pp3` INI sidecars with partial profiles, HaldCLUT film simulation (8-bit sRGB PNG/TIFF, level 12 recommended). rtengine is "not designed to be used as a library" (Hubert Figuiere's integration attempt). https://rawpedia.pixls.us/color_management , https://rawpedia.pixls.us/film_simulation , https://www.figuiere.net/hub/wlog/integrating-rtengine/
- vkdt: BSD-2 (with some GPL-marked files), Vulkan/GLSL-only DAG, GPU required. https://github.com/hanatos/vkdt
- Licensing consequence: copying or linking darktable/RawTherapee code into the core library makes the core GPL-3 as a whole. If the core library is to be reusable under LGPL/MIT (recommended, see 4.3), GPL code must not enter it. Algorithms are not copyrightable: reimplement from the papers and module docs (e.g. darktable's color balance rgb, filmic, local contrast via guided filter/local Laplacian; RawTherapee's CIECAM, wavelet denoise) and cite them.

### 2.16 How comparable apps structure the pipeline (take-aways)

| App | Engine | Working space / depth | ROI and preview strategy | GPU | Edit storage |
|---|---|---|---|---|---|
| darktable | own C iop pipe | linear Rec.2020, float32x4 | preview pipe (downsampled), full pipe on visible ROI, export pipe, tiling on export, per-module cache | OpenCL per module, CPU fallback | XMP sidecar, hex struct blobs |
| RawTherapee / ART | rtengine | linear ProPhoto, float | thumbnail/navigator/preview/output stages, changeSinceLast mask | none | .pp3 / .arp INI |
| GIMP 3 | GEGL graph + babl | selectable, float linear default | GEGL tile cache, mipmap preview | OpenCL opt-in, partial | XCF with NDE filters |
| Krita | own tiled KisPaintDevice (64x64) | selectable, OCIO for scene-linear | filter masks/layers recomposed | OpenGL canvas only | .kra |
| Pinta | Cairo, 8-bit | sRGB 8-bit | whole image | none | none |
| Photoflare | GraphicsMagick Magick++ | Q16 integer | whole image | none | none |
| vkdt | Vulkan DAG | linear float, GPU buffers | tiles on GPU | Vulkan only | own graph files |

Pattern to copy: a float linear working space, a list of modules with fixed semantic order, three pipe sizes (navigator thumbnail, visible-ROI preview, full export), a per-module output cache invalidated from the edited module onward, and tiling only where memory demands it.

---

## 3. LUT and preset formats

### 3.1 LUT formats for interchange

- Adobe/Iridas `.cube`: text, `TITLE`, `LUT_1D_SIZE N` or `LUT_3D_SIZE N`, `DOMAIN_MIN`/`DOMAIN_MAX` (default 0..1), N^3 float triples with red varying fastest. Resolve adds `LUT_3D_INPUT_RANGE` and shaper 1D+3D combos. Adobe's spec PDF URL is dead; archived copy and the colour-science reader document it. https://colour.readthedocs.io/en/master/_modules/colour/io/luts/iridas_cube.html , https://ffmpeg.org/pipermail/ffmpeg-trac/2023-June/066367.html
- Autodesk `.3dl`: first line is input sampling (e.g. 17 values 0..1023), then integer output triples (10/12-bit), blue varies fastest (opposite of .cube); Lustre uses a 3DMESH header. https://download.autodesk.com/us/systemdocs/help/2011/lustre/files/WSc4e151a45a3b785a-5e71347c11df929805c-7fff.htm
- HaldCLUT: square image of side L^3 holding an N^3 cube with N = L^2 (level 8 = 512x512 / 64 steps, level 12 = 1728x1728 / 144 steps, level 16 = 4096x4096 / 256 steps), red fastest then green then blue in scanline order, identity value index/(N-1). PNG/TIFF 8 or 16-bit, colour space implied (RawTherapee assumes sRGB). https://www.quelsolaar.com/technology/clut.html , https://usage.imagemagick.org/color_mods/#hald-clut
- OCIO FileTransform reads 3dl, cube, csp, clf/ctf, spi1d/3d, cc/ccc, look; interpolation LINEAR (trilinear) vs TETRAHEDRAL (best). https://opencolorio.readthedocs.io/en/latest/api/enums.html
- ICC device links as LUTs: lcms2 cmsTransform2DeviceLink() from any transform; apply with the device link as input profile and NULL output. https://manpages.ubuntu.com/manpages/noble/man1/linkicc.1.html
- Interpolation: use tetrahedral (4 samples, more accurate than trilinear, matches trilinear quality with 20-25% smaller LUTs per the SMPTE BT.2100 study). https://journal.smpte.org/periodicals/SMPTE%20Motion%20Imaging%20Journal/129/2/17/
- Ambiguity: none of these formats declare their input encoding. darktable's lut3d makes the user choose (sRGB, Rec709, linear Rec709, linear Rec2020, Lab) and notes .cube is usually Rec.709 while HaldCLUT is usually sRGB. Our preset format must store an explicit `input_encoding` per LUT (default display sRGB) and a `strength`. https://docs.darktable.org/usermanual/development/en/module-reference/processing-modules/lut-3d/

### 3.2 Preset formats worth importing

- Lightroom / Camera Raw `.xmp` presets: XMP (ISO 16684-1) with the `crs` namespace (http://ns.adobe.com/camera-raw-settings/1.0/). Keys include Version, ProcessVersion, Exposure2012, Contrast2012, Highlights2012, Shadows2012, Whites2012, Blacks2012, Clarity2012, Texture, Dehaze, Vibrance, Saturation, Temperature, Tint, ToneCurvePV2012 (+Red/Green/Blue, "x, y" points 0..255), HueAdjustment*/SaturationAdjustment*/LuminanceAdjustment* per colour, SplitToning*, ColorGrade*, GrainAmount/Size/Frequency, PostCropVignetteAmount, Sharpness, LuminanceSmoothing, ColorNoiseReduction, CameraProfile, and the Look struct with embedded (undocumented) RGBTable look tables. https://exiftool.org/TagNames/XMP.html#crs . Legacy `.lrtemplate` is a Lua table with the same key names. Mapping is semantic only (Adobe process versions operate in linear ProPhoto with proprietary tone mapping), so results will never match; darktable's importer says the same and maps crop, straighten, exposure, vignette, clarity, tone curve, HSL, split toning, grain, spot removal. https://docs.darktable.org/usermanual/development/en/overview/sidecar-files/sidecar-import/
- RawTherapee `.pp3` / ART `.arp`: INI text, readable without RT code, partial profiles; semantic mapping again approximate. https://rawpedia.rawtherapee.com/Sidecar_Files_-_Processing_Profiles
- darktable `.dtstyle` / XMP: opaque per-module struct blobs; importing requires darktable's struct layouts per modversion. Not worth it.
- Capture One `.costyle` (undocumented text, packs are ZIP) and DxO `.preset` (Lua-like table): proprietary, undocumented; skip.
- Adobe DCP camera profiles: ProfileHueSatMap/ProfileLookTable/ProfileToneCurve tags in a TIFF container; DNG spec 1.7.1 published by Adobe with a patent licence; usable for RAW rendering later, not as a preset format. https://awaresystems.be/imaging/tiff/tifftags/profilelooktabledata.html
- Legal: parameter names and file formats are not copyrightable and the XMP/DNG specs are published for implementation; shipping Adobe's bundled presets/profiles or extracting their LUT data is not allowed. Name the importer "import Lightroom-compatible XMP preset" (interoperability description), not a product claim.

---

## 4. Recommended core stack, with reasoning

### 4.1 Architecture

```
+-------------------------------------------------------------------+
| PixelFx app (Qt 6.11 Widgets, GPL-3.0-or-later)                    |
|   QRhiWidget preview  |  Qt adapter (QImage <-> pixelcore::Image)  |
+-------------------------------------------------------------------+
| pixelcore (own repo, C++20, CMake, no Qt)                          |
|   pipeline/   ops/ (CPU, Highway SIMD)   gpu/ (shader sources,     |
|   graph JSON  lut/  color/ (lcms2)       GLSL 450 + optional SPIR-V)|
|   io/ (libvips-backed loader/saver, metadata adapters)             |
|   raw/ (LibRaw, optional)                                           |
+-------------------------------------------------------------------+
| libvips | lcms2 | Exiv2 (isolated) | LibRaw | codec libs | Highway |
+-------------------------------------------------------------------+
```

Working format: planar or interleaved RGBA float32 (owner's choice; interleaved RGBA matches libvips and GPU textures, so interleaved is the default), linear light, working colour space fixed per document (default linear Rec.2020 like darktable; ProPhoto is wider but has imaginary primaries; Rec.2020 keeps GPU half-float textures sane). Input images are converted once: decode -> ICC (embedded or assumed sRGB) -> linear working RGB via lcms2 float transform. Output: working RGB -> output ICC (sRGB by default, or the input profile for "preserve") -> codec.

Pipeline: an ordered list of operations with a canonical semantic order (geometry first: crop/rotate/straighten/perspective/resize-for-preview; then exposure, white balance, levels, curves, highlights/shadows, HSL, vibrance, clarity/local contrast, denoise, sharpen, LUT/look, vignette, grain; output transform last). Each op has `params`, `enabled`, `opacity`, optional mask. Every op implements `process(TileView in, TileView out, const Params&, const Context&)` on float tiles and declares `roi_in(roi_out)` (so blur/local-contrast can request padding) and `kind` (pointwise, local with radius r, global needing whole-image statistics such as auto levels). The pipeline evaluates in tiles (256x256 default, radius-padded) through a thread pool, with a per-op output cache keyed by (op index, params hash, roi, scale) so changing op k recomputes only k..n, exactly as darktable does.

Three evaluation scales: navigator thumbnail (~256 px), preview (the visible viewport at the displayed zoom, computed on a downscaled source produced once by libvips `vips_resize` with lanczos3 for zoom < 1, or on the full-res crop for zoom >= 1), and export (full resolution, streamed). Global-statistics ops compute their statistics on the preview scale and reuse them for export (darktable does this too), which keeps export deterministic and avoids a two-pass render.

Why an own engine rather than GEGL/OIIO/OpenCV: (a) none provides the full adjustment set with float linear semantics; (b) the ops must be mirrored one-to-one in GPU shaders for live preview, which is only possible if we own both implementations and their parameter semantics; (c) presets must stay stable across years, so parameter semantics must be ours and versioned; (d) the library must stay small and Qt-free and avoid GLib. The actual math of each adjustment is small (most are 20-200 lines); the engineering is in the pipeline, caching, tiling and GPU parity, which no candidate library gives us anyway.

### 4.2 Where libvips fits (and how far it can go)

- I/O: one loader/saver API for JPEG, PNG, TIFF, WebP, HEIF/AVIF, JXL, EXR, GIF, PPM and RAW (dcrawload via LibRaw), with embedded ICC and Exif/XMP blobs as image metadata, plus vips_thumbnail for fast shrink-on-load (JPEG DCT scaling, libjxl/libheif downscale). This replaces writing seven codec adapters.
- Resampling and geometry: vips_resize (lanczos3), vips_affine/similarity/rotate, vips_mapim for perspective, lbb/nohalo interpolators for rotation quality.
- Batch and streaming: build the per-image pipeline as a libvips graph (`VipsImage` chain ending in our custom op), run with vips_sink_disc / vips_image_write_to_file, and memory stays bounded to a few tiles per worker regardless of image size. Set VIPS_CONCURRENCY to the thread budget and vips_cache_set_max_mem to bound the op cache.
- Preview: vips_sink_screen is a ready-made asynchronous tile renderer with priorities, usable on the CPU fallback path.
- Custom ops: a pixelcore op can be registered as a VipsOperation (C) or wrapped with vips_image_generate() so libvips drives tiling and threads for it. This is the route if the owner wants libvips to be the only scheduler; the alternative is pixelcore owning its tile scheduler and using libvips only at the edges. Recommendation: start with libvips only at the edges (decode, resample, encode) plus our own tile scheduler, because the GPU path needs the same scheduler semantics and libvips cannot drive GPU work; revisit if profiling shows the double scheduling costs anything.
- Build: reduced libvips (jpeg, png, tiff, webp, heif, jxl, exr, lcms, highway, exif, optional libraw; no magick/poppler/rsvg/openslide/pango/fftw) keeps the attack surface and binary small (3-4 MB core plus codecs). Build it as a CMake ExternalProject/FetchContent of the meson project or via vcpkg manifest on Windows/macOS.
- Gaps to cover ourselves: float ICC transforms (vips_icc_* outputs 8/16-bit; do the float lcms2 transform directly on tiles), metadata writing for BMFF formats, GPU.

### 4.3 Licence plan

- App: GPL-3.0-or-later (plus the proprietary licensing module, which must stay a separate program or a clearly separate, dynamically loaded component with a documented boundary; this is outside the scope of this report).
- pixelcore: recommend LGPL-2.1-or-later or MIT/BSD so other projects can reuse it. Consequences: no darktable/RawTherapee/GIMP/GEGL-GPL3+ op code inside; Exiv2 (GPL-2+) must not be a hard dependency of the permissive library. Put Exiv2 behind a `MetadataStore` interface in a separate `pixelcore-exiv2` component (GPL-2+ or GPL-3+) that the GPL app links; ship a minimal fallback (copy Exif/XMP/ICC blobs opaquely through libvips metadata, which is what libvips itself does) inside the permissive core so format conversion still preserves metadata without Exiv2. lcms2 fast_float stays out. x265 stays out (kvazaar or no HEIC encode).
- If the owner prefers simplicity, make pixelcore GPL-3.0-or-later too; then Exiv2 and GEGL's GPL ops become usable in-tree, at the cost of reusability by non-GPL projects.

### 4.4 GPU preview path

- Use Qt RHI: QRhiWidget (since 6.7) with the QRhi obtained in initialize(); upload the preview-scale source as an RGBA16F/RGBA32F texture once, run the op stack as a chain of fragment-shader passes (ping-pong between two textures) or QRhiComputePipeline dispatches (since 6.6; needs OpenGL 4.3 / ES 3.1 or any of Vulkan/Metal/D3D), then blit with the output transform (working RGB -> display) in the last pass. Shaders are written once in Vulkan GLSL and baked by qsb / qt_add_shaders() (`.comp` needs `--glsl 310es,430`). https://doc.qt.io/qt-6/qrhiwidget.html , https://doc.qt.io/qt-6/qrhicomputepipeline.html , https://doc.qt.io/qt-6/qtshadertools-qsb.html
- Backend defaults: Metal on macOS, D3D11 on Windows, OpenGL on Linux (switch to Vulkan via setApi() where available). One QRhi backend per window; do not mix with QOpenGLWidget. QRhi has no binary compatibility promise across minor Qt versions, so pin the Qt version in the snap/flatpak build (the SDK snap does this anyway).
- Flatpak: `--device=dri`; Mesa GL/Vulkan from org.freedesktop.Platform's GL extension, NVIDIA via org.freedesktop.Platform.GL.nvidia-*. Snap core24: gpu-2404 content interface with mesa-2404 provider supplies libGL/EGL/libvulkan and supports host NVIDIA drivers; OpenCL is not on its supported list. https://docs.flatpak.org/en/latest/sandbox-permissions.html , https://ubuntu.com/frame/docs/24/how-to/use-snap-graphics/
- Rejected: OpenCL (deprecated on macOS since 10.14, ICD packaging pain in Flatpak/snap, exactly darktable's recurring support problem), raw QVulkanWindow (same portability as QRhi with more code), OpenGL-only QOpenGLWidget (deprecated on macOS).
- Parity rule: the CPU Highway implementation is the reference; GPU passes must match it within a tolerance checked by tests on a fixed image set. Export always uses the CPU path (deterministic, tiled, memory-bounded); GPU is for the viewport only in v1. Later, GPU export via readback is an optimisation.
- CPU SIMD: Google Highway 1.4.0 (Apache-2.0/BSD-3, runtime dispatch SSE2..AVX-512, NEON, SVE, RVV; used by libvips, libjxl, Chromium). std::simd (C++26) is not shippable yet. https://github.com/google/highway

### 4.5 RAW

Optional `pixelcore-raw` component on LibRaw 0.22: decode with output_bps=16, gamm {1,1}, no_auto_bright, use_camera_wb, use_camera_matrix, output_color = Rec.2020 (8) or XYZ (5), demosaic DHT or AAHD, then enter the float pipeline as linear data with a "raw" flag that enables exposure headroom and highlight reconstruction. DNG via LibRaw's native decoder. Lens correction via lensfun (LGPL-3) is a possible later addition.

### 4.6 Risks

| Risk | Mitigation |
|---|---|
| Snap runtime (kf6-core24) versions of libjxl (0.7, and its JXL plugin is broken), Exiv2 (0.27), libheif (1.17), LibRaw (0.21) are old and ABI-different from upstream; Flathub lacks Exiv2/LibRaw/libvips entirely | Vendor these as build-from-source parts in the snap and as Flatpak modules (see section 7) and as vcpkg manifest deps on Windows/macOS; never rely on noble's libjxl |
| HEIC decode in snap needs libde265 plugin staged; HEVC encode needs x265 (GPL-2) | Stage libheif-plugin-libde265; encode HEIC with kvazaar or not at all; AVIF/JXL/WebP as the modern outputs |
| Exiv2 cannot write BMFF (HEIF/AVIF/JXL) metadata | Re-embed Exif/XMP via codec APIs (libheif/libavif/libjxl boxes), as darktable does |
| QRhi API churn between Qt minors | Pin Qt in CI; keep the shader/pass layer thin and isolated |
| GPU/CPU result drift | Golden-image tests with tolerance; CPU is authoritative for export |
| libvips brings GLib | Acceptable: GLib is in every target runtime and GLib is not GObject-heavy in our usage; hide it behind pixelcore::io so consumers never see it |
| Own engine means every op is our maintenance | Keep op set focused, document math per op with references (darktable/RT/GEGL sources as reading material), fuzz the preset parser |
| Memory on huge images (100 MP, float RGBA = 1.6 GB) | Preview scale caps memory; export streams through tiles; add a memory budget that lowers tile count and thread count |

---

## 5. Preset / filter file format sketch

Goals: human-readable, versioned, diffable, forward-compatible, independent of pixel dimensions, carries colour-space context, supports both a simple stack and (later) a graph, embeds or references LUTs, and records provenance when imported.

```json
{
  "$schema": "https://pixelfx.app/schemas/preset-1.json",
  "format": "pixelfx-preset",
  "format_version": 1,
  "id": "0b1e7d1e-0a2d-4b8f-9c7a-2d2d3f4e5a6b",
  "name": "Warm Portrait",
  "author": "someone",
  "description": "Soft contrast, warm split tone",
  "tags": ["portrait", "warm"],
  "created": "2026-11-04T12:00:00Z",
  "app_version": "2.0.0",
  "engine_version": "1.0.0",
  "working_space": "linear-rec2020",
  "reference_resolution_px": 3000,
  "nodes": [
    { "id": "n1", "op": "exposure", "version": 1, "enabled": true, "params": { "ev": 0.35, "black": 0.0 } },
    { "id": "n2", "op": "white_balance", "version": 1, "params": { "temperature_k": 5600, "tint": 4 } },
    { "id": "n3", "op": "tone_curve", "version": 2, "params": {
        "space": "working-log2", "interp": "monotone-cubic",
        "rgb": [[0,0],[0.25,0.22],[0.75,0.80],[1,1]], "r": null, "g": null, "b": null } },
    { "id": "n4", "op": "hsl", "version": 1, "params": {
        "bands": { "red": {"h": 0, "s": -5, "l": 0}, "orange": {"h": -3, "s": 8, "l": 2} } } },
    { "id": "n5", "op": "local_contrast", "version": 1, "params": { "amount": 0.3, "radius_rel": 0.02 },
      "opacity": 1.0, "blend": "normal",
      "mask": { "type": "luminosity", "range": [0.2, 0.9], "feather": 0.1 } },
    { "id": "n6", "op": "lut3d", "version": 1, "params": {
        "lut": { "ref": "luts/warm-film.cube", "sha256": "..." },
        "input_encoding": "srgb-display", "interp": "tetrahedral", "strength": 0.7 } },
    { "id": "n7", "op": "vignette", "version": 1, "params": { "amount": -0.25, "midpoint": 0.6, "roundness": 0.5 } },
    { "id": "n8", "op": "grain", "version": 1, "params": { "amount": 0.15, "size_rel": 0.0015, "seed": 42 } }
  ],
  "edges": null,
  "assets": {
    "luts/warm-film.cube": { "embedded": null, "size": 33, "sha256": "..." }
  },
  "provenance": { "imported_from": "lightroom-xmp", "source_process_version": "15.4", "lossy": true }
}
```

Rules:
- `nodes` in file order is the evaluation order when `edges` is null (the common "stack"). `edges` (list of `{"from": "n1", "to": "n5", "port": "mask"}`) upgrades the file to a DAG later without a format break; a reader that does not understand edges must refuse files that have them (`"requires": ["graph"]` capability list in the header).
- Every op has its own `version`; the engine keeps migration functions `op@v1 -> op@v2`; unknown ops are kept verbatim and reported as "unsupported" so a newer preset round-trips through an older app.
- Geometry ops (crop, rotate, straighten, perspective, resize) are allowed in presets but normalised: crop as fractions, rotation in degrees, perspective as a 3x3 homography on the unit square, resize as a target long edge or scale, so they apply to any image size. Spatial radii are relative to `reference_resolution_px` (or fractions of the long edge) so that preview and export produce the same look.
- Parameters use SI-ish units (EV, kelvin, degrees, fractions), never pixel counts or 0-255 values.
- LUTs: referenced by relative path plus sha256, or embedded as base64 (gzip) for self-contained sharing; `input_encoding` is mandatory. Imported .cube/.3dl/HaldCLUT are normalised to an internal `{"size": N, "domain": [...], "data": float32 r-fastest}` blob.
- Per-image edits (the sidecar) use the same schema with `"format": "pixelfx-edit"`, plus `source` (file hash, dimensions, ICC id) and the full stack; a preset is simply an edit stack without `source`. Sidecar name `<image>.<ext>.pxfx.json` next to the image, optionally mirrored into XMP as a single `pixelfx:edit` property for apps that index XMP.
- A "custom filter" the user defines is a preset with `"kind": "filter"` and a declared parameter interface (`"exposed": [{"node": "n5", "param": "amount", "label": "Pop", "range": [0, 1]}]`) so it appears as a single slider-able op in the UI and can be nested in other presets by reference (`{"op": "preset", "params": {"ref": "<id>", "exposed": {...}}}`).
- Validation via a JSON Schema shipped with the library; fuzz the parser.

Importers: `lightroom-xmp` (crs keys above, mapped to exposure/contrast/highlights-shadows/whites-blacks/tone_curve/hsl/split-tone/vignette/grain/sharpen/denoise with documented approximations and `lossy: true`), `rawtherapee-pp3` (subset), `cube`/`3dl`/`haldclut` (exact, into a lut3d node). Exporter: `.cube` of the whole colour stack baked on a 33^3 or 65^3 identity cube in the chosen encoding (gives Lightroom/Resolve/other-app users a "look" file; geometry and spatial ops are excluded by definition).

---

## 6. Batch processing design

- Work unit: (input path, edit stack or preset, output spec {format, quality, resize, colour profile, metadata policy}).
- Scheduler: one job queue; `N_jobs = clamp(1, cores / 2)` images in flight, each libvips/pixelcore pipeline using `threads_per_job = cores / N_jobs` (set per pipeline via vips_concurrency_set on the pipeline's own thread, or our thread pool's partition). This beats "one image per core" because large images already parallelise inside, and keeps peak memory predictable.
- Memory bound: budget B (default 50% of physical RAM, user-settable). Per job estimate = tiles_in_flight * tile_bytes * (max padding factor) + decoder buffers + statistics scale image; reduce N_jobs until the sum fits. For global-statistics ops compute statistics on a 1-2 MP downscale produced by vips_thumbnail (shrink-on-load), then stream the full-res pass through tiles; no full-res float image is ever resident.
- Streaming: decode -> libvips pipeline (shrink/rotate/affine) -> pixelcore tile op (registered as a VipsOperation or via vips_image_generate) -> output ICC transform -> encoder, all demand-driven; vips_sink_disc writes strips as they complete. For encoders that need the whole image (libjxl with certain settings, HEIF), libvips buffers internally; for very large outputs prefer TIFF/JPEG/PNG which stream.
- Determinism: the same edit on the same input yields identical bytes regardless of thread count (tile seams handled by padding; grain uses a hash of pixel coordinates and seed, never thread-local RNG).
- Metadata policy: copy all (default), strip, or copy minus GPS; write through Exiv2 for JPEG/PNG/TIFF/WebP and through codec boxes for HEIF/AVIF/JXL; always embed the output ICC profile.
- Cancellation and progress: per tile callback; libvips `eval` signals and `vips_image_set_kill` for abort.
- CLI: `pixelfx-cli apply preset.json --out-dir ... --format jxl --quality 90 *.jpg` for scripting (the core library ships a small CLI, which also replaces the old `pixel` binary role).

---

## 7. Runtime availability (Flathub KDE runtime and kde-neon snap SDK)

Checked 2026-11-04 by cloning the freedesktop-sdk and flatpak-kde-runtime recipes, inspecting an installed org.kde.Platform//6.11 and org.freedesktop.Platform.codecs-extra//25.08-extra, and by downloading and listing the published kf6-core24 snap (rev 69) plus reading the kde-neon snap-packaging recipes. Checkouts are in the session scratchpad (`runtime-check/`).

### 7.1 Flathub: org.kde.Platform and the freedesktop runtime

- The KDE runtime recipe is https://invent.kde.org/packaging/flatpak-kde-runtime (one branch per Qt: qt6.9, qt6.10, qt6.11 default, qt6.12 beta). There is no github.com/flathub/org.kde.Sdk repo.
- org.kde.Platform//6.9 -> freedesktop 24.08 (EOL on Flathub). //6.10 -> 25.08. //6.11 (Qt 6.11.3, KF 6.31, published 2026-10-31, built with BuildStream against freedesktop-sdk-25.08.18) -> 25.08. This is the target. qt6.12 branch -> 26.08 (org.freedesktop.Platform//26.08 is already published).
- The KDE runtime adds no imaging libraries beyond kimageformats (built with `-DKIMAGEFORMATS_HEIF=ON`) and inherits the `org.freedesktop.Platform.codecs-extra` extension. Installed org.kde.Platform//6.11 contains: libaom 3.13.3, libavif 16.3.0 (1.3.0), libdav1d 7, libexif 12, libheif 1.20.2, libhwy 1.3.0, libjpeg 62 (libjpeg-turbo 3.1.4), libjxl 0.11.2 (+cms, +threads), liblcms2 2.18, libopenjp2 2.5.4, libpng 1.6.59, SvtAv1Enc 3.1.2, libtiff 6.1.0 (4.7.0), libwebp 7.2.0 (1.6.0). kimageformats plugins present: avif, heif, jxl, jp2 (no exr, no raw).
- Not present in the Platform (app must bundle as modules): libvips, OpenCV, ImageMagick, GraphicsMagick, GEGL/babl, OpenImageIO, OpenColorIO, Halide, Skia, CImg, Exiv2, exempi, LibRaw, OpenEXR/Imath, libde265, x265, rav1e. `find elements/ -iname` over freedesktop-sdk 24.08/25.08/26.08 confirms there are no elements for any of them except de265/x265 in the extension directories. https://gitlab.com/freedesktop-sdk/freedesktop-sdk
- flathub/shared-modules has none of the imaging libraries either (its directories are appstream, glew, gtk2, lua, SDL, ... ). No imaging BaseApp exists. https://github.com/flathub/shared-modules

Versions from `elements/components/*.bst` refs:

| Library | fdo 24.08 (EOL) | fdo 25.08 (KDE 6.10 / 6.11) | fdo 26.08 (KDE 6.12 beta) | In org.freedesktop.Platform? |
|---|---|---|---|---|
| libjpeg-turbo | 3.0.4 | 3.1.4.1 | 3.2.1 | yes |
| libpng | 1.6.44 | 1.6.59 | 1.6.59 | yes |
| libtiff | 4.6.0 | 4.7.0 | 4.7.2 | yes |
| libwebp | 1.4.0 | 1.6.0 | 1.6.0 | yes |
| openjpeg | 2.5.4 | 2.5.4 | 2.5.4 | yes |
| lcms2 | 2.18 | 2.18 | 2.19.1 | yes |
| libexif | 0.6.26 | 0.6.26 | 0.6.26 | yes |
| libjxl | 0.10.5 | 0.11.2 (cms + threads) | 0.12.0 | yes |
| libavif | 1.1.1 | 1.3.0 (aom, dav1d, svt) | 1.4.2 | yes |
| libheif | 1.18.2 | 1.20.2 | 1.23.1 | yes, AV1/JPEG/J2K codecs only |
| aom / dav1d / svt-av1 | 3.10.0 / 1.5.4 / 2.2.1 | 3.13.3 / 1.5.1 / 3.1.2 | 3.15.3 / 1.5.4 / 4.2.0 | yes |
| highway | 1.2.0 | 1.3.0 | 1.4.0 | yes (dep of libjxl) |
| libde265 | 1.1.1 (ffmpeg-full ext) | 1.1.3 (codecs-extra ext) | 1.1.3 (codecs-extra ext) | no, extension only |
| x265 | 4.2 (ffmpeg-full ext) | 4.2 (codecs-extra ext) | 4.2 (codecs-extra ext) | no, extension only |
| OpenEXR / Imath, Exiv2, exempi, LibRaw, libvips, OpenCV, IM/GM, GEGL/babl, OIIO/OCIO, Halide, Skia, rav1e | none | none | none | no |

HEVC/HEIC on Flathub: `elements/include/libheif.yml` (25.08) builds the Platform libheif with `-DENABLE_PLUGIN_LOADING=ON -DPLUGIN_DIRECTORY=/usr/lib/<triplet>/codecs-extra/lib/libheif -DWITH_LIBDE265_PLUGIN=OFF -DWITH_X265_PLUGIN=OFF -DWITH_DAV1D=ON -DWITH_AOM_ENCODER=ON -DWITH_AOM_DECODER=ON -DWITH_SvtEnc=ON ...`; the codecs-extra extension (`elements/extensions/codecs-extra/`) provides libde265.so, libx265.so and `libheif/libheif-libde265.so`, `libheif/libheif-x265.so` (verified on the installed 25.08-extra extension). Its metainfo says "This extension contains patent-restricted codecs not shipped in Platform". The in-runtime ffmpeg is built with `--disable-decoder=h264,hevc,vc1,vvc`. The extension is not installed by default; because the Platform libheif's plugin path points at it, HEIC decode and HEVC encode simply start working when the user (or the app manifest via add-extensions) installs it. 25.08 NEWS: "openh264 and ffmpeg-full extensions have been replaced by codecs-extra runtime extension", openh264 dropped. https://gitlab.com/freedesktop-sdk/freedesktop-sdk/-/blob/release/25.08/elements/include/libheif.yml , https://gitlab.com/freedesktop-sdk/freedesktop-sdk/-/blob/release/25.08/NEWS.yml

Flatpak plan: modules for libvips (reduced), Exiv2 0.28, LibRaw 0.22, OpenEXR/Imath (only if EXR I/O is wanted), Highway is present; everything codec-related comes from the Platform at current versions; declare codecs-extra as an optional extension for HEIC.

### 7.2 KDE neon snap SDK and runtime (core24)

Recipes under https://invent.kde.org/neon/snap-packaging/ (branch `work.core24` in each): `kde-qt6-core-sdk` (snap kde-qt6-core24-sdk, Qt 6.11.2, base core24, neon user archive for noble), `qt6-snap-runtime` (kde-qt6-core24 content snap = SDK minus headers/cmake/pc, 783 MB), `kf6-core-sdk` (kf6-core24-sdk, version 6.11.2-6.31.0-6.7.5-26.08.2 = Qt/KF/Plasma/Gear) and `kf6-core` (kf6-core24 content snap, rev 69, 1744 MB). Mechanism: the content snap is byte-for-byte the primed SDK minus dev files plus a few stage-packages, so any -dev package in the SDK has its runtime .so in the content snap; the app must not re-stage those and must stage everything absent. (One useful detail: the kf6-core24 recipe explicitly excludes `libheif.so.1.17.6` and `libheif/plugins/libheif-aomdec.so` from its own stage list to avoid duplicate-file conflicts with the SDK.)

Actual contents of kf6-core24 rev 69 (unsquashfs listing, usr/lib/x86_64-linux-gnu):

| Library | In kf6-core24? | Version in snap | Ubuntu noble archive version | Notes |
|---|---|---|---|---|
| libjpeg-turbo | yes (also in kde-qt6-core24) | 2.1.5 (libjpeg.so.8, libturbojpeg.so.0) | 2.1.5-2ubuntu2 | |
| libpng | yes | 1.6.43 | 1.6.43-5ubuntu0.7 | |
| libtiff | yes | 4.5.1 | 4.5.1+git230720-4ubuntu2.5 | |
| libwebp | yes | 1.3.2 | 1.3.2-0.4build3 | |
| openjpeg | yes | 2.5.0 | 2.5.0-2ubuntu0.5 | |
| lcms2 | yes | 2.16 (neon rebuild) | 2.14-2ubuntu0.1 | neon ships newer than Ubuntu |
| libexif | yes | 0.6.24 | 0.6.24-1ubuntu0.24.04.1 | |
| Exiv2 | yes (via kfilemetadata, libkexiv2) | 0.27.6 (libexiv2.so.27) | 0.27.6-1ubuntu0.3 | 0.27 API, not 0.28 |
| exempi | no | - | 2.6.5-1build1 | |
| LibRaw | yes | 0.21.2 (libraw.so.23) | 0.21.2-2.1ubuntu0.24.04.2 | kimg_raw present |
| libheif | yes | 1.17.6, aomdec plugin only | 1.17.6-1ubuntu4.9 | no kimg_heif; no HEVC |
| libde265 | no | - | 1.0.15-1ubuntu0.2 | stage libde265-0 + libheif-plugin-libde265 |
| x265 | yes (ffmpeg dep) | 3.5 (libx265.so.199) | 3.5-2build1 | libheif-plugin-x265 not staged |
| libavif | yes | 1.0.4 | 1.0.4-1ubuntu3 | kimg_avif present |
| libjxl | BROKEN | libjxl.so.0.7 only; kimg_jxl.so NEEDS libjxl.so.0.11 + libjxl_cms.so.0.11 | Ubuntu 0.7.0; neon archive libjxl0.11 0.11.1-1zneon | stage libjxl0.11 from the neon archive or bundle libjxl |
| highway | yes | 1.3.0 (neon rebuild) | 1.0.7-8.1build1 | |
| brotli | yes | 1.1.0 | 1.1.0 | |
| OpenEXR / Imath | yes | 3.1.5 / 3.1.9 | 3.1.5-5.1build3 / 3.1.9-3.1ubuntu2 | kimg_exr present |
| dav1d / libaom / rav1e / svt-av1 | yes | 1.4.1 / 3.8.2 / 0.7.1 / 1.7.0 | same | |
| OpenCV | yes | 4.10.0 full module set (neon rebuild) plus a stray libopencv_core.so.4.6.0 | 4.6.0+dfsg-13.1ubuntu1 | link against neon's libopencv-dev 4.10 if used; do not re-stage |
| libvips | no | - | 8.15.1-1.1build4 | stage from noble or build from source |
| ImageMagick / GraphicsMagick | no (IM is build-only for icons) | - | 6.9.12.98 / 1.3.42 | |
| GEGL / babl | no | - | 0.4.48 / 0.1.108 | |
| OpenImageIO / OpenColorIO | no | - | 2.4.17 / 2.1.3 | |
| Halide / CImg / Skia | no | - | 17.0.1 / 3.2.1 header / not packaged | |
| kimageformats | yes | 6.31.0: avif, exr, jp2, jxl (broken), raw; no heif | - | |

Findings that affect the snap build:
- JXL bug: the kf6-core-sdk kimageformats part uses build-package `libjxl-dev`, which resolves to neon's rebuilt 0.11.1, while its stage-packages still list Ubuntu's `libjxl0.7`; the published snap ships only libjxl.so.0.7 and kimg_jxl cannot load (readelf NEEDED libjxl.so.0.11). Worth reporting to neon; for PixelFx, bundle libjxl (0.12) inside the app snap regardless, since even 0.11 lacks features we want.
- HEVC: craft-parts installs stage-packages with `Apt::Install-Recommends False`, so only `libheif-plugin-aomdec` lands. For HEIC decode stage `libde265-0` and `libheif-plugin-libde265` (and `libheif-plugin-x265` for encode, GPL-2) in the app snap and point `LIBHEIF_PLUGIN_PATH` at the app's plugin dir, because the content snap's libheif plugin directory is read-only. https://packages.ubuntu.com/noble/libheif1
- Versions in the snap are two years behind Flathub 25.08 (libheif 1.17.6 vs 1.20.2, libavif 1.0.4 vs 1.3.0, libtiff 4.5.1 vs 4.7.0, libjpeg-turbo 2.1.5 vs 3.1.4, LibRaw 0.21.2 vs upstream 0.22.3, Exiv2 0.27.6 vs 0.28.9). Build Exiv2 0.28, LibRaw 0.22, libjxl 0.12 and libvips 8.18 from source as parts, linking them into the app snap with distinct SONAMEs so they do not clash with the content snap's copies (exiv2.so.28 vs .27, libraw.so.24 vs .23, libjxl.so.0.12 vs .0.7). libheif/libavif/libwebp/libtiff/libpng/libjpeg can come from the content snap for v1; revisit when kf6-core26 lands (work.core26 branches already exist).
- GPU: core24 snaps use the `gpu-2404` content interface (mesa-2404 provider) supplying libGL/EGL/libvulkan and host NVIDIA; no OpenCL. https://ubuntu.com/frame/docs/24/how-to/use-snap-graphics/

---

## 8. Decision summary and next steps

1. Create `pixelcore` repo: CMake, C++20, no Qt; subdirectories pipeline/, ops/, color/, io/, lut/, preset/, cli/; options PIXELCORE_WITH_RAW, PIXELCORE_WITH_EXIV2, PIXELCORE_WITH_GPU_SHADERS (ships shader sources only).
2. Dependencies: libvips (reduced build), lcms2, Highway, nlohmann/json or simdjson for presets, optional LibRaw, optional Exiv2 (separate component). Codec libs enter only through libvips except for metadata box writing.
3. Implement the pipeline with tiles and per-op cache, the first ten ops in CPU/Highway, golden tests, then the QRhi preview in the app with the same ten ops as fragment passes.
4. Preset schema v1 and the .cube/HaldCLUT importers first (exact), Lightroom XMP importer second (approximate, documented).
5. Snap: build libvips 8.18, libjxl 0.12, Exiv2 0.28 and LibRaw 0.22 from source as parts (kf6-core24 ships 8.15-era noble versions or nothing, and its JXL plugin is broken), take libheif/libavif/libwebp/libtiff/libpng/libjpeg/lcms2/highway from the content snap, stage `libde265-0` + `libheif-plugin-libde265` with `LIBHEIF_PLUGIN_PATH`. Flathub: modules for libvips, Exiv2, LibRaw only (codecs and lcms2 2.18 come from freedesktop 25.08; HEIC through the codecs-extra extension). Windows/macOS: vcpkg manifest (all deps have ports; no ImageMagick needed).
6. Report the kf6-core24 libjxl 0.7 vs 0.11 mismatch to KDE neon (kf6-core-sdk work.core24, part kimageformats stage-packages).
7. Later experiments: Halide for local contrast/denoise kernels; lensfun; GPU export.
