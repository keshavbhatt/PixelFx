// SPDX-License-Identifier: Apache-2.0
// pfx: the libpixelfx command line tool. Lists the operations, describes images, and applies
// presets and recipes to files.
#include "pfx/auto.h"
#include "pfx/document.h"
#include "pfx/io.h"
#include "pfx/log.h"
#include "pfx/lut.h"
#include "pfx/pipeline.h"
#include "pfx/registry.h"

#include <charconv>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <iostream>
#include <locale>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr const char* kUsage = R"(Usage:
  pfx ops                                   list operations and their parameters
  pfx info FILE...                          describe images
  pfx apply [options] FILE...               apply settings and save copies

Options for apply:
  --preset FILE      apply a .pfxpreset or .pfx recipe (repeatable, later ones win)
  --auto             start each photo from Auto (light and colour from its statistics)
  --set KEY=NUMBER   set one value, for example --set light.exposure=0.5 (repeatable)
  -o, --output PATH  output folder (created if missing), or a file name for one input
  --format EXT       output format by extension: jpg, png, tif, webp, avif, jxl (default: same as input)
  --quality N        1 to 100 for lossy formats (default 90)
  --bits 8|16        bits per channel for PNG and TIFF (default 8)
  --long-edge N      downscale so the long edge is at most N pixels
  --lut FILE[:MIX]   apply a .cube LUT after the settings, MIX 0 to 100 (default 100)
)";

int fail(std::string_view message)
{
    std::cerr << "pfx: " << message << "\n";
    return 1;
}

std::string_view typeName(pfx::ParamType type)
{
    switch (type) {
    case pfx::ParamType::Number:
        return "number";
    case pfx::ParamType::Boolean:
        return "boolean";
    case pfx::ParamType::Choice:
        return "choice";
    case pfx::ParamType::Points:
        return "curve";
    case pfx::ParamType::SpotList:
        return "spots";
    }
    return "?";
}

int listOps()
{
    for (const pfx::OpSpec& op : pfx::registry().ops()) {
        std::cout << op.id << " (group " << op.group << ", stage " << op.stage << ", v" << op.version
                  << (op.pointwise ? ", pointwise" : "") << ")\n";
        for (const pfx::ParamSpec& p : op.params) {
            std::cout << "  " << p.key << "  " << typeName(p.type);
            if (p.type == pfx::ParamType::Number) {
                std::cout << " " << p.minimum << " to " << p.maximum << ", default " << p.defaultValue;
                if (!p.unit.empty()) {
                    std::cout << " " << p.unit;
                }
            } else if (p.type == pfx::ParamType::Choice) {
                for (const auto& c : p.choices) {
                    std::cout << " " << c;
                }
            }
            std::cout << "\n";
        }
    }
    return 0;
}

int info(const std::vector<std::string>& files)
{
    int status = 0;
    for (const std::string& file : files) {
        const auto result = pfx::probe(file);
        if (!result) {
            std::cerr << file << ": " << result.error().message << "\n";
            status = 1;
            continue;
        }
        const pfx::ImageInfo& i = result.value();
        std::cout << file << ": " << i.width << " x " << i.height << ", " << i.format << ", "
                  << i.bitsPerSample << " bit" << (i.hasAlpha ? ", alpha" : "")
                  << (i.hasProfile ? ", colour profile" : "") << "\n";
    }
    return status;
}

struct ApplyArgs
{
    pfx::Settings settings;
    std::filesystem::path output;
    std::string format;
    pfx::SaveOptions save;
    int longEdge = 0;
    bool autoFirst = false;
    std::optional<pfx::Lut> lut;
    int lutMix = 100;
    std::vector<std::string> files;
};

bool parseInt(const std::string& text, int& out)
{
    const auto* end = text.data() + text.size();
    return std::from_chars(text.data(), end, out).ptr == end;
}

std::optional<std::string> addPreset(const std::string& file, ApplyArgs& args)
{
    pfx::ParseReport report;
    const auto doc = pfx::loadDocument(file, &report);
    if (!doc) {
        return file + ": " + doc.error().message;
    }
    for (const auto& w : report.warnings) {
        std::cerr << file << ": " << w << "\n";
    }
    args.settings = args.settings.overlaidWith(doc.value().settings);
    return std::nullopt;
}

std::optional<std::string> addSetting(const std::string& keyValue, ApplyArgs& args)
{
    const auto eq = keyValue.find('=');
    if (eq == std::string::npos) {
        return "--set needs KEY=NUMBER";
    }
    // A stream in the classic locale: Apple's libc++ has no floating-point std::from_chars.
    double value = 0.0;
    std::istringstream number(keyValue.substr(eq + 1));
    number.imbue(std::locale::classic());
    if (!(number >> value) || !number.eof()) {
        return "--set needs a number after =";
    }
    args.settings.set(keyValue.substr(0, eq), value);
    return std::nullopt;
}

std::optional<std::string> wholeNumber(const std::string& option, const std::string& text, int& out)
{
    if (!parseInt(text, out)) {
        return option + " needs a whole number";
    }
    return std::nullopt;
}

using OptionHandler = std::function<std::optional<std::string>(const std::string&, ApplyArgs&)>;

const std::map<std::string, OptionHandler>& optionHandlers()
{
    static const std::map<std::string, OptionHandler> handlers = {
        {"--preset", addPreset},
        {"--set", addSetting},
        {"--output",
         [](const std::string& v, ApplyArgs& a) {
             a.output = v;
             return std::optional<std::string>();
         }},
        {"--format",
         [](const std::string& v, ApplyArgs& a) {
             a.format = v;
             return std::optional<std::string>();
         }},
        {"--quality",
         [](const std::string& v, ApplyArgs& a) { return wholeNumber("--quality", v, a.save.quality); }},
        {"--bits",
         [](const std::string& v, ApplyArgs& a) { return wholeNumber("--bits", v, a.save.bitsPerSample); }},
        {"--long-edge",
         [](const std::string& v, ApplyArgs& a) { return wholeNumber("--long-edge", v, a.longEdge); }},
        {"--lut",
         [](const std::string& v, ApplyArgs& a) -> std::optional<std::string> {
             std::string file = v;
             if (const auto colon = v.rfind(':'); colon != std::string::npos && colon > 1) {
                 file = v.substr(0, colon);
                 if (auto error = wholeNumber("--lut mix", v.substr(colon + 1), a.lutMix)) {
                     return error;
                 }
             }
             auto lut = pfx::loadCube(file);
             if (!lut) {
                 return file + ": " + lut.error().message;
             }
             a.lut = std::move(lut).value();
             return std::nullopt;
         }},
    };
    return handlers;
}

std::optional<std::string> parseApply(int argc, char* const* argv, ApplyArgs& args)
{
    for (int i = 2; i < argc; ++i) {
        std::string option = argv[i];
        if (option == "-o") {
            option = "--output";
        }
        const auto& handlers = optionHandlers();
        if (const auto it = handlers.find(option); it != handlers.end()) {
            if (i + 1 >= argc) {
                return option + " needs a value";
            }
            if (auto error = it->second(argv[++i], args)) {
                return error;
            }
        } else if (option == "--auto") {
            args.autoFirst = true;
        } else if (!option.empty() && option[0] == '-') {
            return "unknown option " + option;
        } else {
            args.files.push_back(option);
        }
    }
    if (args.files.empty()) {
        return std::string("apply needs at least one image");
    }
    return std::nullopt;
}

std::filesystem::path outputPathFor(const ApplyArgs& args, const std::filesystem::path& input)
{
    std::filesystem::path name = input.filename();
    if (!args.format.empty()) {
        name.replace_extension("." + args.format);
    }
    if (args.output.empty()) {
        return input.parent_path() / (input.stem().string() + "-pfx" + name.extension().string());
    }
    const bool singleFile =
        args.files.size() == 1 && args.output.has_extension() && !std::filesystem::is_directory(args.output);
    return singleFile ? args.output : args.output / name;
}

int apply(int argc, char* const* argv)
{
    ApplyArgs args;
    if (const auto error = parseApply(argc, argv, args)) {
        return fail(*error);
    }
    for (const auto& line : pfx::registry().normalize(args.settings)) {
        std::cerr << line << "\n";
    }
    if (!args.output.empty() && !args.output.has_extension()) {
        std::filesystem::create_directories(args.output);
    }
    int status = 0;
    for (const std::string& file : args.files) {
        auto loaded = pfx::load(file, {args.longEdge});
        if (!loaded) {
            std::cerr << file << ": " << loaded.error().message << "\n";
            status = 1;
            continue;
        }
        const pfx::Settings settings = args.autoFirst
                                           ? pfx::autoAdjust(loaded.value().image).overlaidWith(args.settings)
                                           : args.settings;
        auto rendered = pfx::render(std::move(loaded.value().image), settings);
        if (!rendered) {
            std::cerr << file << ": " << rendered.error().message << "\n";
            status = 1;
            continue;
        }
        if (args.lut) {
            pfx::applyLut(rendered.value(), *args.lut, static_cast<float>(args.lutMix) / 100.0F);
        }
        const auto target = outputPathFor(args, file);
        const auto saved = pfx::save(rendered.value(), target, args.save, &loaded.value().metadata);
        if (!saved) {
            std::cerr << file << ": " << saved.error().message << "\n";
            status = 1;
            continue;
        }
        std::cout << file << " -> " << target.string() << "\n";
    }
    return status;
}

} // namespace

int run(int argc, char* const* argv)
{
    pfx::setLogSink([](pfx::LogLevel level, std::string_view message) {
        if (level >= pfx::LogLevel::Warning) {
            std::cerr << "pfx: " << message << "\n";
        }
    });
    if (argc < 2) {
        std::cerr << kUsage;
        return 2;
    }
    const std::string command = argv[1];
    if (command == "ops") {
        return listOps();
    }
    if (command == "info") {
        return info({argv + 2, argv + argc});
    }
    if (command == "apply") {
        return apply(argc, argv);
    }
    if (command == "--version") {
        std::cout << "pfx " << pfx::versionString() << "\n";
        return 0;
    }
    std::cerr << kUsage;
    return command == "-h" || command == "--help" ? 0 : 2;
}

int main(int argc, char* argv[])
{
    try {
        return run(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "pfx: " << e.what() << "\n";
        return 1;
    }
}
