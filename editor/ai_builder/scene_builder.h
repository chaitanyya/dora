#ifndef SCENE_BUILDER_H
#define SCENE_BUILDER_H

#include "core/object/ref_counted.h"
#include "scene/main/node.h"
#include "scene/2d/node_2d.h"
#include "scene/2d/animated_sprite_2d.h"
#include "scene/2d/camera_2d.h"
#include "scene/2d/tile_map.h"

class SceneBuilder : public RefCounted {
    GDCLASS(SceneBuilder, RefCounted);

    public:
        SceneBuilder();
        Node* create_scene_from_dict(const Array &tasks, Node *root);

    private:
        Node* _create_node_recursive(const Dictionary& node_data, Node* parent, Node* root);
        void _setup_node_properties(Node* node, const Dictionary& node_data);
        void _setup_collision_shape(class CollisionShape2D* collision, const Dictionary& props);
        void _setup_sprite(class Sprite2D* sprite, const Dictionary& props);
        void _setup_script(Node* node, const Dictionary& script_data);
        Node* _create_node_of_class(const String& type, const String& name, Node* root);
        void _process_task(const Dictionary& task, Node* root);
        void _setup_sprite_frame_animation(Node *node, const Dictionary &props);

        // Helper functions
        void _setup_node2d_properties(Node2D *node2d, const Dictionary &props);
        void _handle_animated_sprite(AnimatedSprite2D *sprite, const Dictionary &props);
        void _setup_camera(Camera2D *camera, const Dictionary &props);
        void _set_generic_properties(Node *node, const Dictionary &props);
        void _setup_tilemap(TileMapLayer *tilemaplayer, const Dictionary &props);

    protected:
        static void _bind_methods();
};

#endif