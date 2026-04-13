#include "word_searcher.h"
#include <algorithm>
#include <cassert>
#include <format>
#include <fstream>
#include <ranges>
#include <regex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

void loadFile(std::string_view fileName, std::vector<std::string>& vec)
{
    std::ifstream file(std::string { fileName }.c_str());
    if (!file) {
        throw std::runtime_error(std::format("Could not load file {}", fileName));
    }
    // A large reservation to minimise early re-allocations
    // as the vector grows
    vec.reserve(0xFFFF);
    std::string line;
    while (std::getline(file, line)) {
        vec.emplace_back(line);
    }
}

void loadThesaurus(
    std::string_view fileName,
    std::unordered_map<std::string, std::vector<std::string>>& map)
{
    // File structure is a line of comma-separated words where the
    // first one is the key and the rest populate the vec inside the map
    std::ifstream file(std::string { fileName }.c_str());
    if (!file) {
        throw std::runtime_error(std::format("Could not load file {}", fileName));
    }
    std::string line;
    while (std::getline(file, line)) {
        // Note: not using std::ranges::to below because it's not yet supported
        // on some of my machines (Linux Mint Zena) without using a different libstdc++
        std::vector<std::string> tokens;
        for (auto subrange : line | std::views::split(',')) {
            tokens.emplace_back(subrange.begin(), subrange.end());
        }
        if (tokens.size() > 0) {
            map.emplace(
                tokens.front(), std::vector<std::string>(std::next(tokens.begin()), tokens.end()));
        }
    }
}

void loadDefinitions(
    std::string_view fileName,
    std::unordered_map<std::string, std::vector<std::string>>& map)
{
    // File structure is a word, a pipe delimiter, then the definition.
    // Note that there may be multiple definitions for the same word.
    std::ifstream file(std::string { fileName }.c_str());
    if (!file) {
        throw std::runtime_error(std::format("Could not load file {}", fileName));
    }
    std::string line;
    while (std::getline(file, line)) {
        // Note: not using std::ranges::to below because it's not yet supported
        // on some of my machines (Linux Mint Zena) without using a different libstdc++
        std::vector<std::string> tokens;
        for (auto subrange : line | std::ranges::views::split('|')) {
            tokens.emplace_back(subrange.begin(), subrange.end());
        }
        assert(tokens.size() == 2);
        if (auto it = map.find(tokens[0]); it != map.end()) {
            it->second.emplace_back(tokens[1]);
        } else {
            map.emplace(tokens[0], std::vector<std::string> { tokens[1] });
        }
    }
}

} // anonymous namespace

namespace wordSearcher {

WordSearcher::WordSearcher(
    std::filesystem::path wordsFile,
    std::filesystem::path thesaurusFile,
    std::filesystem::path definitionsFile,
    std::filesystem::path phrasesFile)
{
    loadFile(wordsFile.string(), m_words);
    loadThesaurus(thesaurusFile.string(), m_thesaurus);
    loadDefinitions(definitionsFile.string(), m_definitions);
    loadFile(phrasesFile.string(), m_words);
}

WordSearcher::~WordSearcher() { }

std::vector<std::string> WordSearcher::regexSearch(const std::string& regexString)
{
    const std::regex regex(regexString);
    std::vector<std::string> result;
    std::copy_if(
        m_words.begin(), m_words.end(), std::back_inserter(result), [&regex](const std::string& w) {
            return std::regex_match(w, regex);
        });
    return result;
}

std::vector<std::string> WordSearcher::definitions(const std::vector<std::string>& words)
{
    std::vector<std::string> rc;
    for (const auto& w : words) {
        std::string definition { w };
        auto d = m_definitions.find(w);
        if (d != m_definitions.end()) {
            definition.append(" : ");
            bool bFirst = true;
            for(const auto& def : d->second) {
                if (! bFirst) {
                    definition.append(" OR ");
                }
                definition.append(def);
                bFirst = false;
            }
        } else {
            definition.append(" : ---");
        }
        rc.emplace_back(definition);
    }
    return rc;
}

std::vector<std::string> WordSearcher::thesaurus(std::string_view word)
{
    std::string w { word };
    std::vector<std::string> vec;
    auto it = m_thesaurus.find(w);
    if (it != m_thesaurus.end()) {
        for (const auto& w : it->second) {
            vec.emplace_back(w);
        }
    }
    std::sort(vec.begin(), vec.end());
    return vec;
}

} // namespace wordSearcher
