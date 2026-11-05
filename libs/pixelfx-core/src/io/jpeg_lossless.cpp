// SPDX-License-Identifier: Apache-2.0
// Lossless JPEG optimisation through libjpeg-turbo's coefficient API (ADR-010): the DCT
// coefficients are copied untouched and written again with optimised Huffman tables, as
// jpegtran -optimize does, so every pixel decodes exactly as before.
#include "io/compress_internal.h"

#include <csetjmp>
#include <cstdio> // jpeglib.h needs FILE
#include <cstdlib>
#include <cstring>

extern "C" {
#include <jpeglib.h>
}

namespace pfx::detail {

namespace {

struct Marker
{
    int code = 0;
    std::vector<std::uint8_t> data;
};

bool startsWith(const std::vector<std::uint8_t>& data, std::string_view prefix)
{
    return data.size() >= prefix.size() && std::equal(prefix.begin(), prefix.end(), data.begin());
}

constexpr std::string_view kExif{"Exif\0\0", 6};
constexpr std::string_view kXmp{"http://ns.adobe.com/xap/1.0/\0", 29};
constexpr std::string_view kPhotoshop{"Photoshop 3.0\0", 14};

// The marker as it is to be written, or nothing to leave it out.
std::optional<Marker> kept(Marker m, bool removeExtras)
{
    // MPF describes previews stored after the image, which a transcode never carries over.
    if (m.code == JPEG_APP0 + 2 && startsWith(m.data, std::string_view("MPF\0", 4))) {
        return std::nullopt;
    }
    if (!removeExtras) {
        return m;
    }
    if ((m.code == JPEG_APP0 && startsWith(m.data, std::string_view("JFXX\0", 5))) ||
        (m.code == JPEG_APP0 + 2 && startsWith(m.data, std::string_view("FPXR\0", 5)))) {
        return std::nullopt; // thumbnails and FlashPix previews
    }
    if (m.code == JPEG_APP0 + 1 && startsWith(m.data, kExif)) {
        m.data = exifWithoutExtras(std::move(m.data));
    } else if (m.code == JPEG_APP0 + 1 && startsWith(m.data, kXmp)) {
        const std::vector<std::uint8_t> body(m.data.begin() + static_cast<std::ptrdiff_t>(kXmp.size()),
                                             m.data.end());
        const std::vector<std::uint8_t> clean = xmpWithoutExtras(body);
        m.data.resize(kXmp.size());
        m.data.insert(m.data.end(), clean.begin(), clean.end());
    } else if (m.code == JPEG_APP0 + 13 && startsWith(m.data, kPhotoshop)) {
        m.data = photoshopWithoutPreviews(m.data);
    }
    return m;
}

// The APPn and COM segments before the image data, or nothing when this is not a JPEG.
std::optional<std::vector<Marker>> readMarkers(const std::vector<std::uint8_t>& file, bool removeExtras)
{
    if (file.size() < 4 || file[0] != 0xFF || file[1] != 0xD8) {
        return std::nullopt;
    }
    std::vector<Marker> markers;
    std::size_t at = 2;
    while (at + 4 <= file.size()) {
        if (file[at] != 0xFF) {
            return std::nullopt;
        }
        const int code = file[at + 1];
        if (code == 0xFF) {
            ++at; // fill byte
            continue;
        }
        if (code == 0xDA || code == 0xD9) {
            return markers; // start of scan, or end: no more markers before the image
        }
        if ((code >= 0xD0 && code <= 0xD7) || code == 0x01) {
            at += 2;
            continue;
        }
        const std::size_t length = (std::size_t{file[at + 2]} << 8U) | file[at + 3];
        if (length < 2 || at + 2 + length > file.size()) {
            return std::nullopt;
        }
        if ((code >= JPEG_APP0 && code <= JPEG_APP0 + 15) || code == JPEG_COM) {
            Marker m{code,
                     {file.begin() + static_cast<std::ptrdiff_t>(at + 4),
                      file.begin() + static_cast<std::ptrdiff_t>(at + 2 + length)}};
            if (auto k = kept(std::move(m), removeExtras); k && k->data.size() <= 65533) {
                markers.push_back(std::move(*k));
            }
        }
        at += 2 + length;
    }
    return std::nullopt;
}

struct ErrorManager
{
    jpeg_error_mgr base{};
    std::jmp_buf jump{};
    std::array<char, JMSG_LENGTH_MAX> message{};
};

[[noreturn]] void onError(j_common_ptr info)
{
    // libjpeg hands back the struct it was given; ErrorManager starts with it.
    auto* e =
        reinterpret_cast<ErrorManager*>(info->err); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    (*info->err->format_message)(info, e->message.data());
    std::longjmp(e->jump, 1); // NOLINT(cert-err52-cpp,modernize-avoid-setjmp-longjmp): libjpeg error path
}

void ignoreMessage(j_common_ptr /*info*/, int /*level*/) {}

struct Output
{
    unsigned char* data = nullptr;
    unsigned long size = 0; // NOLINT(google-runtime-int): libjpeg's type
};

bool isJfif(const Marker& m)
{
    return m.code == JPEG_APP0 && startsWith(m.data, std::string_view("JFIF\0", 5));
}

bool isAdobe(const Marker& m)
{
    return m.code == JPEG_APP0 + 14 && startsWith(m.data, "Adobe");
}

// One transcode. Every object with a destructor lives before setjmp, so a longjmp from
// libjpeg skips none.
std::optional<std::vector<std::uint8_t>> transcode(const std::vector<std::uint8_t>& file, bool progressive,
                                                   const std::vector<Marker>& markers, std::string& error)
{
    jpeg_decompress_struct src{};
    jpeg_compress_struct dst{};
    ErrorManager err;
    Output out;
    std::vector<std::uint8_t> result;
    src.err = jpeg_std_error(&err.base);
    dst.err = &err.base;
    err.base.error_exit = onError;
    err.base.emit_message = ignoreMessage;
    if (setjmp(err.jump) != 0) { // NOLINT(cert-err52-cpp,modernize-avoid-setjmp-longjmp)
        error = err.message.data();
        jpeg_destroy_compress(&dst);
        jpeg_destroy_decompress(&src);
        std::free(out.data); // NOLINT(cppcoreguidelines-no-malloc): libjpeg allocates it
        return std::nullopt;
    }
    jpeg_create_decompress(&src);
    jpeg_create_compress(&dst);
    jpeg_mem_src(&src, file.data(), static_cast<unsigned long>(file.size())); // NOLINT(google-runtime-int)
    jpeg_read_header(&src, TRUE);
    jvirt_barray_ptr* coefficients = jpeg_read_coefficients(&src);
    jpeg_copy_critical_parameters(&src, &dst);
    dst.optimize_coding = TRUE;
    if (progressive) {
        jpeg_simple_progression(&dst);
    }
    jpeg_mem_dest(&dst, &out.data, &out.size);
    jpeg_write_coefficients(&dst, coefficients);
    for (const Marker& m : markers) {
        // libjpeg writes its own JFIF and Adobe markers from the copied parameters.
        if ((dst.write_JFIF_header != 0 && isJfif(m)) || (dst.write_Adobe_marker != 0 && isAdobe(m))) {
            continue;
        }
        jpeg_write_marker(&dst, m.code, m.data.data(), static_cast<unsigned int>(m.data.size()));
    }
    jpeg_finish_compress(&dst);
    jpeg_finish_decompress(&src);
    result.assign(out.data, out.data + out.size);
    jpeg_destroy_compress(&dst);
    jpeg_destroy_decompress(&src);
    std::free(out.data); // NOLINT(cppcoreguidelines-no-malloc)
    return result;
}

} // namespace

Result<std::vector<std::uint8_t>> jpegLossless(const std::vector<std::uint8_t>& file, bool removeExtras)
{
    const auto markers = readMarkers(file, removeExtras);
    if (!markers) {
        return Error{ErrorCode::DecodeFailed, "The file is damaged or incomplete."};
    }
    std::string error;
    auto baseline = transcode(file, false, *markers, error);
    if (!baseline) {
        return Error{ErrorCode::DecodeFailed, "The JPEG could not be read (" + error + ")."};
    }
    auto progressive = transcode(file, true, *markers, error);
    if (progressive && progressive->size() < baseline->size()) {
        return std::move(*progressive);
    }
    return std::move(*baseline);
}

} // namespace pfx::detail
