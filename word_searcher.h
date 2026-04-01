#pragma once

#include <filesystem>
#include <unordered_map>
#include <vector>

namespace wordSearcher {

class WordSearcher final {
public:
    WordSearcher(
        std::filesystem::path words1File,
        std::filesystem::path words2File,
        std::filesystem::path words3File,
        std::filesystem::path thesaurusFile,
        std::filesystem::path definitionsFile);
    ~WordSearcher();
    // Doesn't make sense to copy this class:
    WordSearcher(const WordSearcher&) = delete;
    WordSearcher& operator=(const WordSearcher&) = delete;

    std::vector<std::string> regexSearch(const std::string& regexString);

private:
    // Note, m_words{n} is (or should be) a subset of m_words{n+1}
    // TODO the word list being used should be selectable (or
    // just use m_words3 everywhere)
    std::vector<std::string> m_words1; // most common words
    std::vector<std::string> m_words2; // slightly less common words
    std::vector<std::string> m_words3; // least common words
    std::unordered_map<std::string, std::vector<std::string>> m_thesaurus;
    std::unordered_map<std::string, std::string> m_definitions;
};

} // namespace wordSearcher
