#include "word_searcher.h"
#include <algorithm>
#include <format>
#include <fstream>
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
        bool first = true;
        std::size_t pos = 0;
        std::string key;
        std::vector<std::string> values;
        while (pos < line.size()) {
            auto const next_pos = line.find(",", pos);
            if (first) {
                key = line.substr(pos, next_pos - pos);
                first = false;
            } else {
                values.push_back(line.substr(pos, next_pos - pos));
            }
            pos = next_pos == std::string::npos ? line.size() : next_pos + 1;
        }
        map.insert({ key, values });
    }
}

void loadDefinitions(std::string_view fileName, std::unordered_map<std::string, std::string>& map)
{
    // File structure is a word, a pipe delimiter, then the definition.
    std::ifstream file(std::string { fileName }.c_str());
    if (!file) {
        throw std::runtime_error(std::format("Could not load file {}", fileName));
    }
    std::string line;
    while (std::getline(file, line)) {
        bool first = true;
        std::size_t pos = 0;
        std::string key;
        std::string value;
        while (pos < line.size()) {
            auto const next_pos = line.find(",", pos);
            if (first) {
                key = line.substr(pos, next_pos - pos);
                first = false;
            } else {
                value = line.substr(pos, next_pos - pos);
            }
            pos = next_pos == std::string::npos ? line.size() : next_pos + 1;
        }
        map.insert({ key, value });
    }
}

} // anonymous namespace

WordSearcher::WordSearcher(
    std::filesystem::path words1File,
    std::filesystem::path words2File,
    std::filesystem::path words3File,
    std::filesystem::path thesaurusFile,
    std::filesystem::path definitionsFile)
{
    loadFile(words1File.string(), m_words1);
    loadFile(words2File.string(), m_words2);
    loadFile(words3File.string(), m_words3);
    loadThesaurus(thesaurusFile.string(), m_thesaurus);
    loadDefinitions(definitionsFile.string(), m_definitions);
}

WordSearcher::~WordSearcher() { }

std::vector<std::string> WordSearcher::regexSearch(const std::string& regexString)
{
    std::regex regex(regexString);
    std::vector<std::string> result;
    std::copy_if(
        m_words3.begin(),
        m_words3.end(),
        std::back_inserter(result),
        [&regex](const std::string& w) { return std::regex_match(w, regex); });
    return result;
}