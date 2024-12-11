#ifndef CHAT_DOCK_H
#define CHAT_DOCK_H

#include "scene/gui/box_container.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/rich_text_label.h"

class ChatDock : public VBoxContainer {
    GDCLASS(ChatDock, VBoxContainer);

private:
    private:
    VBoxContainer *chat_container;
    ScrollContainer *scroll_container;
    VBoxContainer *message_container;
    LineEdit *input_field;
    Vector<String> messages;

    enum MessageType {
        MESSAGE_USER,
        MESSAGE_SYSTEM,
        MESSAGE_ERROR,
        MESSAGE_SUCCESS
    };

    void _text_submitted(const String &p_text);
    void _on_response_received(const Dictionary &p_scene_data);
    void _on_request_failed(const String &p_error);

    void gather_project_resources(Dictionary &p_resources);
    void gather_current_scene_info(Dictionary &p_scene_info);
    
    void _add_message(const String &p_text, MessageType p_type);
    void _add_log(const String &p_text);

protected:
    static void _bind_methods();

public:
    ChatDock();
};

#endif // CHAT_DOCK_H