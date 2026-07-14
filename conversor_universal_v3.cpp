// ============================================================
// CONVERSOR UNIVERSAL V3 - C++ Win32 GUI (CORREGIDO)
// Compilar: cl /EHsc /std:c++17 /DUNICODE /D_UNICODE conversor_universal_v3_fixed.cpp user32.lib comctl32.lib shell32.lib shlwapi.lib ole32.lib
// ============================================================

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <regex>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <map>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ole32.lib")

// ============================================================
// 1. ESTRUCTURAS DE DATOS
// ============================================================
namespace fs = std::filesystem;

struct SRow {
    std::vector<std::string> cols;
};

struct SData {
    bool isStructured = false;
    std::vector<SRow> rows;
    std::string plainText;
    int maxCols = 0;
};

// ============================================================
// 2. FUNCIONES DE ARCHIVOS Y ESCAPE
// ============================================================
std::string ReadFileToString(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return "";
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

bool WriteStringToFile(const std::string& path, const std::string& content) {
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) return false;
    file.write(content.c_str(), content.size());
    return true;
}

std::string EscapeHtml(const std::string& text) {
    std::string result;
    for (char c : text) {
        if (c == '&') result += "&amp;";
        else if (c == '<') result += "&lt;";
        else if (c == '>') result += "&gt;";
        else if (c == '"') result += "&quot;";
        else if (c == '\n') result += "<br>";
        else result += c;
    }
    return result;
}

std::string EscapePdfText(const std::string& text) {
    std::string result;
    for (char c : text) {
        if (c == '(' || c == ')' || c == '\\') result += '\\';
        result += c;
    }
    return result;
}

// ============================================================
// 3. DETECCIÓN DE DATOS ESTRUCTURADOS
// ============================================================
std::string ExtractBetween(const std::string& str, const std::string& startTag, const std::string& endTag, size_t& pos) {
    size_t start = str.find(startTag, pos);
    if (start == std::string::npos) return "";
    start += startTag.length();
    size_t end = str.find(endTag, start);
    if (end == std::string::npos) return "";
    pos = end + endTag.length();
    return str.substr(start, end - start);
}

std::string CleanCell(const std::string& cell) {
    std::string result = cell;
    std::regex tag_re("<[^>]*>");
    result = std::regex_replace(result, tag_re, "");
    while (!result.empty() && (result.front() == ' ' || result.front() == '\t' || result.front() == '\r' || result.front() == '\n'))
        result.erase(0, 1);
    while (!result.empty() && (result.back() == ' ' || result.back() == '\t' || result.back() == '\r' || result.back() == '\n'))
        result.pop_back();
    return result;
}

SData ParseHtmlTable(const std::string& html) {
    SData data;
    data.isStructured = false;
    size_t tablePos = html.find("<table");
    if (tablePos == std::string::npos) {
        data.plainText = html;
        return data;
    }
    size_t pos = tablePos;
    while (true) {
        std::string row = ExtractBetween(html, "<tr", "</tr>", pos);
        if (row.empty()) break;
        SRow r;
        size_t cellPos = 0;
        while (true) {
            size_t tdStart = row.find("<td", cellPos);
            size_t thStart = row.find("<th", cellPos);
            size_t start = std::string::npos;
            if (tdStart != std::string::npos && (thStart == std::string::npos || tdStart < thStart))
                start = tdStart;
            else if (thStart != std::string::npos)
                start = thStart;
            else
                break;
            size_t tagEnd = row.find(">", start);
            if (tagEnd == std::string::npos) break;
            size_t cellEnd = row.find("</td>", tagEnd);
            if (cellEnd == std::string::npos) {
                cellEnd = row.find("</th>", tagEnd);
                if (cellEnd == std::string::npos) break;
            }
            std::string cellContent = row.substr(tagEnd + 1, cellEnd - tagEnd - 1);
            r.cols.push_back(CleanCell(cellContent));
            cellPos = cellEnd + 5;
        }
        if (!r.cols.empty()) {
            data.rows.push_back(r);
            if ((int)r.cols.size() > data.maxCols) data.maxCols = (int)r.cols.size();
        }
    }
    if (!data.rows.empty()) {
        data.isStructured = true;
        for (auto& r : data.rows)
            while ((int)r.cols.size() < data.maxCols) r.cols.push_back("");
    } else {
        data.plainText = html;
    }
    return data;
}

SData ParseCsvTsv(const std::string& text, char delimiter = '\t') {
    SData data;
    data.isStructured = false;
    std::istringstream iss(text);
    std::string line;
    int lineCount = 0, maxCols = 0;
    while (std::getline(iss, line)) {
        if (line.find_first_not_of(" \t\r\n") == std::string::npos) continue;
        if (line.find(delimiter) == std::string::npos) {
            if (lineCount > 1) { data.plainText = text; return data; }
            if (delimiter == '\t' && line.find(',') != std::string::npos)
                return ParseCsvTsv(text, ',');
            data.plainText = text;
            return data;
        }
        std::vector<std::string> fields;
        std::string field;
        bool inQuotes = false;
        for (char c : line) {
            if (c == '"' && !inQuotes) { inQuotes = true; continue; }
            if (c == '"' && inQuotes) { inQuotes = false; continue; }
            if (c == delimiter && !inQuotes) { fields.push_back(field); field.clear(); }
            else field += c;
        }
        fields.push_back(field);
        SRow r;
        for (const auto& f : fields) {
            std::string clean = f;
            while (!clean.empty() && (clean.front() == ' ' || clean.front() == '\t' || clean.front() == '\r' || clean.front() == '\n'))
                clean.erase(0, 1);
            while (!clean.empty() && (clean.back() == ' ' || clean.back() == '\t' || clean.back() == '\r' || clean.back() == '\n'))
                clean.pop_back();
            r.cols.push_back(clean);
        }
        if ((int)r.cols.size() > maxCols) maxCols = (int)r.cols.size();
        data.rows.push_back(r);
        lineCount++;
    }
    if (lineCount > 1 && maxCols > 1) {
        data.isStructured = true;
        data.maxCols = maxCols;
        for (auto& r : data.rows)
            while ((int)r.cols.size() < maxCols) r.cols.push_back("");
    } else {
        data.plainText = text;
    }
    return data;
}

SData DetectStructuredData(const std::string& content, const std::string& fmt_in) {
    if (fmt_in == ".html" || fmt_in == ".htm")
        return ParseHtmlTable(content);
    if (fmt_in == ".txt") {
        SData data = ParseCsvTsv(content, '\t');
        if (data.isStructured) return data;
        return ParseCsvTsv(content, ',');
    }
    if (fmt_in == ".xlsx" || fmt_in == ".xls" || fmt_in == ".docx" || fmt_in == ".doc" || fmt_in == ".pdf") {
        if (content.find("<table") != std::string::npos)
            return ParseHtmlTable(content);
        SData data = ParseCsvTsv(content, '\t');
        if (data.isStructured) return data;
        return ParseCsvTsv(content, ',');
    }
    SData data; data.isStructured = false; data.plainText = content;
    return data;
}

// ============================================================
// 4. GENERADORES DE SALIDA
// ============================================================
std::string GenerateHtmlFromData(const SData& data) {
    std::string html = "<!DOCTYPE html>\n<html>\n<head>\n<meta charset='UTF-8'>\n<title>Documento Convertido</title>\n";
    html += "<style>body{font-family:Arial;margin:20px;background:#f9f9f9;} table{border-collapse:collapse;width:100%;background:white;} ";
    html += "th,td{border:1px solid #ddd;padding:8px;text-align:left;} th{background:#4CAF50;color:white;} ";
    html += "tr:nth-child(even){background:#f2f2f2;} pre{background:#f4f4f4;padding:15px;border-radius:5px;white-space:pre-wrap;word-wrap:break-word;}</style>\n</head>\n<body>\n";
    if (data.isStructured && !data.rows.empty()) {
        html += "<h2>📊 Datos Estructurados</h2>\n<table>\n";
        html += "<thead><tr>";
        for (const auto& col : data.rows[0].cols) html += "<th>" + EscapeHtml(col) + "</th>";
        html += "</tr></thead>\n<tbody>\n";
        for (size_t i = 1; i < data.rows.size(); ++i) {
            html += "<tr>";
            for (const auto& col : data.rows[i].cols) html += "<td>" + EscapeHtml(col) + "</td>";
            html += "</tr>\n";
        }
        html += "</tbody>\n</table>\n";
    } else {
        html += "<h2>📄 Texto</h2>\n<pre>" + EscapeHtml(data.plainText) + "</pre>\n";
    }
    html += "</body>\n</html>";
    return html;
}

// Generar Excel (.xlsx) - en realidad HTML con extensión .xlsx
std::string GenerateExcelFromData(const SData& data) {
    std::string html = "<html xmlns:o='urn:schemas-microsoft-com:office:office' xmlns:x='urn:schemas-microsoft-com:office:excel' xmlns='http://www.w3.org/TR/REC-html40'>\n<head>\n<meta charset='UTF-8'>\n";
    html += "<!--[if gte mso 9]><xml><x:ExcelWorkbook><x:ExcelWorksheets><x:ExcelWorksheet><x:Name>Hoja1</x:Name><x:WorksheetOptions><x:DisplayGridlines/></x:WorksheetOptions></x:ExcelWorksheet></x:ExcelWorksheets></x:ExcelWorkbook></xml><![endif]-->\n";
    html += "<style>table{border-collapse:collapse;} td,th{border:1px solid #000;padding:5px;}</style>\n</head>\n<body>\n";
    if (data.isStructured && !data.rows.empty()) {
        html += "<table>\n";
        for (const auto& row : data.rows) {
            html += "<tr>";
            for (const auto& col : row.cols) html += "<td>" + EscapeHtml(col) + "</td>";
            html += "</tr>\n";
        }
        html += "</table>\n";
    } else {
        html += "<table><tr><td>" + EscapeHtml(data.plainText) + "</td></tr></table>\n";
    }
    html += "</body>\n</html>";
    return html;
}

// Generar Word (.docx) - HTML con extensión .docx
std::string GenerateWordFromData(const SData& data) {
    std::string html = "<html xmlns:o='urn:schemas-microsoft-com:office:office' xmlns:w='urn:schemas-microsoft-com:office:word' xmlns='http://www.w3.org/TR/REC-html40'>\n<head>\n<meta charset='UTF-8'>\n";
    html += "<!--[if gte mso 9]><xml><w:WordDocument><w:View>Print</w:View></w:WordDocument></xml><![endif]-->\n";
    html += "<style>body{font-family:Arial;font-size:11pt;} table{border-collapse:collapse;} td,th{border:1px solid #000;padding:4px;}</style>\n</head>\n<body>\n";
    if (data.isStructured && !data.rows.empty()) {
        html += "<table>\n";
        for (const auto& row : data.rows) {
            html += "<tr>";
            for (const auto& col : row.cols) html += "<td>" + EscapeHtml(col) + "</td>";
            html += "</tr>\n";
        }
        html += "</table>\n";
    } else {
        html += "<p>" + EscapeHtml(data.plainText) + "</p>\n";
    }
    html += "</body>\n</html>";
    return html;
}

// ============================================================
// PROTOTIPO de la función GeneratePdfFromData (se define después)
// ============================================================
std::string GeneratePdfFromData(const SData& data);

// ============================================================
// 5. GENERADOR DE PDF (CLASE CPDFWriter)
// ============================================================
class CPDFWriter {
private:
    std::stringstream m_pdf;
    int m_pageCount;
    float m_y, m_x;
    float m_marginTop, m_marginBottom, m_marginLeft, m_marginRight;
    float m_pageWidth, m_pageHeight;
    float m_fontSize, m_lineHeight, m_charWidth;
    int m_maxCharsPerLine;
    std::vector<std::string> m_pagesContent;
    std::string m_currentPageContent;

    void NewPage() {
        if (!m_currentPageContent.empty()) {
            m_pagesContent.push_back(m_currentPageContent);
            m_currentPageContent.clear();
        }
        m_y = m_marginTop;
        m_pageCount++;
        m_currentPageContent = "";
    }

    void AddTextInternal(const std::string& text) {
        std::string escaped = EscapePdfText(text);
        std::string remaining = escaped;
        while (!remaining.empty()) {
            if (m_y > m_pageHeight - m_marginBottom) NewPage();
            if ((int)remaining.length() <= m_maxCharsPerLine) {
                m_currentPageContent += "BT /F1 " + std::to_string((int)m_fontSize) + " Tf " +
                                        std::to_string(m_x) + " " + std::to_string(m_y) + " Td (" + remaining + ") Tj ET\n";
                m_y -= m_lineHeight;
                break;
            } else {
                int cutPos = m_maxCharsPerLine;
                while (cutPos > 0 && remaining[cutPos] != ' ') cutPos--;
                if (cutPos == 0) cutPos = m_maxCharsPerLine;
                std::string line = remaining.substr(0, cutPos);
                m_currentPageContent += "BT /F1 " + std::to_string((int)m_fontSize) + " Tf " +
                                        std::to_string(m_x) + " " + std::to_string(m_y) + " Td (" + line + ") Tj ET\n";
                m_y -= m_lineHeight;
                remaining = remaining.substr(cutPos + 1);
                if (remaining.empty()) break;
            }
        }
    }

    void AddTableInternal(const SData& data) {
        if (data.rows.empty()) return;
        int numCols = data.maxCols;
        if (numCols == 0) return;
        float colWidth = (m_pageWidth - m_marginLeft - m_marginRight) / numCols;
        float cellPadding = 4.0f;
        float lineH = m_fontSize + 2;
        for (size_t rowIdx = 0; rowIdx < data.rows.size(); ++rowIdx) {
            const auto& row = data.rows[rowIdx];
            if (m_y < m_marginBottom + lineH * 2) NewPage();
            float xStart = m_x;
            float yStart = m_y;
            float maxHeight = lineH;
            for (int colIdx = 0; colIdx < numCols; ++colIdx) {
                std::string cellText = (colIdx < (int)row.cols.size()) ? row.cols[colIdx] : "";
                std::string escaped = EscapePdfText(cellText);
                if ((int)escaped.length() > m_maxCharsPerLine / 2)
                    escaped = escaped.substr(0, m_maxCharsPerLine / 2 - 3) + "...";
                m_currentPageContent += "BT /F1 " + std::to_string((int)m_fontSize) + " Tf " +
                                        std::to_string(xStart + cellPadding) + " " + std::to_string(m_y - cellPadding) + " Td (" + escaped + ") Tj ET\n";
                xStart += colWidth;
            }
            m_currentPageContent += "BT /F1 " + std::to_string((int)m_fontSize) + " Tf " +
                                    std::to_string(m_x) + " " + std::to_string(m_y - m_fontSize - 2) + " Td ( ) Tj ET\n";
            m_y -= (m_fontSize + 6);
        }
    }

public:
    CPDFWriter() {
        m_pageWidth = 595.0f; m_pageHeight = 842.0f;
        m_marginLeft = 50.0f; m_marginRight = 50.0f;
        m_marginTop = 50.0f; m_marginBottom = 50.0f;
        m_fontSize = 10.0f; m_lineHeight = 14.0f;
        m_charWidth = 5.5f;
        m_maxCharsPerLine = (int)((m_pageWidth - m_marginLeft - m_marginRight) / m_charWidth);
        if (m_maxCharsPerLine > 80) m_maxCharsPerLine = 80;
        if (m_maxCharsPerLine < 20) m_maxCharsPerLine = 20;
        m_pageCount = 0;
        m_y = m_marginTop;
        m_x = m_marginLeft;
        m_currentPageContent = "";
    }

    std::string Generate(const SData& data) {
        NewPage();
        AddTextInternal("Documento Convertido - Página " + std::to_string(m_pageCount));
        m_y -= m_lineHeight;
        AddTextInternal("----------------------------------------");
        m_y -= m_lineHeight * 2;
        if (data.isStructured && !data.rows.empty())
            AddTableInternal(data);
        else {
            std::string text = data.plainText;
            std::replace(text.begin(), text.end(), '\n', ' ');
            std::replace(text.begin(), text.end(), '\r', ' ');
            AddTextInternal(text);
        }
        if (!m_currentPageContent.empty())
            m_pagesContent.push_back(m_currentPageContent);

        // Construir PDF con offsets calculados
        std::stringstream pdf;
        int catalogId = 1, pagesId = 2, fontId = 3;
        int pageStart = 4;
        int numPages = (int)m_pagesContent.size();
        pdf << "%PDF-1.4\n";
        pdf << catalogId << " 0 obj\n<< /Type /Catalog /Pages " << pagesId << " 0 R >>\nendobj\n";
        std::string kids;
        for (int i = 0; i < numPages; ++i) {
            int pageId = pageStart + i * 2;
            int contentId = pageId + 1;
            kids += std::to_string(pageId) + " 0 R ";
        }
        pdf << pagesId << " 0 obj\n<< /Type /Pages /Kids [" << kids << "] /Count " << numPages << " >>\nendobj\n";
        pdf << fontId << " 0 obj\n<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>\nendobj\n";
        for (int i = 0; i < numPages; ++i) {
            int pageId = pageStart + i * 2;
            int contentId = pageId + 1;
            std::string stream = m_pagesContent[i];
            pdf << contentId << " 0 obj\n<< /Length " << stream.size() << " >>\nstream\n" << stream << "endstream\nendobj\n";
            pdf << pageId << " 0 obj\n<< /Type /Page /Parent " << pagesId << " 0 R /MediaBox [0 0 " << (int)m_pageWidth << " " << (int)m_pageHeight << "] /Contents " << contentId << " 0 R /Resources << /Font << /F1 " << fontId << " 0 R >> >> >>\nendobj\n";
        }
        int totalObjs = pageStart + numPages * 2;
        // Escribir xref y trailer con offsets calculados
        // Para simplificar, calculamos los offsets de cada objeto escribiendo el PDF en un string auxiliar
        // y luego reescribimos con los offsets correctos.
        // Método: guardamos todos los objetos en un string y luego generamos xref
        std::stringstream fullPdf;
        std::map<int, int> offsets;
        auto writeObj = [&](int id, const std::string& objStr) {
            offsets[id] = (int)fullPdf.tellp();
            fullPdf << id << " 0 obj\n" << objStr << "\nendobj\n";
        };
        writeObj(catalogId, "<< /Type /Catalog /Pages " + std::to_string(pagesId) + " 0 R >>");
        writeObj(pagesId, "<< /Type /Pages /Kids [" + kids + "] /Count " + std::to_string(numPages) + " >>");
        writeObj(fontId, "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>");
        for (int i = 0; i < numPages; ++i) {
            int pageId = pageStart + i * 2;
            int contentId = pageId + 1;
            writeObj(contentId, "<< /Length " + std::to_string(m_pagesContent[i].size()) + " >>\nstream\n" + m_pagesContent[i] + "\nendstream");
            writeObj(pageId, "<< /Type /Page /Parent " + std::to_string(pagesId) + " 0 R /MediaBox [0 0 " + 
                     std::to_string((int)m_pageWidth) + " " + std::to_string((int)m_pageHeight) + 
                     "] /Contents " + std::to_string(contentId) + " 0 R /Resources << /Font << /F1 " + 
                     std::to_string(fontId) + " 0 R >> >> >>");
        }
        int startXref = (int)fullPdf.tellp();
        std::string xref = "xref\n0 " + std::to_string(totalObjs + 1) + "\n0000000000 65535 f\n";
        for (int i = 1; i <= totalObjs; ++i) {
            if (offsets.find(i) != offsets.end()) {
                char buf[11];
                snprintf(buf, 11, "%010d", offsets[i]);
                xref += std::string(buf) + " 00000 n\n";
            } else {
                xref += "0000000000 00000 n\n";
            }
        }
        fullPdf << xref;
        fullPdf << "trailer\n<< /Size " << totalObjs + 1 << " /Root " << catalogId << " 0 R >>\n";
        fullPdf << "startxref\n" << startXref << "\n%%EOF";
        return fullPdf.str();
    }
};

// ============================================================
// DEFINICIÓN de GeneratePdfFromData (usando la clase)
// ============================================================
std::string GeneratePdfFromData(const SData& data) {
    CPDFWriter writer;
    return writer.Generate(data);
}

// ============================================================
// 6. CONVERSIÓN PRINCIPAL
// ============================================================
struct ConversionResult {
    int total = 0, success = 0, failed = 0;
    std::string last_error;
};

ConversionResult ConvertFolder(const std::string& folder,
                               const std::string& ext_in,
                               const std::string& ext_out,
                               HWND hProgress, HWND hStatus) {
    ConversionResult result;
    std::string fmt_in = ext_in, fmt_out = ext_out;
    std::transform(fmt_in.begin(), fmt_in.end(), fmt_in.begin(), ::tolower);
    std::transform(fmt_out.begin(), fmt_out.end(), fmt_out.begin(), ::tolower);

    std::vector<fs::path> files;
    try {
        for (const auto& entry : fs::recursive_directory_iterator(folder)) {
            if (entry.is_regular_file()) {
                auto ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == fmt_in) files.push_back(entry.path());
            }
        }
    } catch (...) { result.last_error = "Error al leer la carpeta"; return result; }

    result.total = (int)files.size();
    if (result.total == 0) { result.last_error = "No se encontraron archivos con extensión " + fmt_in; return result; }

    SendMessage(hProgress, PBM_SETRANGE, 0, MAKELPARAM(0, result.total));
    SendMessage(hProgress, PBM_SETPOS, 0, 0);

    for (size_t i = 0; i < files.size(); ++i) {
        const auto& input_path = files[i];
        std::string base_name = input_path.stem().string();
        fs::path output_path = input_path.parent_path() / (base_name + "_convertido" + ext_out);

        std::wstring status = L"📄 Procesando (" + std::to_wstring(i + 1) + L"/" + std::to_wstring(result.total) + L"): " + input_path.filename().wstring();
        SetWindowTextW(hStatus, status.c_str());
        SendMessage(hProgress, PBM_SETPOS, (int)(i + 1), 0);

        try {
            std::string content = ReadFileToString(input_path.string());
            if (content.empty()) { result.failed++; continue; }

            SData data = DetectStructuredData(content, fmt_in);
            std::string output_content;
            bool is_html = (fmt_out == ".html" || fmt_out == ".htm");
            bool is_excel = (fmt_out == ".xlsx" || fmt_out == ".xls");
            bool is_word = (fmt_out == ".docx" || fmt_out == ".doc");
            bool is_pdf = (fmt_out == ".pdf");
            bool is_txt = (fmt_out == ".txt");

            if (is_html) output_content = GenerateHtmlFromData(data);
            else if (is_excel) output_content = GenerateExcelFromData(data);
            else if (is_word) output_content = GenerateWordFromData(data);
            else if (is_pdf) output_content = GeneratePdfFromData(data);
            else if (is_txt) {
                if (data.isStructured) {
                    std::stringstream ss;
                    for (const auto& row : data.rows) {
                        for (size_t j = 0; j < row.cols.size(); ++j) {
                            if (j > 0) ss << "\t";
                            ss << row.cols[j];
                        }
                        ss << "\n";
                    }
                    output_content = ss.str();
                } else output_content = data.plainText;
            } else output_content = content;

            if (!WriteStringToFile(output_path.string(), output_content)) { result.failed++; continue; }
            result.success++;
        } catch (...) { result.failed++; }

        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    SetWindowTextW(hStatus, L"✅ ¡Conversión completada!");
    return result;
}

// ============================================================
// 7. INTERFAZ GRÁFICA (DISEÑO MEJORADO)
// ============================================================
#define IDC_BTN_BROWSE       1001
#define IDC_BTN_CONVERT      1002
#define IDC_EDIT_PATH        1003
#define IDC_COMBO_INPUT      1004
#define IDC_COMBO_OUTPUT     1005
#define IDC_PROGRESS         1006
#define IDC_LBL_STATUS       1007
#define IDC_GROUP_FILES      1008

HWND g_hEditPath, g_hComboIn, g_hComboOut, g_hProgress, g_hStatus;
HWND g_hBtnConvert, g_hBtnBrowse;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            // Grupo
            CreateWindowW(L"BUTTON", L" Configuración de Conversión ",
                WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                10, 10, 460, 110, hwnd, (HMENU)IDC_GROUP_FILES, NULL, NULL);

            // Etiqueta Carpeta
            CreateWindowW(L"STATIC", L"Carpeta:", WS_CHILD | WS_VISIBLE,
                20, 35, 60, 20, hwnd, NULL, NULL, NULL);

            // Edit ruta
            g_hEditPath = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_READONLY,
                80, 32, 280, 24, hwnd, (HMENU)IDC_EDIT_PATH, NULL, NULL);

            // Botón Buscar
            g_hBtnBrowse = CreateWindowW(L"BUTTON", L"📁 Buscar", WS_CHILD | WS_VISIBLE,
                365, 32, 90, 24, hwnd, (HMENU)IDC_BTN_BROWSE, NULL, NULL);

            // Etiqueta Entrada
            CreateWindowW(L"STATIC", L"Entrada:", WS_CHILD | WS_VISIBLE,
                20, 70, 60, 20, hwnd, NULL, NULL, NULL);
            g_hComboIn = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | CBS_HASSTRINGS,
                80, 68, 120, 100, hwnd, (HMENU)IDC_COMBO_INPUT, NULL, NULL);
            SendMessageW(g_hComboIn, CB_ADDSTRING, 0, (LPARAM)L".html");
            SendMessageW(g_hComboIn, CB_ADDSTRING, 0, (LPARAM)L".txt");
            SendMessageW(g_hComboIn, CB_ADDSTRING, 0, (LPARAM)L".xlsx");
            SendMessageW(g_hComboIn, CB_ADDSTRING, 0, (LPARAM)L".docx");
            SendMessageW(g_hComboIn, CB_ADDSTRING, 0, (LPARAM)L".pdf");
            SendMessageW(g_hComboIn, CB_SETCURSEL, 0, 0);

            // Etiqueta Salida
            CreateWindowW(L"STATIC", L"Salida:", WS_CHILD | WS_VISIBLE,
                220, 70, 60, 20, hwnd, NULL, NULL, NULL);
            g_hComboOut = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | CBS_HASSTRINGS,
                280, 68, 120, 100, hwnd, (HMENU)IDC_COMBO_OUTPUT, NULL, NULL);
            SendMessageW(g_hComboOut, CB_ADDSTRING, 0, (LPARAM)L".html");
            SendMessageW(g_hComboOut, CB_ADDSTRING, 0, (LPARAM)L".txt");
            SendMessageW(g_hComboOut, CB_ADDSTRING, 0, (LPARAM)L".xlsx");
            SendMessageW(g_hComboOut, CB_ADDSTRING, 0, (LPARAM)L".docx");
            SendMessageW(g_hComboOut, CB_ADDSTRING, 0, (LPARAM)L".pdf");
            SendMessageW(g_hComboOut, CB_SETCURSEL, 2, 0); // .xlsx por defecto

            // Barra de progreso
            g_hProgress = CreateWindowW(PROGRESS_CLASSW, L"", WS_CHILD | WS_VISIBLE,
                20, 135, 440, 20, hwnd, (HMENU)IDC_PROGRESS, NULL, NULL);
            SendMessageW(g_hProgress, PBM_SETSTEP, 1, 0);

            // Etiqueta estado
            g_hStatus = CreateWindowW(L"STATIC", L"📂 Selecciona una carpeta y haz clic en Convertir",
                WS_CHILD | WS_VISIBLE,
                20, 165, 440, 20, hwnd, (HMENU)IDC_LBL_STATUS, NULL, NULL);

            // Botón Convertir
            g_hBtnConvert = CreateWindowW(L"BUTTON", L"⚡ Convertir Archivos (V3 Mejorado)",
                WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                120, 200, 240, 40, hwnd, (HMENU)IDC_BTN_CONVERT, NULL, NULL);

            break;
        }

        case WM_COMMAND: {
            if (LOWORD(wParam) == IDC_BTN_BROWSE) {
                BROWSEINFOW bi = { 0 };
                bi.hwndOwner = hwnd;
                bi.lpszTitle = L"Selecciona la carpeta con tus archivos";
                bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
                LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
                if (pidl) {
                    wchar_t path[MAX_PATH];
                    if (SHGetPathFromIDListW(pidl, path))
                        SetWindowTextW(g_hEditPath, path);
                    CoTaskMemFree(pidl);
                }
            }
            else if (LOWORD(wParam) == IDC_BTN_CONVERT) {
                wchar_t folder[MAX_PATH];
                GetWindowTextW(g_hEditPath, folder, MAX_PATH);
                if (wcslen(folder) == 0) {
                    MessageBoxW(hwnd, L"Por favor, selecciona una carpeta primero.", L"Error", MB_ICONWARNING);
                    break;
                }

                wchar_t bufIn[10], bufOut[10];
                SendMessageW(g_hComboIn, CB_GETLBTEXT, SendMessageW(g_hComboIn, CB_GETCURSEL, 0, 0), (LPARAM)bufIn);
                SendMessageW(g_hComboOut, CB_GETLBTEXT, SendMessageW(g_hComboOut, CB_GETCURSEL, 0, 0), (LPARAM)bufOut);

                std::wstring wsIn(bufIn), wsOut(bufOut);
                if (wsIn == wsOut) {
                    MessageBoxW(hwnd, L"Los formatos de entrada y salida son iguales.", L"Advertencia", MB_ICONWARNING);
                    break;
                }

                int len_in = WideCharToMultiByte(CP_UTF8, 0, wsIn.c_str(), -1, NULL, 0, NULL, NULL);
                int len_out = WideCharToMultiByte(CP_UTF8, 0, wsOut.c_str(), -1, NULL, 0, NULL, NULL);
                int len_folder = WideCharToMultiByte(CP_UTF8, 0, folder, -1, NULL, 0, NULL, NULL);
                std::string ext_in(len_in, 0), ext_out(len_out, 0), folderA(len_folder, 0);
                WideCharToMultiByte(CP_UTF8, 0, wsIn.c_str(), -1, &ext_in[0], len_in, NULL, NULL);
                WideCharToMultiByte(CP_UTF8, 0, wsOut.c_str(), -1, &ext_out[0], len_out, NULL, NULL);
                WideCharToMultiByte(CP_UTF8, 0, folder, -1, &folderA[0], len_folder, NULL, NULL);
                ext_in.pop_back(); ext_out.pop_back(); folderA.pop_back();

                EnableWindow(g_hBtnConvert, FALSE);
                EnableWindow(g_hBtnBrowse, FALSE);

                ConversionResult res = ConvertFolder(folderA, ext_in, ext_out, g_hProgress, g_hStatus);

                EnableWindow(g_hBtnConvert, TRUE);
                EnableWindow(g_hBtnBrowse, TRUE);

                wchar_t msg[512];
                swprintf(msg, 512, L"✅ Proceso completado.\n\n📁 Archivos procesados: %d\n✅ Exitosos: %d\n❌ Fallidos: %d\n\n📂 Carpeta: %s",
                    res.total, res.success, res.failed, folder);
                MessageBoxW(hwnd, msg, L"Resultado", MB_ICONINFORMATION);
            }
            break;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"ConversorUniversalV3Class";
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(0, L"ConversorUniversalV3Class", L"🔄 Conversor Universal V3 (Excel/Word 2024)",
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 500, 300,
        NULL, NULL, hInstance, NULL);

    if (!hwnd) return 0;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}