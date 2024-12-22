#include "chat_dock.h"

#include "ai_builder/oai_req.h"
#include "core/io/json.h"
#include "editor/editor_file_system.h"
#include "modules/gdscript/gdscript.h"
#include "scene/2d/physics/character_body_2d.h"
#include "scene/2d/physics/collision_shape_2d.h"
#include "scene/2d/physics/static_body_2d.h"
#include "scene/2d/sprite_2d.h"
#include "scene/gui/label.h"
#include "scene/gui/margin_container.h"
#include "scene/gui/option_button.h"
#include "scene/gui/panel_container.h"
#include "scene/resources/2d/rectangle_shape_2d.h"
#include "scene/resources/packed_scene.h"
#include "scene/resources/style_box_flat.h"

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

Dictionary ChatDock::gather_node_info(Node *p_node) {
	Dictionary node_info;

	if (!p_node) {
		return node_info;
	}

	node_info["name"] = p_node->get_name();
	node_info["class"] = p_node->get_class();
	node_info["path"] = p_node->get_path();

	List<PropertyInfo> props;
	p_node->get_property_list(&props);
	Dictionary properties;

	for (const PropertyInfo &E : props) {
		if (!(E.usage & PROPERTY_USAGE_STORAGE)) {
			continue;
		}

		Variant value = p_node->get(E.name);

		// Handle special property types
		if (value.get_type() == Variant::OBJECT) {
			Object *obj = value;
			if (obj) {
				Resource *res = Object::cast_to<Resource>(obj);
				if (res && !res->get_path().is_empty()) {
					properties[E.name] = res->get_path();
					continue;
				}
				Node *node_ref = Object::cast_to<Node>(obj);
				if (node_ref) {
					properties[E.name] = p_node->get_path_to(node_ref);
					continue;
				}
			}
		}

		properties[E.name] = value;
	}

	node_info["properties"] = properties;

	Array groups;
	List<Node::GroupInfo> group_info;
	p_node->get_groups(&group_info);
	for (const Node::GroupInfo &E : group_info) {
		if (E.persistent) {
			groups.push_back(E.name);
		}
	}
	node_info["groups"] = groups;

	Array children;
	for (int i = 0; i < p_node->get_child_count(); i++) {
		children.push_back(gather_node_info(p_node->get_child(i)));
	}
	node_info["children"] = children;

	return node_info;
}

void ChatDock::_text_submitted(const String &p_text) {
	if (p_text.is_empty()) {
		return;
	}

	_add_message(p_text, MESSAGE_USER, Array());
	_add_log("Sending request to LLM");

	Dictionary project_resources;
	Dictionary current_nodes;

	gather_project_resources(project_resources);

	for (const Variant &node_var : selected_nodes) {
		Node *selected_node = Object::cast_to<Node>(node_var);
		if (selected_node) {
			current_nodes[selected_node->get_name()] = gather_node_info(selected_node);
		}
	}

	OpenAIRequest *oai_request = memnew(OpenAIRequest);
	add_child(oai_request);

	oai_request->connect("scene_received", callable_mp(this, &ChatDock::_on_response_received));
	oai_request->connect("request_failed", callable_mp(this, &ChatDock::_on_request_failed));

	// Defer the request to ensure we're in the scene trewe
	oai_request->call_deferred("request_scene", p_text, project_resources, current_nodes);

	input_field->set_text("");
}

void ChatDock::_on_response_received(const Dictionary &p_scene_data) {
	// Check and display message if present
	if (p_scene_data.has("message")) {
		String message = p_scene_data["message"];
		Array tasks = p_scene_data.has("tasks") ? Array(p_scene_data["tasks"]) : Array();
		_add_message(message, MESSAGE_SYSTEM, tasks);
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

	builder->create_scene_from_dict(p_tasks, root);
	packed_scene->pack(root);

	Error save_err = ResourceSaver::save(packed_scene, "res://main.tscn");
	if (save_err == OK) {
		_add_log("The scene changes have been applied.");
	} else {
		_add_log("Failed to save scene.");
	}
}

void ChatDock::_add_message(const String &p_text, MessageType p_type, const Array &p_tasks) {
	// Create container for the message
	PanelContainer *msg_container = memnew(PanelContainer);
	msg_container->set_h_size_flags(SIZE_SHRINK_END | SIZE_FILL);

	msg_container->add_theme_constant_override("margin_left", 10);
	msg_container->add_theme_constant_override("margin_right", 10);
	msg_container->add_theme_constant_override("margin_top", 5);
	msg_container->add_theme_constant_override("margin_bottom", 5);

	VBoxContainer *content_vbox = memnew(VBoxContainer);
	content_vbox->set_h_size_flags(SIZE_EXPAND_FILL);

	// Header with sender and apply button
	HBoxContainer *header_hbox = memnew(HBoxContainer);
	header_hbox->set_h_size_flags(SIZE_EXPAND_FILL);

	HBoxContainer *left_container = memnew(HBoxContainer);
	left_container->set_h_size_flags(SIZE_EXPAND_FILL);
	header_hbox->add_child(left_container);

	// Sender label
	Label *sender_label = memnew(Label);
	sender_label->set_text(p_type == MESSAGE_USER ? "USER" : "SYSTEM");
	sender_label->add_theme_font_size_override("font_size", get_theme_font_size("font_size") * 0.8);
	sender_label->add_theme_color_override("font_color", Color(0.6, 0.6, 0.6));
	sender_label->add_theme_font_override("font", get_theme_font(SNAME("bold"), EditorStringName(EditorFonts)));
	left_container->add_child(sender_label);

	// Add Apply button if tasks are present
	if (!p_tasks.is_empty()) {
		Button *apply_button = memnew(Button);
		apply_button->set_text("Apply Changes");
		apply_button->add_theme_font_size_override("font_size", get_theme_font_size("font_size") * 0.8);
		apply_button->add_theme_color_override("font_color", Color(0.6, 0.6, 0.6));
		apply_button->add_theme_font_override("font", get_theme_font(SNAME("bold"), EditorStringName(EditorFonts)));
		apply_button->set_h_size_flags(SIZE_SHRINK_END);
		apply_button->connect(SceneStringName(pressed), callable_mp(this, &ChatDock::_process_scene_changes).bind(p_tasks));
		header_hbox->add_child(apply_button);
	}

	content_vbox->add_child(header_hbox);

	// Message label
	RichTextLabel *msg_label = memnew(RichTextLabel);
	msg_label->set_use_bbcode(true);

	Ref<Font> mono_font = get_theme_font(SNAME("source"), EditorStringName(EditorFonts));
	msg_label->add_theme_font_override("mono_font", mono_font);

	msg_label->set_text(p_text);
	msg_label->set_selection_enabled(true);
	msg_label->set_h_size_flags(SIZE_EXPAND_FILL);
	msg_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	msg_label->add_theme_font_size_override("font_size", get_theme_font_size("font_size") * 0.8);
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

void ChatDock::_update_selected_node_label() {
	// Clear existing pills
	for (int i = node_pill->get_child_count() - 1; i >= 0; i--) {
		node_pill->get_child(i)->queue_free();
	}

	// Create container for pills
	HBoxContainer *pills_container = memnew(HBoxContainer);
	pills_container->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	pills_container->add_theme_constant_override("separation", 5);
	node_pill->add_child(pills_container);

	EditorSelection *editor_selection = EditorNode::get_singleton()->get_editor_selection();
	selected_nodes = editor_selection->get_selected_nodes();

	static Ref<StyleBoxFlat> pill_style;
	if (pill_style.is_null()) {
		pill_style = memnew(StyleBoxFlat);
		pill_style->set_corner_radius_all(8);
		pill_style->set_content_margin_individual(8, 6, 8, 6);
		pill_style->set_bg_color(Color(0.7, 0.7, 0.7, 0.2));
	}

	if (selected_nodes.size() > 0) {
		for (const Variant &node_var : selected_nodes) {
			Node *selected_node = Object::cast_to<Node>(node_var);
			if (!selected_node) {
				continue;
			}

			PanelContainer *pill = memnew(PanelContainer);
			pill->add_theme_style_override("panel", pill_style);

			Label *node_label = memnew(Label);
			node_label->set_text(selected_node->get_name());
			node_label->add_theme_font_size_override("font_size", 18);
			node_label->set_tooltip_text(selected_node->get_path());
			pill->add_child(node_label);

			pills_container->add_child(pill);
		}
	} else {
		PanelContainer *pill = memnew(PanelContainer);
		pill->add_theme_style_override("panel", pill_style);

		Label *no_selection_label = memnew(Label);
		no_selection_label->set_text("No Node Selected");
		no_selection_label->add_theme_font_size_override("font_size", 18);
		no_selection_label->add_theme_color_override("font_color", Color(0.5, 0.5, 0.5));
		pill->add_child(no_selection_label);

		pills_container->add_child(pill);
	}
}

void ChatDock::_on_selection_changed() {
	_update_selected_node_label();
}

void ChatDock::_text_editor_gui_input(const Ref<InputEvent> &p_event) {
	Ref<InputEventKey> k = p_event;

	if (k.is_valid() && k->is_pressed()) {
		if (k->is_action_pressed("ui_text_submit") && k->is_command_or_control_pressed()) {
			String text = input_field->get_text().strip_edges();
			if (!text.is_empty()) {
				_text_submitted(text);
			}
			accept_event();
		}
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

	// Create and configure the stylebox for the background
	Ref<StyleBoxFlat> input_style = memnew(StyleBoxFlat);
	input_style->set_corner_radius_all(5);
	input_style->set_content_margin_all(10);
	input_style->set_bg_color(Color(1, 1, 1));
	input_style->set_border_color(Color(0.8, 0.8, 0.8));
	input_style->set_border_width_all(1);

	PanelContainer *input_container = memnew(PanelContainer);
	input_container->set_h_size_flags(SIZE_EXPAND_FILL);
	input_container->set_v_size_flags(SIZE_SHRINK_END);
	input_container->add_theme_style_override("panel", input_style);

	VBoxContainer *input_vbox = memnew(VBoxContainer);
	input_vbox->set_v_size_flags(SIZE_SHRINK_END);
	input_container->add_child(input_vbox);

	node_pill = memnew(HBoxContainer);
	input_vbox->add_child(node_pill);

	// Connect to editor selection changes
	EditorSelection *editor_selection = EditorNode::get_singleton()->get_editor_selection();
	editor_selection->connect("selection_changed", callable_mp(this, &ChatDock::_on_selection_changed));
	_update_selected_node_label();

	// Create input field with custom height
	input_field = memnew(TextEdit);
	input_field->set_line_wrapping_mode(TextEdit::LINE_WRAPPING_BOUNDARY);
	input_field->add_theme_color_override("background_color", Color(1, 1, 1));
	input_field->set_context_menu_enabled(false);
	input_field->set_highlight_all_occurrences(false);
	input_field->set_highlight_current_line(false);
	input_field->set_draw_tabs(false);
	input_field->set_draw_spaces(false);
	input_field->set_v_scroll(false);
	input_field->set_fit_content_height_enabled(true);
	input_field->set_custom_minimum_size(Size2(0, 30));
	input_field->set_v_size_flags(SIZE_EXPAND_FILL);
	input_field->set_placeholder(TTR("Ask anything (Cmd + Enter to send)"));
	input_field->connect("gui_input", callable_mp(this, &ChatDock::_text_editor_gui_input));
	input_vbox->add_child(input_field);

	add_child(input_container);
}