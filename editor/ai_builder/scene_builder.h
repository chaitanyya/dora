#ifndef SCENE_BUILDER_H
#define SCENE_BUILDER_H

#include "core/object/ref_counted.h"
#include "scene/main/node.h"

class SceneBuilder : public RefCounted {
    GDCLASS(SceneBuilder, RefCounted);

    public:
        SceneBuilder();
        Node* create_scene_from_dict(const Dictionary& scene_data, Node* root);

    private:
        Node* _create_node_recursive(const Dictionary& node_data, Node* parent, Node* root);
        void _setup_node_properties(Node* node, const Dictionary& node_data);
        void _setup_collision_shape(class CollisionShape2D* collision, const Dictionary& props);
        void _setup_sprite(class Sprite2D* sprite, const Dictionary& props);
        void _setup_script(Node* node, const Dictionary& script_data);
        Node* _create_node_of_type(const String& type, const String& name, Node* root);
        void _set_node_property(Node* node, const String& property, const Variant& value);
        void _process_task(const Dictionary& task, Node* root);

    protected:
        static void _bind_methods();
};

#endif