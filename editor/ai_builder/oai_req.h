#ifndef OPENAI_REQUEST_H
#define OPENAI_REQUEST_H

#include "scene/main/node.h"
#include "scene/main/http_request.h"

class OpenAIRequest : public Node {
    GDCLASS(OpenAIRequest, Node);

private:
    HTTPRequest* http_request;
    String api_key;

    void _on_request_completed(int p_result, int p_code, const PackedStringArray& headers, const PackedByteArray& p_data);

protected:
    static void _bind_methods();

public:
    OpenAIRequest();
    Error request_scene(const String& prompt);
    void set_api_key(const String& key);
};

#endif // OPENAI_REQUEST_H