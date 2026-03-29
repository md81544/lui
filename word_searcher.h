#pragma once

#include <string_view>
#include <vector>

class WordSearcher final {
public:
    WordSearcher(
        std::string_view words1FileName,
        std::string_view words2FileName,
        std::string_view words3FileName,
        std::string_view thesaurusFileName,
        std::string_view definitionsFileName);
    ~WordSearcher();

private:
    std::vector<std::string> m_words1; // most common words
    std::vector<std::string> m_words2; // slightly less common words
    std::vector<std::string> m_words3; // least common words
};