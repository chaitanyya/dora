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

	// Add message to history
	messages.push_back(p_text);
	chat_history->add_text("You: " + p_text + "\n");
	chat_history->add_text("Generating instructions...\n");

	Dictionary project_resources;
	Dictionary current_scene;

	gather_project_resources(project_resources);
	gather_current_scene_info(current_scene);

    // log all animations that are available in the scene;

	OpenAIRequest* oai_request = memnew(OpenAIRequest);
	add_child(oai_request);

	oai_request->connect("scene_received", callable_mp(this, &ChatDock::_on_response_received));
	oai_request->connect("request_failed", callable_mp(this, &ChatDock::_on_request_failed));

	// Defer the request to ensure we're in the scene trewe
	oai_request->call_deferred("request_scene", p_text, project_resources, current_scene);

// 	String json_str = R"({
//     "tasks": [
//         {
//             "action": "create_node",
//             "name": "Game",
//             "class_name": "Node2D"
//         },
//         {
//             "action": "create_node",
//             "name": "CharacterBody2D",
//             "class_name": "CharacterBody2D"
//         },
//         {
//             "action": "add_child",
//             "parent": "Game",
//             "child": "CharacterBody2D"
//         },
//         {
//             "action": "create_node",
//             "name": "PlayerSprite",
//             "class_name": "AnimatedSprite2D"
//         },
//         {
//             "action": "set_properties",
//             "node": "PlayerSprite",
//             "properties": {
//                 "sprite_frames": {
//                     "texture": "res://assets/sprites/knight.png",
//                     "frames_horizontal_count": 8,
//                     "frames_vertical_count": 8,
//                     "frames_count": 4,
//                     "animation_name": "walk",
//                     "frame_duration": 0.1,
//                     "autoplay": true
//                 },
//                 "position": {
//                     "x": 0,
//                     "y": 0
//                 }
//             }
//         },
//         {
//             "action": "add_child",
//             "parent": "CharacterBody2D",
//             "child": "PlayerSprite"
//         },
//         {
//             "action": "create_node",
//             "name": "CharacterCollision",
//             "class_name": "CollisionShape2D"
//         },
//         {
//             "action": "set_properties",
//             "node": "CharacterCollision",
//             "properties": {
//                 "shape_type": "CircleShape2D",
//                 "shape_radius": 6,
//                 "shape_position": {"x": 0, "y": 0}
//             }
//         },
//         {
//             "action": "add_child",
//             "parent": "CharacterBody2D",
//             "child": "CharacterCollision"
//         },
//         {
//             "action": "create_node",
//             "name": "MainCamera",
//             "class_name": "Camera2D"
//         },
//         {
//             "action": "set_properties",
//             "node": "MainCamera",
//             "properties": {
//                 "position": {"x": 0, "y": 0},
//                 "zoom": {"x": 4, "y": 4}
//             }
//         },
//         {
//             "action": "add_child",
//             "parent": "Game",
//             "child": "MainCamera"
//         },
//         {
//             "action": "create_node",
//             "name": "GameTileMap",
//             "class_name": "TileMap"
//         },
//         {
//             "action": "add_child",
//             "parent": "Game",
//             "child": "GameTileMap"
//         },
//         {
//             "action": "set_properties",
//             "node": "GameTileMap",
//             "properties": {
//                 "tileset": {
//                     "tile_width": 16,
//                     "tile_height": 16,
//                     "texture": "res://assets/sprites/world_tileset.png"
//                 }
//             }
//         }
//     ]
// })";
	// Dictionary scene_data = JSON::parse_string(json_str);
	// _on_response_received(scene_data);

	input_field->set_text("");
}

void ChatDock::_on_response_received(const Dictionary &p_scene_data) {
	// Create main scene
	Node2D *root = memnew(Node2D);
	root->set_name("RootNode");

	Ref<SceneBuilder> builder = memnew(SceneBuilder);
	builder->create_scene_from_dict(p_scene_data, root);

	Ref<PackedScene> packed_scene;
	packed_scene.instantiate();
	packed_scene->pack(root);

	Error save_err = ResourceSaver::save(packed_scene, "res://jumping_character.tscn");
	if (save_err == OK) {
		chat_history->add_text("Created jumping character scene at res://jumping_character.tscn\n");
	} else {
		chat_history->add_text("Failed to save scene.\n");
	}

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