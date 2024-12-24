#include "oai_req.h"
#include "core/io/json.h"
#include "core/variant/variant.h"
#include "prompt_template.h"
#include "editor/editor_settings.h"

void OpenAIRequest::_init_base_request() {
    base_request_data.clear();
    base_request_data["model"] = EditorSettings::get_singleton()->get("artifical_intelligence/models/openai/model");
    base_request_data["temperature"] = 0.2;
    base_request_data["max_tokens"] = 2048;
    base_request_data["top_p"] = 1;
    base_request_data["frequency_penalty"] = 0;
    base_request_data["presence_penalty"] = 0;
    
    Dictionary response_format;
    response_format["type"] = "json_object";

    base_request_data["response_format"] = response_format;
}

void OpenAIRequest::clear_message_history() {
    message_history.clear();
}

OpenAIRequest::OpenAIRequest() {
    http_request = memnew(HTTPRequest);
    add_child(http_request);
    http_request->set_use_threads(true);
    message_history = Array();

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
    } else {
        WARN_PRINT("OpenAI API key not found. Please set it in Editor Settings -> Artifical Intelligence -> Models -> OpenAI");
    }
}

Dictionary OpenAIRequest::_create_message(const String& role, const String& text) const {
    Dictionary message;
    message["role"] = role;
    message["content"] = _create_message_content(text);
    return message;
}

Array OpenAIRequest::_create_message_content(const String& text) const {
    Array content;
    Dictionary content_item;
    content_item["type"] = "text";
    content_item["text"] = text;
    content.push_back(content_item);
    return content;
}

Error OpenAIRequest::request_scene(const String& prompt, const Dictionary& project_resources, const Dictionary& current_scene) {
    if (base_request_data.is_empty()) {
        _init_base_request();
    }

    Dictionary request_data = base_request_data;
    Dictionary user_message = _create_message("user", prompt);
    
    // Check if we need to add system message
    if (message_history.is_empty()) {
        message_history.push_back(_create_message("system", AIPromptTemplate::format_prompt(project_resources, current_scene)));
    }
    
    message_history.push_back(user_message);
    request_data["messages"] = message_history;

    static Vector<String> headers = {
        "Content-Type: application/json",
        "Authorization: Bearer " + api_key
    };

    Error err = http_request->request(
        "https://api.openai.com/v1/chat/completions",
        headers,
        HTTPClient::METHOD_POST,
        JSON::stringify(request_data)
    );

    if (err == OK) {
        print_line("HTTP request initiated successfully");
    } else {
        WARN_PRINT("Failed to send HTTP request: " + itos(err));
        emit_signal("request_failed", "Failed to send request");
    }

    return err;
}

void OpenAIRequest::_on_request_completed(int p_result, int p_code, const PackedStringArray& headers, const PackedByteArray& p_data) {
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
    
    Error scene_json_err = json.parse(content);
    if (scene_json_err != OK) {
        emit_signal("request_failed", "Scene JSON parse error");
        return;
    }

    Dictionary scene_data = json.get_data();
    emit_signal("scene_received", scene_data);

    print_line("Response Data:");
    print_line(JSON::stringify(scene_data));

    message_history.push_back(_create_message("assistant", JSON::stringify(scene_data)));
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