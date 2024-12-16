#include "oai_req.h"
#include "core/io/json.h"
#include "core/variant/variant.h"
#include "prompt_template.h"
#include "editor/editor_settings.h"

OpenAIRequest::OpenAIRequest() {
    http_request = memnew(HTTPRequest);
    add_child(http_request);
    http_request->set_use_threads(true);

    // Connect signal immediately instead of deferring
    print_line("Connecting request_completed signal...");
    Error err = http_request->connect("request_completed", callable_mp(this, &OpenAIRequest::_on_request_completed));
    if (err != OK) {
        WARN_PRINT("Failed to connect request_completed signal! Error: " + itos(err));
    } else {
        print_line("Successfully connected request_completed signal");
    }

    String stored_key = EditorSettings::get_singleton()->get("artifical_intelligence/models/openai/api_key");
    if (!stored_key.is_empty()) {
        set_api_key(stored_key);
        print_line("API key loaded from editor settings");
    } else {
        WARN_PRINT("OpenAI API key not found. Please set it in Editor Settings -> Artifical Intelligence -> Models -> OpenAI");
    }
}

Error OpenAIRequest::request_scene(const String& prompt, const Dictionary& project_resources, const Dictionary& current_scene) {
    // Prepare headers
    Vector<String> headers;
    headers.push_back("Content-Type: application/json");
    headers.push_back("Authorization: Bearer " + api_key);

    // Prepare request body
    Dictionary request_data;
    request_data["model"] = EditorSettings::get_singleton()->get("artifical_intelligence/models/openai/model");

    Array messages;
    Dictionary system_message;
    system_message["role"] = "system";
    
    Array content;
    Dictionary content_item;
    content_item["type"] = "text";
    content_item["text"] = OAIPromptTemplate::format_prompt(project_resources, current_scene);
    content.push_back(content_item);

    system_message["content"] = content;
    messages.push_back(system_message);

    Dictionary user_message;
    user_message["role"] = "user";
    
    Array user_content;
    Dictionary user_content_item;
    user_content_item["type"] = "text";
    user_content_item["text"] = prompt;
    user_content.push_back(user_content_item);
    
    user_message["content"] = user_content;
    messages.push_back(user_message);

    request_data["messages"] = messages;

    // Add additional parameters
    request_data["temperature"] = 0.2;
    request_data["max_tokens"] = 2048;
    request_data["top_p"] = 1;
    request_data["frequency_penalty"] = 0;
    request_data["presence_penalty"] = 0;

    Dictionary response_format;
    response_format["type"] = "json_object"; // Ensure this matches the expected format
    request_data["response_format"] = response_format;

    String json_str = JSON::stringify(request_data);
    
    // Make request
    Error err = http_request->request(
        "https://api.openai.com/v1/chat/completions",
        headers,
        HTTPClient::METHOD_POST,
        json_str
    );

    if (err != OK) {
        WARN_PRINT("Failed to send HTTP request: " + itos(err));
        emit_signal("request_failed", "Failed to send request");
    } else {
        print_line("HTTP request initiated successfully");
    }

    return err;
}

void OpenAIRequest::_on_request_completed(int p_result, int p_code, const PackedStringArray& headers, const PackedByteArray& p_data) {
    print_line("Request completed with result: " + itos(p_result) + ", code: " + itos(p_code));

    if (p_result != HTTPRequest::RESULT_SUCCESS) {
        emit_signal("request_failed", "HTTP Request failed");
        return;
    }

    if (p_code != 200) {
        emit_signal("request_failed", "Invalid response code: " + String::num(p_code));
        return;
    }

    String response_text;
    response_text.parse_utf8((const char*)p_data.ptr(), p_data.size());

    JSON json;
    Error json_err = json.parse(response_text);
    
    if (json_err != OK) {
        emit_signal("request_failed", "JSON parse error");
        return;
    }

    Dictionary response = json.get_data();

    Array choices = response.get("choices", Array());
    if (choices.is_empty()) {
        emit_signal("request_failed", "No choices in response");
        return;
    }

    Dictionary choice = choices[0].operator Dictionary();
    Dictionary message = choice.get("message", Dictionary());
    String content = message.get("content", "");
    
    if (content.is_empty()) {
        emit_signal("request_failed", "No content in response");
        return;
    }
    
    // Parse the JSON content string into a Dictionary
    Error scene_json_err = json.parse(content);
    if (scene_json_err != OK) {
        emit_signal("request_failed", "Scene JSON parse error");
        return;
    }

    Dictionary scene_data = json.get_data();
    emit_signal("scene_received", scene_data);
}

void OpenAIRequest::set_api_key(const String& key) {
    api_key = key;
}

void OpenAIRequest::_bind_methods() {
    ClassDB::bind_method(D_METHOD("request_scene", "prompt"), &OpenAIRequest::request_scene);
    ClassDB::bind_method(D_METHOD("set_api_key", "key"), &OpenAIRequest::set_api_key);
    
    ADD_SIGNAL(MethodInfo("scene_received", PropertyInfo(Variant::DICTIONARY, "scene_data")));
    ADD_SIGNAL(MethodInfo("request_failed", PropertyInfo(Variant::STRING, "error")));
}