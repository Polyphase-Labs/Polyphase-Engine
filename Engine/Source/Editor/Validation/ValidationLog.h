#pragma once

#if EDITOR

#include <cstdint>
#include <string>
#include <vector>

namespace Validation
{
    enum class Severity : uint8_t
    {
        Info,
        Warning,
        Error,
    };

    struct Message
    {
        Severity severity = Severity::Info;
        std::string code;
        std::string message;
        std::string details;
    };

    struct Report
    {
        bool valid = false;
        std::vector<Message> messages;
        std::string summary;
        std::string contextHeader;
    };

    bool HasError(const Report& report);
    bool HasWarning(const Report& report);
    void AddInfo(Report& report, const char* code, std::string message, std::string details = {});
    void AddWarning(Report& report, const char* code, std::string message, std::string details = {});
    void AddError(Report& report, const char* code, std::string message, std::string details = {});

    const char* SeverityLabel(Severity s);

    std::string FormatReportPlainText(const Report& report);
    std::string FormatReportMarkdown(const Report& report);

    // Writes the plain-text report to <projectDir>/Logs/<subfolder>/<timestamp>.log.
    // Creates the directory tree if it does not yet exist.
    // outPath receives the absolute path of the written file.
    bool SaveReportToLogsFolder(const Report& report,
                                const std::string& projectDir,
                                const char* subfolder,
                                std::string& outPath);

    void CopyReportToClipboard(const Report& report);
}

#endif // EDITOR
