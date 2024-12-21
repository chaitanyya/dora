#include "anth_req.h"
#include "core/io/json.h"
#include "core/variant/variant.h"
#include "prompt_template.h"
#include "editor/editor_settings.h"

AnthropicRequest::AnthropicRequest() {
    http_request = memnew(HTTPRequest);
    add_child(http_request);
    http_request->set_use_threads(true);

    // Connect signal immediately instead of deferring
    print_line("Connecting request_completed signal...");
    Error err = http_request->connect("request_completed", callable_mp(this, &AnthropicRequest::_on_request_completed));
    if (err != OK) {
        WARN_PRINT("Failed to connect request_completed signal! Error: " + itos(err));
    } else {
        print_line("Successfully connected request_completed signal");
    }

    String stored_key = EditorSettings::get_singleton()->get("artifical_intelligence/models/anthropic/api_key");
    if (!stored_key.is_empty()) {
        set_api_key(stored_key);
        print_line("API key loaded from editor settings");
    } else {
        WARN_PRINT("Anthropic API key not found. Please set it in Editor Settings -> Artifical Intelligence -> Models -> Anthropic");
    }
}

Error AnthropicRequest::request_scene(const String& prompt, const Dictionary& project_resources, const Dictionary& current_scene) {
    // Prepare headers
    Vector<String> headers;
    headers.push_back("Content-Type: application/json");
    headers.push_back("x-api-key: " + api_key);  // Changed from Bearer auth
    headers.push_back("anthropic-version: 2023-06-01");  // Added version header

    // Prepare request body
    Dictionary request_data;
    request_data["model"] = EditorSettings::get_singleton()->get("artifical_intelligence/models/anthropic/model");
    request_data["max_tokens"] = 2000;
    request_data["temperature"] = 0.1;
    request_data["system"] = AIPromptTemplate::format_prompt(project_resources, current_scene);;

    Array messages;

    Dictionary user_message;
    user_message["role"] = "user";
    user_message["content"] = prompt;
    messages.push_back(user_message);

    request_data["messages"] = messages;

    String json_str = JSON::stringify(request_data);
    
    // Update API endpoint
    Error err = http_request->request(
        "https://api.anthropic.com/v1/messages",
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

void AnthropicRequest::_on_request_completed(int p_result, int p_code, const PackedStringArray& headers, const PackedByteArray& p_data) {
    print_line("Request completed with result code: " + itos(p_code));

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

    Array content = response.get("content", Array());
    if (content.is_empty()) {
        emit_signal("request_failed", "No content in response");
        return;
    }

    Dictionary first_content = content[0];
    String text_content = first_content.get("text", "");
    
    if (text_content.is_empty()) {
        emit_signal("request_failed", "No text content in response");
        return;
    }
    
    // Parse the JSON content string into a Dictionary
    Error scene_json_err = json.parse(text_content);
    if (scene_json_err != OK) {
        emit_signal("request_failed", "Scene JSON parse error");
        return;
    }

    Dictionary scene_data = json.get_data();
    emit_signal("scene_received", scene_data);
    
    print_line("Response Data:");
    print_line(JSON::stringify(scene_data));
}

void AnthropicRequest::set_api_key(const String& key) {
    api_key = key;
}

void AnthropicRequest::_bind_methods() {
    ClassDB::bind_method(D_METHOD("request_scene", "prompt"), &AnthropicRequest::request_scene);
    ClassDB::bind_method(D_METHOD("set_api_key", "key"), &AnthropicRequest::set_api_key);
    
    ADD_SIGNAL(MethodInfo("scene_received", PropertyInfo(Variant::DICTIONARY, "scene_data")));
    ADD_SIGNAL(MethodInfo("request_failed", PropertyInfo(Variant::STRING, "error")));
}