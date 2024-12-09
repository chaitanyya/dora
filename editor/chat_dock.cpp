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

	chat_history->push_color(get_theme_color(SNAME("accent_color"), EditorStringName(Editor)));
	chat_history->add_text("You: ");
	chat_history->pop(); // color
	chat_history->add_text(p_text + "\n");

	chat_history->push_color(get_theme_color(SNAME("value_color"), EditorStringName(Editor)));
	chat_history->add_text("Assistant: ");
	chat_history->pop(); // color
	chat_history->add_text("Generating instructions...\n");

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
	// TODO: remove hardcoded value
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
			chat_history->add_text("Error: Root node must be a Node2D\n");
			return;
		}
	} else {
		// Fallback to instantiating if no edited scene
		Node *existing_root = packed_scene->instantiate();
		root = Object::cast_to<Node2D>(existing_root);
	}

	// Update scene with new data
	Ref<SceneBuilder> builder = memnew(SceneBuilder);

	print_line("OpenAI response: " + JSON::stringify(p_scene_data));

	builder->create_scene_from_dict(p_scene_data, root);
	packed_scene->pack(root);

	Error save_err = ResourceSaver::save(packed_scene, "res://main.tscn");
	if (save_err == OK) {
		chat_history->push_color(get_theme_color(SNAME("success_color"), EditorStringName(Editor)));
		chat_history->add_text("The scene changes have been applied.\n");
	} else {
		chat_history->push_color(get_theme_color(SNAME("error_color"), EditorStringName(Editor)));
		chat_history->add_text("Failed to save scene.\n");
	}
	chat_history->add_text("\n");
	chat_history->pop();

	OpenAIRequest *request = Object::cast_to<OpenAIRequest>(get_child(get_child_count() - 1));
	if (request) {
		request->queue_free();
	}
}

void ChatDock::_on_request_failed(const String &p_error) {
	chat_history->add_text("Error: " + p_error + "\n");

	OpenAIRequest *request = Object::cast_to<OpenAIRequest>(get_child(get_child_count() - 1));
	if (request) {
		request->queue_free();
	}
}

ChatDock::ChatDock() {
	set_name("Chat");

	// Create chat history display
	chat_history = memnew(RichTextLabel);
	chat_history->set_v_size_flags(SIZE_EXPAND_FILL);
	chat_history->set_selection_enabled(true);
	chat_history->set_context_menu_enabled(true);
	chat_history->set_scroll_follow(true);
	add_child(chat_history);

	// Create input field
	input_field = memnew(LineEdit);
	input_field->set_h_size_flags(SIZE_EXPAND_FILL);
	input_field->set_placeholder(TTR("Type your message..."));
	input_field->connect(SceneStringName(text_submitted), callable_mp(this, &ChatDock::_text_submitted));
	add_child(input_field);
}