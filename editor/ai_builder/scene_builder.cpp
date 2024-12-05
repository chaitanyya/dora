#include "scene_builder.h"

#include "core/object/class_db.h"
#include "scene/2d/sprite_2d.h"
#include "scene/2d/physics/collision_shape_2d.h"
#include "scene/resources/2d/rectangle_shape_2d.h"
#include "modules/gdscript/gdscript.h"

SceneBuilder::SceneBuilder() {
}

void SceneBuilder::_bind_methods() {
}

Node* SceneBuilder::create_scene_from_dict(const Dictionary& scene_data, Node* root) {
    if (!root) {
        root = memnew(Node2D);
        root->set_name("RootNode");
    }

    // Create scene hierarchy from JSON
    Node* scene_node = _create_node_recursive(scene_data["scene"], root, root);
    if (scene_data.has("ground")) {
        _create_node_recursive(scene_data["ground"], root, root);
    }
    return scene_node;
}

Node* SceneBuilder::_create_node_recursive(const Dictionary& node_data, Node* parent, Node* root) {
    String type = node_data["type"];
    String name = node_data["name"];
    
    // Debug log to verify node data
    print_line("Creating node of type: " + type + ", name: " + name);

    Node* node = _create_node_of_type(type, name, parent, root);
    if (!node) return nullptr;

    // Setup basic properties
    _setup_node_properties(node, node_data);

    // Handle children recursively
    if (node_data.has("children")) {
        Array children = node_data["children"];
        for (int i = 0; i < children.size(); i++) {
            _create_node_recursive(children[i], node, root);
        }
    }

    return node;
}

Node* SceneBuilder::_create_node_of_type(const String& type, const String& name, Node* parent, Node* root) {
    if (!ClassDB::class_exists(type)) return nullptr;
    
    Node* node = Object::cast_to<Node>(ClassDB::instantiate(type));
    if (!node) return nullptr;

    node->set_name(name);
    parent->add_child(node);
    node->set_owner(root);
    
    return node;
}

void SceneBuilder::_setup_node_properties(Node* node, const Dictionary& node_data) {
    // Handle position
    if (node_data.has("position")) {
        Dictionary pos = node_data["position"];
        if (Node2D* node2d = Object::cast_to<Node2D>(node)) {
            node2d->set_position(Vector2(pos["x"], pos["y"]));
        }
    }

    // Handle properties
    if (node_data.has("properties")) {
        Dictionary props = node_data["properties"];
        
        if (CollisionShape2D* collision = Object::cast_to<CollisionShape2D>(node)) {
            _setup_collision_shape(collision, props);
        } else if (Sprite2D* sprite = Object::cast_to<Sprite2D>(node)) {
            _setup_sprite(sprite, props);
        } else {
            for (const Variant* key = props.next(); key; key = props.next(key)) {
                node->set(String(*key), props[*key]);
            }
        }
    }

    // Handle script
    if (node_data.has("script")) {
        _setup_script(node, node_data["script"]);
    }
}

void SceneBuilder::_setup_collision_shape(CollisionShape2D* collision, const Dictionary& props) {
    if (props.has("shape_type") && props["shape_type"] == "RectangleShape2D") {
        Dictionary extents = props["shape_extents"];
        Ref<RectangleShape2D> shape;
        shape.instantiate();
        shape->set_size(Vector2(extents["x"], extents["y"]) * 2.0);
        collision->set_shape(shape);
    }
}

void SceneBuilder::_setup_sprite(Sprite2D* sprite, const Dictionary& props) {
    if (!props.has("texture")) return;

    String texture_path = props["texture"];
    Ref<Texture2D> texture = ResourceLoader::load(texture_path);
    if (!texture.is_valid()) return;

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

void SceneBuilder::_setup_script(Node* node, const Dictionary& script_data) {
    if (script_data["language"] == "GDScript") {
        Ref<GDScript> script = memnew(GDScript);
        script->set_source_code(script_data["code"]);
        script->reload();
        node->set_script(script);
    }
}