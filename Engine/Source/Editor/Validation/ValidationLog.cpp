#if EDITOR

#include "Editor/Validation/ValidationLog.h"

#include "System/System.h"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <sstream>

namespace Validation
{
    const char* SeverityLabel(Severity s)
    {
        switch (s)
        {
            case Severity::Info:    return "INFO";
            case Severity::Warning: return "WARNING";
            case Severity::Error:   return "ERROR";
        }
        return "INFO";
    }

    bool HasError(const Report& report)
    {
        for (const Message& m : report.messages)
        {
            if (m.severity == Severity::Error)
                return true;
        }
        return false;
    }

    bool HasWarning(const Report& report)
    {
        for (const Message& m : report.messages)
        {
            if (m.severity == Severity::Warning)
                return true;
        }
        return false;
    }

    static void AddMessage(Report& report, Severity sev, const char* code,
                           std::string message, std::string details)
    {
        Message msg;
        msg.severity = sev;
        msg.code = code != nullptr ? code : "";
        msg.message = std::move(message);
        msg.details = std::move(details);
        report.messages.push_back(std::move(msg));
    }

    void AddInfo(Report& report, const char* code, std::string message, std::string details)
    {
        AddMessage(report, Severity::Info, code, std::move(message), std::move(details));
    }

    void AddWarning(Report& report, const char* code, std::string message, std::string details)
    {
        AddMessage(report, Severity::Warning, code, std::move(message), std::move(details));
    }

    void AddError(Report& report, const char* code, std::string message, std::string details)
    {
        AddMessage(report, Severity::Error, code, std::move(message), std::move(details));
    }

    std::string FormatReportPlainText(const Report& report)
    {
        std::ostringstream ss;
        ss << "Polyphase Validation Report\n";
        ss << "===========================\n";
        if (!report.summary.empty())
            ss << "Summary: " << report.summary << "\n";
        ss << "Valid:   " << (report.valid ? "yes" : "no") << "\n";
        if (!report.contextHeader.empty())
            ss << report.contextHeader;
        ss << "\n";

        if (report.messages.empty())
        {
            ss << "(no messages)\n";
            return ss.str();
        }

        for (size_t i = 0; i < report.messages.size(); ++i)
        {
            const Message& m = report.messages[i];
            ss << "[" << SeverityLabel(m.severity) << "] " << m.code;
            if (!m.message.empty())
                ss << ": " << m.message;
            ss << "\n";
            if (!m.details.empty())
            {
                std::istringstream lines(m.details);
                std::string line;
                while (std::getline(lines, line))
                    ss << "    " << line << "\n";
            }
        }
        return ss.str();
    }

    std::string FormatReportMarkdown(const Report& report)
    {
        std::ostringstream ss;
        ss << "# Polyphase Validation Report\n\n";
        if (!report.summary.empty())
            ss << "**Summary:** " << report.summary << "\n\n";
        ss << "**Valid:** " << (report.valid ? "yes" : "no") << "\n\n";
        if (!report.contextHeader.empty())
        {
            ss << "```\n" << report.contextHeader;
            if (report.contextHeader.back() != '\n')
                ss << "\n";
            ss << "```\n\n";
        }

        if (report.messages.empty())
        {
            ss << "_no messages_\n";
            return ss.str();
        }

        for (const Message& m : report.messages)
        {
            ss << "- **" << SeverityLabel(m.severity) << "** `" << m.code << "`";
            if (!m.message.empty())
                ss << " — " << m.message;
            ss << "\n";
            if (!m.details.empty())
            {
                std::istringstream lines(m.details);
                std::string line;
                while (std::getline(lines, line))
                    ss << "    - " << line << "\n";
            }
        }
        return ss.str();
    }

    // Builds <projectDir>/Logs/<subfolder>/, creating each segment lazily.
    static bool EnsureLogsSubfolder(const std::string& projectDir,
                                    const char* subfolder,
                                    std::string& outFolder)
    {
        if (projectDir.empty() || subfolder == nullptr || subfolder[0] == '\0')
            return false;

        std::string base = projectDir;
        if (!base.empty() && base.back() != '/' && base.back() != '\\')
            base += '/';

        std::string logsRoot = base + "Logs";
        SYS_CreateDirectory(logsRoot.c_str());

        std::string subfolderPath = logsRoot + "/" + subfolder;
        SYS_CreateDirectory(subfolderPath.c_str());

        outFolder = subfolderPath;
        return true;
    }

    static std::string FormatTimestampFilename()
    {
        std::time_t now = std::time(nullptr);
        std::tm tmBuf{};
#ifdef _WIN32
        localtime_s(&tmBuf, &now);
#else
        localtime_r(&now, &tmBuf);
#endif
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y%m%d-%H%M%S", &tmBuf);
        return std::string(buf);
    }

    bool SaveReportToLogsFolder(const Report& report,
                                const std::string& projectDir,
                                const char* subfolder,
                                std::string& outPath)
    {
        std::string folder;
        if (!EnsureLogsSubfolder(projectDir, subfolder, folder))
            return false;

        std::string path = folder + "/" + FormatTimestampFilename() + ".log";

        std::ofstream out(path, std::ios::binary);
        if (!out.is_open())
            return false;
        out << FormatReportPlainText(report);
        if (!out.good())
            return false;
        out.close();

        outPath = path;
        return true;
    }

    void CopyReportToClipboard(const Report& report)
    {
        SYS_SetClipboardText(FormatReportPlainText(report));
    }
}

#endif // EDITOR
