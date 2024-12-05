#include "scene_builder.h"

#include "core/object/class_db.h"
#include "modules/gdscript/gdscript.h"
#include "scene/2d/physics/collision_shape_2d.h"
#include "scene/2d/sprite_2d.h"
#include "scene/resources/2d/rectangle_shape_2d.h"

SceneBuilder::SceneBuilder() {
}

void SceneBuilder::_bind_methods() {
}

Node *SceneBuilder::create_scene_from_dict(const Dictionary &scene_data, Node *root) {
	ERR_FAIL_NULL_V(root, nullptr);

	// Process tasks sequentially
	if (scene_data.has("tasks")) {
		Array tasks = scene_data["tasks"];
		for (int i = 0; i < tasks.size(); i++) {
			Dictionary task = tasks[i];
			_process_task(task, root);
		}
	}

	return root;
}

void SceneBuilder::_process_task(const Dictionary &task, Node *root) {
	String action = task["action"];
    print_line("Processing task: " + action); 

	if (action == "create_node") {
		String name = task["name"];
		String type = task["type"];

		_create_node_of_type(type, name, root);

	} else if (action == "set_property" || action == "set_properties") {
		String node_name = task["node"];
		Node *node = root->get_node_or_null(NodePath(node_name));

		if (node) {
			if (action == "set_property") {
				String property = task["property"];
				Variant value = task["value"];
				_set_node_property(node, property, value);
			} else { // set_properties
				Dictionary properties = task["properties"];
				_setup_node_properties(node, properties);
			}
		} else {
			print_line("Node '" + node_name + "' not found when setting properties");
		}
	} else if (action == "attach_script") {
		String node_name = task["node"];
		Node *node = root->get_node_or_null(NodePath(node_name));

		if (node) {
			String language = task["language"];
			String code = task["code"];

			if (language == "GDScript") {
				Ref<GDScript> script = memnew(GDScript);
				script->set_source_code(code);
				script->reload();
				node->set_script(script);
			} else {
				print_line("Unsupported script language: " + language);
			}
		} else {
			print_line("Node '" + node_name + "' not found when attaching script");
		}
	} else if (action == "add_child") {
		String parent_name = task["parent"];
		String child_name = task["child"];

		Node *parent = root->find_child(parent_name, true, false);
        Node *child = root->find_child(child_name, true, false);

		if (parent && child) {
            // Unset the owner before removing from parent
            child->set_owner(nullptr);
            if (child->get_parent()) {
				child->get_parent()->remove_child(child);
			}
			parent->add_child(child);
			child->set_owner(root);
		} else {
			print_line(parent_name + " or "  + child_name + " node not found when adding child");
		}

	} else {
		print_line("Unknown action: " + action);
	}
}

Node *SceneBuilder::_create_node_of_type(const String &type, const String &name, Node *root) {
    if (!ClassDB::class_exists(type)) {
        print_line("Class type '" + type + "' does not exist");
        return nullptr;
    }

    Node *node = Object::cast_to<Node>(ClassDB::instantiate(type));
    if (!node) {
        print_line("Failed to instantiate node of type '" + type + "'");
        return nullptr;
    }

    // Validate name is not empty and set it
    String node_name = name.is_empty() ? type : name;
    node->set_name(node_name.validate_node_name());
    
    root->add_child(node);
    node->set_owner(root);

    return node;
}

void SceneBuilder::_set_node_property(Node *node, const String &property, const Variant &value) {
	if (property == "position") {
		if (Node2D *node2d = Object::cast_to<Node2D>(node)) {
			Dictionary pos = value;
			node2d->set_position(Vector2(pos["x"], pos["y"]));
		} else {
			print_line("Node '" + node->get_name() + "' is not a Node2D, cannot set position");
		}
	} else {
		node->set(property, value);
	}
}

void SceneBuilder::_setup_node_properties(Node *node, const Dictionary &props) {
	// Handle position if present
	if (props.has("position")) {
		if (Node2D *node2d = Object::cast_to<Node2D>(node)) {
			Dictionary pos = props["position"];
			node2d->set_position(Vector2(pos["x"], pos["y"]));
		} else {
			print_line("Node '" + node->get_name() + "' is not a Node2D, cannot set position");
		}
	}

	// Handle specific node types
	if (CollisionShape2D *collision = Object::cast_to<CollisionShape2D>(node)) {
		_setup_collision_shape(collision, props);
	} else if (Sprite2D *sprite = Object::cast_to<Sprite2D>(node)) {
		_setup_sprite(sprite, props);
	} else {
		// Set generic properties
		for (const Variant *key = props.next(); key; key = props.next(key)) {
			node->set(String(*key), props[*key]);
		}
	}
}

void SceneBuilder::_setup_collision_shape(CollisionShape2D *collision, const Dictionary &props) {
	if (props.has("shape_type") && props["shape_type"] == "RectangleShape2D") {
		Dictionary extents = props["shape_extents"];
		Ref<RectangleShape2D> shape;
		shape.instantiate();
		shape->set_size(Vector2(extents["x"], extents["y"]) * 2.0);
		collision->set_shape(shape);
	} else {
		print_line("Unsupported shape type or missing 'shape_type' in properties");
	}
}

void SceneBuilder::_setup_sprite(Sprite2D *sprite, const Dictionary &props) {
	if (!props.has("texture")) {
		print_line("Missing 'texture' property for Sprite2D");
		return;
	}

	String texture_path = props["texture"];
	Ref<Texture2D> texture = ResourceLoader::load(texture_path);
	if (!texture.is_valid()) {
		print_line("Failed to load texture at path: " + texture_path);
		return;
	}

	sprite->set_texture(texture);

	if (props.has("frame_width") && props.has("frame_height")) {
		int frame_width = props["frame_width"];
		int frame_height = props["frame_height"];
		int frame_index = props.has("frame_index") ? int(props["frame_index"]) : 0;

		sprite->set_region_enabled(true);

		int frames_per_row = texture->get_width() / frame_width;
		int frame_x = (frame_index % frames_per_row) * frame_width;
		int frame_y = (frame_index / frames_per_row) * frame_height;

		sprite->set_region_rect(Rect2(frame_x, frame_y, frame_width, frame_height));
	}
}

void SceneBuilder::_setup_script(Node *node, const Dictionary &script_data) {
	if (script_data["language"] == "GDScript") {
		Ref<GDScript> script = memnew(GDScript);
		script->set_source_code(script_data["code"]);
		script->reload();
		node->set_script(script);
	} else {
		print_line("Unsupported script language: " + String(script_data["language"]));
	}
}