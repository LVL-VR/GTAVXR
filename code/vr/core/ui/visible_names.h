#pragma once
#include <string>
namespace menyoo_vr
{
inline char foldedAscii(char c)
{
    return c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c;
}
inline size_t findFolded(const std::string& text, const char* word, size_t start = 0)
{
    const std::string needle(word);
    if (needle.empty())
        return std::string::npos;
    for (size_t i = start; i + needle.size() <= text.size(); ++i)
    {
        bool equal = true;
        for (size_t j = 0; j < needle.size(); ++j)
            if (foldedAscii(text[i + j]) != foldedAscii(needle[j]))
            {
                equal = false;
                break;
            }
        if (equal)
            return i;
    }
    return std::string::npos;
}
inline void replaceFolded(std::string& text, const char* oldText, const char* newText)
{
    const size_t oldSize = std::string(oldText).size(), newSize = std::string(newText).size();
    if (!oldSize)
        return;
    for (size_t pos = findFolded(text, oldText); pos != std::string::npos;
         pos = findFolded(text, oldText, pos + newSize))
        text.replace(pos, oldSize, newText);
}
// Presentation ONLY. Never run this on source/license notices, serialized keys,
// filenames, upstream provenance or stable submenu/feature identifiers.
inline std::string visibleName(const std::string& original, const char* vehicleWorkshopName)
{
    std::string text = original;
    if (findFolded(text, "The People Behind Menyoo") == 0 &&
        text.size() == std::string("The People Behind Menyoo").size())
        return "Credits";
    // Branding must not falsely attribute upstream authors/donations to GTAVXR.
    replaceFolded(text, "Support Menyoo on Patreon", "Support the original trainer's maintainers");
    replaceFolded(text, "Menyoo Customs", vehicleWorkshopName);
    if (findFolded(text, "menyoo") == 0 && text.size() == 6)
        return "Trainer";
    replaceFolded(text, "Menyoo", "GTAVXR");
    return text;
}
}
