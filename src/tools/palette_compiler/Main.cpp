#include "genesis/style/PaletteLoader.hpp"

#include <spdlog/spdlog.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace Genesis::Tools::PaletteCompiler
{

struct Options
{
    fs::path input;
    fs::path output;
    std::vector<std::string> themes; // empty -> all
    bool emit_cpp{true};
    bool emit_glsl{true};
    bool emit_csv{true};
    bool quiet{false};
};

bool IsFlag(const std::string& arg, std::string_view flag)
{
    return arg == flag || arg == ("--" + std::string(flag));
}

std::optional<Options> ParseArguments(int argc, char** argv, std::string& error, bool& showHelp)
{
    showHelp = false;
    Options options;
    for (int index = 1; index < argc; ++index)
    {
        std::string arg = argv[index];
        if (IsFlag(arg, "input"))
        {
            if (index + 1 >= argc)
            {
                error = "--input 缺少参数";
                return std::nullopt;
            }
            options.input = fs::path(argv[++index]);
        }
        else if (IsFlag(arg, "output"))
        {
            if (index + 1 >= argc)
            {
                error = "--output 缺少参数";
                return std::nullopt;
            }
            options.output = fs::path(argv[++index]);
        }
        else if (IsFlag(arg, "theme"))
        {
            if (index + 1 >= argc)
            {
                error = "--theme 缺少参数";
                return std::nullopt;
            }
            options.themes.emplace_back(argv[++index]);
        }
        else if (IsFlag(arg, "skip-cpp"))
        {
            options.emit_cpp = false;
        }
        else if (IsFlag(arg, "skip-glsl"))
        {
            options.emit_glsl = false;
        }
        else if (IsFlag(arg, "skip-csv"))
        {
            options.emit_csv = false;
        }
        else if (IsFlag(arg, "quiet"))
        {
            options.quiet = true;
        }
        else if (IsFlag(arg, "help") || arg == "-h")
        {
            showHelp = true;
            std::ostringstream oss;
            oss << "用法: palette_compiler --input <path> --output <dir> [--theme <id> ...]\n"
                << "可选参数:\n"
                << "  --skip-cpp    不生成 C++ 头文件\n"
                << "  --skip-glsl   不生成 GLSL include\n"
                << "  --skip-csv    不生成 CSV 导出\n"
                << "  --quiet       降低日志输出\n"
                << "  --help        显示本帮助\n";
            error = oss.str();
            return std::nullopt;
        }
        else
        {
            error = "未知参数: " + arg;
            return std::nullopt;
        }
    }

    if (options.input.empty())
    {
        error = "必须指定 --input";
        return std::nullopt;
    }
    if (options.output.empty())
    {
        error = "必须指定 --output";
        return std::nullopt;
    }
    if (!options.emit_cpp && !options.emit_glsl && !options.emit_csv)
    {
        error = "所有输出类型均被禁用，至少保留一种";
        return std::nullopt;
    }

    return options;
}

std::string EscapeString(const std::string& text)
{
    std::string escaped;
    escaped.reserve(text.size());
    for (char c : text)
    {
        switch (c)
        {
        case '\\':
            escaped += "\\\\";
            break;
        case '\"':
            escaped += "\\\"";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            escaped += c;
            break;
        }
    }
    return escaped;
}

std::string EscapeCsv(const std::string& text)
{
    std::string escaped;
    escaped.reserve(text.size());
    for (char c : text)
    {
        if (c == '"')
        {
            escaped += "\"\"";
        }
        else
        {
            escaped += c;
        }
    }
    return escaped;
}

std::string FormatFloat(float value)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(6) << value << 'f';
    return oss.str();
}

int ClampChannel(float channel)
{
    const auto value = static_cast<int>(std::round(channel * 255.0f));
    return std::clamp(value, 0, 255);
}

std::string ToHex(const Genesis::Style::RgbaColor& color)
{
    std::ostringstream oss;
    oss << std::uppercase << std::hex << std::setfill('0');
    oss << std::setw(2) << ClampChannel(color.r);
    oss << std::setw(2) << ClampChannel(color.g);
    oss << std::setw(2) << ClampChannel(color.b);
    if (ClampChannel(color.a) != 255)
    {
        oss << std::setw(2) << ClampChannel(color.a);
    }
    return oss.str();
}

char NormalizeChar(char c, bool upper)
{
    if (std::isalnum(static_cast<unsigned char>(c)))
    {
        return upper ? static_cast<char>(std::toupper(static_cast<unsigned char>(c)))
                     : static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return '_';
}

std::string ToSnakeCase(const std::string& input, bool upper)
{
    std::string result;
    bool prevUnderscore = true;
    for (char c : input)
    {
        char normalized = NormalizeChar(c, upper);
        if (normalized == '_')
        {
            if (!prevUnderscore)
            {
                result.push_back('_');
                prevUnderscore = true;
            }
            continue;
        }
        result.push_back(normalized);
        prevUnderscore = false;
    }
    if (!result.empty() && result.front() == '_')
    {
        result.erase(result.begin());
    }
    if (!result.empty() && result.back() == '_')
    {
        result.pop_back();
    }
    if (result.empty())
    {
        return "palette";
    }
    if (std::isdigit(static_cast<unsigned char>(result.front())))
    {
        result.insert(result.begin(), upper ? 'P' : 'p');
    }
    return result;
}

std::string ToPascalCase(const std::string& input)
{
    std::string result;
    bool makeUpper = true;
    for (char c : input)
    {
        if (std::isalnum(static_cast<unsigned char>(c)))
        {
            char ch = makeUpper ? static_cast<char>(std::toupper(static_cast<unsigned char>(c)))
                                : static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            result.push_back(ch);
            makeUpper = false;
        }
        else
        {
            makeUpper = true;
        }
    }
    if (result.empty())
    {
        return "Palette";
    }
    if (std::isdigit(static_cast<unsigned char>(result.front())))
    {
        result.insert(result.begin(), 'P');
    }
    return result;
}

struct ThemeContext
{
    const Genesis::Style::PaletteTheme* theme{nullptr};
    std::vector<const Genesis::Style::PaletteEntry*> entries;
    std::string snake_lower;
    std::string snake_upper;
    std::string pascalName;
};

std::vector<ThemeContext> CollectThemes(const Genesis::Style::PaletteDocument& document,
                                        const Options& options,
                                        std::string& error)
{
    std::vector<ThemeContext> contexts;
    std::vector<std::string> requested = options.themes;
    if (requested.empty())
    {
        for (const auto& [id, _] : document.themes)
        {
            requested.push_back(id);
        }
    }

    for (const auto& id : requested)
    {
        const auto it = document.themes.find(id);
        if (it == document.themes.end())
        {
            error = "找不到主题 " + id;
            return {};
        }
        ThemeContext ctx;
        ctx.theme = &it->second;
        ctx.snake_lower = ToSnakeCase(it->second.id, false);
        ctx.snake_upper = ToSnakeCase(it->second.id, true);
        ctx.pascalName = ToPascalCase(it->second.id);

        ctx.entries.reserve(it->second.entries.size());
        for (const auto& [token, entry] : it->second.entries)
        {
            ctx.entries.push_back(&entry);
        }
        std::sort(ctx.entries.begin(), ctx.entries.end(), [](const auto* lhs, const auto* rhs) {
            return lhs->token < rhs->token;
        });
        contexts.push_back(std::move(ctx));
    }

    return contexts;
}

bool WriteFile(const fs::path& path, const std::string& content)
{
    std::ofstream output(path, std::ios::binary);
    if (!output.is_open())
    {
        return false;
    }
    output.write(content.data(), static_cast<std::streamsize>(content.size()));
    return static_cast<bool>(output);
}

std::string GenerateCppHeader(const std::vector<ThemeContext>& contexts)
{
    std::ostringstream oss;
    oss << "// 自动生成文件，请勿手动编辑\n";
    oss << "#pragma once\n\n";
    oss << "#include <array>\n";
    oss << "#include <cstddef>\n\n";
    oss << "namespace Genesis::Style::Generated\n{\n\n";
    oss << "struct ColorSample\n{\n";
    oss << "    float srgb[4];\n";
    oss << "    float linear[4];\n";
    oss << "};\n\n";
    oss << "struct PaletteEntry\n{\n";
    oss << "    const char* token;\n";
    oss << "    const char* category;\n";
    oss << "    const char* description;\n";
    oss << "    ColorSample color;\n";
    oss << "};\n\n";
    oss << "struct ThemeInfo\n{\n";
    oss << "    const char* id;\n";
    oss << "    const char* label;\n";
    oss << "    const char* description;\n";
    oss << "    std::size_t count;\n";
    oss << "    const PaletteEntry* entries;\n";
    oss << "};\n\n";

    for (const auto& ctx : contexts)
    {
        oss << "inline constexpr std::array<PaletteEntry, " << ctx.entries.size() << "> " << ctx.pascalName << "Entries = {\n";
        for (const auto* entry : ctx.entries)
        {
            oss << "    {\""
                << EscapeString(entry->token) << "\", \""
                << EscapeString(entry->category) << "\", \""
                << EscapeString(entry->description) << "\", {{\n"
                << "        {" << FormatFloat(entry->sample.srgb.r) << ", "
                << FormatFloat(entry->sample.srgb.g) << ", "
                << FormatFloat(entry->sample.srgb.b) << ", "
                << FormatFloat(entry->sample.srgb.a) << "},\n"
                << "        {" << FormatFloat(entry->sample.linear.r) << ", "
                << FormatFloat(entry->sample.linear.g) << ", "
                << FormatFloat(entry->sample.linear.b) << ", "
                << FormatFloat(entry->sample.linear.a) << "}\n"
                << "    }}},\n";
        }
        oss << "};\n\n";
    }

    oss << "inline constexpr std::array<ThemeInfo, " << contexts.size() << "> Themes = {\n";
    for (const auto& ctx : contexts)
    {
        oss << "    {\""
            << EscapeString(ctx.theme->id) << "\", \""
            << EscapeString(ctx.theme->label) << "\", \""
            << EscapeString(ctx.theme->description) << "\", "
            << ctx.pascalName << "Entries.size(), "
            << ctx.pascalName << "Entries.data()},\n";
    }
    oss << "};\n\n";
    oss << "} // namespace Genesis::Style::Generated\n";
    return oss.str();
}

std::string GenerateGlslInclude(const std::vector<ThemeContext>& contexts)
{
    std::ostringstream oss;
    oss << "// 自动生成文件，请勿手动编辑\n";
    oss << "#ifndef GENESIS_STYLE_PALETTE_GENERATED_GLSL\n";
    oss << "#define GENESIS_STYLE_PALETTE_GENERATED_GLSL\n\n";
    oss << "#define GENESIS_PALETTE_THEME_COUNT " << contexts.size() << "\n\n";

    for (const auto& ctx : contexts)
    {
        oss << "// Theme: " << ctx.theme->id << " (" << ctx.theme->label << ")\n";
        oss << "#define GENESIS_THEME_" << ctx.snake_upper << "_COUNT " << ctx.entries.size() << "\n";
        for (const auto* entry : ctx.entries)
        {
            std::string tokenMacro = ToSnakeCase(entry->token, true);
            oss << "#define GENESIS_THEME_" << ctx.snake_upper << "_" << tokenMacro
                << " vec4("
                << std::fixed << std::setprecision(6)
                << entry->sample.srgb.r << ", "
                << entry->sample.srgb.g << ", "
                << entry->sample.srgb.b << ", "
                << entry->sample.srgb.a << ")\n";
        }
        oss << "\n";
    }

    oss << "#endif // GENESIS_STYLE_PALETTE_GENERATED_GLSL\n";
    return oss.str();
}

bool GenerateCsvExports(const std::vector<ThemeContext>& contexts, const fs::path& outDir, std::vector<fs::path>& files)
{
    for (const auto& ctx : contexts)
    {
        const fs::path filePath = outDir / ("palette_" + ctx.snake_lower + ".csv");
        std::ofstream csv(filePath);
        if (!csv.is_open())
        {
            spdlog::error("无法写入 CSV: {}", filePath.string());
            return false;
        }

        csv << "token,category,description,srgb_hex,srgb_r,srgb_g,srgb_b,srgb_a,linear_r,linear_g,linear_b,linear_a\n";
        for (const auto* entry : ctx.entries)
        {
            csv << '"' << EscapeCsv(entry->token) << "\",";
            csv << '"' << EscapeCsv(entry->category) << "\",";
            csv << '"' << EscapeCsv(entry->description) << "\",";
            csv << "\"#" << ToHex(entry->sample.srgb) << "\",";
            csv << entry->sample.srgb.r << ','
                << entry->sample.srgb.g << ','
                << entry->sample.srgb.b << ','
                << entry->sample.srgb.a << ','
                << entry->sample.linear.r << ','
                << entry->sample.linear.g << ','
                << entry->sample.linear.b << ','
                << entry->sample.linear.a << '\n';
        }
        files.push_back(filePath);
    }
    return true;
}

json BuildIndexJson(const Genesis::Style::PaletteDocument& document,
                    const std::vector<ThemeContext>& contexts,
                    bool emitCpp,
                    bool emitGlsl,
                    bool emitCsv)
{
    json root;
    root["version"] = document.version;
    root["default_theme"] = document.default_theme_id;

    json themes = json::array();
    for (const auto& ctx : contexts)
    {
        json node;
        node["id"] = ctx.theme->id;
        node["label"] = ctx.theme->label;
        node["description"] = ctx.theme->description;
        node["count"] = ctx.entries.size();
        json files = json::object();
        if (emitCpp)
        {
            files["cpp_header"] = "PaletteGenerated.hpp";
        }
        if (emitGlsl)
        {
            files["glsl_include"] = "PaletteGenerated.glsl";
        }
        if (emitCsv)
        {
            files["csv"] = "palette_" + ctx.snake_lower + ".csv";
        }
        node["files"] = files;
        themes.push_back(node);
    }
    root["themes"] = themes;
    return root;
}

int Run(int argc, char** argv)
{
    std::string error;
    bool showHelp = false;
    const auto optionsOpt = ParseArguments(argc, argv, error, showHelp);
    if (!optionsOpt.has_value())
    {
        if (showHelp)
        {
            std::printf("%s", error.c_str());
            return 0;
        }
        if (!error.empty())
        {
            std::fprintf(stderr, "%s\n", error.c_str());
        }
        return 1;
    }
    const Options options = *optionsOpt;

    if (options.quiet)
    {
        spdlog::set_level(spdlog::level::warn);
    }
    spdlog::set_pattern("[palette] %v");

    Genesis::Style::PaletteDocument document;
    const auto loadResult = Genesis::Style::LoadPaletteDocument(options.input, document);
    if (!loadResult.ok)
    {
        spdlog::error("加载调色板失败: {} ({})", options.input.string(), loadResult.error);
        return 1;
    }

    std::string collectError;
    const auto contexts = CollectThemes(document, options, collectError);
    if (!collectError.empty())
    {
        spdlog::error("{}", collectError);
        return 1;
    }
    if (contexts.empty())
    {
        spdlog::error("未找到任何主题用于生成");
        return 1;
    }

    fs::create_directories(options.output);

    if (options.emit_cpp)
    {
        const auto headerContent = GenerateCppHeader(contexts);
        const auto headerPath = options.output / "PaletteGenerated.hpp";
        if (!WriteFile(headerPath, headerContent))
        {
            spdlog::error("写入 C++ 头文件失败: {}", headerPath.string());
            return 1;
        }
        spdlog::info("生成 {}", headerPath.string());
    }

    if (options.emit_glsl)
    {
        const auto glslContent = GenerateGlslInclude(contexts);
        const auto glslPath = options.output / "PaletteGenerated.glsl";
        if (!WriteFile(glslPath, glslContent))
        {
            spdlog::error("写入 GLSL include 失败: {}", glslPath.string());
            return 1;
        }
        spdlog::info("生成 {}", glslPath.string());
    }

    std::vector<fs::path> csvFiles;
    if (options.emit_csv)
    {
        if (!GenerateCsvExports(contexts, options.output, csvFiles))
        {
            return 1;
        }
        for (const auto& file : csvFiles)
        {
            spdlog::info("生成 {}", file.string());
        }
    }

    const auto indexJson = BuildIndexJson(document, contexts, options.emit_cpp, options.emit_glsl, options.emit_csv);
    const auto indexPath = options.output / "PaletteIndex.json";
    if (!WriteFile(indexPath, indexJson.dump(2)))
    {
        spdlog::error("写入索引文件失败: {}", indexPath.string());
        return 1;
    }

    spdlog::info("Palette 编译完成");
    return 0;
}

} // namespace Genesis::Tools::PaletteCompiler

int main(int argc, char** argv)
{
    return Genesis::Tools::PaletteCompiler::Run(argc, argv);
}
