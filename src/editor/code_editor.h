#pragma once
#include "extensions/text-editor/TextEditor.h"

static void RenderCodeEditor(size_t slot) {
    static std::array<std::unique_ptr<TextEditor>, kScriptSlots> editors;
    static std::array<std::string, kScriptSlots> previous;
    static std::array<int, kScriptSlots> languages;
    auto& doc = StudioScripts()[slot];
    auto& editor = editors[slot];
    if (!editor) { editor.reset(new TextEditor); languages[slot] = -1; }
    if (languages[slot] != doc.Language) {
        auto language = doc.Language ? TextEditor::LanguageDefinition::CPlusPlus()
                                    : TextEditor::LanguageDefinition::Lua();
        if (doc.Language) {
            language.mName = "C#";
            language.mKeywords = {"using","namespace","public","private","protected","internal",
                "static","class","struct","record","sealed","abstract","override","virtual",
                "void","int","float","double","bool","string","ulong","var","new","return",
                "if","else","for","foreach","while","switch","case","break","continue",
                "try","catch","throw","null","true","false","this","base","async","await",
                "get","set","readonly","in","out","is","as","typeof"};
        }
        editor->SetLanguageDefinition(language);
        editor->SetShowWhitespaces(false);
        languages[slot] = doc.Language;
    }
    if (previous[slot] != doc.Code) editor->SetText(doc.Code);
    TextEditor::ErrorMarkers errors;
    if (doc.Language) for (const auto& diagnostic : g_CSharpDiagnostics)
        if (diagnostic.value("file", std::string()) == doc.Name)
            errors[diagnostic.value("line", 1)] = diagnostic.value("message", std::string());
    editor->SetErrorMarkers(errors);
    if (ImGui::Button("Undo code")) editor->Undo();
    ImGui::SameLine();
    if (ImGui::Button("Redo code")) editor->Redo();
    ImGui::SameLine();
    auto cursor = editor->GetCursorPosition();
    ImGui::TextDisabled("%s | %d:%d | UTF-8 | Tab indent | Ctrl+S save",
                       doc.Language ? "C#" : "Luaryx / Lua 5.4", cursor.mLine+1, cursor.mColumn+1);
    if (g_CodeFont) ImGui::PushFont(g_CodeFont);
    editor->Render("Source", ImVec2(-1,-1), true);
    if (g_CodeFont) ImGui::PopFont();
    auto text = editor->GetText();
    if (text.size() >= sizeof(doc.Code)) {
        editor->Undo();
        AppendConsole("Script exceeds the 65535-byte document limit. Split the code into modules.\n");
        text = editor->GetText();
        if (text.size() >= sizeof(doc.Code)) { editor->SetText(doc.Code); text = doc.Code; }
    }
    if (text != doc.Code) std::snprintf(doc.Code, sizeof(doc.Code), "%s", text.c_str());
    previous[slot] = doc.Code;
}
