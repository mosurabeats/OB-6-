#include "ob6/params.hpp"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace ob6 {

namespace {

const std::array<ParamInfo, kNumParams> kParams = {{
#define OB6_INFO(id, key, mn, mx, df, kind, cc, desc) \
    {P::id, key, static_cast<float>(mn), static_cast<float>(mx), static_cast<float>(df), Kind::kind, cc, desc},
    OB6_PARAMS(OB6_INFO)
#undef OB6_INFO
}};

std::string_view trim(std::string_view s) {
    const char* ws = " \t\r\n";
    const size_t b = s.find_first_not_of(ws);
    if (b == std::string_view::npos) return {};
    const size_t e = s.find_last_not_of(ws);
    return s.substr(b, e - b + 1);
}

std::string lower(std::string_view s) {
    std::string r(s);
    for (char& c : r) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return r;
}

// Labels for stepped params whose description is a `|` separated list.
int labelIndex(const ParamInfo& info, const std::string& word) {
    std::string_view d = info.description;
    if (d.find('|') == std::string_view::npos) return -1;
    int i = 0;
    while (true) {
        const size_t bar = d.find('|');
        if (lower(trim(d.substr(0, bar))) == word) return i;
        if (bar == std::string_view::npos) return -1;
        d.remove_prefix(bar + 1);
        ++i;
    }
}

bool parseValue(const ParamInfo& info, std::string_view raw, float& out) {
    const std::string word = lower(raw);
    if (info.kind == Kind::Toggle) {
        if (word == "on" || word == "true" || word == "yes") return out = 1.0f, true;
        if (word == "off" || word == "false" || word == "no") return out = 0.0f, true;
    }
    if (const int idx = labelIndex(info, word); idx >= 0) return out = static_cast<float>(idx), true;
    char* end = nullptr;
    const std::string s(raw);
    const double v = std::strtod(s.c_str(), &end);
    if (end == s.c_str() || *end != '\0' || !std::isfinite(v)) return false;
    out = static_cast<float>(v);
    return true;
}

}  // namespace

const std::array<ParamInfo, kNumParams>& allParams() { return kParams; }

const ParamInfo& paramInfo(P p) { return kParams[static_cast<int>(p)]; }

const ParamInfo* findParam(std::string_view key) {
    for (const auto& info : kParams)
        if (key == info.key) return &info;
    return nullptr;
}

const ParamInfo* findParamByCC(int cc) {
    for (const auto& info : kParams)
        if (info.cc == cc) return &info;
    return nullptr;
}

Patch::Patch() {
    for (const auto& info : kParams) values[static_cast<int>(info.id)] = info.def;
}

void Patch::set(P p, float v) {
    const ParamInfo& info = paramInfo(p);
    v = std::fmin(std::fmax(v, info.min), info.max);
    if (info.kind == Kind::Toggle) v = v >= 0.5f ? 1.0f : 0.0f;
    if (info.kind == Kind::Stepped) v = std::round(v);
    values[static_cast<int>(p)] = v;
}

bool parsePatch(std::string_view text, Patch& out, std::vector<std::string>* errors) {
    bool ok = true;
    auto fail = [&](int line, const std::string& msg) {
        ok = false;
        if (errors) errors->push_back("line " + std::to_string(line) + ": " + msg);
    };

    int lineNo = 0;
    while (!text.empty()) {
        ++lineNo;
        const size_t nl = text.find('\n');
        std::string_view line = text.substr(0, nl);
        text = nl == std::string_view::npos ? std::string_view{} : text.substr(nl + 1);

        if (const size_t hash = line.find('#'); hash != std::string_view::npos) line = line.substr(0, hash);
        line = trim(line);
        if (line.empty()) continue;

        const size_t eq = line.find('=');
        if (eq == std::string_view::npos) {
            fail(lineNo, "expected `key = value`");
            continue;
        }
        const std::string_view key = trim(line.substr(0, eq));
        const std::string_view value = trim(line.substr(eq + 1));

        if (key == "name") {
            out.name = std::string(value);
            continue;
        }
        const ParamInfo* info = findParam(key);
        if (!info) {
            fail(lineNo, "unknown parameter `" + std::string(key) + "`");
            continue;
        }
        float v = 0.0f;
        if (!parseValue(*info, value, v)) {
            fail(lineNo, "bad value `" + std::string(value) + "` for " + info->key);
            continue;
        }
        if (v < info->min || v > info->max) {
            fail(lineNo, std::string(info->key) + " out of range");
            continue;
        }
        out.set(info->id, v);
    }
    return ok;
}

bool loadPatchFile(const std::string& path, Patch& out, std::vector<std::string>* errors) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        if (errors) errors->push_back("cannot open " + path);
        return false;
    }
    std::stringstream ss;
    ss << in.rdbuf();
    return parsePatch(ss.str(), out, errors);
}

std::string serializePatch(const Patch& patch) {
    std::string s = "name = " + patch.name + "\n";
    char buf[64];
    for (const auto& info : kParams) {
        const float v = patch[info.id];
        if (info.kind == Kind::Continuous)
            std::snprintf(buf, sizeof buf, "%.4g", v);
        else
            std::snprintf(buf, sizeof buf, "%d", static_cast<int>(std::lround(v)));
        s += std::string(info.key) + " = " + buf + "\n";
    }
    return s;
}

}  // namespace ob6
