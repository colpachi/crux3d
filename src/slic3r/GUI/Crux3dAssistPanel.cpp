#include "Crux3dAssistPanel.hpp"

#include <algorithm>
#include <fstream>
#include <string>
#include <utility>

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/dcclient.h>
#include <wx/clipbrd.h>
#include <wx/dataobj.h>
#include <wx/dcbuffer.h>
#include <wx/filedlg.h>
#include <wx/font.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/textdlg.h>

// wx/intl.h may define the conventional `_` macro before the project's
// localization header is included. Clear it so `_L` is declared consistently.
#ifdef _
#undef _
#endif
#include "I18N.hpp"

namespace Slic3r::GUI {

namespace {

struct CruxPalette
{
    wxColour canvas;
    wxColour surface;
    wxColour surface_alt;
    wxColour border;
    wxColour text;
    wxColour muted;
    wxColour accent;
    wxColour accent_hover;
    wxColour accent_soft;
    wxColour success;
};

CruxPalette palette_for(bool is_dark)
{
    if (is_dark) {
        return {
            wxColour("#1B1F2A"), // canvas
            wxColour("#242A38"), // card
            wxColour("#2D3445"), // input / secondary card
            wxColour("#3D4860"), // border
            wxColour("#F3F6FF"), // text
            wxColour("#9EAAC2"), // muted
            wxColour("#6CA6FF"), // blue accent
            wxColour("#8BB9FF"), // hover
            wxColour("#263C62"), // accent soft
            wxColour("#57D6A0")  // success
        };
    }

    return {
        wxColour("#F4F6FA"),
        wxColour("#FFFFFF"),
        wxColour("#F0F3F8"),
        wxColour("#D6DCE8"),
        wxColour("#1F2A3D"),
        wxColour("#65738A"),
        wxColour("#3D76D6"),
        wxColour("#2D62B8"),
        wxColour("#E2ECFF"),
        wxColour("#238A66")
    };
}

wxFont panel_font(const wxWindow* window, int size, bool bold = false)
{
    wxFont font = window->GetFont();
    font.SetPointSize(size);
    font.SetWeight(bold ? wxFONTWEIGHT_BOLD : wxFONTWEIGHT_NORMAL);
    return font;
}

} // namespace

Crux3dAssistPanel::Crux3dAssistPanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY)
{
    SetMinSize(FromDIP(wxSize(330, 0)));
    build_ui();
    apply_theme(true);
}

wxPanel* Crux3dAssistPanel::create_card()
{
    auto* card = new wxPanel(this, wxID_ANY);
    card->SetMinSize(FromDIP(wxSize(0, -1)));
    card->SetBackgroundStyle(wxBG_STYLE_PAINT);
    card->Bind(wxEVT_PAINT, [card](wxPaintEvent&) {
        wxAutoBufferedPaintDC dc(card);
        dc.SetBackground(wxBrush(card->GetParent()->GetBackgroundColour()));
        dc.Clear();
        const wxColour background = card->GetBackgroundColour();
        const wxColour border = background.GetLuminance() < 0.5 ? wxColour("#3D4860") : wxColour("#D6DCE8");
        wxRect rect = card->GetClientRect();
        rect.Deflate(1);
        dc.SetPen(wxPen(border, 1));
        dc.SetBrush(wxBrush(background));
        dc.DrawRoundedRectangle(rect, 8);
    });
    m_cards.push_back(card);
    return card;
}

wxStaticText* Crux3dAssistPanel::create_label(wxWindow* parent, const wxString& text, bool muted)
{
    auto* label = new wxStaticText(parent, wxID_ANY, text);
    label->SetFont(panel_font(this, muted ? 9 : 10, !muted));
    (muted ? m_muted_labels : m_section_labels).push_back(label);
    return label;
}

wxStaticText* Crux3dAssistPanel::create_section_label(wxWindow* parent, const wxString& text)
{
    auto* label = create_label(parent, text, false);
    label->SetFont(panel_font(this, 11, true));
    return label;
}

wxButton* Crux3dAssistPanel::create_button(wxWindow* parent, const wxString& text, bool primary)
{
    auto* button = new wxButton(parent, wxID_ANY, text, wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
    button->SetFont(panel_font(this, 9, true));
    button->SetMinSize(FromDIP(wxSize(-1, 30)));
    if (primary)
        m_primary_buttons.push_back(button);
    else
        m_secondary_buttons.push_back(button);
    return button;
}

wxTextCtrl* Crux3dAssistPanel::create_field(wxWindow* parent, const wxString& value, const wxString& hint)
{
    auto* field = new wxTextCtrl(parent, wxID_ANY, value, wxDefaultPosition, wxDefaultSize,
                                  wxTE_PROCESS_ENTER);
    field->SetHint(hint);
    field->SetFont(panel_font(this, 9));
    field->SetMinSize(FromDIP(wxSize(-1, 30)));
    m_input_fields.push_back(field);
    return field;
}

void Crux3dAssistPanel::build_ui()
{
    auto* root = new wxBoxSizer(wxVERTICAL);
    root->AddSpacer(FromDIP(12));

    auto* header = new wxBoxSizer(wxHORIZONTAL);
    m_brand = new wxStaticText(this, wxID_ANY, "CRUX3D");
    m_brand->SetFont(panel_font(this, 15, true));
    m_section_labels.push_back(m_brand);
    header->Add(m_brand, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(14));
    auto* subtitle = new wxStaticText(this, wxID_ANY, _L("Assistente de impressão"));
    subtitle->SetFont(panel_font(this, 9));
    m_muted_labels.push_back(subtitle);
    header->Add(subtitle, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(9));
    root->Add(header, 0, wxEXPAND | wxBOTTOM, FromDIP(11));

    auto* printer_card = create_card();
    auto* printer_sizer = new wxBoxSizer(wxVERTICAL);
    printer_sizer->Add(create_section_label(printer_card, _L("Perfil da impressora")), 0, wxBOTTOM, FromDIP(8));
    printer_sizer->Add(create_label(printer_card, _L("Selecione a máquina e mantenha o perfil sincronizado."), true), 0, wxBOTTOM, FromDIP(8));

    auto* printer_row = new wxBoxSizer(wxHORIZONTAL);
    m_printer_type = new wxChoice(printer_card, wxID_ANY);
    m_printer_type->Append(_L("FDM · CoreXY"));
    m_printer_type->Append(_L("FDM · Bedslinger"));
    m_printer_type->Append(_L("Resina · MSLA"));
    m_printer_type->SetSelection(0);
    m_printer_type->SetFont(panel_font(this, 9));
    m_printer_type->SetMinSize(FromDIP(wxSize(-1, 30)));
    m_choice_fields.push_back(m_printer_type);
    printer_row->Add(m_printer_type, 1, wxEXPAND | wxRIGHT, FromDIP(6));

    auto* apply = create_button(printer_card, _L("Aplicar"), true);
    apply->Bind(wxEVT_BUTTON, &Crux3dAssistPanel::on_apply_profile, this);
    printer_row->Add(apply, 0, wxEXPAND);
    printer_sizer->Add(printer_row, 0, wxEXPAND | wxBOTTOM, FromDIP(9));

    auto* printer_meta = new wxBoxSizer(wxHORIZONTAL);
    auto* connected = create_label(printer_card, _L("●  Perfil local ativo"), true);
    connected->SetForegroundColour(wxColour("#57D6A0"));
    printer_meta->Add(connected, 1, wxALIGN_CENTER_VERTICAL);
    auto* edit_printer = create_button(printer_card, _L("Editar perfil"));
    edit_printer->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        append_chat(_L("CRUX3D"), _L("O editor de perfil está pronto para receber os parâmetros da sua máquina."));
    });
    printer_meta->Add(edit_printer, 0, wxALIGN_CENTER_VERTICAL);
    printer_sizer->Add(printer_meta, 0, wxEXPAND);
    printer_card->SetSizer(printer_sizer);
    root->Add(printer_card, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(10));

    auto* params_card = create_card();
    auto* params_sizer = new wxBoxSizer(wxVERTICAL);
    params_sizer->Add(create_section_label(params_card, _L("Parâmetros rápidos")), 0, wxBOTTOM, FromDIP(8));
    params_sizer->Add(create_label(params_card, _L("Ajustes essenciais deste projeto"), true), 0, wxBOTTOM, FromDIP(8));

    auto* profile_row = new wxBoxSizer(wxHORIZONTAL);
    auto* profile_label = create_label(params_card, _L("Preset"), true);
    profile_row->Add(profile_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(10));
    m_profile = new wxChoice(params_card, wxID_ANY);
    m_profile->Append(_L("CRUX3D · Qualidade"));
    m_profile->Append(_L("CRUX3D · Rascunho"));
    m_profile->Append(_L("CRUX3D · Alta resistência"));
    m_profile->SetSelection(0);
    m_profile->SetFont(panel_font(this, 9));
    m_profile->SetMinSize(FromDIP(wxSize(-1, 30)));
    m_choice_fields.push_back(m_profile);
    profile_row->Add(m_profile, 1, wxEXPAND);
    params_sizer->Add(profile_row, 0, wxEXPAND | wxBOTTOM, FromDIP(8));

    auto* values_row = new wxFlexGridSizer(2, FromDIP(7), FromDIP(7));
    values_row->AddGrowableCol(1, 1);
    const std::vector<std::pair<wxString, wxString>> fields = {
        {_L("Altura da camada"), "0.20 mm"},
        {_L("Preenchimento"), "20 %"},
        {_L("Temperatura"), "210 °C"},
        {_L("Velocidade"), "120 mm/s"}
    };
    for (const auto& [label, value] : fields) {
        values_row->Add(create_label(params_card, label, true), 0, wxALIGN_CENTER_VERTICAL);
        values_row->Add(create_field(params_card, value, label), 1, wxEXPAND);
    }
    params_sizer->Add(values_row, 0, wxEXPAND | wxBOTTOM, FromDIP(8));

    auto* save_profile = create_button(params_card, _L("Salvar como preset compartilhável"));
    save_profile->Bind(wxEVT_BUTTON, &Crux3dAssistPanel::on_share_parameters, this);
    params_sizer->Add(save_profile, 0, wxEXPAND);
    params_card->SetSizer(params_sizer);
    root->Add(params_card, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(10));

    auto* share_card = create_card();
    auto* share_sizer = new wxBoxSizer(wxVERTICAL);
    share_sizer->Add(create_section_label(share_card, _L("Compartilhar parâmetros")), 0, wxBOTTOM, FromDIP(8));
    share_sizer->Add(create_label(share_card, _L("O projeto fica pronto para sincronização segura entre os membros do grupo."), true), 0, wxBOTTOM, FromDIP(9));
    auto* share_row = new wxBoxSizer(wxHORIZONTAL);
    auto* share_button = create_button(share_card, _L("Compartilhar"), true);
    share_button->Bind(wxEVT_BUTTON, &Crux3dAssistPanel::on_share_parameters, this);
    share_row->Add(share_button, 1, wxEXPAND | wxRIGHT, FromDIP(6));
    auto* group_button = create_button(share_card, _L("Grupo"));
    group_button->Bind(wxEVT_BUTTON, &Crux3dAssistPanel::on_manage_group, this);
    share_row->Add(group_button, 1, wxEXPAND);
    share_sizer->Add(share_row, 0, wxEXPAND | wxBOTTOM, FromDIP(8));
    m_sync_status = create_label(share_card, _L("●  P2P preparado · nenhum servidor central necessário"), true);
    share_sizer->Add(m_sync_status, 0, wxEXPAND);
    share_card->SetSizer(share_sizer);
    root->Add(share_card, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(10));

    auto* ai_card = create_card();
    auto* ai_sizer = new wxBoxSizer(wxVERTICAL);
    auto* ai_header = new wxBoxSizer(wxHORIZONTAL);
    ai_header->Add(create_section_label(ai_card, _L("Chat de IA")), 1, wxALIGN_CENTER_VERTICAL);
    auto* ai_badge = create_label(ai_card, _L("● online"), true);
    ai_badge->SetForegroundColour(wxColour("#57D6A0"));
    ai_header->Add(ai_badge, 0, wxALIGN_CENTER_VERTICAL);
    ai_sizer->Add(ai_header, 0, wxEXPAND | wxBOTTOM, FromDIP(7));
    ai_sizer->Add(create_label(ai_card, _L("Sugestões conscientes do seu perfil de impressão"), true), 0, wxBOTTOM, FromDIP(7));

    m_chat_log = new wxTextCtrl(ai_card, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                                wxTE_MULTILINE | wxTE_READONLY);
    m_chat_log->SetMinSize(FromDIP(wxSize(-1, 135)));
    m_chat_log->SetFont(panel_font(this, 9));
    ai_sizer->Add(m_chat_log, 1, wxEXPAND | wxBOTTOM, FromDIP(7));

    auto* provider_row = new wxBoxSizer(wxHORIZONTAL);
    m_provider = new wxChoice(ai_card, wxID_ANY);
    m_provider->Append(_L("Provedor genérico"));
    m_provider->Append(_L("DeepSeek · sua subscription"));
    m_provider->Append(_L("Claude Code · sua subscription"));
    m_provider->Append(_L("Servidor MCP local"));
    m_provider->SetSelection(0);
    m_provider->SetFont(panel_font(this, 9));
    m_provider->SetMinSize(FromDIP(wxSize(-1, 30)));
    m_provider->Bind(wxEVT_CHOICE, &Crux3dAssistPanel::on_provider_changed, this);
    m_choice_fields.push_back(m_provider);
    provider_row->Add(m_provider, 1, wxEXPAND);
    ai_sizer->Add(provider_row, 0, wxEXPAND | wxBOTTOM, FromDIP(5));

    m_provider_hint = create_label(ai_card, _L("Configure a chave nas preferências para ativar o provedor."), true);
    ai_sizer->Add(m_provider_hint, 0, wxEXPAND | wxBOTTOM, FromDIP(6));

    auto* chat_row = new wxBoxSizer(wxHORIZONTAL);
    m_chat_input = create_field(ai_card, wxEmptyString, _L("Pergunte sobre o fatiamento…"));
    m_chat_input->Bind(wxEVT_TEXT_ENTER, &Crux3dAssistPanel::on_send_message, this);
    chat_row->Add(m_chat_input, 1, wxEXPAND | wxRIGHT, FromDIP(6));
    auto* send = create_button(ai_card, _L("Enviar"), true);
    send->Bind(wxEVT_BUTTON, &Crux3dAssistPanel::on_send_message, this);
    chat_row->Add(send, 0, wxEXPAND);
    ai_sizer->Add(chat_row, 0, wxEXPAND | wxBOTTOM, FromDIP(7));

    m_auto_suggest = new wxCheckBox(ai_card, wxID_ANY, _L("Sugerir ajustes após cada alteração"));
    m_auto_suggest->SetValue(true);
    m_auto_suggest->SetFont(panel_font(this, 9));
    ai_sizer->Add(m_auto_suggest, 0, wxEXPAND);

    ai_card->SetSizer(ai_sizer);
    root->Add(ai_card, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(10));

    auto* footer = new wxStaticText(this, wxID_ANY, _L("MCP  ·  P2P  ·  Presets versionados"));
    footer->SetFont(panel_font(this, 8));
    m_muted_labels.push_back(footer);
    root->Add(footer, 0, wxALIGN_CENTER | wxBOTTOM, FromDIP(9));

    SetSizer(root);
    root->SetSizeHints(this);

    append_chat(_L("CRUX3D IA"), _L("Olá! Posso ajudar a preparar seu perfil, revisar parâmetros ou explicar um alerta de slicing."));
}

void Crux3dAssistPanel::apply_theme(bool is_dark)
{
    const auto palette = palette_for(is_dark);
    SetBackgroundColour(palette.canvas);
    SetForegroundColour(palette.text);

    for (wxPanel* card : m_cards) {
        card->SetBackgroundColour(palette.surface);
        card->SetForegroundColour(palette.text);
    }
    for (wxStaticText* label : m_section_labels)
        label->SetForegroundColour(palette.text);
    for (wxStaticText* label : m_muted_labels)
        label->SetForegroundColour(palette.muted);
    if (m_brand)
        m_brand->SetForegroundColour(palette.accent);
    for (wxTextCtrl* field : m_input_fields) {
        field->SetBackgroundColour(palette.surface_alt);
        field->SetForegroundColour(palette.text);
    }
    for (wxChoice* choice : m_choice_fields) {
        choice->SetBackgroundColour(palette.surface_alt);
        choice->SetForegroundColour(palette.text);
    }
    for (wxButton* button : m_primary_buttons) {
        button->SetBackgroundColour(palette.accent);
        button->SetForegroundColour(wxColour("#FFFFFF"));
    }
    for (wxButton* button : m_secondary_buttons) {
        button->SetBackgroundColour(palette.surface_alt);
        button->SetForegroundColour(palette.text);
    }

    if (m_chat_log) {
        m_chat_log->SetBackgroundColour(palette.surface_alt);
        m_chat_log->SetForegroundColour(palette.text);
    }
    if (m_auto_suggest) {
        m_auto_suggest->SetForegroundColour(palette.muted);
        m_auto_suggest->SetBackgroundColour(palette.surface);
    }
    if (m_sync_status)
        m_sync_status->SetForegroundColour(palette.success);
    if (m_provider_hint)
        m_provider_hint->SetForegroundColour(palette.muted);

    Refresh();
    Layout();
}

void Crux3dAssistPanel::append_chat(const wxString& speaker, const wxString& message)
{
    if (!m_chat_log)
        return;
    if (!m_chat_log->GetValue().empty())
        m_chat_log->AppendText("\n\n");
    m_chat_log->AppendText(speaker + ":\n" + message);
    m_chat_log->ShowPosition(m_chat_log->GetLastPosition());
}

void Crux3dAssistPanel::on_apply_profile(wxCommandEvent& event)
{
    const wxString printer = m_printer_type ? m_printer_type->GetStringSelection() : _L("sua impressora");
    append_chat(_L("CRUX3D IA"), _L("Perfil aplicado para ") + printer + _L(". Os parâmetros rápidos permanecem editáveis."));
}

void Crux3dAssistPanel::on_share_parameters(wxCommandEvent& event)
{
    wxFileDialog dialog(this,
                        _L("Exportar parâmetros do CRUX3D"),
                        wxEmptyString,
                        "crux3d-profile.crux-profile.json",
                        _L("Perfil CRUX3D (*.crux-profile.json)|*.crux-profile.json|Todos os arquivos (*.*)|*.*"),
                        wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dialog.ShowModal() == wxID_OK) {
        auto field_value = [this](size_t index, const char* fallback) {
            return index < m_input_fields.size() ? m_input_fields[index]->GetValue().ToStdString() : std::string(fallback);
        };
        std::ofstream profile(dialog.GetPath().ToStdString(), std::ios::binary);
        const char quote = char(34);
        auto write_json_field = [&profile, quote](const char* key, const std::string& value, bool last = false) {
            profile << "  " << quote << key << quote << ": " << quote << value << quote << (last ? "\n" : ",\n");
        };
        profile << "{\n";
        write_json_field("format", "crux3d-profile");
        write_json_field("printer", m_printer_type->GetStringSelection().ToStdString());
        write_json_field("preset", m_profile->GetStringSelection().ToStdString());
        write_json_field("layer_height", field_value(0, "0.20 mm"));
        write_json_field("infill", field_value(1, "20 %"));
        write_json_field("temperature", field_value(2, "210 °C"));
        write_json_field("speed", field_value(3, "120 mm/s"), true);
        profile << "}\n";
        if (m_sync_status)
            m_sync_status->SetLabel(_L("●  Arquivo exportado · pronto para compartilhar por P2P"));
        append_chat(_L("CRUX3D IA"), _L("Perfil exportado com sucesso. O arquivo pode ser enviado ao seu grupo."));
    }
    Layout();
}

void Crux3dAssistPanel::on_manage_group(wxCommandEvent& event)
{
    wxTextEntryDialog dialog(this,
                             _L("Dê um nome ao grupo que vai compartilhar estes parâmetros:"),
                             _L("Criar grupo de impressão"),
                             _L("Equipe CRUX3D"));
    if (dialog.ShowModal() == wxID_OK && !dialog.GetValue().Trim().empty()) {
        const wxString group_name = dialog.GetValue().Trim();
        const wxString invite = wxString("crux3d://group/") + group_name;
        if (wxTheClipboard && wxTheClipboard->Open()) {
            wxTheClipboard->SetData(new wxTextDataObject(invite));
            wxTheClipboard->Close();
        }
        if (m_sync_status)
            m_sync_status->SetLabel(_L("●  Grupo criado · convite copiado para a área de transferência"));
        append_chat(_L("CRUX3D IA"), _L("Grupo ") + group_name + _L(" criado. O convite está pronto para compartilhar."));
    }
    Layout();
}

void Crux3dAssistPanel::on_provider_changed(wxCommandEvent& event)
{
    const wxString provider = m_provider ? m_provider->GetStringSelection() : wxString();
    if (m_provider_hint)
        m_provider_hint->SetLabel(_L("Selecionado: ") + provider + _L(" · configure a credencial em Preferências."));
    Layout();
}

void Crux3dAssistPanel::on_send_message(wxCommandEvent& event)
{
    if (!m_chat_input || m_chat_input->GetValue().Trim().empty())
        return;
    const wxString message = m_chat_input->GetValue().Trim();
    append_chat(_L("Você"), message);
    append_chat(_L("CRUX3D IA"), _L("Entendi. Posso relacionar essa pergunta ao preset ativo e sugerir um próximo ajuste."));
    m_chat_input->Clear();
}

} // namespace Slic3r::GUI
