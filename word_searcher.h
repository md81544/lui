#pragma once

#include <filesystem>
#include <unordered_map>
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
    std::vector<std::string> regexSearch(const std::string& regexString);

private:
    std::vector<std::string> m_words1; // most common words
    std::vector<std::string> m_words2; // slightly less common words
    std::vector<std::string> m_words3; // least common words
    std::unordered_map<std::string, std::vector<std::string>> m_thesaurus;
    std::unordered_map<std::string, std::string> m_definitions;
};