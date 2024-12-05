#include "oai_req.h"
#include "core/io/json.h"
#include "core/variant/variant.h"

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

    // Try to load API key from environment
    char* env_key = "YOUT_API_KEY";
    if (env_key != nullptr) {
        set_api_key(String(env_key));
        print_line("API key loaded from environment");
    } else {
        WARN_PRINT("OpenAI API key not found. Please set the OPENAI_API_KEY environment variable.");
    }
}

Error OpenAIRequest::request_scene(const String& prompt) {
    // Prepare headers
    Vector<String> headers;
    headers.push_back("Content-Type: application/json");
    headers.push_back("Authorization: Bearer " + api_key);

    // Prepare request body
    Dictionary request_data;
    request_data["model"] = "gpt-4o";

    Array messages;
    Dictionary system_message;
    system_message["role"] = "system";
    
    Array content;
    Dictionary content_item;
    content_item["type"] = "text";
    content_item["text"] = "You are godot game engine version 4 expert, and your job is to return instructions in JSON Format that will be executed in the godot engine internally. Here is a sample request:\n\nRequest:\nCreate a character that is jumping on a surface.\n\nResponse:\n{\n  \"tasks\": [\n    {\n      \"action\": \"create_node\",\n      \"name\": \"MainScene\",\n      \"type\": \"Node2D\"\n    },\n    {\n      \"action\": \"create_node\",\n      \"name\": \"JumpingCharacterScene\",\n      \"type\": \"CharacterBody2D\"\n    },\n    {\n      \"action\": \"set_property\",\n      \"node\": \"JumpingCharacterScene\",\n      \"property\": \"position\",\n      \"value\": { \"x\": 400, \"y\": 100 }\n    },\n    {\n      \"action\": \"attach_script\",\n      \"node\": \"JumpingCharacterScene\",\n      \"language\": \"GDScript\",\n      \"code\": \"extends CharacterBody2D\\n\\nvar gravity = 500.0\\nvar jump_force = -400.0\\n\\nfunc _physics_process(delta):\\n    if not is_on_floor():\\n        velocity.y += gravity * delta\\n    if is_on_floor():\\n        velocity.y = jump_force\\n    move_and_slide()\"\n    },\n    {\n      \"action\": \"add_child\",\n      \"parent\": \"MainScene\",\n      \"child\": \"JumpingCharacterScene\"\n    },\n    {\n      \"action\": \"create_node\",\n      \"name\": \"CharacterSprite\",\n      \"type\": \"Sprite2D\"\n    },\n    {\n      \"action\": \"set_properties\",\n      \"node\": \"CharacterSprite\",\n      \"properties\": {\n        \"texture\": \"res://assets/sprites/knight.png\",\n        \"frame_width\": 35,\n        \"frame_height\": 35,\n        \"frame_index\": 0\n      }\n    },\n    {\n      \"action\": \"add_child\",\n      \"parent\": \"JumpingCharacterScene\",\n      \"child\": \"CharacterSprite\"\n    },\n    {\n      \"action\": \"create_node\",\n      \"name\": \"CharacterCollision\",\n      \"type\": \"CollisionShape2D\"\n    },\n    {\n      \"action\": \"set_properties\",\n      \"node\": \"CharacterCollision\",\n      \"properties\": {\n        \"shape_type\": \"RectangleShape2D\",\n        \"shape_extents\": { \"x\": 16, \"y\": 32 }\n      }\n    },\n    {\n      \"action\": \"add_child\",\n      \"parent\": \"JumpingCharacterScene\",\n      \"child\": \"CharacterCollision\"\n    },\n    {\n      \"action\": \"create_node\",\n      \"name\": \"Ground\",\n      \"type\": \"StaticBody2D\"\n    },\n    {\n      \"action\": \"set_property\",\n      \"node\": \"Ground\",\n      \"property\": \"position\",\n      \"value\": { \"x\": 0, \"y\": 200 }\n    },\n    {\n      \"action\": \"add_child\",\n      \"parent\": \"MainScene\",\n      \"child\": \"Ground\"\n    },\n    {\n      \"action\": \"create_node\",\n      \"name\": \"GroundCollision\",\n      \"type\": \"CollisionShape2D\"\n    },\n    {\n      \"action\": \"set_properties\",\n      \"node\": \"GroundCollision\",\n      \"properties\": {\n        \"shape_type\": \"RectangleShape2D\",\n        \"shape_extents\": { \"x\": 400, \"y\": 32 }\n      }\n    },\n    {\n      \"action\": \"add_child\",\n      \"parent\": \"Ground\",\n      \"child\": \"GroundCollision\"\n    }\n  ]\n}";
    content.push_back(content_item);

    system_message["content"] = content;
    messages.push_back(system_message);
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

    print_line("Sending OpenAI request with data: " + json_str);
    print_line("booo: " + json_str);
    
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