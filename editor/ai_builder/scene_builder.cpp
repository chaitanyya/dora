#include "scene_builder.h"

#include "core/object/class_db.h"
#include "modules/gdscript/gdscript.h"
#include "scene/2d/physics/collision_shape_2d.h"
#include "scene/2d/sprite_2d.h"
#include "scene/2d/animated_sprite_2d.h"
#include "scene/resources/atlas_texture.h"
#include "core/config/project_settings.h"
#include "scene/2d/camera_2d.h"
#include "editor/plugins/tiles/tile_set_atlas_source_editor.h"

// 2D shapes
#include "scene/resources/2d/rectangle_shape_2d.h"
#include "scene/resources/2d/circle_shape_2d.h"
#include "scene/resources/2d/world_boundary_shape_2d.h"

SceneBuilder::SceneBuilder() {
}

void SceneBuilder::_bind_methods() {
}

Node *SceneBuilder::create_scene_from_dict(const Array &tasks, Node *root) {
    ERR_FAIL_NULL_V(root, nullptr);

    // Process tasks sequentially
    for (int i = 0; i < tasks.size(); i++) {
        Dictionary task = tasks[i];
        _process_task(task, root);
    }

    return root;
}

void SceneBuilder::_process_task(const Dictionary &task, Node *root) {
	String action = task["action"];
    print_line("Processing task: " + action); 

	if (action == "create_node") {
		String name = task["name"];
		String class_name = task["class_name"];

		_create_node_of_class(class_name, name, root);

	} else if (action == "set_properties") {
		String node_name = task["node"];
		Node *node = root->find_child(NodePath(node_name));

		if (node) {
            Dictionary properties = task["properties"];
            _setup_node_properties(node, properties);
		} else {
			print_line("Node '" + node_name + "' not found when setting properties");
		}
	} else if (action == "attach_script") {
		String node_name = task["node"];
		Node *node = root->find_child(NodePath(node_name));

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

void SceneBuilder::_setup_sprite_frame_animation(Node *node, const Dictionary &props) {
    AnimatedSprite2D *animated_sprite = Object::cast_to<AnimatedSprite2D>(node);
    ERR_FAIL_NULL(animated_sprite);

    // Create sprite frames resource
    Ref<SpriteFrames> frames;
    frames.instantiate();
    
    String texture_path = props["texture"];
    print_line("Attempting to load texture from: " + texture_path); // Debug line
    print_line("Project path: " + ProjectSettings::get_singleton()->get_resource_path());
    
    Ref<Texture2D> texture = ResourceLoader::load(texture_path);
    if (!texture.is_valid()) {
        print_line("Failed to load texture from path: " + texture_path); // Debug line
        ERR_FAIL_COND_MSG(!texture.is_valid(), "Could not load texture from path: " + texture_path);
        return;
    }

    // Get frame dimensions
    int texture_width = texture->get_width();
    int texture_height = texture->get_height();

    int frame_horizontal_count = props.has("frames_horizontal_count") ? int(props["frames_horizontal_count"]) : 1;
    int frame_vertical_count = props.has("frames_vertical_count") ? int(props["frames_vertical_count"]) : 1;

    int frames_to_use = props.has("frames_count") ? int(props["frames_count"]) : 1;

    String anim_name = props.has("animation_name") ? String(props["animation_name"]) : "default";
    float frame_duration = props.has("frame_duration") ? float(props["frame_duration"]) : 1.0;
    
    // Add animation to sprite frames
    frames->add_animation(anim_name);
    frames->set_animation_loop(anim_name, true);
    frames->set_animation_speed(anim_name, 1.0 / frame_duration);

    int frame_width = texture_width / frame_horizontal_count;
    int frame_height = texture_height / frame_vertical_count;

    // Add frames to animation
    for (int i = 0; i < frames_to_use; i++) {
        // Calculate row and column position in the grid
        int row = i / frame_horizontal_count;
        int col = i % frame_horizontal_count;
        
        // Calculate frame position in pixels
        int frame_x = col * frame_width;
        int frame_y = row * frame_height;
        
        Ref<AtlasTexture> atlas_texture;
        atlas_texture.instantiate();
        atlas_texture->set_atlas(texture);
        atlas_texture->set_region(Rect2(frame_x, frame_y, frame_width, frame_height));
    
        frames->add_frame(anim_name, atlas_texture);
    }

    // Set frames to sprite and configure animation
    animated_sprite->set_sprite_frames(frames);
    animated_sprite->set_animation(anim_name);
    
    if (props.has("autoplay") && bool(props["autoplay"])) {
        animated_sprite->set_autoplay(props["autoplay"]);
    }
}

Node *SceneBuilder::_create_node_of_class(const String &class_name, const String &name, Node *root) {
    if (!ClassDB::class_exists(class_name)) {
        print_line("Class name '" + class_name + "' does not exist");
        return nullptr;
    }

    Node *node = Object::cast_to<Node>(ClassDB::instantiate(class_name));
    if (!node) {
        print_line("Failed to instantiate node of type '" + class_name + "'");
        return nullptr;
    }

    // Validate name is not empty and set it
    String node_name = name.is_empty() ? class_name : name;
    node->set_name(node_name.validate_node_name());
    
    root->add_child(node);
    node->set_owner(root);

    return node;
}

void SceneBuilder::_setup_tilemap(TileMapLayer *tilemaplayer, const Dictionary &props) {
    ERR_FAIL_NULL(tilemaplayer);

    // Create and set up tileset
    if (props.has("tile_set")) {
        Dictionary tileset_props = props["tile_set"];
        Ref<TileSet> tileset;
        tileset.instantiate();

        int tile_width = tileset_props.has("tile_width") ? int(tileset_props["tile_width"]) : 16;
        int tile_height = tileset_props.has("tile_height") ? int(tileset_props["tile_height"]) : 16;

        Vector2i tile_size = Vector2i(tile_width, tile_height);

        tileset->set_tile_size(Vector2(tile_size));
        tilemaplayer->set_tile_set(tileset);

        print_line("Setting tile set for tilemap layer");

        if (tileset_props.has("texture")) {
            String texture_path = tileset_props["texture"];
            Ref<Texture2D> texture = ResourceLoader::load(texture_path);
            ERR_FAIL_COND_MSG(!texture.is_valid(), "Failed to load texture at path: " + texture_path);

            // Create atlas source and set basic properties
            Ref<TileSetAtlasSource> atlas_source;
            atlas_source.instantiate();
            atlas_source->set_texture(texture);
            
            atlas_source->set_texture_region_size(tile_size);
    
            // Add source to tileset
            tileset->add_source(atlas_source);
        }
        
    }
}

void SceneBuilder::_setup_node_properties(Node *node, const Dictionary &props) {
    ERR_FAIL_NULL(node);

    // Handle common Node2D properties
    if (Node2D *node2d = Object::cast_to<Node2D>(node)) {
        _setup_node2d_properties(node2d, props);
    }

    // Handle specific node types
    if (AnimatedSprite2D *sprite = Object::cast_to<AnimatedSprite2D>(node)) {
        _handle_animated_sprite(sprite, props);
    } else if (CollisionShape2D *collision = Object::cast_to<CollisionShape2D>(node)) {
        _setup_collision_shape(collision, props);
    } else if (Sprite2D *sprite = Object::cast_to<Sprite2D>(node)) {
        _setup_sprite(sprite, props);
    } else if (Camera2D *camera = Object::cast_to<Camera2D>(node)) {
        _setup_camera(camera, props);
    } else if (TileMapLayer *tilemaplayer = Object::cast_to<TileMapLayer>(node)) {
        _setup_tilemap(tilemaplayer, props);
    }

    // Set remaining generic properties
    _set_generic_properties(node, props);
}

void SceneBuilder::_setup_node2d_properties(Node2D *node2d, const Dictionary &props) {
    if (props.has("position")) {
        Dictionary pos = props["position"];
        ERR_FAIL_COND_MSG(!pos.has("x") || !pos.has("y"), "Invalid position format");
        node2d->set_position(Vector2(pos["x"], pos["y"]));
    }
}

void SceneBuilder::_handle_animated_sprite(AnimatedSprite2D *sprite, const Dictionary &props) {
    if (props.has("sprite_frames")) {
        _setup_sprite_frame_animation(sprite, props["sprite_frames"]);
    }
}

void SceneBuilder::_setup_camera(Camera2D *camera, const Dictionary &props) {
    if (props.has("zoom")) {
        Dictionary zoom = props["zoom"];
        ERR_FAIL_COND_MSG(!zoom.has("x") || !zoom.has("y"), "Invalid zoom format");
        
        // Convert zoom values to their inverse for Camera2D
        float zoom_x = float(zoom["x"]);
        float zoom_y = float(zoom["y"]);
        
        ERR_FAIL_COND_MSG(Math::is_zero_approx(zoom_x) || Math::is_zero_approx(zoom_y), 
            "Zoom values cannot be zero");
            
        camera->set_zoom(Vector2(zoom_x, zoom_y));
    }
}



void SceneBuilder::_set_generic_properties(Node *node, const Dictionary &props) {
    static HashSet<String> reserved_props;
    
    reserved_props.insert("position");
    reserved_props.insert("sprite_frames");
    reserved_props.insert("zoom");

    for (const Variant *key = props.next(); key; key = props.next(key)) {
        String prop_name = String(*key);
        if (!reserved_props.has(prop_name)) {
            node->set(prop_name, props[*key]);
        }
    }
}

void SceneBuilder::_setup_collision_shape(CollisionShape2D *collision, const Dictionary &props) {
	if (props.has("shape_type")) {
        Dictionary shape_pos = props["shape_position"];
        Vector2 position = props.has("shape_position") ? 
            Vector2(float(shape_pos["x"]), float(shape_pos["y"])) : 
            Vector2();

        if (props["shape_type"] == "RectangleShape2D") {
            Dictionary extents = props["shape_size"];
            Ref<RectangleShape2D> shape;
            shape.instantiate();
            shape->set_size(Vector2(extents["x"], extents["y"]));
            collision->set_shape(shape);
            collision->set_position(position);
        }
        else if (props["shape_type"] == "CircleShape2D") {
            float radius = props.has("shape_radius") ? float(props["shape_radius"]) : 1.0;
            Ref<CircleShape2D> shape;
            shape.instantiate();
            shape->set_radius(radius);
            collision->set_shape(shape);
            collision->set_position(position);
        } else if (props["shape_type"] == "WorldBoundaryShape2D") {
            Ref<WorldBoundaryShape2D> shape;
            shape.instantiate();
            collision->set_shape(shape);
            collision->set_position(position);
        } else {
            print_line("Unsupported shape type: " + String(props["shape_type"]));
        }
    }
    else {
        print_line("Missing 'shape_type' in properties");
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