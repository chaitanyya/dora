#ifndef OPENAI_REQUEST_H
#define OPENAI_REQUEST_H

#include "scene/main/node.h"
#include "scene/main/http_request.h"

class OpenAIRequest : public Node {
    GDCLASS(OpenAIRequest, Node);

private:
    Dictionary base_request_data; 
    Array message_history;
    HTTPRequest* http_request;
    String api_key;

    Dictionary _create_message(const String& role, const String& text) const;
    Array _create_message_content(const String& text) const;
    void _on_request_completed(int p_result, int p_code, const PackedStringArray& headers, const PackedByteArray& p_data);
    void _init_base_request();

protected:
    static void _bind_methods();

public:
    OpenAIRequest();
    Error request_scene(const String& prompt, const Dictionary& project_resources, const Dictionary& current_scene);
    void set_api_key(const String& key);
    void clear_message_history();
};

#endif // OPENAI_REQUEST_H