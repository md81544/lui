#include "word_searcher.h"
#include <format>
#include <fstream>
#include <stdexcept>

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

} // anonymous namespace

WordSearcher::WordSearcher(
    std::filesystem::path words1File,
    std::filesystem::path words2File,
    std::filesystem::path words3File,
    [[maybe_unused]] std::filesystem::path thesaurusFile,
    [[maybe_unused]] std::filesystem::path definitionsFile)
{
    loadFile(words1File.string(), m_words1);
    loadFile(words2File.string(), m_words2);
    loadFile(words3File.string(), m_words3);
    // TODO load thesaurus and definitions
}

WordSearcher::~WordSearcher() { }