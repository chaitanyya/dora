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
#include "scene/2d/node_2d.h" 
#include "scene/resources/packed_scene.h" 

void ChatDock::_bind_methods() {
}

void ChatDock::_text_submitted(const String &p_text) {
    if (p_text.is_empty()) {
        return;
    }

    // Add message to history
    messages.push_back(p_text);
    
    // Update chat display
    chat_history->add_text("You: " + p_text + "\n");
    
    // Handle scene creation command
    if (p_text == "scene") {
        Node2D *root = memnew(Node2D);
        root->set_name("game");
        
        // Create scene tree
        SceneTree *scene_tree = SceneTree::get_singleton();
        ERR_FAIL_NULL(scene_tree);
        
        // Create packed scene
        Ref<PackedScene> packed_scene;
        packed_scene.instantiate();
        packed_scene->pack(root);
        
        // Save the scene
        String scene_path = "res://game.tscn";
        Error err = ResourceSaver::save(packed_scene, scene_path);
        if (err == OK) {
            chat_history->add_text("Created new 2D scene 'game' at " + scene_path + "\n");
        } else {
            chat_history->add_text("Failed to create scene.\n");
        }
        
        memdelete(root); // Clean up the temporary root node
    }
    
    // Clear input field
    input_field->clear();
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