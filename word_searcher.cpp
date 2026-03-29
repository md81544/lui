#include "word_searcher.h"
#include <format>
#include <fstream>
#include <stdexcept>

namespace {
void loadFile(std::string_view fileName, std::vector<std::string>& vec)
{
    std::ifstream file(std::string{fileName}.c_str());
    if (!file) {
        throw std::runtime_error(std::format("Could not load file {}", fileName));
    }
    std::string line;
    while (std::getline(file, line)) {
        vec.emplace_back(line);
    }
}
} // anonymous namespace

WordSearcher::WordSearcher(
    std::string_view words1FileName,
    std::string_view words2FileName,
    std::string_view words3FileName,
    [[maybe_unused]] std::string_view thesaurusFileName,
    [[maybe_unused]] std::string_view definitionsFileName)
{
    loadFile(words1FileName, m_words1);
    loadFile(words2FileName, m_words2);
    loadFile(words3FileName, m_words3);
}

WordSearcher::~WordSearcher() { }
