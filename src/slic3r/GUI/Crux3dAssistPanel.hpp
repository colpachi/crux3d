#ifndef slic3r_Crux3dAssistPanel_hpp_
#define slic3r_Crux3dAssistPanel_hpp_

#include <vector>

#include <wx/panel.h>

class wxButton;
class wxCheckBox;
class wxChoice;
class wxStaticText;
class wxTextCtrl;

namespace Slic3r::GUI {

// The side panel is intentionally UI-only for now. Its controls expose the
// integration points for the future provider, MCP and P2P services without
// coupling the slicer core to a network implementation.
class Crux3dAssistPanel final : public wxPanel
{
public:
    explicit Crux3dAssistPanel(wxWindow* parent);

    void apply_theme(bool is_dark);

private:
    wxPanel* create_card();
    wxStaticText* create_label(wxWindow* parent, const wxString& text, bool muted = false);
    wxStaticText* create_section_label(wxWindow* parent, const wxString& text);
    wxButton* create_button(wxWindow* parent, const wxString& text, bool primary = false);
    wxTextCtrl* create_field(wxWindow* parent, const wxString& value, const wxString& hint);
    void build_ui();
    void append_chat(const wxString& speaker, const wxString& message);

    void on_apply_profile(wxCommandEvent& event);
    void on_share_parameters(wxCommandEvent& event);
    void on_manage_group(wxCommandEvent& event);
    void on_provider_changed(wxCommandEvent& event);
    void on_send_message(wxCommandEvent& event);

    wxStaticText* m_brand{nullptr};
    wxChoice* m_printer_type{nullptr};
    wxChoice* m_profile{nullptr};
    wxChoice* m_provider{nullptr};
    wxTextCtrl* m_chat_input{nullptr};
    wxTextCtrl* m_chat_log{nullptr};
    wxStaticText* m_sync_status{nullptr};
    wxStaticText* m_provider_hint{nullptr};
    wxCheckBox* m_auto_suggest{nullptr};

    std::vector<wxPanel*> m_cards;
    std::vector<wxStaticText*> m_section_labels;
    std::vector<wxStaticText*> m_muted_labels;
    std::vector<wxButton*> m_primary_buttons;
    std::vector<wxButton*> m_secondary_buttons;
    std::vector<wxTextCtrl*> m_input_fields;
    std::vector<wxChoice*> m_choice_fields;
};

} // namespace Slic3r::GUI

#endif // slic3r_Crux3dAssistPanel_hpp_
