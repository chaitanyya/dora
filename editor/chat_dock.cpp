/**************************************************************************/
/*  chat_dock.cpp                                                         */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "chat_dock.h"

#include "core/io/json.h"
#include "modules/gdscript/gdscript.h"
#include "scene/2d/sprite_2d.h"
#include "scene/2d/physics/character_body_2d.h"
#include "scene/2d/physics/static_body_2d.h"
#include "scene/2d/physics/collision_shape_2d.h"
#include "scene/resources/packed_scene.h"
#include "scene/resources/2d/rectangle_shape_2d.h"

#include "ai_builder/scene_builder.h"

void ChatDock::_bind_methods() {
}

void ChatDock::_text_submitted(const String &p_text) {
    if (p_text.is_empty()) {
        return;
    }

    // Add message to history
    messages.push_back(p_text);
    chat_history->add_text("You: " + p_text + "\n");

    String json_str = R"({
  "scene": {
    "name": "JumpingCharacterScene",
    "type": "CharacterBody2D",
    "position": { "x": 400, "y": 100 },
    "children": [
        {
            "type": "Sprite2D",
            "name": "CharacterSprite",
            "properties": {
                "texture": "res://assets/sprites/knight.png",
                "frame_width": 35,
                "frame_height": 35,
                "frame_index": 0
            }
        },
        {
            "type": "CollisionShape2D",
            "name": "CharacterCollision",
            "properties": {
                "shape_type": "RectangleShape2D",
                "shape_extents": { "x": 16, "y": 32 }
            }
        }
    ],
    "script": {
      "language": "GDScript",
      "code": "extends CharacterBody2D\n\nvar gravity = 500.0\nvar jump_force = -400.0\n\nfunc _physics_process(delta):\n    if not is_on_floor():\n        velocity.y += gravity * delta\n    if is_on_floor():\n        velocity.y = jump_force\n    move_and_slide();"
    }
  },
  "ground": {
    "name": "Ground",
    "type": "StaticBody2D",
    "children": [
      {
        "type": "CollisionShape2D",
        "name": "GroundCollision",
        "properties": {
            "shape_type": "RectangleShape2D",
            "shape_extents": { "x": 400, "y": 32 }
        }
      }
    ],
    "position": { "x": 0, "y": 200 }
  }
})";

    JSON json;
    Error err = json.parse(json_str);
    if (err != OK) {
        chat_history->add_text("Failed to parse scene instructions.\n");
        return;
    }

    Dictionary scene_data = json.get_data();
    
    // Create main scene
    Node2D *root = memnew(Node2D);

    Ref<SceneBuilder> builder = memnew(SceneBuilder);
    builder->create_scene_from_dict(scene_data, root);

    Ref<PackedScene> packed_scene;
    packed_scene.instantiate();
    packed_scene->pack(root);
    
    Error save_err = ResourceSaver::save(packed_scene, "res://jumping_character.tscn");
    if (save_err == OK) {
        chat_history->add_text("Created jumping character scene at res://jumping_character.tscn\n");
    } else {
        chat_history->add_text("Failed to save scene.\n");
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