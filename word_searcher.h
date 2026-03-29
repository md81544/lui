#pragma once

#include <filesystem>
#include <vector>

class WordSearcher final {
public:
    WordSearcher(
        std::filesystem::path words1File,
        std::filesystem::path words2File,
        std::filesystem::path words3File,
        std::filesystem::path thesaurusFile,
        std::filesystem::path definitionsFile);
    ~WordSearcher();

private:
    std::vector<std::string> m_words1; // most common words
    std::vector<std::string> m_words2; // slightly less common words
    std::vector<std::string> m_words3; // least common words
};