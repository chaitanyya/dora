#include "chat_dock.h"

#include "ai_builder/oai_req.h"
#include "core/io/json.h"
#include "editor/editor_file_system.h"
#include "modules/gdscript/gdscript.h"
#include "scene/2d/physics/character_body_2d.h"
#include "scene/2d/physics/collision_shape_2d.h"
#include "scene/2d/physics/static_body_2d.h"
#include "scene/2d/sprite_2d.h"
#include "scene/resources/2d/rectangle_shape_2d.h"
#include "scene/resources/packed_scene.h"
#include "scene/gui/margin_container.h"
#include "scene/gui/panel_container.h"
#include "scene/resources/style_box_flat.h"
#include "scene/gui/label.h"

#include "scene/2d/animated_sprite_2d.h"

#include "ai_builder/scene_builder.h"
#include "editor_string_names.h"

void ChatDock::_bind_methods() {
}

void ChatDock::gather_project_resources(Dictionary &p_resources) {
	// Get the filesystem singleton
	EditorFileSystem *efs = EditorFileSystem::get_singleton();
	if (!efs)
		return;

	// Create arrays to store different resource types
	Array scenes;
	Array scripts;
	Array textures;
	Array audio;
	Array shaders;
	Array materials;
	Array meshes;
	Array fonts;
	Array other_resources;

	std::function<void(EditorFileSystemDirectory *)> scan_dir = [&](EditorFileSystemDirectory *p_dir) {
		// Scan all files in current directory
		for (int i = 0; i < p_dir->get_file_count(); i++) {
			String type = p_dir->get_file_type(i);
			String path = p_dir->get_file_path(i);

			if (type == "PackedScene") {
				scenes.push_back(path);
			} else if (type == "GDScript") {
				scripts.push_back(path);
			} else if (type == "Texture2D" || type == "ImageTexture" || type == "CompressedTexture2D") {
				textures.push_back(path);
			} else if (type == "AudioStream" || type == "AudioStreamOGGVorbis" || type == "AudioStreamMP3") {
				audio.push_back(path);
			} else if (type == "Shader" || type == "VisualShader") {
				shaders.push_back(path);
			} else if (type == "Material" || type == "ShaderMaterial" || type == "StandardMaterial3D") {
				materials.push_back(path);
			} else if (type == "Mesh" || type == "ArrayMesh" || type == "PrimitiveMesh") {
				meshes.push_back(path);
			} else if (type == "Font" || type == "FontFile" || type == "DynamicFont") {
				fonts.push_back(path);
			} else if (!type.is_empty()) {
				other_resources.push_back(path);
			}
		}

		// Recursively scan subdirectories
		for (int i = 0; i < p_dir->get_subdir_count(); i++) {
			scan_dir(p_dir->get_subdir(i));
		}
	};

	// Start scanning from root
	scan_dir(efs->get_filesystem());

	// Store in dictionary
	p_resources["scenes"] = scenes;
	p_resources["scripts"] = scripts;
	p_resources["textures"] = textures;
	p_resources["audio"] = audio;
	p_resources["shaders"] = shaders;
	p_resources["materials"] = materials;
	p_resources["meshes"] = meshes;
	p_resources["fonts"] = fonts;
	p_resources["other"] = other_resources;
}

void ChatDock::gather_current_scene_info(Dictionary &p_scene_info) {
	Node *edited_scene = get_tree()->get_edited_scene_root();
	if (!edited_scene)
		return;

	// Helper function to recursively gather node information
	std::function<Dictionary(Node *)> gather_node_info = [&](Node *p_node) -> Dictionary {
		Dictionary node_info;
		node_info["name"] = p_node->get_name();
		node_info["class_name"] = p_node->get_class();

		// Get node properties
		List<PropertyInfo> props;
		p_node->get_property_list(&props);
		Dictionary properties;
		for (const PropertyInfo &E : props) {
			if (!(E.usage & PROPERTY_USAGE_STORAGE)) {
				continue;
			}
			properties[E.name] = p_node->get(E.name);
		}

		node_info["properties"] = properties;

		// Gather child nodes
		Array children;
		for (int i = 0; i < p_node->get_child_count(); i++) {
			children.push_back(gather_node_info(p_node->get_child(i)));
		}
		node_info["children"] = children;

		return node_info;
	};

	p_scene_info["root"] = gather_node_info(edited_scene);
	p_scene_info["scene_file"] = edited_scene->get_scene_file_path();
}

void ChatDock::_text_submitted(const String &p_text) {
	if (p_text.is_empty()) {
		return;
	}

	_add_message(p_text, MESSAGE_USER);
    _add_log("Sending request to OpenAI");

	Dictionary project_resources;
	Dictionary current_scene;

	gather_project_resources(project_resources);
	gather_current_scene_info(current_scene);

	// log all animations that are available in the scene;

	OpenAIRequest *oai_request = memnew(OpenAIRequest);
	add_child(oai_request);

	oai_request->connect("scene_received", callable_mp(this, &ChatDock::_on_response_received));
	oai_request->connect("request_failed", callable_mp(this, &ChatDock::_on_request_failed));

	// Defer the request to ensure we're in the scene trewe
	oai_request->call_deferred("request_scene", p_text, project_resources, current_scene);

	input_field->set_text("");
}

void ChatDock::_on_response_received(const Dictionary &p_scene_data) {
    // Check and display message if present
    if (p_scene_data.has("message")) {
        String message = p_scene_data["message"];
        _add_message(message, MESSAGE_SYSTEM);
    }

    // Only process scene if tasks are present
    if (p_scene_data.has("tasks")) {
        Array tasks = p_scene_data["tasks"];
        if (!tasks.is_empty()) {
            _process_scene_changes(tasks);
        }
    }

    // Cleanup request object
    OpenAIRequest *request = Object::cast_to<OpenAIRequest>(get_child(get_child_count() - 1));
    if (request) {
        request->queue_free();
    }
}
void ChatDock::_process_scene_changes(const Array &p_tasks) {
    Ref<PackedScene> packed_scene;

    if (FileAccess::exists("res://main.tscn")) {
        packed_scene = ResourceLoader::load("res://main.tscn");
    } else {
        packed_scene.instantiate();
        Node2D *initial_root = memnew(Node2D);
        initial_root->set_name("Root");
        packed_scene->pack(initial_root);
        memdelete(initial_root);
    }

    Node2D *root;
    Node *root_node = get_tree()->get_edited_scene_root();
    
    if (root_node) {
        root = Object::cast_to<Node2D>(root_node);
        if (!root) {
            _add_log("Failed to get root node.");
            return;
        }
    } else {
        Node *existing_root = packed_scene->instantiate();
        root = Object::cast_to<Node2D>(existing_root);
    }

    // Update scene with new data
    Ref<SceneBuilder> builder = memnew(SceneBuilder);
    print_line("OpenAI response: " + JSON::stringify(p_tasks));

    builder->create_scene_from_dict(p_tasks, root);
    packed_scene->pack(root);

    Error save_err = ResourceSaver::save(packed_scene, "res://main.tscn");
    if (save_err == OK) {
        _add_log("The scene changes have been applied.");
    } else {
        _add_log("Failed to save scene.");
    }
}

void ChatDock::_add_message(const String &p_text, MessageType p_type) {
    // Create container for the message
    PanelContainer *msg_container = memnew(PanelContainer);
    msg_container->set_h_size_flags(SIZE_SHRINK_END | SIZE_FILL);

    msg_container->add_theme_constant_override("margin_left", 10);
    msg_container->add_theme_constant_override("margin_right", 10);
    msg_container->add_theme_constant_override("margin_top", 5);
    msg_container->add_theme_constant_override("margin_bottom", 5);
    
    VBoxContainer *content_vbox = memnew(VBoxContainer);
    content_vbox->set_h_size_flags(SIZE_EXPAND_FILL);
    
    // Sender label
    Label *sender_label = memnew(Label);
    sender_label->set_text(p_type == MESSAGE_USER ? "USER" : "SYSTEM");
    sender_label->add_theme_font_size_override("font_size", get_theme_font_size("font_size") * 0.8);
    sender_label->add_theme_color_override("font_color", Color(0.6, 0.6, 0.6));
    sender_label->add_theme_font_override("font", get_theme_font(SNAME("bold"), EditorStringName(EditorFonts)));
    content_vbox->add_child(sender_label);
    
    // Message label
    RichTextLabel *msg_label = memnew(RichTextLabel);
    msg_label->set_use_bbcode(true);

    Ref<Font> mono_font = get_theme_font(SNAME("source"), EditorStringName(EditorFonts));
    msg_label->add_theme_font_override("mono_font", mono_font);

    msg_label->set_text(p_text);
    msg_label->set_selection_enabled(true);
    msg_label->set_h_size_flags(SIZE_EXPAND_FILL);
    msg_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
    sender_label->add_theme_font_size_override("font_size", get_theme_font_size("font_size") * 0.9);
    msg_label->set_custom_minimum_size(Size2(100, 0));
    msg_label->set_fit_content(true);
    msg_label->set_scroll_active(false);
    content_vbox->add_child(msg_label);
    
    // Create and configure the stylebox for the background
    Ref<StyleBoxFlat> style = memnew(StyleBoxFlat);
    style->set_corner_radius_all(5);
    style->set_content_margin_all(10);
    style->set_bg_color(Color(1, 1, 1));
    style->set_border_color(Color(0.8, 0.8, 0.8));
    
    msg_container->add_theme_style_override("panel", style);
    msg_container->add_child(content_vbox);
    
    message_container->add_child(msg_container);
    scroll_container->set_v_scroll(scroll_container->get_v_scroll_bar()->get_max());
}

void ChatDock::_add_log(const String &p_text) {
    Label *note_label = memnew(Label);
    note_label->set_text(p_text);
    note_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    note_label->add_theme_font_size_override("font_size", get_theme_font_size("font_size") * 0.8);
    note_label->add_theme_color_override("font_color", Color(0.6, 0.6, 0.6));
    
    message_container->add_child(note_label);
    scroll_container->set_v_scroll(scroll_container->get_v_scroll_bar()->get_max());
}


void ChatDock::_on_request_failed(const String &p_error) {
	OpenAIRequest *request = Object::cast_to<OpenAIRequest>(get_child(get_child_count() - 1));
	if (request) {
		request->queue_free();
	}
}

ChatDock::ChatDock() {
	set_name("Chat");

	// Replace the RichTextLabel with a VBoxContainer
    chat_container = memnew(VBoxContainer);
    chat_container->set_v_size_flags(SIZE_EXPAND_FILL);
    chat_container->set_h_size_flags(SIZE_EXPAND_FILL);
    add_child(chat_container);

    // Create a ScrollContainer to handle scrolling
    scroll_container = memnew(ScrollContainer);
    scroll_container->set_v_size_flags(SIZE_EXPAND_FILL);
    scroll_container->set_h_size_flags(SIZE_EXPAND_FILL);
    chat_container->add_child(scroll_container);

    // Add the message container
    message_container = memnew(VBoxContainer);
    message_container->set_h_size_flags(SIZE_EXPAND_FILL);
    scroll_container->add_child(message_container);

	// Create input field
	input_field = memnew(LineEdit);
	input_field->set_h_size_flags(SIZE_EXPAND_FILL);
	input_field->set_placeholder(TTR("Type your message..."));
	input_field->connect(SceneStringName(text_submitted), callable_mp(this, &ChatDock::_text_submitted));
	add_child(input_field);
}