#pragma once

#include <filesystem>
#include <unordered_map>
#include <vector>

namespace wordSearcher {

class WordSearcher final {
public:
    WordSearcher(
        std::filesystem::path wordsFile,
        std::filesystem::path thesaurusFile,
        std::filesystem::path definitionsFile,
        std::filesystem::path phrasesFile);
    ~WordSearcher();
    // Doesn't make sense to copy this class:
    WordSearcher(const WordSearcher&) = delete;
    WordSearcher& operator=(const WordSearcher&) = delete;

    std::vector<std::string> regexSearch(const std::string& regexString);
    std::vector<std::string> definitions(const std::vector<std::string>& words);

private:
    std::vector<std::string> m_words;
    std::unordered_map<std::string, std::vector<std::string>> m_thesaurus;
    std::unordered_map<std::string, std::vector<std::string>> m_definitions;
};

} // namespace wordSearcher
